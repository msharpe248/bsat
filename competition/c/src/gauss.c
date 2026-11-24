/*********************************************************************
 * BSAT Competition Solver - Dynamic GF(2) Gaussian Elimination
 *
 * Implements:
 * 1. Exact 2..6-variable XOR extraction from irredundant CNF clauses.
 * 2. Connected-component partitioning + root-level Gauss-Jordan RREF
 *    (deriving contradictions, root units, and binary equivalences).
 * 3. Bird / Han-Jiang two-watched-column dynamic GF(2) elimination
 *    during CDCL search:
 *    - Each row maintains a unique RREF pivot `basic_col[r]` (set ONLY
 *      in row r) and a non-basic watch `nb_col[r]`.
 *    - Pivoting preserves the solution space across all decision levels,
 *      so backtracking requires ZERO matrix undo (just clamping qhead).
 *    - Implied literals and conflicts construct exact CNF reason/conflict
 *      clauses stored in temporary arena slots (never polluting watch
 *      lists or the learned clause database) for seamless 1-UIP analysis.
 *********************************************************************/

#include "../include/gauss.h"
#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>

#define GAUSS_MAX_MATS       16u
#define GAUSS_MAX_COLS       2048u
#define GAUSS_MAX_ROWS       2048u
#define GAUSS_MIN_LIVE_ROWS  8u
#define GAUSS_MAX_TOTAL_ROWS 4096u

typedef struct {
    uint32_t *data;
    uint32_t size;
    uint32_t cap;
} GaussWatchList;

typedef struct {
    uint32_t ncols;
    uint32_t nrows;
    uint32_t nwords;
    Var *col_to_var;
    uint64_t *rows;          /* nrows * nwords */
    uint8_t *rhs;            /* nrows */
    uint32_t *basic_col;     /* nrows: unique pivot col for row r */
    uint32_t *nb_col;        /* nrows: second watched col, or UINT32_MAX if weight 1 */
    uint32_t *basic_row;     /* ncols: row where col is basic, or UINT32_MAX */
    GaussWatchList *nb_watches; /* ncols */
} GaussMatrix;

struct GaussState {
    GaussMatrix *mats;
    uint32_t nmats;
    uint32_t var_cap;
    uint16_t *var_mat;       /* var_cap: matrix index or UINT16_MAX */
    uint32_t *var_col;       /* var_cap: local column index */
    uint32_t qhead;
    CRef *temp_clauses;
    uint32_t temp_size;
    uint32_t temp_cap;
    Lit *clause_buf;
    uint32_t clause_buf_cap;
    uint64_t propagations;
    uint64_t conflicts;
    uint64_t pivots;
};

/* =====================================================================
 * Helpers
 * ===================================================================== */

static inline uint64_t *mat_row(GaussMatrix *m, uint32_t r) {
    return m->rows + (size_t)r * m->nwords;
}

static inline const uint64_t *mat_row_const(const GaussMatrix *m, uint32_t r) {
    return m->rows + (size_t)r * m->nwords;
}

static inline bool row_has_bit(const uint64_t *row, uint32_t c) {
    return (row[c >> 6] & (UINT64_C(1) << (c & 63u))) != 0;
}

static bool nb_watch_add(Solver *s, GaussMatrix *m, uint32_t col, uint32_t r) {
    GaussWatchList *wl = &m->nb_watches[col];
    if (wl->size == wl->cap) {
        uint32_t ncap = wl->cap ? wl->cap * 2u : 8u;
        uint32_t *p = realloc(wl->data, (size_t)ncap * sizeof *p);
        if (!p) { s->error = true; return false; }
        wl->data = p;
        wl->cap = ncap;
    }
    wl->data[wl->size++] = r;
    return true;
}

static void nb_watch_remove(GaussMatrix *m, uint32_t col, uint32_t r) {
    GaussWatchList *wl = &m->nb_watches[col];
    for (uint32_t i = 0; i < wl->size; ++i) {
        if (wl->data[i] == r) {
            wl->data[i] = wl->data[--wl->size];
            return;
        }
    }
}

static void free_matrix(GaussMatrix *m) {
    if (!m) return;
    if (m->nb_watches) {
        for (uint32_t c = 0; c < m->ncols; ++c) free(m->nb_watches[c].data);
        free(m->nb_watches);
    }
    free(m->col_to_var);
    free(m->rows);
    free(m->rhs);
    free(m->basic_col);
    free(m->nb_col);
    free(m->basic_row);
    memset(m, 0, sizeof *m);
}

static bool temp_clause_locked(const Solver *s, CRef cr) {
    if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) return false;
    uint32_t sz = CLAUSE_SIZE(s->arena, cr);
    const Lit *ls = CLAUSE_LITS(s->arena, cr);
    if (sz > 0 && s->values[var(ls[0])] != UNDEF && s->vars[var(ls[0])].reason == cr) return true;
    if (sz == 2 && s->values[var(ls[1])] != UNDEF && s->vars[var(ls[1])].reason == cr) return true;
    return false;
}

static void gauss_gc_temp_clauses(Solver *s) {
    GaussState *g = s->gauss;
    if (!g || g->temp_size == 0) return;
    uint32_t out = 0;
    for (uint32_t i = 0; i < g->temp_size; ++i) {
        CRef cr = g->temp_clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        if (temp_clause_locked(s, cr)) {
            g->temp_clauses[out++] = cr;
        } else {
            arena_delete(s->arena, cr);
        }
    }
    g->temp_size = out;
}

void gauss_free(Solver *s) {
    if (!s || !s->gauss) return;
    GaussState *g = s->gauss;
    /* Delete any unlocked temp clauses cleanly (none are in watches or learnts). */
    for (uint32_t i = 0; i < g->temp_size; ++i) {
        CRef cr = g->temp_clauses[i];
        if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr)) {
            arena_delete(s->arena, cr);
        }
    }
    for (uint32_t mi = 0; mi < g->nmats; ++mi) free_matrix(&g->mats[mi]);
    free(g->mats);
    free(g->var_mat);
    free(g->var_col);
    free(g->temp_clauses);
    free(g->clause_buf);
    free(g);
    s->gauss = NULL;
}

bool gauss_var_protected(const Solver *s, Var v) {
    if (!s || !s->gauss || v >= s->gauss->var_cap) return false;
    return s->gauss->var_mat[v] != UINT16_MAX;
}

bool gauss_has_work(const Solver *s) {
    return s && s->gauss && s->gauss->qhead < s->trail_size;
}

void gauss_backtrack(Solver *s, uint32_t trail_pos) {
    GaussState *g = s->gauss;
    if (!g) return;
    if (g->qhead > trail_pos) g->qhead = trail_pos;
    if (g->temp_size > 0) gauss_gc_temp_clauses(s);
}

void gauss_relocate_clauses(Solver *s, const Arena *old_arena) {
    GaussState *g = s->gauss;
    if (!g || g->temp_size == 0) return;
    uint32_t out = 0;
    for (uint32_t i = 0; i < g->temp_size; ++i) {
        CRef old_cr = g->temp_clauses[i];
        if (old_cr == INVALID_CLAUSE) continue;
        CRef new_cr = CLAUSE_HEADER(old_arena, old_cr)->search;
        if (new_cr != INVALID_CLAUSE) g->temp_clauses[out++] = new_cr;
    }
    g->temp_size = out;
}

/* =====================================================================
 * Root-Level Helpers for Extraction
 * ===================================================================== */

static bool gauss_enqueue_root_unit(Solver *s, Lit lit) {
    Var v = var(lit);
    uint8_t cur = s->values[v];
    uint8_t want = (uint8_t)(2u - sign(lit));
    if (cur != UNDEF) {
        if (cur != want) { s->base_unsat = true; s->result = FALSE; return false; }
        return true;
    }
    s->values[v] = want;
    if (s->lit_values_live) {
        s->lit_values[lit] = TRUE;
        s->lit_values[lit ^ 1u] = FALSE;
    }
    s->vars[v].level = 0;
    s->vars[v].reason = INVALID_CLAUSE;
    s->binary_reasons[v] = LIT_UNDEF;
    s->vars[v].trail_pos = s->trail_size;
    s->trail[s->trail_size++].lit = lit;
    if (s->opts.phase_saving) s->vars[v].polarity = !sign(lit);
    return true;
}

static bool gauss_has_binary_watch(Solver *s, Lit a, Lit b) {
    WatchList *wl = watch_list(s->watches, a);
    for (uint32_t i = 0; i < wl->size; ++i) {
        Watch w = wl->watches[i];
        if ((is_binary_watch(w) || is_arena_binary_watch(w)) && w.blocker == b) return true;
    }
    return false;
}

#define GAUSS_MAX_DIRECT_K   7u

typedef struct {
    Var vars[GAUSS_MAX_DIRECT_K];
    uint8_t k;
    uint8_t used_as_xor;
    uint64_t mask[2]; /* 128 bits: supports up to 2^7 = 128 polarity masks */
} XorSlot;

typedef struct {
    Var *vars;
    uint32_t k;
    uint8_t rhs;
    uint8_t alive;
} XorEq;

static uint64_t hash_xor_tuple(const Var *vs, uint8_t k) {
    uint64_t h = (uint64_t)k * UINT64_C(0x9e3779b97f4a7c15);
    for (uint8_t i = 0; i < k; ++i) {
        h ^= (uint64_t)vs[i] + UINT64_C(0x9e3779b97f4a7c15) + (h << 6) + (h >> 2);
    }
    h ^= h >> 30; h *= UINT64_C(0xbf58476d1ce4e5b9);
    h ^= h >> 27; h *= UINT64_C(0x94d049bb133111eb);
    return h ^ (h >> 31);
}

static inline bool slot_has_mask(const XorSlot *sl, uint32_t m) {
    return (sl->mask[m >> 6] & (UINT64_C(1) << (m & 63u))) != 0;
}

static const XorSlot *xor_tab_find(const XorSlot *tab, size_t cap, const Var *vs, uint8_t k) {
    size_t pos = (size_t)(hash_xor_tuple(vs, k) & (cap - 1u));
    for (;;) {
        if (tab[pos].k == 0) return NULL;
        if (tab[pos].k == k && memcmp(tab[pos].vars, vs, (size_t)k * sizeof(Var)) == 0) {
            return &tab[pos];
        }
        pos = (pos + 1u) & (cap - 1u);
    }
}

