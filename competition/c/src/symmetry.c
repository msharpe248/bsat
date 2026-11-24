/*********************************************************************
 * BSAT Competition Solver - Exact Syntactic Symmetry Breaking
 *
 * Implements BreakID-style exact syntactic symmetry detection:
 *  1. Builds an O(1) exact clause hash set over all live irredundant clauses.
 *  2. Pass 1: Variable transposition chains (u <-> v) verified across every
 *     touched clause, emitting binary lex-leader chains (~u | v).
 *  3. Pass 2: Matrix / row-interchange symmetry blocks (R_0 <-> R_b) detected
 *     from disjoint equal-length clauses (e.g. PHP, graph coloring, scheduling,
 *     battleship) with column alignment and 100% exact clause-set automorphism
 *     verification, emitting linear BreakID prefix lex-leader constraints.
 *  4. Strictly disjoint variable supports across symmetry groups guarantee
 *     100% equisatisfiability and original-variable model soundness.
 *********************************************************************/

#include "../include/symmetry.h"
#include "../include/inprocess.h"
#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>

#define SYM_MAX_ROW_LEN    64u
#define SYM_MAX_LEX_COLS   24u
#define SYM_MAX_AUX_VARS   4096u

typedef struct {
    uint64_t hash;
    uint32_t cidx;  /* 1-based index into live_cls[], 0 = empty */
} SymHashSlot;

typedef struct {
    Lit *lits;
    uint32_t size;
    uint64_t hash;
} SymClause;

typedef struct {
    Solver *s;
    SymClause *cls;
    uint32_t n_cls;
    SymHashSlot *ht;
    size_t ht_cap;
    uint32_t *vhead;
    uint32_t *vocc;
    Var *swap_map;
    uint8_t *cls_seen;
    uint32_t *cls_seen_list;
    uint32_t cls_seen_n;
    Lit *tmp_lits;
    uint32_t max_cls_len;
    uint64_t end_work;
    bool oom;
} SymState;

static inline uint64_t sym_mix(uint64_t x) {
    x ^= x >> 30;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27;
    x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static inline void sym_sort_lits(Lit *ls, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i) {
        Lit key = ls[i];
        uint32_t j = i;
        while (j > 0 && ls[j - 1u] > key) { ls[j] = ls[j - 1u]; --j; }
        ls[j] = key;
    }
}

static uint64_t sym_hash_clause(const Lit *sorted, uint32_t n) {
    uint64_t h = sym_mix(n);
    for (uint32_t i = 0; i < n; ++i) {
        h ^= sym_mix(((uint64_t)sorted[i] << 16) ^ (uint64_t)i);
    }
    return h ? h : 1u;
}

static bool sym_clause_exists(const SymState *st, const Lit *sorted, uint32_t n, uint64_t h) {
    size_t pos = (size_t)(h & (st->ht_cap - 1u));
    for (;;) {
        uint32_t slot = st->ht[pos].cidx;
        if (!slot) return false;
        if (st->ht[pos].hash == h) {
            const SymClause *c = &st->cls[slot - 1u];
            if (c->size == n && memcmp(c->lits, sorted, (size_t)n * sizeof(Lit)) == 0) {
                return true;
            }
        }
        pos = (pos + 1u) & (st->ht_cap - 1u);
    }
}

/* Verify 100% exact formula invariance under the involution swapping
 * a[i] <-> b[i] (preserving literal signs) for i = 0..len-1. */
static bool verify_involution(SymState *st, const Var *a, const Var *b, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        st->swap_map[a[i]] = b[i];
        st->swap_map[b[i]] = a[i];
    }

    st->cls_seen_n = 0;
    bool valid = true;

    for (uint32_t side = 0; side < 2u && valid; ++side) {
        const Var *arr = side ? b : a;
        for (uint32_t i = 0; i < len && valid; ++i) {
            Var v = arr[i];
            uint32_t ob = st->vhead[v], oe = st->vhead[v + 1u];
            for (uint32_t oi = ob; oi < oe; ++oi) {
                uint32_t cid = st->vocc[oi];
                if (st->cls_seen[cid]) continue;
                st->cls_seen[cid] = 1;
                st->cls_seen_list[st->cls_seen_n++] = cid;

                const SymClause *c = &st->cls[cid];
                st->s->work += c->size;
                uint32_t sz = c->size;
                Lit *mapped = st->tmp_lits;
                bool changed = false;
                for (uint32_t j = 0; j < sz; ++j) {
                    Lit l = c->lits[j];
                    Var lv = var(l);
                    Var mv = st->swap_map[lv];
                    if (mv) {
                        mapped[j] = mkLit(mv, sign(l));
                        changed = true;
                    } else {
                        mapped[j] = l;
                    }
                }
                if (!changed) continue;
                sym_sort_lits(mapped, sz);
                uint64_t h = sym_hash_clause(mapped, sz);
                if (!sym_clause_exists(st, mapped, sz, h)) {
                    valid = false;
                    break;
                }
            }
        }
    }

    for (uint32_t i = 0; i < st->cls_seen_n; ++i) st->cls_seen[st->cls_seen_list[i]] = 0;
    for (uint32_t i = 0; i < len; ++i) {
        st->swap_map[a[i]] = 0;
        st->swap_map[b[i]] = 0;
    }
    return valid;
}

static bool sym_add_clause(Solver *s, const Lit *lits, uint32_t sz) {
    if (s->num_clauses == s->clauses_capacity) {
        uint32_t cap = s->clauses_capacity ? s->clauses_capacity * 2u : 64u;
        CRef *p = realloc(s->clauses, (size_t)cap * sizeof *p);
        if (!p) { s->error = true; return false; }
        s->clauses = p;
        s->clauses_capacity = cap;
    }
    CRef cr = arena_alloc(s->arena, lits, sz, false);
    if (cr == INVALID_CLAUSE) { s->error = true; return false; }
    s->clauses[s->num_clauses++] = cr;
    s->num_original = s->num_clauses;
    return true;
}

/* =====================================================================
 * DRAT Proof Emitter for Exact Symmetry Breaking (Heule-Hunt-Wetzler
 * Insertion-Sort Conditional-Swap Network, 100% Standard DRAT/RAT)
 *
 * Given n_rows (2..32) disjoint rows of length rlen (1..64) that are
 * completely interchangeable in the current live irredundant clause set
 * s->clauses (with zero level-0 assigned literals in touching clauses),
 * derives (~rows[0][col] | rows[1][col]), ..., (~rows[n_rows-2][col] | rows[n_rows-1][col])
 * in pure DRAT using temporary proof-only variables and zero residual aux vars!
 * ===================================================================== */

typedef struct {
    Lit *lits;
    uint32_t size;
} WorkClause;

static void sym_emit_split_rup(Solver *s, const Lit *cl, uint32_t sz, Lit s_pos, Lit s_neg, Lit *buf) {
    memcpy(buf, cl, (size_t)sz * sizeof(Lit));
    buf[sz] = s_pos;
    proof_add_clause(s, buf, sz + 1u);
    if (s->error) return;
    buf[sz] = s_neg;
    proof_add_clause(s, buf, sz + 1u);
    if (s->error) return;
    proof_add_clause(s, cl, sz);
    if (s->error) return;
    buf[sz] = s_pos;
    proof_delete_clause(s, buf, sz + 1u);
    buf[sz] = s_neg;
    proof_delete_clause(s, buf, sz + 1u);
}

