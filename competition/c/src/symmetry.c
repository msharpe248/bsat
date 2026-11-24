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
    if (!s || s->decision_level || s->has_solved || s->proof_file || s->proof_journal || s->error ||
        s->interrupted || s->result == FALSE || s->num_vars < 4 || s->num_clauses < 8 ||
        s->num_clauses > 150000 || s->num_vars > 50000 || solver_budget_exhausted(s)) {
        return true;
    }

    const uint32_t orig_nv = s->num_vars;
    SymState st;
    memset(&st, 0, sizeof st);
    st.s = s;
    st.end_work = s->work + tick_budget;

    /* Count live irredundant clauses with all literals UNDEF. */
    uint32_t live_cnt = 0;
    uint32_t max_len = 0;
    uint64_t total_occ = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 2) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool skip = false;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { skip = true; break; }
        }
        if (skip) continue;
        live_cnt++;
        if (sz > max_len) max_len = sz;
        total_occ += sz;
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
        if (sz < 2) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool skip = false;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { skip = true; break; }
        }
        if (skip) continue;

        Lit *dst = lit_pool + pool_off;
        pool_off += sz;
        memcpy(dst, ls, (size_t)sz * sizeof(Lit));
        sym_sort_lits(dst, sz);
        uint64_t h = sym_hash_clause(dst, sz);

        uint32_t cid = st.n_cls++;
        st.cls[cid] = (SymClause){dst, sz, h};

        /* Insert into exact clause hash set (deduplicating identical clauses). */
        size_t pos = (size_t)(h & (st.ht_cap - 1u));
        for (;;) {
            if (!st.ht[pos].cidx) {
                st.ht[pos] = (SymHashSlot){h, cid + 1u};
                break;
            }
            if (st.ht[pos].hash == h) {
                const SymClause *ex = &st.cls[st.ht[pos].cidx - 1u];
                if (ex->size == sz && memcmp(ex->lits, dst, (size_t)sz * sizeof(Lit)) == 0) break;
            }
            pos = (pos + 1u) & (st.ht_cap - 1u);
        }

        for (uint32_t j = 0; j < sz; ++j) {
            Var v = var(dst[j]);
            st.vhead[v + 1u]++;
            if (sign(dst[j])) neg_deg[v]++;
            else pos_deg[v]++;
            var_sig_acc[v] += sym_mix(((uint64_t)sz << 1) | sign(dst[j]));
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
                    for (uint32_t k = 0; k < clen; ++k) used_var[chain[k]] = 1;
                    for (uint32_t k = 0; k + 1u < clen && !s->error; ++k) {
                        Lit sb[2] = {mkLit(chain[k], true), mkLit(chain[k + 1u], false)};
                        if (!sym_add_clause(s, sb, 2u)) break;
                        clauses_added++;
                    }
                }
            }
        }
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