/* Check whether the clause (vars[0..k-1] with polarity bits `neg_mask`) is
 * subsumed by an existing shorter live clause (size 2 or k-1) in `tab`. */
static bool xor_clause_subsumed(const XorSlot *tab, size_t cap,
                                const Var *vs, uint8_t k, uint32_t neg_mask) {
    /* 1. Check all binary subsets (size 2). */
    for (uint8_t i = 0; i < k; ++i) {
        for (uint8_t j = i + 1u; j < k; ++j) {
            Var sub[2] = {vs[i], vs[j]};
            const XorSlot *p = xor_tab_find(tab, cap, sub, 2);
            if (!p) continue;
            uint32_t sm = ((neg_mask >> i) & 1u) | (((neg_mask >> j) & 1u) << 1);
            if (slot_has_mask(p, sm)) return true;
        }
    }
    /* 2. Check all (k-1) subsets when k >= 4. */
    if (k >= 4) {
        Var sub[GAUSS_MAX_DIRECT_K];
        for (uint8_t drop = 0; drop < k; ++drop) {
            uint8_t dst = 0;
            uint32_t sm = 0;
            for (uint8_t i = 0; i < k; ++i) {
                if (i == drop) continue;
                sub[dst] = vs[i];
                if ((neg_mask >> i) & 1u) sm |= (1u << dst);
                dst++;
            }
            const XorSlot *p = xor_tab_find(tab, cap, sub, (uint8_t)(k - 1u));
            if (p && slot_has_mask(p, sm)) return true;
        }
    }
    return false;
}

static bool xor_parity_covered(const XorSlot *tab, size_t cap, const XorSlot *sl,
                               uint64_t need0, uint64_t need1) {
    uint64_t miss0 = need0 & ~sl->mask[0];
    uint64_t miss1 = need1 & ~sl->mask[1];
    if (miss0 == 0 && miss1 == 0) return true;
    /* Only attempt subsumption recovery for k = 3..6 with <= 4 missing clauses. */
    if (sl->k < 3 || sl->k > 6) return false;
    uint32_t n_miss = (uint32_t)__builtin_popcountll(miss0) + (uint32_t)__builtin_popcountll(miss1);
    if (n_miss > 4u) return false;
    while (miss0) {
        uint32_t m = (uint32_t)__builtin_ctzll(miss0);
        if (!xor_clause_subsumed(tab, cap, sl->vars, sl->k, m)) return false;
        miss0 &= miss0 - 1;
    }
    while (miss1) {
        uint32_t m = 64u + (uint32_t)__builtin_ctzll(miss1);
        if (!xor_clause_subsumed(tab, cap, sl->vars, sl->k, m)) return false;
        miss1 &= miss1 - 1;
    }
    return true;
}