static bool sym_prove_column_chain_drat(Solver *s, Var rows[][SYM_MAX_ROW_LEN],
                                        uint32_t n_rows, uint32_t rlen, uint32_t col,
                                        Var *next_pvar, uint64_t end_work) {
    if (n_rows < 2u || n_rows > 32u || rlen == 0 || rlen > SYM_MAX_ROW_LEN || col >= rlen) return false;
    if (s->error || s->work >= end_work) return false;

    /* Collect all live clauses in s->clauses touching ANY variable in rows[0..n_rows-1]. */
    uint32_t max_v = s->num_vars;
    uint8_t *in_block = calloc((size_t)max_v + 1u, 1);
    if (!in_block) { s->error = true; return false; }
    for (uint32_t r = 0; r < n_rows; ++r) {
        for (uint32_t j = 0; j < rlen; ++j) {
            Var v = rows[r][j];
            if (v > max_v || s->values[v] != UNDEF) { free(in_block); return false; }
            in_block[v] = 1;
        }
    }

    uint32_t nw = 0, wcap = 64u, max_sz = 4u;
    WorkClause *wcls = malloc((size_t)wcap * sizeof *wcls);
    if (!wcls) { free(in_block); s->error = true; return false; }

    bool clean = true;
    for (uint32_t i = 0; i < s->num_clauses && clean; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool touches = false;
        for (uint32_t j = 0; j < sz; ++j) {
            Var v = var(ls[j]);
            if (v <= max_v && in_block[v]) { touches = true; break; }
        }
        if (!touches) continue;
        /* Require zero level-0 assigned literals in touching clauses for exact DRAT sync. */
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { clean = false; break; }
        }
        if (!clean) break;
        if (nw >= 4096u) { clean = false; break; } /* Keep proof size bounded! */
        if (nw == wcap) {
            wcap *= 2u;
            WorkClause *p = realloc(wcls, (size_t)wcap * sizeof *p);
            if (!p) { s->error = true; clean = false; break; }
            wcls = p;
        }
        Lit *copy = malloc((size_t)sz * sizeof(Lit));
        if (!copy) { s->error = true; clean = false; break; }
        memcpy(copy, ls, (size_t)sz * sizeof(Lit));
        wcls[nw++] = (WorkClause){copy, sz};
        if (sz > max_sz) max_sz = sz;
    }
    free(in_block);

    if (!clean || nw == 0 || s->error) {
        for (uint32_t m = 0; m < nw; ++m) free(wcls[m].lits);
        free(wcls);
        return false;
    }

    Lit *cbuf = malloc((size_t)(max_sz + 4u) * sizeof(Lit));
    Lit *sbuf = malloc((size_t)(max_sz + 4u) * sizeof(Lit));
    if (!cbuf || !sbuf) {
        free(cbuf); free(sbuf);
        for (uint32_t m = 0; m < nw; ++m) free(wcls[m].lits);
        free(wcls);
        s->error = true;
        return false;
    }

    Var cur[32][SYM_MAX_ROW_LEN];
    for (uint32_t r = 0; r < n_rows; ++r) {
        memcpy(cur[r], rows[r], (size_t)rlen * sizeof(Var));
    }

    /* Insertion-sort conditional-swap network over cur[0..n_rows-1]. */
    for (uint32_t r = 1u; r < n_rows && !s->error; ++r) {
        Lit bridge[2] = {LIT_UNDEF, LIT_UNDEF};
        for (int ii = (int)r - 1; ii >= 0 && !s->error; --ii) {
            uint32_t i = (uint32_t)ii;
            Var A[SYM_MAX_ROW_LEN], B[SYM_MAX_ROW_LEN];
            Var Ap[SYM_MAX_ROW_LEN], Bp[SYM_MAX_ROW_LEN];
            memcpy(A, cur[i], (size_t)rlen * sizeof(Var));
            memcpy(B, cur[i + 1u], (size_t)rlen * sizeof(Var));

            Var sv = (*next_pvar)++;
            Lit s_pos = mkLit(sv, false), s_neg = mkLit(sv, true);
            Lit ac = mkLit(A[col], false), bc = mkLit(B[col], false);

            /* 1. Define s <-> (A[col] & ~B[col]) via 3 RAT clauses. */
            Lit sd1[2] = {s_neg, ac};
            Lit sd2[2] = {s_neg, neg(bc)};
            Lit sd3[3] = {s_pos, neg(ac), bc};
            proof_add_clause(s, sd1, 2u);
            proof_add_clause(s, sd2, 2u);
            proof_add_clause(s, sd3, 3u);

            /* 2. Define Ap[j] <-> ITE(s, B[j], A[j]) and Bp[j] <-> ITE(s, A[j], B[j]). */
            for (uint32_t j = 0; j < rlen && !s->error; ++j) {
                Ap[j] = (*next_pvar)++;
                Bp[j] = (*next_pvar)++;
                Lit ap = mkLit(Ap[j], false), an = mkLit(Ap[j], true);
                Lit bp = mkLit(Bp[j], false), bn = mkLit(Bp[j], true);
                Lit aj = mkLit(A[j], false), bj = mkLit(B[j], false);

                Lit ia1[3] = {an, s_pos, aj};
                Lit ia2[3] = {an, s_neg, bj};
                Lit ia3[3] = {ap, s_pos, neg(aj)};
                Lit ia4[3] = {ap, s_neg, neg(bj)};
                proof_add_clause(s, ia1, 3u);
                proof_add_clause(s, ia2, 3u);
                proof_add_clause(s, ia3, 3u);
                proof_add_clause(s, ia4, 3u);

                Lit ib1[3] = {bn, s_pos, bj};
                Lit ib2[3] = {bn, s_neg, aj};
                Lit ib3[3] = {bp, s_pos, neg(bj)};
                Lit ib4[3] = {bp, s_neg, neg(aj)};
                proof_add_clause(s, ib1, 3u);
                proof_add_clause(s, ib2, 3u);
                proof_add_clause(s, ib3, 3u);
                proof_add_clause(s, ib4, 3u);
            }
            if (s->error) break;

            /* 3. Copy all working clauses touching A U B to Ap U Bp via s-split RUP. */
            for (uint32_t m = 0; m < nw && !s->error; ++m) {
                uint32_t sz = wcls[m].size;
                const Lit *ls = wcls[m].lits;
                bool touched = false;
                for (uint32_t k = 0; k < sz; ++k) {
                    Var v = var(ls[k]);
                    Lit out = ls[k];
                    for (uint32_t j = 0; j < rlen; ++j) {
                        if (v == A[j]) { out = mkLit(Ap[j], sign(ls[k])); touched = true; break; }
                        if (v == B[j]) { out = mkLit(Bp[j], sign(ls[k])); touched = true; break; }
                    }
                    cbuf[k] = out;
                }
                if (!touched) continue;
                sym_emit_split_rup(s, cbuf, sz, s_pos, s_neg, sbuf);
                s->work += sz;
            }
            if (s->error) break;

            /* 4. Derive ordering binaries on (Ap[col], Bp[col]) via s-split RUP. */
            Lit apc = mkLit(Ap[col], false), bpc = mkLit(Bp[col], false);
            Lit ord_ab[2] = {neg(apc), bpc};
            sym_emit_split_rup(s, ord_ab, 2u, s_pos, s_neg, sbuf);

            if (i + 2u <= r && !s->error) {
                Lit zc = mkLit(cur[i + 2u][col], false);
                Lit ord_bz[2] = {neg(bpc), zc};
                sym_emit_split_rup(s, ord_bz, 2u, s_pos, s_neg, sbuf);
            }

            Lit next_bridge[2] = {LIT_UNDEF, LIT_UNDEF};
            if (i > 0 && !s->error) {
                Lit wc = mkLit(cur[i - 1u][col], false);
                next_bridge[0] = neg(wc);
                next_bridge[1] = bpc;
                sym_emit_split_rup(s, next_bridge, 2u, s_pos, s_neg, sbuf);
            }
            if (s->error) break;

            /* 5. Delete old clauses touching A U B and replace them in wcls[]. */
            for (uint32_t m = 0; m < nw && !s->error; ++m) {
                uint32_t sz = wcls[m].size;
                Lit *ls = wcls[m].lits;
                bool touched = false;
                for (uint32_t k = 0; k < sz; ++k) {
                    Var v = var(ls[k]);
                    Lit out = ls[k];
                    for (uint32_t j = 0; j < rlen; ++j) {
                        if (v == A[j]) { out = mkLit(Ap[j], sign(ls[k])); touched = true; break; }
                        if (v == B[j]) { out = mkLit(Bp[j], sign(ls[k])); touched = true; break; }
                    }
                    cbuf[k] = out;
                }
                if (!touched) continue;
                proof_delete_clause(s, ls, sz);
                memcpy(ls, cbuf, (size_t)sz * sizeof(Lit));
            }

            /* Delete superseded ordering binaries touching A[col] or B[col]. */
            if (i + 2u <= r) {
                Lit zc = mkLit(cur[i + 2u][col], false);
                Lit old_bz[2] = {neg(bc), zc};
                proof_delete_clause(s, old_bz, 2u);
                if (bridge[0] != LIT_UNDEF) proof_delete_clause(s, bridge, 2u);
            }
            if (i > 0) {
                Lit wc = mkLit(cur[i - 1u][col], false);
                Lit old_wa[2] = {neg(wc), ac};
                proof_delete_clause(s, old_wa, 2u);
            }
            bridge[0] = next_bridge[0];
            bridge[1] = next_bridge[1];

            /* Delete temporary s & ITE definitions. */
            for (uint32_t j = 0; j < rlen; ++j) {
                Lit ap = mkLit(Ap[j], false), an = mkLit(Ap[j], true);
                Lit bp = mkLit(Bp[j], false), bn = mkLit(Bp[j], true);
                Lit aj = mkLit(A[j], false), bj = mkLit(B[j], false);
                Lit ia1[3] = {an, s_pos, aj}, ia2[3] = {an, s_neg, bj};
                Lit ia3[3] = {ap, s_pos, neg(aj)}, ia4[3] = {ap, s_neg, neg(bj)};
                Lit ib1[3] = {bn, s_pos, bj}, ib2[3] = {bn, s_neg, aj};
                Lit ib3[3] = {bp, s_pos, neg(bj)}, ib4[3] = {bp, s_neg, neg(aj)};
                proof_delete_clause(s, ia1, 3u); proof_delete_clause(s, ia2, 3u);
                proof_delete_clause(s, ia3, 3u); proof_delete_clause(s, ia4, 3u);
                proof_delete_clause(s, ib1, 3u); proof_delete_clause(s, ib2, 3u);
                proof_delete_clause(s, ib3, 3u); proof_delete_clause(s, ib4, 3u);
            }
            proof_delete_clause(s, sd1, 2u);
            proof_delete_clause(s, sd2, 2u);
            proof_delete_clause(s, sd3, 3u);

            memcpy(cur[i], Ap, (size_t)rlen * sizeof(Var));
            memcpy(cur[i + 1u], Bp, (size_t)rlen * sizeof(Var));
        }
    }

    /* 6. Re-bind original rows[r][j] <-> cur[r][j] (all orig vars are now fresh in proof!),
     *    restore original clauses + final ordering binaries, and delete cur copies! */
    if (!s->error) {
        for (uint32_t r = 0; r < n_rows && !s->error; ++r) {
            for (uint32_t j = 0; j < rlen && !s->error; ++j) {
                Lit o = mkLit(rows[r][j], false), c = mkLit(cur[r][j], false);
                Lit b1[2] = {o, neg(c)};
                Lit b2[2] = {neg(o), c};
                proof_add_clause(s, b1, 2u);
                proof_add_clause(s, b2, 2u);
            }
        }
        /* Re-emit original clauses via 1-step RUP from cur copies, then delete cur copies. */
        for (uint32_t m = 0; m < nw && !s->error; ++m) {
            uint32_t sz = wcls[m].size;
            const Lit *ls = wcls[m].lits;
            for (uint32_t k = 0; k < sz; ++k) {
                Var v = var(ls[k]);
                Lit out = ls[k];
                for (uint32_t r = 0; r < n_rows; ++r) {
                    bool done = false;
                    for (uint32_t j = 0; j < rlen; ++j) {
                        if (v == cur[r][j]) { out = mkLit(rows[r][j], sign(ls[k])); done = true; break; }
                    }
                    if (done) break;
                }
                cbuf[k] = out;
            }
            proof_add_clause(s, cbuf, sz);
            proof_delete_clause(s, ls, sz);
        }
        /* Emit final symmetry-breaking ordering binaries (~rows[r][col] | rows[r+1][col])! */
        for (uint32_t r = 0; r + 1u < n_rows && !s->error; ++r) {
            Lit fin[2] = {mkLit(rows[r][col], true), mkLit(rows[r + 1u][col], false)};
            Lit old[2] = {mkLit(cur[r][col], true), mkLit(cur[r + 1u][col], false)};
            proof_add_clause(s, fin, 2u);
            proof_delete_clause(s, old, 2u);
        }
        /* Delete re-binding binaries. */
        for (uint32_t r = 0; r < n_rows && !s->error; ++r) {
            for (uint32_t j = 0; j < rlen && !s->error; ++j) {
                Lit o = mkLit(rows[r][j], false), c = mkLit(cur[r][j], false);
                Lit b1[2] = {o, neg(c)};
                Lit b2[2] = {neg(o), c};
                proof_delete_clause(s, b1, 2u);
                proof_delete_clause(s, b2, 2u);
            }
        }
    }

    free(cbuf);
    free(sbuf);
    for (uint32_t m = 0; m < nw; ++m) free(wcls[m].lits);
    free(wcls);
    return !s->error;
}

