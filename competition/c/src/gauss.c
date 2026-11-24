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

typedef struct {
    Var vars[6];
    uint8_t k;
    uint64_t mask;
} XorSlot;

typedef struct {
    Var vars[6];
    uint8_t k;
    uint8_t rhs;
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
 * Extraction, Root RREF, and Live Matrix Construction
 * ===================================================================== */

bool gauss_extract_and_init(Solver *s, bool keep_matrices) {
    if (!s || s->proof_file || s->proof_journal) return true;
    if (!s->num_vars || !s->num_clauses || s->num_clauses > 250000 ||
        s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;

    if (keep_matrices) gauss_free(s);

    /* Count eligible clauses of size 2..6. */
    uint32_t elig = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz >= 2 && sz <= 6) elig++;
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
        if (sz < 2 || sz > 6) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        Lit tmp[6];
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
        Var vs[6] = {0, 0, 0, 0, 0, 0};
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
                tab[pos].mask = UINT64_C(1) << neg_mask;
                if (n_used < elig) used_slots[n_used++] = (uint32_t)pos;
                break;
            }
            if (tab[pos].k == sz && memcmp(tab[pos].vars, vs, (size_t)sz * sizeof(Var)) == 0) {
                tab[pos].mask |= UINT64_C(1) << neg_mask;
                break;
            }
            pos = (pos + 1u) & (cap - 1u);
        }
    }

    uint64_t even_need[7] = {0}, odd_need[7] = {0};
    for (uint32_t k = 2; k <= 6; ++k) {
        for (uint32_t m = 0; m < (1u << k); ++m) {
            uint32_t parity = 0, x = m;
            while (x) { parity ^= (x & 1u); x >>= 1; }
            if (parity == 0) even_need[k] |= (UINT64_C(1) << m);
            else odd_need[k] |= (UINT64_C(1) << m);
        }
    }

    uint32_t n_ge3 = 0, n_eqs = 0;
    for (uint32_t ui = 0; ui < n_used; ++ui) {
        XorSlot *sl = &tab[used_slots[ui]];
        uint8_t k = sl->k;
        bool has_even = (sl->mask & even_need[k]) == even_need[k];
        bool has_odd  = (sl->mask & odd_need[k]) == odd_need[k];
        if (has_even && has_odd) {
            free(used_slots); free(tab);
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (has_even || has_odd) {
            n_eqs++;
            if (k >= 3) n_ge3++;
        }
    }
    if (n_ge3 < 2 || n_eqs > 16384u) {
        free(used_slots); free(tab);
        return true;
    }

    XorEq *eqs = malloc((size_t)n_eqs * sizeof *eqs);
    uint32_t *var_id = malloc((size_t)(s->num_vars + 1u) * sizeof *var_id);
    if (!eqs || !var_id) {
        free(eqs); free(var_id); free(used_slots); free(tab);
        s->error = true; return false;
    }
    memset(var_id, 0xFF, (size_t)(s->num_vars + 1u) * sizeof *var_id);

    uint32_t eq_count = 0, n_all_vars = 0;
    for (uint32_t ui = 0; ui < n_used; ++ui) {
        XorSlot *sl = &tab[used_slots[ui]];
        uint8_t k = sl->k;
        bool has_even = (sl->mask & even_need[k]) == even_need[k];
        bool has_odd  = (sl->mask & odd_need[k]) == odd_need[k];
        if (!has_even && !has_odd) continue;
        XorEq *e = &eqs[eq_count++];
        e->k = k;
        e->rhs = has_even ? 1u : 0u;
        memcpy(e->vars, sl->vars, sizeof e->vars);
        for (uint8_t j = 0; j < k; ++j) {
            Var v = sl->vars[j];
            if (var_id[v] == UINT32_MAX) var_id[v] = n_all_vars++;
        }
    }
    free(used_slots); free(tab);

    if (n_all_vars == 0 || eq_count < 2) {
        free(eqs); free(var_id);
        return true;
    }

    /* Partition XOR equations into connected components via Union-Find. */
    uint32_t *uf = malloc((size_t)n_all_vars * sizeof *uf);
    uint32_t *comp_id = malloc((size_t)n_all_vars * sizeof *comp_id);
    uint32_t *comp_eqs = calloc(n_all_vars, sizeof *comp_eqs);
    uint32_t *comp_vars = calloc(n_all_vars, sizeof *comp_vars);
    uint32_t *comp_ge3 = calloc(n_all_vars, sizeof *comp_ge3);
    if (!uf || !comp_id || !comp_eqs || !comp_vars || !comp_ge3) {
        free(uf); free(comp_id); free(comp_eqs); free(comp_vars); free(comp_ge3);
        free(eqs); free(var_id);
        s->error = true; return false;
    }
    for (uint32_t i = 0; i < n_all_vars; ++i) uf[i] = i;
    for (uint32_t i = 0; i < eq_count; ++i) {
        uint32_t r0 = var_id[eqs[i].vars[0]];
        for (uint8_t j = 1; j < eqs[i].k; ++j) uf_union(uf, r0, var_id[eqs[i].vars[j]]);
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
    bool added_unit = false;

    uint32_t *local_col = malloc((size_t)(s->num_vars + 1u) * sizeof *local_col);
    if (!local_col) {
        free(comp_off); free(eq_by_comp);
        free(comp_eqs); free(comp_vars); free(comp_ge3);
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
        uint64_t *mat = calloc((size_t)ceq * nwords, sizeof *mat);
        uint8_t *rhs = calloc(ceq, sizeof *rhs);
        if (!c2v || !mat || !rhs) {
            free(c2v); free(mat); free(rhs);
            s->error = true; break;
        }

        uint32_t ncols = 0;
        for (uint32_t idx = ob; idx < oe; ++idx) {
            const XorEq *e = &eqs[eq_by_comp[idx]];
            uint32_t r = idx - ob;
            uint64_t *rr = mat + (size_t)r * nwords;
            rhs[r] = e->rhs;
            for (uint8_t j = 0; j < e->k; ++j) {
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
                uint8_t tr = rhs[pivot_row]; rhs[pivot_row] = rhs[found]; rhs[found] = tr;
            }
            uint64_t *pr = mat + (size_t)pivot_row * nwords;
            uint8_t prhs = rhs[pivot_row];
            for (uint32_t r = 0; r < ceq; ++r) {
                if (r == pivot_row) continue;
                uint64_t *rr = mat + (size_t)r * nwords;
                if (rr[w] & bit) {
                    for (uint32_t k = 0; k < nwords; ++k) rr[k] ^= pr[k];
                    rhs[r] ^= prhs;
                }
            }
            pivot_row++;
            s->work += (uint64_t)ceq * nwords;
        }

        /* Harvest contradictions, units, binary equivalences, and count live rows (weight >= 2). */
        uint32_t n_live_ge2 = 0, n_live_ge3 = 0;
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
                if (!gauss_enqueue_root_unit(s, unit)) { unsat = true; break; }
                added_unit = true;
            } else if (cnt == 2) {
                n_live_ge2++;
                Lit a = mkLit(c2v[c0], false);
                Lit b = mkLit(c2v[c1], rhs[r] != 0);
                Lit pairs[2][2] = {{neg(a), b}, {neg(b), a}};
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