static uint32_t uf_find(uint32_t *parent, uint32_t x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

static void uf_union(uint32_t *parent, uint32_t a, uint32_t b) {
    a = uf_find(parent, a);
    b = uf_find(parent, b);
    if (a != b) parent[b] = a;
}

/* =====================================================================
 * DRAT Proof Emitter for Root GF(2) RREF (Philipp & Rebola-Pardo Linear
 * Tseitin XOR-Chain Addition, 100% Standard DRAT/RAT)
 *
 * Given a subset of CNF-backed input XOR equations (2 <= k <= 7) whose
 * GF(2) sum is a contradiction (0=1), root unit (x=p), or binary
 * equivalence (x^y=p), proves the derived fact in O(sum(L_i)) DRAT steps
 * using temporary proof-only variables and zero residual aux variables.
 * ===================================================================== */

typedef struct {
    Var *vars;
    Var *aux;
    uint32_t k;
    uint8_t rhs;
    bool owns_final;
} ProofXorChain;

static void gauss_proof_emit_xor_eq(Solver *s, const Var *vs, uint32_t m, uint8_t rhs, bool del) {
    if (s->error) return;
    if (m == 0) {
        if (rhs) {
            if (del) proof_delete_clause(s, NULL, 0);
            else proof_add_clause(s, NULL, 0);
        }
        return;
    }
    uint32_t limit = 1u << m;
    for (uint32_t mask = 0; mask < limit && !s->error; ++mask) {
        if (((uint32_t)__builtin_popcount(mask) & 1u) != rhs) {
            Lit cl[3];
            for (uint32_t j = 0; j < m; ++j) {
                cl[j] = mkLit(vs[j], ((mask >> j) & 1u) != 0);
            }
            if (del) proof_delete_clause(s, cl, m);
            else proof_add_clause(s, cl, m);
        }
    }
}

static void gauss_proof_split_rup(Solver *s, const Lit *base, uint32_t blen,
                                  const Var *split_vs, uint32_t nsplit, uint32_t depth) {
    if (s->error) return;
    if (depth == nsplit) {
        proof_add_clause(s, base, blen);
        return;
    }
    Lit bpos[16], bneg[16];
    memcpy(bpos, base, (size_t)blen * sizeof(Lit));
    memcpy(bneg, base, (size_t)blen * sizeof(Lit));
    bpos[blen] = mkLit(split_vs[depth], false);
    bneg[blen] = mkLit(split_vs[depth], true);
    gauss_proof_split_rup(s, bpos, blen + 1u, split_vs, nsplit, depth + 1u);
    if (s->error) return;
    gauss_proof_split_rup(s, bneg, blen + 1u, split_vs, nsplit, depth + 1u);
    if (s->error) return;
    proof_add_clause(s, base, blen);
    if (s->error) return;
    proof_delete_clause(s, bpos, blen + 1u);
    proof_delete_clause(s, bneg, blen + 1u);
}

static void gauss_proof_free_chain(Solver *s, ProofXorChain *c, bool delete_final) {
    if (!c) return;
    if (s && !s->error) {
        if (delete_final && c->owns_final) {
            if (c->k == 1u) {
                gauss_proof_emit_xor_eq(s, c->vars, 1u, c->rhs, true);
            } else if (c->k == 2u) {
                gauss_proof_emit_xor_eq(s, c->vars, 2u, c->rhs, true);
            } else if (c->k >= 3u) {
                Var fv[2] = {c->aux[c->k - 3u], c->vars[c->k - 1u]};
                gauss_proof_emit_xor_eq(s, fv, 2u, c->rhs, true);
            }
        }
        if (c->k >= 3u && c->aux) {
            for (int i = (int)c->k - 3; i >= 0 && !s->error; --i) {
                Var tri[3] = {c->aux[i], (i == 0 ? c->vars[0] : c->aux[i - 1]), c->vars[i + 1]};
                gauss_proof_emit_xor_eq(s, tri, 3u, 0u, true);
            }
        }
    }
    free(c->vars);
    free(c->aux);
    memset(c, 0, sizeof *c);
}

/* Build a Tseitin XOR chain from a single CNF-backed input equation (2 <= k <= 7). */
static bool gauss_proof_chain_from_eq(Solver *s, const XorEq *eq,
                                      ProofXorChain *out, Var *next_pvar) {
    memset(out, 0, sizeof *out);
    if (eq->k < 2u || eq->k > GAUSS_MAX_DIRECT_K) return false;
    out->vars = malloc((size_t)eq->k * sizeof(Var));
    if (!out->vars) { s->error = true; return false; }
    memcpy(out->vars, eq->vars, (size_t)eq->k * sizeof(Var));
    out->k = eq->k;
    out->rhs = eq->rhs;

    if (eq->k == 2u) {
        /* Both binary clauses are already live in the CNF (all vars UNDEF). */
        out->owns_final = false;
        return true;
    }

    uint32_t naux = eq->k - 2u;
    out->aux = malloc((size_t)naux * sizeof(Var));
    if (!out->aux) {
        free(out->vars); out->vars = NULL;
        s->error = true; return false;
    }
    for (uint32_t i = 0; i < naux && !s->error; ++i) {
        out->aux[i] = (*next_pvar)++;
        Var tri[3] = {out->aux[i], (i == 0 ? eq->vars[0] : out->aux[i - 1u]), eq->vars[i + 1u]};
        gauss_proof_emit_xor_eq(s, tri, 3u, 0u, false);
    }
    /* Derive the 2 final binary clauses of aux[naux-1] ^ vars[k-1] = rhs
     * by splitting on vars[0 .. k-3] (depth <= 5, <= 32 leaves, 100% RUP!). */
    Var fv[2] = {out->aux[naux - 1u], eq->vars[eq->k - 1u]};
    for (uint32_t mask = 0; mask < 4u && !s->error; ++mask) {
        if (((uint32_t)__builtin_popcount(mask) & 1u) != eq->rhs) {
            Lit bin[2] = {mkLit(fv[0], (mask & 1u) != 0),
                          mkLit(fv[1], ((mask >> 1) & 1u) != 0)};
            gauss_proof_split_rup(s, bin, 2u, eq->vars, eq->k - 2u, 0u);
        }
    }
    out->owns_final = true;
    if (s->error) {
        gauss_proof_free_chain(s, out, false);
        return false;
    }
    return true;
}

static Var gauss_chain_prefix(const ProofXorChain *ch, uint32_t idx, Var z_end, uint8_t *rhs_acc) {
    if (idx == 0) return INVALID_VAR;
    if (idx < ch->k) {
        return (idx == 1u) ? ch->vars[0] : ch->aux[idx - 2u];
    }
    /* idx == ch->k */
    if (z_end != INVALID_VAR) return z_end;
    if (rhs_acc) *rhs_acc ^= ch->rhs;
    return INVALID_VAR;
}

/* Cancel any duplicate variables in vs[0..*m-1] (x ^ x = 0). */
static void gauss_cancel_bridge_vars(Var *vs, uint32_t *m) {
    uint32_t n = *m;
    if (n == 2u && vs[0] == vs[1]) { *m = 0; return; }
    if (n == 3u) {
        if (vs[0] == vs[1]) { vs[0] = vs[2]; *m = 1u; return; }
        if (vs[0] == vs[2]) { vs[0] = vs[1]; *m = 1u; return; }
        if (vs[1] == vs[2]) { *m = 1u; return; }
    }
}

/* Prove C = A ^ B in O(|A| + |B|) DRAT steps via lockstep prefix bridge! */
static bool gauss_proof_xor_chains(Solver *s, const ProofXorChain *A, const ProofXorChain *B,
                                   ProofXorChain *C, Var *next_pvar) {
    memset(C, 0, sizeof *C);
    uint32_t a = A->k, b = B->k;
    Var *cvars = malloc((size_t)(a + b + 1u) * sizeof(Var));
    if (!cvars) { s->error = true; return false; }

    uint32_t ia = 0, ib = 0, c = 0;
    while (ia < a && ib < b) {
        if (A->vars[ia] < B->vars[ib]) cvars[c++] = A->vars[ia++];
        else if (B->vars[ib] < A->vars[ia]) cvars[c++] = B->vars[ib++];
        else { ia++; ib++; }
    }
    while (ia < a) cvars[c++] = A->vars[ia++];
    while (ib < b) cvars[c++] = B->vars[ib++];

    C->vars = cvars;
    C->k = c;
    C->rhs = A->rhs ^ B->rhs;

    Var z_end = INVALID_VAR;
    if (c >= 3u) {
        C->aux = malloc((size_t)(c - 2u) * sizeof(Var));
        if (!C->aux) { free(cvars); memset(C, 0, sizeof *C); s->error = true; return false; }
        for (uint32_t i = 0; i < c - 2u && !s->error; ++i) {
            C->aux[i] = (*next_pvar)++;
            Var tri[3] = {C->aux[i], (i == 0 ? cvars[0] : C->aux[i - 1u]), cvars[i + 1u]};
            gauss_proof_emit_xor_eq(s, tri, 3u, 0u, false);
        }
    }
    if (c >= 2u && !s->error) {
        z_end = (*next_pvar)++;
        Var tri[3] = {z_end, (c == 2u ? cvars[0] : C->aux[c - 3u]), cvars[c - 1u]};
        gauss_proof_emit_xor_eq(s, tri, 3u, 0u, false);
    }

    /* Lockstep merge maintaining bridge: A_prefix(ia) ^ B_prefix(ib) ^ C_prefix(ic) = b_rhs */
    Var b_vars[3] = {0, 0, 0};
    uint32_t bm = 0;
    uint8_t b_rhs = 0;

    ia = 0; ib = 0;
    uint32_t ic = 0;
    while ((ia < a || ib < b) && !s->error) {
        Var sv;
        if (ia < a && (ib == b || A->vars[ia] < B->vars[ib])) {
            sv = A->vars[ia++];
            ic++;
        } else if (ib < b && (ia == a || B->vars[ib] < A->vars[ia])) {
            sv = B->vars[ib++];
            ic++;
        } else {
            sv = A->vars[ia++];
            ib++;
        }

        Var nb_vars[3];
        uint32_t nbm = 0;
        uint8_t nb_rhs = 0;
        Var pa = gauss_chain_prefix(A, ia, INVALID_VAR, &nb_rhs);
        Var pb = gauss_chain_prefix(B, ib, INVALID_VAR, &nb_rhs);
        Var pc = INVALID_VAR;
        if (ic > 0) {
            if (ic < c) pc = (ic == 1u) ? cvars[0] : C->aux[ic - 2u];
            else if (c == 1u) pc = cvars[0];
            else if (c >= 2u) pc = z_end;
        }
        if (pa != INVALID_VAR) nb_vars[nbm++] = pa;
        if (pb != INVALID_VAR) nb_vars[nbm++] = pb;
        if (pc != INVALID_VAR) nb_vars[nbm++] = pc;
        gauss_cancel_bridge_vars(nb_vars, &nbm);

        bool same = (nbm == bm && nb_rhs == b_rhs &&
                     (bm == 0 || memcmp(nb_vars, b_vars, (size_t)bm * sizeof(Var)) == 0));
        if (!same) {
            if (!(nbm == 0 && nb_rhs == 0)) {
                bool sv_in = false;
                for (uint32_t j = 0; j < nbm; ++j) if (nb_vars[j] == sv) sv_in = true;
                if (nbm == 0) {
                    /* Empty clause 0 = 1 via 1-var split on sv! */
                    Lit p1 = mkLit(sv, false), p2 = mkLit(sv, true);
                    proof_add_clause(s, &p1, 1u);
                    if (!s->error) proof_add_clause(s, &p2, 1u);
                    if (!s->error) proof_add_clause(s, NULL, 0);
                    if (!s->error) {
                        proof_delete_clause(s, &p1, 1u);
                        proof_delete_clause(s, &p2, 1u);
                    }
                } else {
                    uint32_t lim = 1u << nbm;
                    for (uint32_t mask = 0; mask < lim && !s->error; ++mask) {
                        if (((uint32_t)__builtin_popcount(mask) & 1u) != nb_rhs) {
                            Lit cl[3];
                            for (uint32_t j = 0; j < nbm; ++j) {
                                cl[j] = mkLit(nb_vars[j], ((mask >> j) & 1u) != 0);
                            }
                            if (sv_in) {
                                proof_add_clause(s, cl, nbm);
                            } else {
                                gauss_proof_split_rup(s, cl, nbm, &sv, 1u, 0u);
                            }
                        }
                    }
                }
            }
            if (!(bm == 0 && b_rhs == 0) && !s->error) {
                gauss_proof_emit_xor_eq(s, b_vars, bm, b_rhs, true);
            }
            memcpy(b_vars, nb_vars, (size_t)nbm * sizeof(Var));
            bm = nbm;
            b_rhs = nb_rhs;
        }
        s->work += 4u;
    }

    if (!s->error && c >= 2u) {
        /* Final bridge is unit (z_end = C->rhs). Derive final binary of C (1-step RUP!),
         * then delete the unit (z_end = C->rhs) and the 4 clauses defining z_end! */
        Var fv[2] = {(c == 2u ? cvars[0] : C->aux[c - 3u]), cvars[c - 1u]};
        gauss_proof_emit_xor_eq(s, fv, 2u, C->rhs, false);
        if (!s->error) gauss_proof_emit_xor_eq(s, &z_end, 1u, C->rhs, true);
        Var tri[3] = {z_end, (c == 2u ? cvars[0] : C->aux[c - 3u]), cvars[c - 1u]};
        if (!s->error) gauss_proof_emit_xor_eq(s, tri, 3u, 0u, true);
    }
    C->owns_final = (c >= 1u);
    if (s->error) {
        gauss_proof_free_chain(s, C, false);
        return false;
    }
    return true;
}

/* Prove the GF(2) sum of a subset of component equations eqs[eq_by_comp[ob + r]]
 * selected by bitmask `hist_row[0..hwords-1]`, ordering equations greedily by
 * overlap with the running accumulator to keep intermediate chains tiny! */
static bool gauss_proof_sum_subset(Solver *s, const XorEq *eqs, const uint32_t *eq_by_comp,
                                   uint32_t ob, uint32_t ceq, const uint64_t *hist_row,
                                   uint32_t hwords, Var *next_pvar,
                                   uint32_t expected_k, uint8_t expected_rhs) {
    uint32_t *sel = malloc((size_t)ceq * sizeof *sel);
    if (!sel) { s->error = true; return false; }
    uint32_t nsel = 0;
    for (uint32_t w = 0; w < hwords; ++w) {
        uint64_t x = hist_row[w];
        while (x) {
            uint32_t r = (w << 6) + (uint32_t)__builtin_ctzll(x);
            if (r < ceq) sel[nsel++] = r;
            x &= x - 1;
        }
    }
    if (nsel == 0) { free(sel); return false; }

    /* Track variables currently in `acc` for greedy minimum-growth ordering! */
    uint8_t *in_acc = calloc((size_t)s->num_vars + 1u, 1);
    uint8_t *used = calloc(nsel, 1);
    if (!in_acc || !used) {
        free(sel); free(in_acc); free(used);
        s->error = true; return false;
    }

    ProofXorChain acc;
    memset(&acc, 0, sizeof acc);
    const XorEq *first = &eqs[eq_by_comp[ob + sel[0]]];
    if (!gauss_proof_chain_from_eq(s, first, &acc, next_pvar)) {
        free(sel); free(in_acc); free(used);
        return false;
    }
    used[0] = 1;
    for (uint32_t j = 0; j < acc.k; ++j) in_acc[acc.vars[j]] = 1;

    for (uint32_t step = 1; step < nsel && !s->error; ++step) {
        /* Pick unused equation with best score = 2*overlap - k (minimizes new acc.k!). */
        uint32_t best_i = UINT32_MAX;
        int best_score = -1000000;
        for (uint32_t i = 0; i < nsel; ++i) {
            if (used[i]) continue;
            const XorEq *e = &eqs[eq_by_comp[ob + sel[i]]];
            int ov = 0;
            for (uint32_t j = 0; j < e->k; ++j) {
                if (in_acc[e->vars[j]]) ov++;
            }
            int sc = 2 * ov - (int)e->k;
            if (ov > 0 && sc > best_score) { best_score = sc; best_i = i; }
        }
        if (best_i == UINT32_MAX) {
            for (uint32_t i = 0; i < nsel; ++i) {
                if (!used[i]) { best_i = i; break; }
            }
        }
        used[best_i] = 1;

        ProofXorChain rhs_ch, next_acc;
        const XorEq *e = &eqs[eq_by_comp[ob + sel[best_i]]];
        if (!gauss_proof_chain_from_eq(s, e, &rhs_ch, next_pvar)) {
            gauss_proof_free_chain(s, &acc, true);
            free(sel); free(in_acc); free(used);
            return false;
        }
        if (!gauss_proof_xor_chains(s, &acc, &rhs_ch, &next_acc, next_pvar)) {
            gauss_proof_free_chain(s, &rhs_ch, true);
            gauss_proof_free_chain(s, &acc, true);
            free(sel); free(in_acc); free(used);
            return false;
        }
        for (uint32_t j = 0; j < acc.k; ++j) in_acc[acc.vars[j]] = 0;
        gauss_proof_free_chain(s, &rhs_ch, true);
        gauss_proof_free_chain(s, &acc, true);
        acc = next_acc;
        for (uint32_t j = 0; j < acc.k; ++j) in_acc[acc.vars[j]] = 1;
    }

    free(sel); free(in_acc); free(used);
    bool ok = !s->error && acc.k == expected_k && acc.rhs == expected_rhs;
    /* Keep the final derived unit/binary clauses in the proof (delete only chain aux vars!). */
    gauss_proof_free_chain(s, &acc, !ok);
    return ok;
}

/* =====================================================================
 * Extraction, Root RREF, and Live Matrix Construction
 * ===================================================================== */

bool gauss_extract_and_init(Solver *s, bool keep_matrices) {
    if (!s || s->proof_journal) return true;
    const bool proof_mode = (s->proof_file != NULL);
    /* In --proof mode, keep tiny RUP-only unit checks RUP while certifying real XOR benchmarks! */
    if (proof_mode) {
        if (s->num_clauses < 64u) return true;
        keep_matrices = false;
    }
    if (!s->num_vars || !s->num_clauses || s->num_clauses > 250000 ||
        s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;

    if (keep_matrices) gauss_free(s);

    /* Count eligible clauses of size 2..GAUSS_MAX_DIRECT_K. */
    uint32_t elig = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz >= 2 && sz <= GAUSS_MAX_DIRECT_K) elig++;
    }
    if (elig < 4) return true;

    size_t cap = 64;
    while (cap < (size_t)elig * 2u && cap < (1u << 21)) cap <<= 1;
    if (cap < (size_t)elig + 16u) return true;
    XorSlot *tab = calloc(cap, sizeof *tab);
    if (!tab) { s->error = true; return false; }

    uint32_t *used_slots = malloc(MIN(cap, (size_t)elig + 1u) * sizeof *used_slots);
    if (!used_slots) { free(tab); s->error = true; return false; }
    uint32_t n_used = 0;

    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 2 || sz > GAUSS_MAX_DIRECT_K) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        Lit tmp[GAUSS_MAX_DIRECT_K];
        bool skip = false;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { skip = true; break; }
            tmp[j] = ls[j];
        }
        if (skip) continue;
        for (uint32_t a = 1; a < sz; ++a) {
            Lit key = tmp[a];
            uint32_t b = a;
            while (b > 0 && var(tmp[b - 1]) > var(key)) { tmp[b] = tmp[b - 1]; --b; }
            tmp[b] = key;
        }
        Var vs[GAUSS_MAX_DIRECT_K] = {0};
        uint32_t neg_mask = 0;
        for (uint32_t j = 0; j < sz; ++j) {
            vs[j] = var(tmp[j]);
            if (j > 0 && vs[j] == vs[j - 1]) { skip = true; break; }
            if (sign(tmp[j])) neg_mask |= (1u << j);
        }
        if (skip) continue;

        size_t pos = (size_t)(hash_xor_tuple(vs, (uint8_t)sz) & (cap - 1u));
        for (;;) {
            if (tab[pos].k == 0) {
                tab[pos].k = (uint8_t)sz;
                memcpy(tab[pos].vars, vs, sizeof vs);
                tab[pos].mask[neg_mask >> 6] = UINT64_C(1) << (neg_mask & 63u);
                if (n_used < elig) used_slots[n_used++] = (uint32_t)pos;
                break;
            }
            if (tab[pos].k == sz && memcmp(tab[pos].vars, vs, (size_t)sz * sizeof(Var)) == 0) {
                tab[pos].mask[neg_mask >> 6] |= UINT64_C(1) << (neg_mask & 63u);
                break;
            }
            pos = (pos + 1u) & (cap - 1u);
        }
    }

    uint64_t even_need[GAUSS_MAX_DIRECT_K + 1][2];
    uint64_t odd_need[GAUSS_MAX_DIRECT_K + 1][2];
    memset(even_need, 0, sizeof even_need);
    memset(odd_need, 0, sizeof odd_need);
    for (uint32_t k = 2; k <= GAUSS_MAX_DIRECT_K; ++k) {
        for (uint32_t m = 0; m < (1u << k); ++m) {
            uint32_t parity = (uint32_t)__builtin_popcount(m) & 1u;
            if (parity == 0) even_need[k][m >> 6] |= (UINT64_C(1) << (m & 63u));
            else odd_need[k][m >> 6] |= (UINT64_C(1) << (m & 63u));
        }
    }

    uint32_t n_ge3 = 0, n_eqs = 0;
    for (uint32_t ui = 0; ui < n_used; ++ui) {
        XorSlot *sl = &tab[used_slots[ui]];
        uint8_t k = sl->k;
        bool has_even = xor_parity_covered(tab, cap, sl, even_need[k][0], even_need[k][1]);
        bool has_odd  = xor_parity_covered(tab, cap, sl, odd_need[k][0], odd_need[k][1]);
        if (has_even && has_odd) {
            if (proof_mode) {
                gauss_proof_split_rup(s, NULL, 0u, sl->vars, k, 0u);
            }
            free(used_slots); free(tab);
            if (s->error) return false;
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (has_even || has_odd) {
            sl->used_as_xor = has_even ? 2u : 1u; /* 2 => rhs=1, 1 => rhs=0 */
            n_eqs++;
            if (k >= 3) n_ge3++;
        }
    }
    if (n_ge3 < 2 || n_eqs > 32768u) {
        free(used_slots); free(tab);
        return true;
    }

    /* Count non-XOR clause occurrences per variable so we can safely stitch
     * pure Tseitin XOR-chain helper variables (non_xor_occ == 0 && xor_deg == 2). */
    uint32_t *non_xor_occ = calloc((size_t)(s->num_vars + 1u), sizeof *non_xor_occ);
    XorEq *eqs = calloc(n_eqs, sizeof *eqs);
    if (!non_xor_occ || !eqs) {
        free(non_xor_occ); free(eqs); free(used_slots); free(tab);
        s->error = true; return false;
    }

    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool in_xor = false;
        if (sz >= 2 && sz <= GAUSS_MAX_DIRECT_K) {
            Var vs[GAUSS_MAX_DIRECT_K] = {0};
            bool ok_tuple = true;
            for (uint32_t j = 0; j < sz; ++j) {
                if (s->values[var(ls[j])] != UNDEF) { ok_tuple = false; break; }
                vs[j] = var(ls[j]);
            }
            if (ok_tuple) {
                for (uint32_t a = 1; a < sz; ++a) {
                    Var key = vs[a]; uint32_t b = a;
                    while (b > 0 && vs[b - 1] > key) { vs[b] = vs[b - 1]; --b; }
                    vs[b] = key;
                }
                const XorSlot *p = xor_tab_find(tab, cap, vs, (uint8_t)sz);
                if (p && p->used_as_xor) in_xor = true;
            }
        }
        if (!in_xor) {
            for (uint32_t j = 0; j < sz; ++j) non_xor_occ[var(ls[j])]++;
        }
    }

    uint32_t eq_count = 0;
    bool eq_oom = false;
    for (uint32_t ui = 0; ui < n_used; ++ui) {
        XorSlot *sl = &tab[used_slots[ui]];
        if (!sl->used_as_xor) continue;
        XorEq *e = &eqs[eq_count];
        e->vars = malloc((size_t)sl->k * sizeof(Var));
        if (!e->vars) { eq_oom = true; break; }
        e->k = sl->k;
        e->rhs = (sl->used_as_xor == 2u) ? 1u : 0u;
        e->alive = 1u;
        memcpy(e->vars, sl->vars, (size_t)sl->k * sizeof(Var));
        eq_count++;
    }
    free(used_slots); free(tab);

    if (eq_oom) {
        for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
        free(eqs); free(non_xor_occ);
        s->error = true; return false;
    }

    /* Tseitin XOR-chain stitching: eliminate any internal XOR-only variable `t`
     * (non_xor_occ[t] == 0) that appears in exactly two XOR equations E1, E2
     * by replacing {E1, E2} with {E1 ^ E2} inside the GF(2) system!
     * Skipped in --proof mode so every equation in `eqs[]` stays directly CNF-backed
     * (2 <= k <= 7) for RREF history bitmask DRAT proof emission. */
    if (!proof_mode) {
        uint32_t *xor_deg = calloc((size_t)(s->num_vars + 1u), sizeof *xor_deg);
        uint32_t *eq_a = malloc((size_t)(s->num_vars + 1u) * sizeof *eq_a);
        uint32_t *eq_b = malloc((size_t)(s->num_vars + 1u) * sizeof *eq_b);
        Var *queue = malloc((size_t)(s->num_vars + 1u) * sizeof *queue);
        uint8_t *in_q = calloc((size_t)(s->num_vars + 1u), 1);
        if (!xor_deg || !eq_a || !eq_b || !queue || !in_q) {
            free(xor_deg); free(eq_a); free(eq_b); free(queue); free(in_q);
            for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
            free(eqs); free(non_xor_occ);
            s->error = true; return false;
        }
        memset(eq_a, 0xFF, (size_t)(s->num_vars + 1u) * sizeof *eq_a);
        memset(eq_b, 0xFF, (size_t)(s->num_vars + 1u) * sizeof *eq_b);
        for (uint32_t ei = 0; ei < eq_count; ++ei) {
            for (uint32_t j = 0; j < eqs[ei].k; ++j) {
                Var v = eqs[ei].vars[j];
                uint32_t d = xor_deg[v]++;
                if (d == 0) eq_a[v] = ei;
                else if (d == 1) eq_b[v] = ei;
            }
        }
        uint32_t qh = 0, qt = 0;
        for (Var v = 1; v <= s->num_vars; ++v) {
            if (non_xor_occ[v] == 0 && xor_deg[v] == 2) {
                queue[qt++] = v;
                in_q[v] = 1;
            }
        }
        bool stitch_unsat = false;
        while (qh < qt && !stitch_unsat && !eq_oom) {
            Var t = queue[qh++];
            in_q[t] = 0;
            if (non_xor_occ[t] != 0 || xor_deg[t] != 2) continue;
            uint32_t e1 = eq_a[t], e2 = eq_b[t];
            if (e1 == UINT32_MAX || e2 == UINT32_MAX || e1 == e2 ||
                !eqs[e1].alive || !eqs[e2].alive) continue;
            if (eqs[e1].k + eqs[e2].k > 256u) continue; /* Keep stitched rows sparse */

            /* Compute sorted symmetric difference of eqs[e1].vars and eqs[e2].vars. */
            uint32_t max_k = eqs[e1].k + eqs[e2].k;
            Var *merged = malloc((size_t)(max_k ? max_k : 1u) * sizeof(Var));
            if (!merged) { eq_oom = true; break; }
            uint32_t i1 = 0, i2 = 0, mk = 0;
            const Var *v1 = eqs[e1].vars, *v2 = eqs[e2].vars;
            uint32_t k1 = eqs[e1].k, k2 = eqs[e2].k;
            while (i1 < k1 && i2 < k2) {
                if (v1[i1] < v2[i2]) {
                    merged[mk++] = v1[i1++];
                } else if (v2[i2] < v1[i1]) {
                    Var u = v2[i2++];
                    merged[mk++] = u;
                    /* u moves from e2 to e1 in eq_a/eq_b tracking */
                    if (eq_a[u] == e2) eq_a[u] = e1;
                    else if (eq_b[u] == e2) eq_b[u] = e1;
                } else {
                    /* Cancelled variable (including t)! */
                    Var u = v1[i1];
                    i1++; i2++;
                    if (xor_deg[u] >= 2) xor_deg[u] -= 2;
                    else xor_deg[u] = 0;
                    if (eq_a[u] == e1 || eq_a[u] == e2) eq_a[u] = UINT32_MAX;
                    if (eq_b[u] == e1 || eq_b[u] == e2) eq_b[u] = UINT32_MAX;
                }
            }
            while (i1 < k1) merged[mk++] = v1[i1++];
            while (i2 < k2) {
                Var u = v2[i2++];
                merged[mk++] = u;
                if (eq_a[u] == e2) eq_a[u] = e1;
                else if (eq_b[u] == e2) eq_b[u] = e1;
            }

            free(eqs[e1].vars);
            free(eqs[e2].vars);
            eqs[e2].vars = NULL;
            eqs[e2].k = 0;
            eqs[e2].alive = 0;
            eqs[e1].vars = merged;
            eqs[e1].k = mk;
            eqs[e1].rhs ^= eqs[e2].rhs;

            if (mk == 0) {
                if (eqs[e1].rhs) { stitch_unsat = true; break; }
                eqs[e1].alive = 0;
            }
        }
        free(xor_deg); free(eq_a); free(eq_b); free(queue); free(in_q);
        free(non_xor_occ);
        if (stitch_unsat) {
            for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
            free(eqs);
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (eq_oom) {
            for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
            free(eqs);
            s->error = true; return false;
        }
    } else {
        free(non_xor_occ);
    }

    /* Compact alive equations and map variables. */
    uint32_t *var_id = malloc((size_t)(s->num_vars + 1u) * sizeof *var_id);
    if (!var_id) {
        for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
        free(eqs);
        s->error = true; return false;
    }
    memset(var_id, 0xFF, (size_t)(s->num_vars + 1u) * sizeof *var_id);

    uint32_t live_eqs = 0, n_all_vars = 0;
    bool root_unsat = false;
    bool stitched_unit = false;
    for (uint32_t i = 0; i < eq_count; ++i) {
        if (!eqs[i].alive || eqs[i].k == 0) { free(eqs[i].vars); eqs[i].vars = NULL; continue; }
        if (eqs[i].k == 1) {
            Lit unit = mkLit(eqs[i].vars[0], eqs[i].rhs == 0);
            free(eqs[i].vars); eqs[i].vars = NULL;
            if (!gauss_enqueue_root_unit(s, unit)) { root_unsat = true; break; }
            stitched_unit = true;
            continue;
        }
        if (live_eqs != i) {
            eqs[live_eqs] = eqs[i];
            eqs[i].vars = NULL;
        }
        for (uint32_t j = 0; j < eqs[live_eqs].k; ++j) {
            Var v = eqs[live_eqs].vars[j];
            if (var_id[v] == UINT32_MAX) var_id[v] = n_all_vars++;
        }
        live_eqs++;
    }
    if (root_unsat) {
        for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
        free(eqs); free(var_id);
        return false;
    }
    eq_count = live_eqs;

    if (n_all_vars == 0 || eq_count < 2) {
        for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
        free(eqs); free(var_id);
        if (stitched_unit && solver_propagate(s) != INVALID_CLAUSE) {
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        return !s->error && s->result != FALSE;
    }

    /* Partition XOR equations into connected components via Union-Find. */
    uint32_t *uf = malloc((size_t)n_all_vars * sizeof *uf);
    uint32_t *comp_id = malloc((size_t)n_all_vars * sizeof *comp_id);
    uint32_t *comp_eqs = calloc(n_all_vars, sizeof *comp_eqs);
    uint32_t *comp_vars = calloc(n_all_vars, sizeof *comp_vars);
    uint32_t *comp_ge3 = calloc(n_all_vars, sizeof *comp_ge3);
    if (!uf || !comp_id || !comp_eqs || !comp_vars || !comp_ge3) {
        free(uf); free(comp_id); free(comp_eqs); free(comp_vars); free(comp_ge3);
        for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
        free(eqs); free(var_id);
        s->error = true; return false;
    }
    for (uint32_t i = 0; i < n_all_vars; ++i) uf[i] = i;
    for (uint32_t i = 0; i < eq_count; ++i) {
        uint32_t r0 = var_id[eqs[i].vars[0]];
        for (uint32_t j = 1; j < eqs[i].k; ++j) uf_union(uf, r0, var_id[eqs[i].vars[j]]);
    }

    memset(comp_id, 0xFF, (size_t)n_all_vars * sizeof *comp_id);
    uint32_t n_comps = 0;
    for (uint32_t i = 0; i < n_all_vars; ++i) {
        uint32_t r = uf_find(uf, i);
        if (comp_id[r] == UINT32_MAX) comp_id[r] = n_comps++;
        comp_id[i] = comp_id[r];
        comp_vars[comp_id[i]]++;
    }
    for (uint32_t i = 0; i < eq_count; ++i) {
        uint32_t cid = comp_id[var_id[eqs[i].vars[0]]];
        comp_eqs[cid]++;
        if (eqs[i].k >= 3) comp_ge3[cid]++;
    }

    /* Group equations by component using CSR offsets. */
    uint32_t *comp_off = calloc((size_t)n_comps + 1u, sizeof *comp_off);
    uint32_t *comp_cur = malloc(((size_t)n_comps + 1u) * sizeof *comp_cur);
    uint32_t *eq_by_comp = malloc((size_t)eq_count * sizeof *eq_by_comp);
    if (!comp_off || !comp_cur || !eq_by_comp) {
        free(comp_off); free(comp_cur); free(eq_by_comp);
        free(uf); free(comp_id); free(comp_eqs); free(comp_vars); free(comp_ge3);
        for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
        free(eqs); free(var_id);
        s->error = true; return false;
    }
    for (uint32_t c = 0; c < n_comps; ++c) comp_off[c + 1u] = comp_off[c] + comp_eqs[c];
    memcpy(comp_cur, comp_off, ((size_t)n_comps + 1u) * sizeof *comp_cur);
    for (uint32_t i = 0; i < eq_count; ++i) {
        uint32_t cid = comp_id[var_id[eqs[i].vars[0]]];
        eq_by_comp[comp_cur[cid]++] = i;
    }
    free(comp_cur);
    free(uf);
    free(comp_id);

    /* Temporary array of live matrices built across components. */
    GaussMatrix built_mats[GAUSS_MAX_MATS];
    uint32_t n_built = 0;
    uint32_t total_live_rows = 0;
    bool unsat = false;
    bool added_unit = stitched_unit;

    uint32_t *local_col = malloc((size_t)(s->num_vars + 1u) * sizeof *local_col);
    if (!local_col) {
        free(comp_off); free(eq_by_comp);
        free(comp_eqs); free(comp_vars); free(comp_ge3);
        for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
        free(eqs); free(var_id);
        s->error = true; return false;
    }
    memset(local_col, 0xFF, (size_t)(s->num_vars + 1u) * sizeof *local_col);

    for (uint32_t cid = 0; cid < n_comps && !unsat && !s->error; ++cid) {
        if (comp_ge3[cid] < 2) continue;
        uint32_t ceq = comp_eqs[cid];
        uint32_t cvar = comp_vars[cid];
        if (cvar > 4096u || ceq > 4096u) continue;

        uint32_t ob = comp_off[cid], oe = comp_off[cid + 1u];
        Var *c2v = malloc((size_t)cvar * sizeof *c2v);
        uint32_t nwords = (cvar + 63u) / 64u;
        uint32_t hwords = proof_mode ? ((ceq + 63u) / 64u) : 0u;
        uint64_t *mat = calloc((size_t)ceq * nwords, sizeof *mat);
        uint8_t *rhs = calloc(ceq, sizeof *rhs);
        uint64_t *hist = proof_mode ? calloc((size_t)ceq * hwords, sizeof *hist) : NULL;
        if (!c2v || !mat || !rhs || (proof_mode && !hist)) {
            free(c2v); free(mat); free(rhs); free(hist);
            s->error = true; break;
        }
        if (hist) {
            for (uint32_t r = 0; r < ceq; ++r) {
                hist[(size_t)r * hwords + (r >> 6)] = UINT64_C(1) << (r & 63u);
            }
        }

        uint32_t ncols = 0;
        for (uint32_t idx = ob; idx < oe; ++idx) {
            const XorEq *e = &eqs[eq_by_comp[idx]];
            uint32_t r = idx - ob;
            uint64_t *rr = mat + (size_t)r * nwords;
            rhs[r] = e->rhs;
            for (uint32_t j = 0; j < e->k; ++j) {
                Var v = e->vars[j];
                uint32_t c = local_col[v];
                if (c == UINT32_MAX) {
                    c = ncols++;
                    local_col[v] = c;
                    c2v[c] = v;
                }
                rr[c >> 6] ^= (UINT64_C(1) << (c & 63u));
            }
        }
        /* Clear local_col marks immediately. */
        for (uint32_t c = 0; c < ncols; ++c) local_col[c2v[c]] = UINT32_MAX;

        /* Gauss-Jordan RREF over GF(2). */
        uint32_t pivot_row = 0;
        for (uint32_t col = 0; col < ncols && pivot_row < ceq; ++col) {
            uint32_t w = col >> 6;
            uint64_t bit = UINT64_C(1) << (col & 63u);
            uint32_t found = ceq;
            for (uint32_t r = pivot_row; r < ceq; ++r) {
                if (mat[(size_t)r * nwords + w] & bit) { found = r; break; }
            }
            if (found == ceq) continue;
            if (found != pivot_row) {
                uint64_t *ra = mat + (size_t)pivot_row * nwords;
                uint64_t *rb = mat + (size_t)found * nwords;
                for (uint32_t k = 0; k < nwords; ++k) {
                    uint64_t t = ra[k]; ra[k] = rb[k]; rb[k] = t;
                }
                if (hist) {
                    uint64_t *ha = hist + (size_t)pivot_row * hwords;
                    uint64_t *hb = hist + (size_t)found * hwords;
                    for (uint32_t k = 0; k < hwords; ++k) {
                        uint64_t t = ha[k]; ha[k] = hb[k]; hb[k] = t;
                    }
                }
                uint8_t tr = rhs[pivot_row]; rhs[pivot_row] = rhs[found]; rhs[found] = tr;
            }
            uint64_t *pr = mat + (size_t)pivot_row * nwords;
            uint64_t *ph = hist ? (hist + (size_t)pivot_row * hwords) : NULL;
            uint8_t prhs = rhs[pivot_row];
            for (uint32_t r = 0; r < ceq; ++r) {
                if (r == pivot_row) continue;
                uint64_t *rr = mat + (size_t)r * nwords;
                if (rr[w] & bit) {
                    for (uint32_t k = 0; k < nwords; ++k) rr[k] ^= pr[k];
                    if (hist) {
                        uint64_t *rh = hist + (size_t)r * hwords;
                        for (uint32_t k = 0; k < hwords; ++k) rh[k] ^= ph[k];
                    }
                    rhs[r] ^= prhs;
                }
            }
            pivot_row++;
            s->work += (uint64_t)ceq * nwords;
        }

        /* In --proof mode, check for any 0 = 1 contradiction FIRST so we immediately emit
         * the single minimal UNSAT chain proof without wasting time on units/binaries!
         * Allocate proof-only vars monotonically from s->proof_aux_next (strictly disjoint
         * from real solver vars + amo/sbva aux vars, and NEVER reused because drat-trim
         * retains deleted unit clauses in backward checking mode!). */
        if (proof_mode && s->proof_aux_next == 0) {
            s->proof_aux_base = s->num_vars + 32768u;
            s->proof_aux_next = s->proof_aux_base;
        }
        if (proof_mode && s->proof_aux_next <= s->num_vars) {
            s->proof_aux_next = s->num_vars + 32768u;
        }
        if (proof_mode) {
            uint32_t best_unsat_r = UINT32_MAX;
            uint32_t best_unsat_w = UINT32_MAX;
            for (uint32_t r = 0; r < ceq; ++r) {
                if (!rhs[r]) continue;
                const uint64_t *rr = mat + (size_t)r * nwords;
                bool zero = true;
                for (uint32_t w = 0; w < nwords; ++w) {
                    if (rr[w]) { zero = false; break; }
                }
                if (zero) {
                    uint32_t hw = 0;
                    const uint64_t *rh = hist + (size_t)r * hwords;
                    for (uint32_t w = 0; w < hwords; ++w) hw += (uint32_t)__builtin_popcountll(rh[w]);
                    if (hw < best_unsat_w) { best_unsat_w = hw; best_unsat_r = r; }
                }
            }
            if (best_unsat_r != UINT32_MAX) {
                const uint64_t *rh = hist + (size_t)best_unsat_r * hwords;
                if (gauss_proof_sum_subset(s, eqs, eq_by_comp, ob, ceq, rh, hwords,
                                           &s->proof_aux_next, 0u, 1u)) {
                    s->base_unsat = true;
                    s->result = FALSE;
                    unsat = true;
                }
                free(c2v); free(mat); free(rhs); free(hist);
                continue;
            }
        }

        /* Harvest contradictions, units, binary equivalences, and count live rows (weight >= 2). */
        uint32_t n_live_ge2 = 0, n_live_ge3 = 0;
        uint32_t proof_facts = 0;
        for (uint32_t r = 0; r < ceq && !unsat && !s->error; ++r) {
            const uint64_t *rr = mat + (size_t)r * nwords;
            uint32_t cnt = 0, c0 = 0, c1 = 0;
            for (uint32_t w = 0; w < nwords && cnt <= 3; ++w) {
                uint64_t x = rr[w];
                while (x) {
                    uint32_t col = (w << 6) + (uint32_t)__builtin_ctzll(x);
                    if (cnt == 0) c0 = col;
                    else if (cnt == 1) c1 = col;
                    cnt++;
                    if (cnt > 3) break;
                    x &= x - 1;
                }
            }
            if (cnt == 0) {
                if (rhs[r]) { s->base_unsat = true; s->result = FALSE; unsat = true; break; }
            } else if (cnt == 1) {
                Lit unit = mkLit(c2v[c0], rhs[r] == 0);
                if (proof_mode) {
                    if (s->values[var(unit)] == (uint8_t)(2u - sign(unit))) continue;
                    if (proof_facts >= 64u) continue;
                    const uint64_t *rh = hist + (size_t)r * hwords;
                    uint32_t hw = 0;
                    for (uint32_t w = 0; w < hwords; ++w) hw += (uint32_t)__builtin_popcountll(rh[w]);
                    if (hw > 512u) continue;
                    if (!gauss_proof_sum_subset(s, eqs, eq_by_comp, ob, ceq, rh, hwords,
                                                &s->proof_aux_next, 1u, rhs[r])) {
                        continue;
                    }
                    proof_facts++;
                }
                if (!gauss_enqueue_root_unit(s, unit)) {
                    if (proof_mode && !s->error) proof_add_clause(s, NULL, 0);
                    unsat = true; break;
                }
                added_unit = true;
            } else if (cnt == 2) {
                n_live_ge2++;
                Lit a = mkLit(c2v[c0], false);
                Lit b = mkLit(c2v[c1], rhs[r] != 0);
                Lit pairs[2][2] = {{neg(a), b}, {neg(b), a}};
                if (proof_mode) {
                    if (gauss_has_binary_watch(s, pairs[0][0], pairs[0][1]) &&
                        gauss_has_binary_watch(s, pairs[1][0], pairs[1][1])) {
                        continue;
                    }
                    if (proof_facts >= 64u) continue;
                    const uint64_t *rh = hist + (size_t)r * hwords;
                    uint32_t hw = 0;
                    for (uint32_t w = 0; w < hwords; ++w) hw += (uint32_t)__builtin_popcountll(rh[w]);
                    if (hw > 256u) continue;
                    if (!gauss_proof_sum_subset(s, eqs, eq_by_comp, ob, ceq, rh, hwords,
                                                &s->proof_aux_next, 2u, rhs[r])) {
                        continue;
                    }
                    proof_facts++;
                }
                for (int p = 0; p < 2 && !s->error; ++p) {
                    if (gauss_has_binary_watch(s, pairs[p][0], pairs[p][1])) continue;
                    if (s->num_learnts == s->learnts_size) {
                        uint32_t ncap = s->learnts_size ? s->learnts_size * 2u : 64u;
                        CRef *ptr = realloc(s->learnts, (size_t)ncap * sizeof *ptr);
                        if (!ptr) { s->error = true; break; }
                        s->learnts = ptr; s->learnts_size = ncap;
                    }
                    CRef cr = arena_alloc(s->arena, pairs[p], 2, true);
                    if (cr == INVALID_CLAUSE) { s->error = true; break; }
                    set_clause_lbd(s->arena, cr, 2);
                    s->learnts[s->num_learnts++] = cr;
                    CRef wref = arena_binary_watch_ref(cr);
                    watch_add(s->watches, pairs[p][0], wref, pairs[p][1]);
                    watch_add(s->watches, pairs[p][1], wref, pairs[p][0]);
                    if (s->watches->failed) { s->error = true; break; }
                }
            } else {
                n_live_ge2++;
                n_live_ge3++;
            }
        }
        free(hist);

        /* Keep component as a live Bird/Han-Jiang matrix if non-trivial and within bounds! */
        if (!unsat && !s->error && keep_matrices &&
            n_built < GAUSS_MAX_MATS &&
            n_live_ge3 >= GAUSS_MIN_LIVE_ROWS &&
            ncols <= GAUSS_MAX_COLS &&
            n_live_ge2 <= GAUSS_MAX_ROWS &&
            total_live_rows + n_live_ge2 <= GAUSS_MAX_TOTAL_ROWS) {
            GaussMatrix m;
            memset(&m, 0, sizeof m);
            m.ncols = ncols;
            m.nrows = n_live_ge2;
            m.nwords = nwords;
            m.col_to_var = c2v;
            m.rows = calloc((size_t)m.nrows * nwords, sizeof(uint64_t));
            m.rhs = malloc(m.nrows * sizeof(uint8_t));
            m.basic_col = malloc(m.nrows * sizeof(uint32_t));
            m.nb_col = malloc(m.nrows * sizeof(uint32_t));
            m.basic_row = malloc(ncols * sizeof(uint32_t));
            m.nb_watches = calloc(ncols, sizeof(GaussWatchList));
            if (!m.rows || !m.rhs || !m.basic_col || !m.nb_col || !m.basic_row || !m.nb_watches) {
                free_matrix(&m);
                free(mat); free(rhs);
                s->error = true; break;
            }
            memset(m.basic_row, 0xFF, ncols * sizeof(uint32_t));

            uint32_t dst = 0;
            bool ok = true;
            for (uint32_t r = 0; r < ceq && ok; ++r) {
                const uint64_t *rr = mat + (size_t)r * nwords;
                uint32_t c0 = UINT32_MAX, c1 = UINT32_MAX;
                for (uint32_t w = 0; w < nwords && c1 == UINT32_MAX; ++w) {
                    uint64_t x = rr[w];
                    while (x) {
                        uint32_t col = (w << 6) + (uint32_t)__builtin_ctzll(x);
                        if (c0 == UINT32_MAX) c0 = col;
                        else { c1 = col; break; }
                        x &= x - 1;
                    }
                }
                if (c0 == UINT32_MAX || c1 == UINT32_MAX) continue; /* weight 0 or 1 */
                memcpy(mat_row(&m, dst), rr, (size_t)nwords * sizeof(uint64_t));
                m.rhs[dst] = rhs[r];
                /* In RREF, c0 (the leading 1 of row r) is guaranteed to be 0 in ALL other rows! */
                m.basic_col[dst] = c0;
                m.basic_row[c0] = dst;
                m.nb_col[dst] = c1;
                if (!nb_watch_add(s, &m, c1, dst)) { ok = false; break; }
                dst++;
            }
            free(mat); free(rhs);
            if (!ok) {
                free_matrix(&m);
                break;
            }
            m.nrows = dst;
            built_mats[n_built++] = m;
            total_live_rows += dst;
        } else {
            free(c2v); free(mat); free(rhs);
        }
    }

    free(local_col);
    free(comp_off); free(eq_by_comp);
    free(comp_eqs); free(comp_vars); free(comp_ge3);
    for (uint32_t i = 0; i < eq_count; ++i) free(eqs[i].vars);
    free(eqs); free(var_id);

    if (unsat || s->error) {
        for (uint32_t mi = 0; mi < n_built; ++mi) free_matrix(&built_mats[mi]);
        return false;
    }

    /* Install live matrices into s->gauss if any were built. */
    if (n_built > 0) {
        GaussState *g = calloc(1, sizeof *g);
        uint32_t vcap = s->num_vars + 1u;
        if (!g) {
            for (uint32_t mi = 0; mi < n_built; ++mi) free_matrix(&built_mats[mi]);
            s->error = true; return false;
        }
        g->mats = malloc((size_t)n_built * sizeof *g->mats);
        g->var_mat = malloc((size_t)vcap * sizeof *g->var_mat);
        g->var_col = malloc((size_t)vcap * sizeof *g->var_col);
        g->clause_buf_cap = GAUSS_MAX_COLS + 4u;
        g->clause_buf = malloc((size_t)g->clause_buf_cap * sizeof(Lit));
        if (!g->mats || !g->var_mat || !g->var_col || !g->clause_buf) {
            free(g->mats); free(g->var_mat); free(g->var_col); free(g->clause_buf); free(g);
            for (uint32_t mi = 0; mi < n_built; ++mi) free_matrix(&built_mats[mi]);
            s->error = true; return false;
        }
        g->nmats = n_built;
        g->var_cap = vcap;
        memset(g->var_mat, 0xFF, (size_t)vcap * sizeof *g->var_mat);
        memset(g->var_col, 0xFF, (size_t)vcap * sizeof *g->var_col);
        for (uint32_t mi = 0; mi < n_built; ++mi) {
            g->mats[mi] = built_mats[mi];
            for (uint32_t c = 0; c < built_mats[mi].ncols; ++c) {
                Var v = built_mats[mi].col_to_var[c];
                g->var_mat[v] = (uint16_t)mi;
                g->var_col[v] = c;
            }
        }
        g->qhead = 0;
        s->gauss = g;
    }

    if (added_unit) {
        if (solver_propagate(s) != INVALID_CLAUSE) {
            if (proof_mode && !s->error) proof_add_clause(s, NULL, 0);
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (s->error || s->interrupted) return false;
    }
    /* Process any root-level units through live matrices right now. */
    if (s->gauss && s->gauss->qhead < s->trail_size) {
        CRef confl = gauss_propagate(s);
        if (confl != INVALID_CLAUSE || s->result == FALSE) {
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (s->error) return false;
        if (s->qhead < s->trail_size) {
            if (solver_propagate(s) != INVALID_CLAUSE) {
                s->base_unsat = true; s->result = FALSE;
                return false;
            }
        }
    }
    return !s->error && s->result != FALSE;
}

bool gauss_sync_after_inprocess(Solver *s) {
    if (!s || !s->gauss) return true;
    GaussState *g = s->gauss;
    bool stale = false;
    for (uint32_t mi = 0; mi < g->nmats && !stale; ++mi) {
        GaussMatrix *m = &g->mats[mi];
        for (uint32_t c = 0; c < m->ncols; ++c) {
            Var v = m->col_to_var[c];
            if (v > s->num_vars || elim_is_eliminated(s, v)) { stale = true; break; }
        }
    }
    if (stale) {
        return gauss_extract_and_init(s, true);
    }
    /* Also catch up qhead on any new root units derived during inprocessing! */
    if (g->qhead > s->trail_size) g->qhead = 0;
    while (g->qhead < s->trail_size && !s->error && !s->interrupted && s->result != FALSE) {
        CRef confl = gauss_propagate(s);
        if (confl != INVALID_CLAUSE || s->result == FALSE) {
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (s->qhead < s->trail_size) {
            if (solver_propagate(s) != INVALID_CLAUSE) {
                s->base_unsat = true; s->result = FALSE;
                return false;
            }
        }
    }
    return !s->error && s->result != FALSE;
}

/* =====================================================================
 * In-Search Bird / Han-Jiang Two-Watched-Column Propagation
 * ===================================================================== */

static uint32_t find_undef_col(const Solver *s, const GaussMatrix *m,
                               const uint64_t *row, uint32_t skip_col) {
    for (uint32_t w = 0; w < m->nwords; ++w) {
        uint64_t x = row[w];
        while (x) {
            uint32_t col = (w << 6) + (uint32_t)__builtin_ctzll(x);
            if (col != skip_col && s->values[m->col_to_var[col]] == UNDEF) return col;
            x &= x - 1;
        }
    }
    return UINT32_MAX;
}

static uint32_t find_any_col(const GaussMatrix *m, const uint64_t *row, uint32_t skip_col) {
    for (uint32_t w = 0; w < m->nwords; ++w) {
        uint64_t x = row[w];
        while (x) {
            uint32_t col = (w << 6) + (uint32_t)__builtin_ctzll(x);
            if (col != skip_col) return col;
            x &= x - 1;
        }
    }
    return UINT32_MAX;
}

static uint8_t eval_row_parity(const Solver *s, const GaussMatrix *m,
                               const uint64_t *row, uint32_t skip_col) {
    uint8_t parity = 0;
    for (uint32_t w = 0; w < m->nwords; ++w) {
        uint64_t x = row[w];
        while (x) {
            uint32_t col = (w << 6) + (uint32_t)__builtin_ctzll(x);
            if (col != skip_col) {
                uint8_t val = s->values[m->col_to_var[col]];
                ASSERT(val != UNDEF);
                parity ^= (val == TRUE ? 1u : 0u);
            }
            x &= x - 1;
        }
    }
    return parity;
}

static bool record_temp_clause(Solver *s, GaussState *g, CRef cr) {
    if (g->temp_size == g->temp_cap) {
        uint32_t ncap = g->temp_cap ? g->temp_cap * 2u : 64u;
        CRef *p = realloc(g->temp_clauses, (size_t)ncap * sizeof *p);
        if (!p) { s->error = true; return false; }
        g->temp_clauses = p;
        g->temp_cap = ncap;
    }
    g->temp_clauses[g->temp_size++] = cr;
    return true;
}

static void enqueue_gauss_prop(Solver *s, GaussState *g, const GaussMatrix *m,
                               uint32_t r, uint32_t imp_col,
                               bool *need_root_unit, Lit *root_unit_lit) {
    const uint64_t *rr = mat_row_const(m, r);
    uint8_t par = eval_row_parity(s, m, rr, imp_col);
    bool need_true = (par ^ m->rhs[r]) != 0;
    Var imp_v = m->col_to_var[imp_col];
    Lit imp_lit = mkLit(imp_v, !need_true);

    Lit *lits = g->clause_buf;
    lits[0] = imp_lit;
    uint32_t n = 1;
    Level max_lvl = 0;
    uint32_t max_idx = 0;

    for (uint32_t w = 0; w < m->nwords; ++w) {
        uint64_t x = rr[w];
        while (x) {
            uint32_t col = (w << 6) + (uint32_t)__builtin_ctzll(x);
            if (col != imp_col) {
                Var v = m->col_to_var[col];
                uint8_t val = s->values[v];
                ASSERT(val != UNDEF);
                lits[n] = mkLit(v, val == TRUE); /* falsified literal */
                Level lv = s->vars[v].level;
                if (max_idx == 0 || lv > max_lvl) { max_lvl = lv; max_idx = n; }
                n++;
            }
            x &= x - 1;
        }
    }

    if (s->decision_level == 0) {
        gauss_enqueue_root_unit(s, imp_lit);
        g->propagations++;
        return;
    }
    if (n == 1) {
        /* Weight-1 row discovered mid-search -> level-0 unit! Defer backtrack until pivot ends. */
        *need_root_unit = true;
        *root_unit_lit = imp_lit;
        return;
    }

    /* Place highest-level antecedent in watch position [1]. */
    if (max_idx > 1) {
        Lit tmp = lits[1]; lits[1] = lits[max_idx]; lits[max_idx] = tmp;
    }

    CRef cr = arena_alloc(s->arena, lits, n, true);
    if (cr == INVALID_CLAUSE) { s->error = true; return; }
    set_clause_lbd(s->arena, cr, MIN(n, 30u));
    CLAUSE_HEADER(s->arena, cr)->used = 0;
    if (!record_temp_clause(s, g, cr)) { arena_delete(s->arena, cr); return; }

    s->values[imp_v] = need_true ? TRUE : FALSE;
    if (s->lit_values_live) {
        s->lit_values[imp_lit] = TRUE;
        s->lit_values[imp_lit ^ 1u] = FALSE;
    }
    s->vars[imp_v].level = s->opts.chrono ? max_lvl : s->decision_level;
    s->vars[imp_v].reason = cr;
    s->binary_reasons[imp_v] = LIT_UNDEF;
    s->vars[imp_v].trail_pos = s->trail_size;
    s->trail[s->trail_size++].lit = imp_lit;
    if (s->opts.phase_saving) s->vars[imp_v].polarity = need_true;
    g->propagations++;
}

static CRef build_gauss_conflict(Solver *s, GaussState *g, const GaussMatrix *m,
                                 uint32_t r, bool *need_root_unit, Lit *root_unit_lit) {
    const uint64_t *rr = mat_row_const(m, r);
    Lit *lits = g->clause_buf;
    uint32_t n = 0;
    Level first_lvl = 0, second_lvl = 0;
    uint32_t first_idx = 0, second_idx = 0;

    for (uint32_t w = 0; w < m->nwords; ++w) {
        uint64_t x = rr[w];
        while (x) {
            uint32_t col = (w << 6) + (uint32_t)__builtin_ctzll(x);
            Var v = m->col_to_var[col];
            uint8_t val = s->values[v];
            ASSERT(val != UNDEF);
            lits[n] = mkLit(v, val == TRUE); /* falsified literal */
            Level lv = s->vars[v].level;
            if (n == 0 || lv > first_lvl) {
                second_idx = first_idx; second_lvl = first_lvl;
                first_idx = n; first_lvl = lv;
            } else if (n == 1 || lv > second_lvl) {
                second_idx = n; second_lvl = lv;
            }
            n++;
            x &= x - 1;
        }
    }

    if (n == 0 || first_lvl == 0) {
        s->base_unsat = true;
        s->result = FALSE;
        return INVALID_CLAUSE;
    }
    if (n == 1) {
        /* Weight-1 row violated at decision level > 0 -> lits[0] is a level-0 unit! */
        *need_root_unit = true;
        *root_unit_lit = lits[0];
        return INVALID_CLAUSE;
    }

    /* Place the two highest-level literals at [0] and [1]. */
    if (first_idx != 0) {
        Lit tmp = lits[0]; lits[0] = lits[first_idx]; lits[first_idx] = tmp;
        if (second_idx == 0) second_idx = first_idx;
    }
    if (second_idx != 1) {
        Lit tmp = lits[1]; lits[1] = lits[second_idx]; lits[second_idx] = tmp;
    }

    CRef cr = arena_alloc(s->arena, lits, n, true);
    if (cr == INVALID_CLAUSE) { s->error = true; return INVALID_CLAUSE; }
    set_clause_lbd(s->arena, cr, MIN(n, 30u));
    CLAUSE_HEADER(s->arena, cr)->used = 0;
    if (!record_temp_clause(s, g, cr)) { arena_delete(s->arena, cr); return INVALID_CLAUSE; }
    g->conflicts++;
    return cr;
}

CRef gauss_propagate(Solver *s) {
    GaussState *g = s->gauss;
    if (!g || s->error || s->interrupted || s->result == FALSE) return INVALID_CLAUSE;
    if (g->temp_size > 256u) gauss_gc_temp_clauses(s);

    while (g->qhead < s->trail_size) {
        if (s->error || s->interrupted || s->result == FALSE) return INVALID_CLAUSE;
        if (solver_budget_exhausted(s)) return INVALID_CLAUSE;

        Lit p = s->trail[g->qhead++].lit;
        Var pv = var(p);
        if (pv >= g->var_cap || g->var_mat[pv] == UINT16_MAX) continue;

        GaussMatrix *m = &g->mats[g->var_mat[pv]];
        uint32_t col = g->var_col[pv];

        bool need_root_unit = false;
        Lit root_unit_lit = LIT_UNDEF;

        /* Part A: Process rows watching `col` as their non-basic watch `nb_col`. */
        uint32_t wi = 0;
        while (wi < m->nb_watches[col].size) {
            uint32_t r = m->nb_watches[col].data[wi];
            uint32_t bcol = m->basic_col[r];
            const uint64_t *rr = mat_row_const(m, r);
            s->work += m->nwords;

            uint32_t c_new = find_undef_col(s, m, rr, bcol);
            if (c_new != UINT32_MAX) {
                /* Move non-basic watch from `col` to `c_new`. */
                GaussWatchList *wl = &m->nb_watches[col];
                wl->data[wi] = wl->data[--wl->size];
                m->nb_col[r] = c_new;
                if (!nb_watch_add(s, m, c_new, r)) return INVALID_CLAUSE;
                continue;
            }

            wi++;
            Var bv = m->col_to_var[bcol];
            if (s->values[bv] == UNDEF) {
                enqueue_gauss_prop(s, g, m, r, bcol, &need_root_unit, &root_unit_lit);
                if (s->error || s->result == FALSE) return INVALID_CLAUSE;
                if (need_root_unit) {
                    solver_backtrack(s, 0);
                    if (!gauss_enqueue_root_unit(s, root_unit_lit)) return INVALID_CLAUSE;
                    goto next_trail_entry;
                }
            } else {
                uint8_t par = eval_row_parity(s, m, rr, UINT32_MAX);
                if (par != m->rhs[r]) {
                    CRef confl = build_gauss_conflict(s, g, m, r, &need_root_unit, &root_unit_lit);
                    if (s->error || s->result == FALSE) return INVALID_CLAUSE;
                    if (need_root_unit) {
                        solver_backtrack(s, 0);
                        if (!gauss_enqueue_root_unit(s, root_unit_lit)) return INVALID_CLAUSE;
                        goto next_trail_entry;
                    }
                    return confl;
                }
            }
        }

        /* Part B: Process the unique row where `col` is the basic pivot (if any). */
        uint32_t r = m->basic_row[col];
        if (r != UINT32_MAX) {
            uint32_t nb = m->nb_col[r];
            uint64_t *rr = mat_row(m, r);
            s->work += m->nwords;

            uint32_t c_new = find_undef_col(s, m, rr, nb);
            if (c_new != UINT32_MAX) {
                /* Pivot basic column of row r from `col` to `c_new`!
                 * Eliminate `c_new` from ALL other rows before returning any conflict
                 * so the RREF basic-column invariant is ALWAYS preserved. */
                g->pivots++;
                m->basic_row[col] = UINT32_MAX;
                m->basic_row[c_new] = r;
                m->basic_col[r] = c_new;

                uint32_t w_idx = c_new >> 6;
                uint64_t bit = UINT64_C(1) << (c_new & 63u);
                uint8_t prhs = m->rhs[r];
                CRef pending_confl = INVALID_CLAUSE;

                for (uint32_t r2 = 0; r2 < m->nrows; ++r2) {
                    if (r2 == r) continue;
                    uint64_t *rr2 = mat_row(m, r2);
                    if (!(rr2[w_idx] & bit)) continue;

                    for (uint32_t k = 0; k < m->nwords; ++k) rr2[k] ^= rr[k];
                    m->rhs[r2] ^= prhs;
                    s->work += m->nwords;

                    /* Repair nb_col[r2] if cleared or assigned. */
                    uint32_t old_nb = m->nb_col[r2];
                    bool nb_gone = (old_nb == UINT32_MAX) || !row_has_bit(rr2, old_nb);
                    bool nb_assigned = !nb_gone && (s->values[m->col_to_var[old_nb]] != UNDEF);
                    if (nb_gone || nb_assigned) {
                        uint32_t b2 = m->basic_col[r2];
                        uint32_t u = find_undef_col(s, m, rr2, b2);
                        if (u != UINT32_MAX) {
                            if (old_nb != UINT32_MAX) nb_watch_remove(m, old_nb, r2);
                            m->nb_col[r2] = u;
                            if (!nb_watch_add(s, m, u, r2)) return INVALID_CLAUSE;
                        } else {
                            if (nb_gone) {
                                if (old_nb != UINT32_MAX) nb_watch_remove(m, old_nb, r2);
                                uint32_t any_c = find_any_col(m, rr2, b2);
                                m->nb_col[r2] = any_c;
                                if (any_c != UINT32_MAX && !nb_watch_add(s, m, any_c, r2))
                                    return INVALID_CLAUSE;
                            }
                            if (pending_confl == INVALID_CLAUSE && !need_root_unit &&
                                !s->error && s->result != FALSE) {
                                Var bv2 = m->col_to_var[b2];
                                if (s->values[bv2] == UNDEF) {
                                    enqueue_gauss_prop(s, g, m, r2, b2, &need_root_unit, &root_unit_lit);
                                } else {
                                    uint8_t par2 = eval_row_parity(s, m, rr2, UINT32_MAX);
                                    if (par2 != m->rhs[r2]) {
                                        pending_confl = build_gauss_conflict(s, g, m, r2,
                                                                             &need_root_unit, &root_unit_lit);
                                    }
                                }
                            }
                        }
                    }
                }

                if (s->error || s->result == FALSE) return INVALID_CLAUSE;
                if (need_root_unit) {
                    solver_backtrack(s, 0);
                    if (!gauss_enqueue_root_unit(s, root_unit_lit)) return INVALID_CLAUSE;
                    goto next_trail_entry;
                }
                if (pending_confl != INVALID_CLAUSE) return pending_confl;
            } else {
                /* No UNDEF column other than possibly `nb` -> unit on `nb` or conflict! */
                if (nb == UINT32_MAX) {
                    uint8_t par = eval_row_parity(s, m, rr, UINT32_MAX);
                    if (par != m->rhs[r]) {
                        CRef confl = build_gauss_conflict(s, g, m, r, &need_root_unit, &root_unit_lit);
                        if (s->error || s->result == FALSE) return INVALID_CLAUSE;
                        if (need_root_unit) {
                            solver_backtrack(s, 0);
                            if (!gauss_enqueue_root_unit(s, root_unit_lit)) return INVALID_CLAUSE;
                            goto next_trail_entry;
                        }
                        return confl;
                    }
                } else if (s->values[m->col_to_var[nb]] == UNDEF) {
                    enqueue_gauss_prop(s, g, m, r, nb, &need_root_unit, &root_unit_lit);
                    if (s->error || s->result == FALSE) return INVALID_CLAUSE;
                    if (need_root_unit) {
                        solver_backtrack(s, 0);
                        if (!gauss_enqueue_root_unit(s, root_unit_lit)) return INVALID_CLAUSE;
                        goto next_trail_entry;
                    }
                } else {
                    uint8_t par = eval_row_parity(s, m, rr, UINT32_MAX);
                    if (par != m->rhs[r]) {
                        CRef confl = build_gauss_conflict(s, g, m, r, &need_root_unit, &root_unit_lit);
                        if (s->error || s->result == FALSE) return INVALID_CLAUSE;
                        if (need_root_unit) {
                            solver_backtrack(s, 0);
                            if (!gauss_enqueue_root_unit(s, root_unit_lit)) return INVALID_CLAUSE;
                            goto next_trail_entry;
                        }
                        return confl;
                    }
                }
            }
        }
    next_trail_entry:;
    }

    return INVALID_CLAUSE;
}