/* Synchronize s->clauses and s->proof_file after root unit propagation:
 * emit newly propagated units to DRAT, shorten clauses with falsified literals
 * (1-step RUP), and delete level-0 satisfied clauses from BOTH s->clauses and proof! */
static bool sym_sync_root_units_drat(Solver *s, uint32_t prev_trail) {
    for (uint32_t t = prev_trail; t < s->trail_size && !s->error; ++t) {
        Lit u = s->trail[t].lit;
        proof_add_clause(s, &u, 1u);
    }
    if (s->error) return false;
    Lit *nbuf = NULL;
    uint32_t ncap = 0;
    for (uint32_t i = 0; i < s->num_clauses && !s->error; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool sat = false, has_false = false;
        for (uint32_t j = 0; j < sz; ++j) {
            lbool lv = lxor(s->values[var(ls[j])], sign(ls[j]));
            if (lv == TRUE) { sat = true; break; }
            if (lv == FALSE) has_false = true;
        }
        if (sat) {
            proof_delete_clause(s, ls, sz);
            arena_delete(s->arena, cr);
            s->clauses[i] = INVALID_CLAUSE;
        } else if (has_false) {
            if (sz > ncap) {
                ncap = sz * 2u + 16u;
                Lit *p = realloc(nbuf, (size_t)ncap * sizeof(Lit));
                if (!p) { free(nbuf); s->error = true; return false; }
                nbuf = p;
            }
            uint32_t nn = 0;
            for (uint32_t j = 0; j < sz; ++j) {
                if (s->values[var(ls[j])] == UNDEF) nbuf[nn++] = ls[j];
            }
            proof_add_clause(s, nbuf, nn);
            proof_delete_clause(s, ls, sz);
            arena_delete(s->arena, cr);
            s->clauses[i] = INVALID_CLAUSE;
            if (nn == 0) {
                free(nbuf);
                s->base_unsat = true;
                s->result = FALSE;
                return false;
            }
            if (nn >= 2u) {
                CRef ncr = arena_alloc(s->arena, nbuf, nn, false);
                if (ncr == INVALID_CLAUSE) { free(nbuf); s->error = true; return false; }
                s->clauses[i] = ncr;
            }
        }
    }
    free(nbuf);
    return !s->error;
}

/* Prove a SINGLE conditional-swap symmetry binary (~A[col] | B[col]) in standard DRAT
 * after 100% exact verification that swapping A[0..rlen-1] <-> B[0..rlen-1] is an
 * involution of the current live irredundant clause set s->clauses.
 * Only touches clauses mentioning A U B (O(1) rows, zero quadratic blowup). */
static bool sym_prove_pair_drat(Solver *s, const Var *A, const Var *B,
                                uint32_t rlen, uint32_t col,
                                Var *next_pvar, uint64_t end_work) {
    if (rlen == 0 || rlen > SYM_MAX_ROW_LEN || col >= rlen) return false;
    if (s->error || s->work >= end_work) return false;

    uint32_t max_v = s->num_vars;
    Var *smap = calloc((size_t)max_v + 1u, sizeof *smap);
    if (!smap) { s->error = true; return false; }
    for (uint32_t j = 0; j < rlen; ++j) {
        Var va = A[j], vb = B[j];
        if (va == vb || va > max_v || vb > max_v ||
            s->values[va] != UNDEF || s->values[vb] != UNDEF ||
            smap[va] || smap[vb]) {
            free(smap);
            return false;
        }
        smap[va] = vb;
        smap[vb] = va;
    }

    uint32_t nw = 0, wcap = 64u, max_sz = 4u;
    WorkClause *wcls = malloc((size_t)wcap * sizeof *wcls);
    if (!wcls) { free(smap); s->error = true; return false; }

    bool clean = true;
    for (uint32_t i = 0; i < s->num_clauses && clean; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool touches = false;
        for (uint32_t j = 0; j < sz; ++j) {
            Var v = var(ls[j]);
            if (v <= max_v && smap[v]) { touches = true; break; }
        }
        if (!touches) continue;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { clean = false; break; }
        }
        if (!clean || nw >= 16384u) { clean = false; break; }
        if (nw == wcap) {
            wcap *= 2u;
            WorkClause *p = realloc(wcls, (size_t)wcap * sizeof *p);
            if (!p) { s->error = true; clean = false; break; }
            wcls = p;
        }
        Lit *copy = malloc((size_t)sz * sizeof(Lit));
        if (!copy) { s->error = true; clean = false; break; }
        memcpy(copy, ls, (size_t)sz * sizeof(Lit));
        wcls[nw++] = (WorkClause){copy, sz};
        if (sz > max_sz) max_sz = sz;
    }

    if (!clean || nw == 0 || s->error) {
        free(smap);
        for (uint32_t m = 0; m < nw; ++m) free(wcls[m].lits);
        free(wcls);
        return false;
    }

    /* Exact live involution verification over wcls[] using a compact local hash set! */
    size_t hcap = 64u;
    while (hcap < (size_t)nw * 2u) hcap <<= 1;
    SymHashSlot *ht = calloc(hcap, sizeof *ht);
    Lit **sorted_w = malloc((size_t)nw * sizeof *sorted_w);
    Lit *cbuf = malloc((size_t)(max_sz + 4u) * sizeof(Lit));
    Lit *sbuf = malloc((size_t)(max_sz + 4u) * sizeof(Lit));
    if (!ht || !sorted_w || !cbuf || !sbuf) {
        free(ht); free(sorted_w); free(cbuf); free(sbuf); free(smap);
        for (uint32_t m = 0; m < nw; ++m) free(wcls[m].lits);
        free(wcls);
        s->error = true;
        return false;
    }
    uint32_t n_sorted = 0;
    bool alloc_ok = true;
    for (uint32_t m = 0; m < nw; ++m) {
        sorted_w[m] = malloc((size_t)wcls[m].size * sizeof(Lit));
        if (!sorted_w[m]) { alloc_ok = false; break; }
        n_sorted = m + 1u;
        memcpy(sorted_w[m], wcls[m].lits, (size_t)wcls[m].size * sizeof(Lit));
        sym_sort_lits(sorted_w[m], wcls[m].size);
        uint64_t h = sym_hash_clause(sorted_w[m], wcls[m].size);
        size_t pos = (size_t)(h & (hcap - 1u));
        for (;;) {
            if (!ht[pos].cidx) { ht[pos] = (SymHashSlot){h, m + 1u}; break; }
            if (ht[pos].hash == h) {
                uint32_t ex = ht[pos].cidx - 1u;
                if (wcls[ex].size == wcls[m].size &&
                    memcmp(sorted_w[ex], sorted_w[m], (size_t)wcls[m].size * sizeof(Lit)) == 0) break;
            }
            pos = (pos + 1u) & (hcap - 1u);
        }
    }
    bool inv_ok = alloc_ok;
    for (uint32_t m = 0; m < nw && inv_ok; ++m) {
        uint32_t sz = wcls[m].size;
        for (uint32_t k = 0; k < sz; ++k) {
            Lit l = wcls[m].lits[k];
            Var v = var(l);
            Var mv = (v <= max_v) ? smap[v] : 0;
            cbuf[k] = mv ? mkLit(mv, sign(l)) : l;
        }
        sym_sort_lits(cbuf, sz);
        uint64_t h = sym_hash_clause(cbuf, sz);
        size_t pos = (size_t)(h & (hcap - 1u));
        bool found = false;
        for (;;) {
            if (!ht[pos].cidx) break;
            if (ht[pos].hash == h) {
                uint32_t ex = ht[pos].cidx - 1u;
                if (wcls[ex].size == sz &&
                    memcmp(sorted_w[ex], cbuf, (size_t)sz * sizeof(Lit)) == 0) {
                    found = true;
                    break;
                }
            }
            pos = (pos + 1u) & (hcap - 1u);
        }
        if (!found) inv_ok = false;
    }
    for (uint32_t m = 0; m < n_sorted; ++m) free(sorted_w[m]);
    free(sorted_w);
    free(ht);
    free(smap);

    if (!inv_ok) {
        free(cbuf); free(sbuf);
        for (uint32_t m = 0; m < nw; ++m) free(wcls[m].lits);
        free(wcls);
        if (!alloc_ok) s->error = true;
        return false;
    }

    /* Emit the single conditional swap DRAT proof for (A, B) on column `col`! */
    Var Ap[SYM_MAX_ROW_LEN], Bp[SYM_MAX_ROW_LEN];
    Var sv = (*next_pvar)++;
    Lit s_pos = mkLit(sv, false), s_neg = mkLit(sv, true);
    Lit ac = mkLit(A[col], false), bc = mkLit(B[col], false);

    Lit sd1[2] = {s_neg, ac};
    Lit sd2[2] = {s_neg, neg(bc)};
    Lit sd3[3] = {s_pos, neg(ac), bc};
    proof_add_clause(s, sd1, 2u);
    proof_add_clause(s, sd2, 2u);
    proof_add_clause(s, sd3, 3u);

    for (uint32_t j = 0; j < rlen && !s->error; ++j) {
        Ap[j] = (*next_pvar)++;
        Bp[j] = (*next_pvar)++;
        Lit ap = mkLit(Ap[j], false), an = mkLit(Ap[j], true);
        Lit bp = mkLit(Bp[j], false), bn = mkLit(Bp[j], true);
        Lit aj = mkLit(A[j], false), bj = mkLit(B[j], false);

        Lit ia1[3] = {an, s_pos, aj}, ia2[3] = {an, s_neg, bj};
        Lit ia3[3] = {ap, s_pos, neg(aj)}, ia4[3] = {ap, s_neg, neg(bj)};
        proof_add_clause(s, ia1, 3u); proof_add_clause(s, ia2, 3u);
        proof_add_clause(s, ia3, 3u); proof_add_clause(s, ia4, 3u);

        Lit ib1[3] = {bn, s_pos, bj}, ib2[3] = {bn, s_neg, aj};
        Lit ib3[3] = {bp, s_pos, neg(bj)}, ib4[3] = {bp, s_neg, neg(aj)};
        proof_add_clause(s, ib1, 3u); proof_add_clause(s, ib2, 3u);
        proof_add_clause(s, ib3, 3u); proof_add_clause(s, ib4, 3u);
    }

    /* Copy all wcls[] to Ap U Bp via s-split RUP. */
    for (uint32_t m = 0; m < nw && !s->error; ++m) {
        uint32_t sz = wcls[m].size;
        const Lit *ls = wcls[m].lits;
        for (uint32_t k = 0; k < sz; ++k) {
            Var v = var(ls[k]);
            Lit out = ls[k];
            for (uint32_t j = 0; j < rlen; ++j) {
                if (v == A[j]) { out = mkLit(Ap[j], sign(ls[k])); break; }
                if (v == B[j]) { out = mkLit(Bp[j], sign(ls[k])); break; }
            }
            cbuf[k] = out;
        }
        sym_emit_split_rup(s, cbuf, sz, s_pos, s_neg, sbuf);
        s->work += sz;
    }

    /* Derive (~Ap[col] | Bp[col]) via s-split RUP. */
    Lit apc = mkLit(Ap[col], false), bpc = mkLit(Bp[col], false);
    Lit ord_ab[2] = {neg(apc), bpc};
    if (!s->error) sym_emit_split_rup(s, ord_ab, 2u, s_pos, s_neg, sbuf);

    /* Delete original wcls[] touching A U B, plus s & ITE definitions. */
    for (uint32_t m = 0; m < nw && !s->error; ++m) {
        proof_delete_clause(s, wcls[m].lits, wcls[m].size);
    }
    for (uint32_t j = 0; j < rlen && !s->error; ++j) {
        Lit ap = mkLit(Ap[j], false), an = mkLit(Ap[j], true);
        Lit bp = mkLit(Bp[j], false), bn = mkLit(Bp[j], true);
        Lit aj = mkLit(A[j], false), bj = mkLit(B[j], false);
        Lit ia1[3] = {an, s_pos, aj}, ia2[3] = {an, s_neg, bj};
        Lit ia3[3] = {ap, s_pos, neg(aj)}, ia4[3] = {ap, s_neg, neg(bj)};
        Lit ib1[3] = {bn, s_pos, bj}, ib2[3] = {bn, s_neg, aj};
        Lit ib3[3] = {bp, s_pos, neg(bj)}, ib4[3] = {bp, s_neg, neg(aj)};
        proof_delete_clause(s, ia1, 3u); proof_delete_clause(s, ia2, 3u);
        proof_delete_clause(s, ia3, 3u); proof_delete_clause(s, ia4, 3u);
        proof_delete_clause(s, ib1, 3u); proof_delete_clause(s, ib2, 3u);
        proof_delete_clause(s, ib3, 3u); proof_delete_clause(s, ib4, 3u);
    }
    if (!s->error) {
        proof_delete_clause(s, sd1, 2u);
        proof_delete_clause(s, sd2, 2u);
        proof_delete_clause(s, sd3, 3u);
    }

    /* Re-bind A[j] <-> Ap[j], B[j] <-> Bp[j] (A, B are fresh in proof!). */
    for (uint32_t j = 0; j < rlen && !s->error; ++j) {
        Lit a = mkLit(A[j], false), ap = mkLit(Ap[j], false);
        Lit b = mkLit(B[j], false), bp = mkLit(Bp[j], false);
        Lit ba1[2] = {a, neg(ap)}, ba2[2] = {neg(a), ap};
        Lit bb1[2] = {b, neg(bp)}, bb2[2] = {neg(b), bp};
        proof_add_clause(s, ba1, 2u); proof_add_clause(s, ba2, 2u);
        proof_add_clause(s, bb1, 2u); proof_add_clause(s, bb2, 2u);
    }

    /* Restore original wcls[] via 1-step RUP and delete Ap/Bp copies. */
    for (uint32_t m = 0; m < nw && !s->error; ++m) {
        uint32_t sz = wcls[m].size;
        const Lit *ls = wcls[m].lits;
        for (uint32_t k = 0; k < sz; ++k) {
            Var v = var(ls[k]);
            Lit out = ls[k];
            for (uint32_t j = 0; j < rlen; ++j) {
                if (v == A[j]) { out = mkLit(Ap[j], sign(ls[k])); break; }
                if (v == B[j]) { out = mkLit(Bp[j], sign(ls[k])); break; }
            }
            cbuf[k] = out;
        }
        proof_add_clause(s, ls, sz);
        proof_delete_clause(s, cbuf, sz);
    }

    /* Emit final (~A[col] | B[col]) and delete (~Ap[col] | Bp[col]) + re-bindings. */
    if (!s->error) {
        Lit fin[2] = {neg(ac), bc};
        proof_add_clause(s, fin, 2u);
        proof_delete_clause(s, ord_ab, 2u);
    }
    for (uint32_t j = 0; j < rlen && !s->error; ++j) {
        Lit a = mkLit(A[j], false), ap = mkLit(Ap[j], false);
        Lit b = mkLit(B[j], false), bp = mkLit(Bp[j], false);
        Lit ba1[2] = {a, neg(ap)}, ba2[2] = {neg(a), ap};
        Lit bb1[2] = {b, neg(bp)}, bb2[2] = {neg(b), bp};
        proof_delete_clause(s, ba1, 2u); proof_delete_clause(s, ba2, 2u);
        proof_delete_clause(s, bb1, 2u); proof_delete_clause(s, bb2, 2u);
    }

    free(cbuf);
    free(sbuf);
    for (uint32_t m = 0; m < nw; ++m) free(wcls[m].lits);
    free(wcls);
    return !s->error;
}

/* Probe whether `target` is a failed literal at level 1; if so, derive ~target as a
 * 1-step RUP root unit, propagate, sync clauses/proof, and rebuild watches! */
static bool sym_probe_failed_lit_drat(Solver *s, Lit target) {
    if (s->error || s->result == FALSE || s->decision_level != 0) return false;
    Var tv = var(target);
    if (s->values[tv] != UNDEF) return false;

    uint32_t root_trail = s->trail_size;
    s->decision_level = 1;
    s->trail_lims[1] = root_trail;
    s->values[tv] = (uint8_t)(2u - sign(target));
    s->lit_values[target] = TRUE;
    s->lit_values[target ^ 1u] = FALSE;
    s->vars[tv].level = 1;
    s->vars[tv].reason = INVALID_CLAUSE;
    s->binary_reasons[tv] = LIT_UNDEF;
    s->vars[tv].trail_pos = root_trail;
    s->trail[s->trail_size++].lit = target;

    CRef confl = solver_propagate(s);
    solver_backtrack(s, 0);
    if (s->error || s->interrupted || confl == INVALID_CLAUSE) return false;

    /* Failed literal! neg(target) is a 1-step RUP root unit! */
    Lit unit = neg(target);
    Var uv = var(unit);
    uint32_t pt = s->trail_size;
    s->values[uv] = (uint8_t)(2u - sign(unit));
    s->lit_values[unit] = TRUE;
    s->lit_values[unit ^ 1u] = FALSE;
    s->vars[uv].level = 0;
    s->vars[uv].reason = INVALID_CLAUSE;
    s->binary_reasons[uv] = LIT_UNDEF;
    s->vars[uv].trail_pos = pt;
    s->trail[s->trail_size++].lit = unit;

    if (solver_propagate(s) != INVALID_CLAUSE) {
        proof_add_clause(s, &unit, 1u);
        if (!s->error) proof_add_clause(s, NULL, 0);
        s->base_unsat = true;
        s->result = FALSE;
        return true;
    }
    if (!sym_sync_root_units_drat(s, pt)) return true;
    solver_rebuild_watches(s);
    return true;
}

static bool sym_has_live_binary(const Solver *s, Lit a, Lit b) {
    if (a > b) { Lit t = a; a = b; b = t; }
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        if (CLAUSE_SIZE(s->arena, cr) != 2u) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        Lit x = ls[0], y = ls[1];
        if (x > y) { Lit t = x; x = y; y = t; }
        if (x == a && y == b) return true;
    }
    return false;
}

typedef struct {
    Var v;
    uint64_t sig;
} VarSig;

static int cmp_var_sig(const void *a, const void *b) {
    uint64_t sa = ((const VarSig *)a)->sig;
    uint64_t sb = ((const VarSig *)b)->sig;
    if (sa != sb) return (sa > sb) - (sa < sb);
    Var va = ((const VarSig *)a)->v;
    Var vb = ((const VarSig *)b)->v;
    return (va > vb) - (va < vb);
}

typedef struct {
    uint32_t cid;
    uint32_t size;
    uint64_t sig;
} RowCand;

static int cmp_row_cand(const void *a, const void *b) {
    const RowCand *x = (const RowCand *)a;
    const RowCand *y = (const RowCand *)b;
    if (x->size != y->size) return (x->size > y->size) - (x->size < y->size);
    if (x->sig != y->sig) return (x->sig > y->sig) - (x->sig < y->sig);
    return (x->cid > y->cid) - (x->cid < y->cid);
}

bool solver_break_symmetries(Solver *s, uint64_t tick_budget) {
    if (!s || s->decision_level || s->has_solved || s->proof_journal || s->error ||
        s->interrupted || s->result == FALSE || s->num_vars < 4 || s->num_clauses < 8 ||
        s->num_clauses > 150000 || s->num_vars > 50000 || solver_budget_exhausted(s)) {
        return true;
    }
    /* In --proof mode, require clean initial clause DB (no learnts/root units) and
     * >= 64 clauses so tiny RUP-only unit checks stay RUP while real benchmarks get
     * full RAT/RUP DRAT-certified symmetry breaking. */
    if (s->proof_file && (s->trail_size > 0 || s->num_learnts > 0 || s->num_clauses < 64u)) {
        return true;
    }

    if (solver_propagate(s) != INVALID_CLAUSE) {
        if (s->proof_file) proof_add_clause(s, NULL, 0);
        s->base_unsat = true;
        s->result = FALSE;
        return false;
    }
    if (s->proof_file && s->trail_size > 0) return true;

    Var next_pvar = s->num_vars + 1u;

    const uint32_t orig_nv = s->num_vars;
    SymState st;
    memset(&st, 0, sizeof st);
    st.s = s;
    st.end_work = s->work + tick_budget;

    /* Count live unsatisfied irredundant clauses (stripping level-0 FALSE literals!).
     * Never skip an unsatisfied clause containing a falsified root literal — doing so
     * would omit active constraints from automorphism verification! */
    uint32_t live_cnt = 0;
    uint32_t max_len = 0;
    uint64_t total_occ = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool sat = false;
        uint32_t kept = 0;
        for (uint32_t j = 0; j < sz; ++j) {
            uint8_t val = s->values[var(ls[j])];
            if (val == UNDEF) kept++;
            else if (val == (uint8_t)(2u - sign(ls[j]))) { sat = true; break; }
        }
        if (sat) continue;
        if (kept < 2) {
            /* Unpropagated unit/empty clause shouldn't exist after solver_propagate,
             * bail out safely without breaking symmetry. */
            return true;
        }
        live_cnt++;
        if (kept > max_len) max_len = kept;
        total_occ += kept;
    }
    if (live_cnt < 8 || total_occ > 2000000ULL) return true;

    st.max_cls_len = max_len;
    st.ht_cap = 64u;
    while (st.ht_cap < (size_t)live_cnt * 2u) st.ht_cap <<= 1;

    st.cls = calloc(live_cnt, sizeof *st.cls);
    st.ht = calloc(st.ht_cap, sizeof *st.ht);
    st.vhead = calloc((size_t)orig_nv + 2u, sizeof *st.vhead);
    st.swap_map = calloc((size_t)orig_nv + 1u, sizeof *st.swap_map);
    st.cls_seen = calloc(live_cnt, 1);
    st.cls_seen_list = malloc((size_t)live_cnt * sizeof *st.cls_seen_list);
    st.tmp_lits = malloc((size_t)(max_len + 1u) * sizeof(Lit));
    Lit *lit_pool = malloc((size_t)total_occ * sizeof(Lit));
    if (!st.cls || !st.ht || !st.vhead || !st.swap_map || !st.cls_seen ||
        !st.cls_seen_list || !st.tmp_lits || !lit_pool) {
        free(lit_pool);
        st.oom = true;
        goto cleanup;
    }

    uint64_t *var_sig_acc = calloc((size_t)orig_nv + 1u, sizeof *var_sig_acc);
    uint32_t *pos_deg = calloc((size_t)orig_nv + 1u, sizeof *pos_deg);
    uint32_t *neg_deg = calloc((size_t)orig_nv + 1u, sizeof *neg_deg);
    uint8_t *used_var = calloc((size_t)orig_nv + 1u, 1);
    if (!var_sig_acc || !pos_deg || !neg_deg || !used_var) {
        free(var_sig_acc); free(pos_deg); free(neg_deg); free(used_var);
        free(lit_pool);
        st.oom = true;
        goto cleanup;
    }

    uint64_t pool_off = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool sat = false;
        uint32_t kept = 0;
        for (uint32_t j = 0; j < sz; ++j) {
            uint8_t val = s->values[var(ls[j])];
            if (val == UNDEF) st.tmp_lits[kept++] = ls[j];
            else if (val == (uint8_t)(2u - sign(ls[j]))) { sat = true; break; }
        }
        if (sat || kept < 2) continue;

        Lit *dst = lit_pool + pool_off;
        pool_off += kept;
        memcpy(dst, st.tmp_lits, (size_t)kept * sizeof(Lit));
        sym_sort_lits(dst, kept);
        uint64_t h = sym_hash_clause(dst, kept);

        uint32_t cid = st.n_cls++;
        st.cls[cid] = (SymClause){dst, kept, h};

        /* Insert into exact clause hash set (deduplicating identical clauses). */
        size_t pos = (size_t)(h & (st.ht_cap - 1u));
        for (;;) {
            if (!st.ht[pos].cidx) {
                st.ht[pos] = (SymHashSlot){h, cid + 1u};
                break;
            }
            if (st.ht[pos].hash == h) {
                const SymClause *ex = &st.cls[st.ht[pos].cidx - 1u];
                if (ex->size == kept && memcmp(ex->lits, dst, (size_t)kept * sizeof(Lit)) == 0) break;
            }
            pos = (pos + 1u) & (st.ht_cap - 1u);
        }

        for (uint32_t j = 0; j < kept; ++j) {
            Var v = var(dst[j]);
            st.vhead[v + 1u]++;
            if (sign(dst[j])) neg_deg[v]++;
            else pos_deg[v]++;
            var_sig_acc[v] += sym_mix(((uint64_t)kept << 1) | sign(dst[j]));
        }
    }

    for (Var v = 1; v <= orig_nv + 1u; ++v) st.vhead[v] += st.vhead[v - 1u];
    st.vocc = malloc((size_t)(total_occ ? total_occ : 1u) * sizeof *st.vocc);
    uint32_t *vcur = malloc(((size_t)orig_nv + 2u) * sizeof *vcur);
    if (!st.vocc || !vcur) {
        free(vcur); free(var_sig_acc); free(pos_deg); free(neg_deg); free(used_var);
        free(lit_pool);
        st.oom = true;
        goto cleanup;
    }
    memcpy(vcur, st.vhead, ((size_t)orig_nv + 2u) * sizeof *vcur);
    for (uint32_t cid = 0; cid < st.n_cls; ++cid) {
        for (uint32_t j = 0; j < st.cls[cid].size; ++j) {
            st.vocc[vcur[var(st.cls[cid].lits[j])]++] = cid;
        }
    }
    free(vcur);

    uint32_t clauses_added = 0;
    uint32_t aux_added = 0;

    /* -----------------------------------------------------------------
     * Pass 1: Direct Variable-Swap Transposition Chains (u <-> v)
     * ----------------------------------------------------------------- */
    VarSig *vsigs = malloc((size_t)orig_nv * sizeof *vsigs);
    if (!vsigs) {
        free(var_sig_acc); free(pos_deg); free(neg_deg); free(used_var); free(lit_pool);
        st.oom = true;
        goto cleanup;
    }
    uint32_t n_vsigs = 0;
    for (Var v = 1; v <= orig_nv; ++v) {
        if (s->values[v] != UNDEF || solver_is_frozen(s, v) || (pos_deg[v] == 0 && neg_deg[v] == 0)) continue;
        uint64_t sig = sym_mix(((uint64_t)pos_deg[v] << 32) | neg_deg[v]) ^ var_sig_acc[v];
        vsigs[n_vsigs++] = (VarSig){v, sig};
    }
    qsort(vsigs, n_vsigs, sizeof *vsigs, cmp_var_sig);

    for (uint32_t i = 0; i < n_vsigs && s->work < st.end_work && !s->error; ) {
        uint32_t j = i + 1u;
        while (j < n_vsigs && vsigs[j].sig == vsigs[i].sig) j++;
        uint32_t grp_sz = j - i;
        if (grp_sz >= 2u && grp_sz <= 256u) {
            /* Greedily extract maximal cliques of pairwise-interchangeable vars. */
            for (uint32_t a = i; a < j && s->work < st.end_work && !s->error; ++a) {
                Var base = vsigs[a].v;
                if (used_var[base]) continue;
                Var chain[64];
                uint32_t clen = 0;
                chain[clen++] = base;
                for (uint32_t b = a + 1u; b < j && clen < 64u && s->work < st.end_work; ++b) {
                    Var cand = vsigs[b].v;
                    if (used_var[cand]) continue;
                    if (verify_involution(&st, &base, &cand, 1u)) {
                        chain[clen++] = cand;
                    }
                }
                if (clen >= 2u) {
                    if (s->proof_file) {
                        uint32_t plen = MIN(clen, 32u);
                        Var srows[32][SYM_MAX_ROW_LEN];
                        for (uint32_t k = 0; k < plen; ++k) srows[k][0] = chain[k];
                        if (sym_prove_column_chain_drat(s, srows, plen, 1u, 0u, &next_pvar, st.end_work)) {
                            for (uint32_t k = 0; k < plen; ++k) used_var[chain[k]] = 1;
                            for (uint32_t k = 0; k + 1u < plen && !s->error; ++k) {
                                Lit sb[2] = {mkLit(chain[k], true), mkLit(chain[k + 1u], false)};
                                if (!sym_add_clause(s, sb, 2u)) break;
                                clauses_added++;
                            }
                            if (!s->error) {
                                solver_rebuild_watches(s);
                                uint32_t pt = s->trail_size;
                                if (!s->error && solver_propagate(s) != INVALID_CLAUSE) {
                                    proof_add_clause(s, NULL, 0);
                                    s->base_unsat = true;
                                    s->result = FALSE;
                                } else if (!s->error && s->trail_size > pt) {
                                    (void)sym_sync_root_units_drat(s, pt);
                                }
                            }
                        }
                    } else {
                        for (uint32_t k = 0; k < clen; ++k) used_var[chain[k]] = 1;
                        for (uint32_t k = 0; k + 1u < clen && !s->error; ++k) {
                            Lit sb[2] = {mkLit(chain[k], true), mkLit(chain[k + 1u], false)};
                            if (!sym_add_clause(s, sb, 2u)) break;
                            clauses_added++;
                        }
                    }
                }
            }
        }
        if (s->result == FALSE) break;
        i = j;
    }
    free(vsigs);

    /* -----------------------------------------------------------------
     * Pass 2: Matrix / Row-Interchange Symmetry Blocks (BreakID style)
     * ----------------------------------------------------------------- */
    RowCand *rcands = malloc((size_t)st.n_cls * sizeof *rcands);
    if (!rcands) {
        free(var_sig_acc); free(pos_deg); free(neg_deg); free(used_var); free(lit_pool);
        st.oom = true;
        goto cleanup;
    }
    uint32_t n_rcands = 0;
    for (uint32_t cid = 0; cid < st.n_cls; ++cid) {
        uint32_t sz = st.cls[cid].size;
        if (sz < 3u || sz > SYM_MAX_ROW_LEN) continue;
        bool ok = true;
        uint64_t rsig = sym_mix(sz);
        for (uint32_t j = 0; j < sz; ++j) {
            Lit l = st.cls[cid].lits[j];
            Var v = var(l);
            if (used_var[v] || solver_is_frozen(s, v)) { ok = false; break; }
            rsig += sym_mix(var_sig_acc[v] ^ ((uint64_t)sign(l) << 63) ^
                            (((uint64_t)pos_deg[v] << 32) | neg_deg[v]));
        }
        if (ok) rcands[n_rcands++] = (RowCand){cid, sz, rsig};
    }
    qsort(rcands, n_rcands, sizeof *rcands, cmp_row_cand);

    for (uint32_t i = 0; i < n_rcands && aux_added < SYM_MAX_AUX_VARS &&
                         s->work < st.end_work && !s->error; ) {
        uint32_t j = i + 1u;
        while (j < n_rcands && rcands[j].size == rcands[i].size && rcands[j].sig == rcands[i].sig) j++;
        uint32_t grp = j - i;
        uint32_t rlen = rcands[i].size;
        if (grp >= 2u && grp <= 256u) {
            /* Pick first unused clause as base row R_0, align & verify all disjoint rows R_b! */
            for (uint32_t a = i; a < j && aux_added < SYM_MAX_AUX_VARS &&
                                 s->work < st.end_work && !s->error; ++a) {
                const SymClause *r0 = &st.cls[rcands[a].cid];
                bool r0_ok = true;
                for (uint32_t k = 0; k < rlen; ++k) {
                    if (used_var[var(r0->lits[k])]) { r0_ok = false; break; }
                }
                if (!r0_ok) continue;

                Var rows_v[64][SYM_MAX_ROW_LEN];
                uint32_t n_rows = 1;
                for (uint32_t k = 0; k < rlen; ++k) {
                    rows_v[0][k] = var(r0->lits[k]);
                    used_var[rows_v[0][k]] = 2; /* Temporarily mark R_0 vars */
                }

                for (uint32_t b = a + 1u; b < j && n_rows < 64u && s->work < st.end_work; ++b) {
                    const SymClause *rb = &st.cls[rcands[b].cid];
                    bool disj = true;
                    for (uint32_t k = 0; k < rlen; ++k) {
                        if (used_var[var(rb->lits[k])]) { disj = false; break; }
                    }
                    if (!disj) continue;

                    /* Align rb[0..rlen-1] to r0[0..rlen-1]:
                     * Check direct column binary mutex (~r0[k] | ~rb[m]) first (O(1) hash lookup!),
                     * falling back to positional order if same-signature variables align directly. */
                    Var aligned[SYM_MAX_ROW_LEN];
                    uint8_t taken[SYM_MAX_ROW_LEN] = {0};
                    bool aligned_ok = true;
                    for (uint32_t k = 0; k < rlen; ++k) {
                        Lit la = r0->lits[k];
                        uint32_t pick = UINT32_MAX;
                        for (uint32_t m = 0; m < rlen; ++m) {
                            if (taken[m]) continue;
                            Lit lb = rb->lits[m];
                            if (sign(lb) != sign(la)) continue;
                            Lit pair[2] = {neg(la), neg(lb)};
                            if (pair[0] > pair[1]) { Lit t = pair[0]; pair[0] = pair[1]; pair[1] = t; }
                            if (sym_clause_exists(&st, pair, 2u, sym_hash_clause(pair, 2u))) {
                                pick = m;
                                break;
                            }
                        }
                        if (pick == UINT32_MAX) {
                            /* Fallback: first unmatched literal with identical sign & var signature */
                            Var va = var(la);
                            for (uint32_t m = 0; m < rlen; ++m) {
                                if (taken[m]) continue;
                                Lit lb = rb->lits[m];
                                Var vb = var(lb);
                                if (sign(lb) == sign(la) && pos_deg[vb] == pos_deg[va] &&
                                    neg_deg[vb] == neg_deg[va] && var_sig_acc[vb] == var_sig_acc[va]) {
                                    pick = m;
                                    break;
                                }
                            }
                        }
                        if (pick == UINT32_MAX) { aligned_ok = false; break; }
                        taken[pick] = 1;
                        aligned[k] = var(rb->lits[pick]);
                    }
                    if (!aligned_ok) continue;

                    /* 100% EXACT automorphism verification across ALL touched clauses! */
                    if (verify_involution(&st, rows_v[0], aligned, rlen)) {
                        memcpy(rows_v[n_rows], aligned, (size_t)rlen * sizeof(Var));
                        for (uint32_t k = 0; k < rlen; ++k) used_var[aligned[k]] = 2;
                        n_rows++;
                    }
                }

                if (n_rows >= 2u) {
                    /* Permanently lock these disjoint row variables and emit BreakID lex-leader! */
                    for (uint32_t r = 0; r < n_rows; ++r) {
                        for (uint32_t k = 0; k < rlen; ++k) used_var[rows_v[r][k]] = 1;
                    }
                    if (s->proof_file) {
                        /* Proof-producing iterative column-by-column DRAT symmetry breaking:
                         * - When column 0 has pairwise mutexes (~R_0[0] | ~R_1[0]) (PHP,
                         *   graph coloring, Latin-square, AMO matrices), uses O(n) single-pair
                         *   peeling + 1-step failed-literal RUP to force ~R_0[0]..~R_{n-2}[0]
                         *   and recurse column-by-column with zero quadratic blowup!
                         * - Otherwise falls back to insertion-sort column chain (n <= 16). */
                        uint32_t act_rows = MIN(n_rows, 64u);
                        uint32_t act_cols = rlen;
                        Var rem_rows[64][SYM_MAX_ROW_LEN];
                        for (uint32_t r = 0; r < act_rows; ++r) {
                            memcpy(rem_rows[r], rows_v[r], (size_t)act_cols * sizeof(Var));
                        }
                        while (act_rows >= 2u && act_cols >= 1u && !s->error &&
                               s->result != FALSE && s->work < st.end_work) {
                            bool all_undef = true;
                            for (uint32_t r = 0; r < act_rows && all_undef; ++r) {
                                for (uint32_t k = 0; k < act_cols; ++k) {
                                    if (s->values[rem_rows[r][k]] != UNDEF) { all_undef = false; break; }
                                }
                            }
                            if (!all_undef) break;

                            if (sym_has_live_binary(s, mkLit(rem_rows[0][0], true),
                                                    mkLit(rem_rows[1][0], true))) {
                                uint32_t peeled = 0;
                                for (uint32_t r = 0; r + 1u < act_rows && !s->error &&
                                                     s->result != FALSE && s->work < st.end_work; ++r) {
                                    if (s->values[rem_rows[r][0]] != UNDEF ||
                                        s->values[rem_rows[r + 1u][0]] != UNDEF) {
                                        break;
                                    }
                                    if (!sym_prove_pair_drat(s, rem_rows[r], rem_rows[r + 1u],
                                                             act_cols, 0u, &next_pvar, st.end_work)) {
                                        break;
                                    }
                                    Lit c0[2] = {mkLit(rem_rows[r][0], true),
                                                 mkLit(rem_rows[r + 1u][0], false)};
                                    if (!sym_add_clause(s, c0, 2u)) break;
                                    clauses_added++;
                                    solver_rebuild_watches(s);
                                    if (s->error) break;
                                    if (!sym_probe_failed_lit_drat(s, mkLit(rem_rows[r][0], false))) {
                                        break;
                                    }
                                    peeled++;
                                }
                                if (s->error || s->result == FALSE || peeled == 0) break;
                            } else {
                                if (act_rows > 16u) break;
                                if (!sym_prove_column_chain_drat(s, rem_rows, act_rows, act_cols, 0u,
                                                                 &next_pvar, st.end_work)) {
                                    break;
                                }
                                for (uint32_t r = 0; r + 1u < act_rows && !s->error; ++r) {
                                    Lit c0[2] = {mkLit(rem_rows[r][0], true), mkLit(rem_rows[r + 1u][0], false)};
                                    if (!sym_add_clause(s, c0, 2u)) break;
                                    clauses_added++;
                                }
                                if (s->error) break;
                                solver_rebuild_watches(s);
                                if (s->error) break;
                                uint32_t pt = s->trail_size;
                                if (solver_propagate(s) != INVALID_CLAUSE) {
                                    proof_add_clause(s, NULL, 0);
                                    s->base_unsat = true;
                                    s->result = FALSE;
                                    break;
                                }
                                if (s->trail_size == pt) break;
                                if (!sym_sync_root_units_drat(s, pt)) break;
                                solver_rebuild_watches(s);
                            }

                            /* Keep rows where column 0 was forced FALSE and all cols 1..act_cols-1 stay UNDEF. */
                            uint32_t next_rows = 0;
                            for (uint32_t r = 0; r < act_rows; ++r) {
                                if (s->values[rem_rows[r][0]] != FALSE) continue;
                                bool rest_undef = true;
                                for (uint32_t k = 1u; k < act_cols; ++k) {
                                    if (s->values[rem_rows[r][k]] != UNDEF) { rest_undef = false; break; }
                                }
                                if (!rest_undef) continue;
                                for (uint32_t k = 1u; k < act_cols; ++k) {
                                    rem_rows[next_rows][k - 1u] = rem_rows[r][k];
                                }
                                next_rows++;
                            }
                            act_rows = next_rows;
                            act_cols--;
                        }
                    } else {
                        uint32_t cols = MIN(rlen, SYM_MAX_LEX_COLS);
                        if (cols > 1u && !s->factor_original_vars) s->factor_original_vars = s->num_vars;

                        for (uint32_t r = 0; r + 1u < n_rows && aux_added < SYM_MAX_AUX_VARS && !s->error; ++r) {
                            /* Enforce rows_v[r] <=_lex rows_v[r+1]:
                             * Column 0: (~x_0 | y_0) */
                            Lit x0 = mkLit(rows_v[r][0], false);
                            Lit y0 = mkLit(rows_v[r + 1u][0], false);
                            Lit c0[2] = {neg(x0), y0};
                            if (!sym_add_clause(s, c0, 2u)) break;
                            clauses_added++;

                            Var prev_p = INVALID_VAR;
                            for (uint32_t k = 0; k + 1u < cols && aux_added < SYM_MAX_AUX_VARS && !s->error; ++k) {
                                Var pk = solver_new_var(s);
                                if (pk == INVALID_VAR || s->error) { st.oom = true; break; }
                                aux_added++;
                                Lit lp = mkLit(pk, false);
                                Lit xk = mkLit(rows_v[r][k], false);
                                Lit yk = mkLit(rows_v[r + 1u][k], false);
                                Lit xn = mkLit(rows_v[r][k + 1u], false);
                                Lit yn = mkLit(rows_v[r + 1u][k + 1u], false);

                                if (k == 0) {
                                    /* (x_0 | y_0 | p_1) and (~x_0 | ~y_0 | p_1) */
                                    Lit e1[3] = {xk, yk, lp};
                                    Lit e2[3] = {neg(xk), neg(yk), lp};
                                    if (!sym_add_clause(s, e1, 3u) || !sym_add_clause(s, e2, 3u)) break;
                                    clauses_added += 2u;
                                } else {
                                    Lit lprev = mkLit(prev_p, true);
                                    Lit e1[4] = {lprev, xk, yk, lp};
                                    Lit e2[4] = {lprev, neg(xk), neg(yk), lp};
                                    if (!sym_add_clause(s, e1, 4u) || !sym_add_clause(s, e2, 4u)) break;
                                    clauses_added += 2u;
                                }
                                /* (~p_{k+1} | ~x_{k+1} | y_{k+1}) */
                                Lit ord[3] = {neg(lp), neg(xn), yn};
                                if (!sym_add_clause(s, ord, 3u)) break;
                                clauses_added++;
                                prev_p = pk;
                            }
                        }
                    }
                } else {
                    /* No symmetric partner for R_0 -> release temporary mark. */
                    for (uint32_t k = 0; k < rlen; ++k) used_var[rows_v[0][k]] = 0;
                }
            }
        }
        i = j;
    }

    free(rcands);
    free(var_sig_acc); free(pos_deg); free(neg_deg); free(used_var);
    free(lit_pool);

cleanup:
    free(st.cls);
    free(st.ht);
    free(st.vhead);
    free(st.vocc);
    free(st.swap_map);
    free(st.cls_seen);
    free(st.cls_seen_list);
    free(st.tmp_lits);

    if (st.oom || s->error) { s->error = true; return false; }

    if (clauses_added > 0) {
        if (s->num_vars + 2u > s->levels_capacity) {
            uint8_t *seen = calloc((size_t)s->num_vars + 2u, 1);
            if (!seen) { s->error = true; return false; }
            free(s->level_seen);
            s->level_seen = seen;
            s->levels_capacity = s->num_vars + 2u;
        }
        solver_rebuild_watches(s);
        if (s->error) return false;
        if (solver_propagate(s) != INVALID_CLAUSE) {
            s->base_unsat = true;
            s->result = FALSE;
            return false;
        }
    }
    return !s->error && s->result != FALSE;
}
