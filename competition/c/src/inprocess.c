/*********************************************************************
 * BSAT Competition Solver - Modern Inprocessing Engine
 *
 * All passes preserve DRAT/RUP proof soundness and model reconstruction
 * (via elim_save / elim_extend_model) and strictly respect solver budgets.
 *********************************************************************/

#include "../include/inprocess.h"
#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =====================================================================
 * Helpers: Clause Allocation, Watch Rebuild, Signatures
 * ===================================================================== */

static inline bool lit_is_true(const Solver *s, Lit l) {
    return s->values[var(l)] == (uint8_t)(2u - sign(l));
}

static inline bool lit_is_false(const Solver *s, Lit l) {
    return s->values[var(l)] == (uint8_t)(1u + sign(l));
}

static inline uint64_t clause_sig(const Lit *lits, uint32_t n) {
    uint64_t sig = 0;
    for (uint32_t i = 0; i < n; ++i) sig |= UINT64_C(1) << (var(lits[i]) & 63);
    return sig;
}

static inline void sort_lits_small(Lit *lits, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i) {
        Lit key = lits[i];
        uint32_t j = i;
        while (j > 0 && lits[j - 1] > key) { lits[j] = lits[j - 1]; --j; }
        lits[j] = key;
    }
}

static bool push_original_clause_ref(Solver *s, CRef cr) {
    if (s->num_clauses == s->clauses_capacity) {
        uint32_t cap = s->clauses_capacity ? s->clauses_capacity * 2 : 64;
        CRef *p = realloc(s->clauses, (size_t)cap * sizeof *p);
        if (!p) { s->error = true; return false; }
        s->clauses = p;
        s->clauses_capacity = cap;
    }
    s->clauses[s->num_clauses++] = cr;
    s->num_original = s->num_clauses;
    return true;
}

static bool enqueue_root_unit(Solver *s, Lit unit) {
    ASSERT(s->decision_level == 0);
    Var v = var(unit);
    if (lit_is_false(s, unit)) {
        s->base_unsat = true;
        s->result = FALSE;
        return false;
    }
    if (s->values[v] == UNDEF) {
        s->values[v] = (uint8_t)(2u - sign(unit));
        s->vars[v].level = 0;
        s->vars[v].reason = INVALID_CLAUSE;
        s->binary_reasons[v] = LIT_UNDEF;
        s->vars[v].trail_pos = s->trail_size;
        s->trail[s->trail_size++].lit = unit;
    }
    return true;
}

static bool inproc_finish_unit_lrat(Solver *s, Lit unit, uint64_t new_id) {
    Var v = var(unit);
    if (lit_is_false(s, unit)) {
        if (s->lrat) lrat_emit_empty_from_pair(s, lrat_get_unit_id(s, v), new_id);
        s->base_unsat = true;
        s->result = FALSE;
        return false;
    }
    if (s->values[v] == UNDEF) {
        if (s->lrat && new_id) lrat_set_unit_id(s, v, new_id);
        return enqueue_root_unit(s, unit);
    }
    if (s->lrat && new_id) lrat_buffer_delete_id(s, new_id);
    return true;
}

static bool root_propagate(Solver *s) {
    if (s->result == FALSE || s->error) return false;
    uint64_t saved_limit = s->work_limit;
    s->work_limit = 0;
    uint32_t prev_trail = s->trail_size;
    CRef c = solver_propagate(s);
    s->work_limit = saved_limit;
    if (s->lrat) lrat_sync_level0_units(s, prev_trail);
    if (c != INVALID_CLAUSE) {
        if (s->lrat) lrat_emit_empty_from_conflict(s, c);
        s->base_unsat = true;
        s->result = FALSE;
        return false;
    }
    return !s->error && !s->interrupted;
}

void solver_rebuild_watches(Solver *s) {
    if (s->decision_level == 0) {
        for (uint32_t i = 0; i < s->trail_size; ++i) {
            Var v = var(s->trail[i].lit);
            s->vars[v].reason = INVALID_CLAUSE;
            s->binary_reasons[v] = LIT_UNDEF;
        }
    }
    watch_clear(s->watches);
    /* Compact clause arrays first. */
    uint32_t out = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr)) s->clauses[out++] = cr;
    }
    s->num_original = s->num_clauses = out;

    out = 0;
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr)) s->learnts[out++] = cr;
    }
    s->num_learnts = out;

    /* Partition watches binary-first: emit ALL binary clauses before long clauses
     * so unit propagation resolves binary implications and conflicts first! */
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (CLAUSE_SIZE(s->arena, cr) == 2) {
            Lit *lits = CLAUSE_LITS(s->arena, cr);
            CRef w = s->lrat ? arena_binary_watch_ref(cr) : INVALID_CLAUSE;
            watch_add(s->watches, lits[0], w, lits[1]);
            watch_add(s->watches, lits[1], w, lits[0]);
        }
    }
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (CLAUSE_SIZE(s->arena, cr) == 2) {
            Lit *lits = CLAUSE_LITS(s->arena, cr);
            CRef w = arena_binary_watch_ref(cr);
            watch_add(s->watches, lits[0], w, lits[1]);
            watch_add(s->watches, lits[1], w, lits[0]);
        }
    }
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (CLAUSE_SIZE(s->arena, cr) > 2) {
            Lit *lits = CLAUSE_LITS(s->arena, cr);
            watch_add(s->watches, lits[0], cr, lits[1]);
            watch_add(s->watches, lits[1], cr, lits[0]);
        }
    }
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (CLAUSE_SIZE(s->arena, cr) > 2) {
            Lit *lits = CLAUSE_LITS(s->arena, cr);
            watch_add(s->watches, lits[0], cr, lits[1]);
            watch_add(s->watches, lits[1], cr, lits[0]);
        }
    }
    if (s->watches->failed) s->error = true;
}

/* =====================================================================
 * Pass 1: Root Cleanup (Delete satisfied, strip level-0 false literals)
 * ===================================================================== */

static bool clean_clause_array(Solver *s, CRef *arr, uint32_t *count, bool learned, bool *changed) {
    Lit small_buf[128];
    uint32_t out = 0;
    for (uint32_t i = 0; i < *count; ++i) {
        CRef cr = arr[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t size = CLAUSE_SIZE(s->arena, cr);
        const Lit *lits = CLAUSE_LITS(s->arena, cr);
        bool sat = false;
        uint32_t false_count = 0;
        for (uint32_t j = 0; j < size; ++j) {
            if (lit_is_true(s, lits[j])) { sat = true; break; }
            if (lit_is_false(s, lits[j])) false_count++;
        }
        if (sat) {
            /* Never delete unit clauses from the DRAT/LRAT proof: later RUP steps depend on root units! */
            if (size > 1) {
                proof_delete_clause(s, lits, size);
                if (s->lrat) lrat_buffer_delete(s, cr);
            }
            arena_delete(s->arena, cr);
            *changed = true;
            continue;
        }
        if (false_count == 0) {
            arr[out++] = cr;
            continue;
        }
        /* Shrink clause by removing level-0 false literals. */
        uint32_t keep = size - false_count;
        Lit *buf = (keep <= 128) ? small_buf : malloc((size_t)(keep ? keep : 1) * sizeof *buf);
        if (!buf) { s->error = true; return false; }
        uint32_t k = 0;
        for (uint32_t j = 0; j < size; ++j) {
            if (!lit_is_false(s, lits[j])) buf[k++] = lits[j];
        }
        uint64_t lrat_new_id = s->lrat ? lrat_emit_root_strengthen(s, cr, buf, k) : 0;
        proof_add_clause(s, buf, k);
        if (s->error) { if (buf != small_buf) free(buf); return false; }
        proof_delete_clause(s, lits, size);
        if (s->lrat) lrat_buffer_delete(s, cr);
        uint32_t old_lbd = clause_lbd(s->arena, cr);
        uint32_t old_used = CLAUSE_HEADER(s->arena, cr)->used;
        arena_delete(s->arena, cr);
        *changed = true;
        if (k == 0) {
            if (buf != small_buf) free(buf);
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (k == 1) {
            bool ok = inproc_finish_unit_lrat(s, buf[0], lrat_new_id);
            if (buf != small_buf) free(buf);
            if (!ok) return false;
            continue;
        }
        CRef ncr = arena_alloc(s->arena, buf, k, learned);
        if (buf != small_buf) free(buf);
        if (ncr == INVALID_CLAUSE) { s->error = true; return false; }
        if (s->lrat && lrat_new_id) lrat_set_cref_id(s, ncr, lrat_new_id);
        if (learned) {
            set_clause_lbd(s->arena, ncr, MIN(old_lbd, k));
            CLAUSE_HEADER(s->arena, ncr)->used = old_used;
        }
        arr[out++] = ncr;
    }
    *count = out;
    return true;
}

static bool inproc_root_clean(Solver *s) {
    for (;;) {
        if (!root_propagate(s)) return false;
        if (s->lrat) lrat_sync_level0_units(s, 0);
        if (s->error) return false;
        /* Emit all newly propagated level-0 units to the DRAT proof BEFORE deleting
         * any satisfied clauses that may have implied them! */
        while (s->proof_root_units < s->trail_size) {
            Lit u = s->trail[s->proof_root_units++].lit;
            proof_add_clause(s, &u, 1);
            if (s->error) return false;
        }
        bool changed = false;
        uint32_t before_trail = s->trail_size;
        if (!clean_clause_array(s, s->clauses, &s->num_clauses, false, &changed)) return false;
        s->num_original = s->num_clauses;
        if (!clean_clause_array(s, s->learnts, &s->num_learnts, true, &changed)) return false;
        if (changed) {
            /* Clear any dangling level-0 reasons pointing to deleted clauses. */
            for (uint32_t i = 0; i < s->trail_size; ++i) {
                Var v = var(s->trail[i].lit);
                s->vars[v].reason = INVALID_CLAUSE;
                s->binary_reasons[v] = LIT_UNDEF;
            }
            solver_rebuild_watches(s);
            if (s->error) return false;
        }
        if (s->trail_size == before_trail) break;
    }
    return true;
}

/* =====================================================================
 * Pass 2: In-Place Tarjan SCC Equivalence Substitution
 * ===================================================================== */

static bool inproc_decompose(Solver *s) {
    if (!s->num_vars || s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;
    uint32_t nl = 2 * (s->num_vars + 1);

    /* Quick check: do any unassigned binary edges exist? */
    bool has_bin = false;
    for (Lit l = 2; l < nl && !has_bin; ++l) {
        if (s->values[var(l)] != UNDEF) continue;
        WatchList *wl = watch_list(s->watches, l);
        for (uint32_t i = 0; i < wl->size; ++i) {
            Watch w = wl->watches[i];
            if ((is_binary_watch(w) || is_arena_binary_watch(w)) && s->values[var(w.blocker)] == UNDEF) {
                has_bin = true; break;
            }
        }
    }
    if (!has_bin) return true;

    uint32_t *idx = calloc(nl, sizeof *idx);
    uint32_t *low = calloc(nl, sizeof *low);
    uint32_t *iter = calloc(nl, sizeof *iter);
    uint8_t  *on_stack = calloc(nl, sizeof *on_stack);
    Lit      *tarjan = malloc((size_t)nl * sizeof *tarjan);
    Lit      *dfs = malloc((size_t)nl * sizeof *dfs);
    Lit      *repr = calloc(nl, sizeof *repr);
    if (!idx || !low || !iter || !on_stack || !tarjan || !dfs || !repr) {
        free(idx); free(low); free(iter); free(on_stack); free(tarjan); free(dfs); free(repr);
        s->error = true; return false;
    }

    uint32_t next_idx = 1, ttop = 0;
    bool budget_hit = false;

    /* Iterative Tarjan SCC on implication graph: p -> q for binary (~p | q). */
    for (Lit root = 2; root < nl && !budget_hit; ++root) {
        if (idx[root] || s->values[var(root)] != UNDEF) continue;
        uint32_t dtop = 0;
        dfs[dtop++] = root;
        idx[root] = low[root] = next_idx++;
        tarjan[ttop++] = root;
        on_stack[root] = 1;
        iter[root] = 0;

        while (dtop) {
            Lit u = dfs[dtop - 1];
            WatchList *wl = watch_list(s->watches, neg(u));
            bool pushed = false;
            while (iter[u] < wl->size) {
                if ((++s->work & 1023) == 0 && solver_budget_exhausted(s)) { budget_hit = true; break; }
                Watch w = wl->watches[iter[u]++];
                if (!is_binary_watch(w) && !is_arena_binary_watch(w)) continue;
                Lit v = w.blocker;
                if (s->values[var(v)] != UNDEF) continue;
                if (!idx[v]) {
                    idx[v] = low[v] = next_idx++;
                    tarjan[ttop++] = v;
                    on_stack[v] = 1;
                    iter[v] = 0;
                    dfs[dtop++] = v;
                    pushed = true;
                    break;
                } else if (on_stack[v] && idx[v] < low[u]) {
                    low[u] = idx[v];
                }
            }
            if (budget_hit) break;
            if (!pushed) {
                --dtop;
                if (dtop) {
                    Lit parent = dfs[dtop - 1];
                    if (low[u] < low[parent]) low[parent] = low[u];
                }
                if (low[u] == idx[u]) {
                    /* Pop SCC and pick minimum literal (preferring frozen vars) as representative. */
                    uint32_t scc_start = ttop;
                    Lit min_lit = u;
                    bool min_frozen = solver_is_frozen(s, var(u));
                    while (scc_start > 0) {
                        Lit x = tarjan[--scc_start];
                        bool xf = solver_is_frozen(s, var(x));
                        if ((xf && !min_frozen) || (xf == min_frozen && x < min_lit)) {
                            min_lit = x;
                            min_frozen = xf;
                        }
                        if (x == u) break;
                    }
                    for (uint32_t k = scc_start; k < ttop; ++k) {
                        Lit x = tarjan[k];
                        on_stack[x] = 0;
                        repr[x] = min_lit;
                    }
                    ttop = scc_start;
                }
            }
        }
    }

    free(idx); free(low); free(iter); free(on_stack); free(tarjan); free(dfs);
    if (budget_hit) { free(repr); return true; }

    /* Normalize representatives so repr[neg(p)] == neg(repr[p]) and detect contradictions. */
    uint32_t substituted = 0;
    for (Var v = 1; v <= s->num_vars; ++v) {
        Lit p = mkLit(v, false), np = mkLit(v, true);
        if (!repr[p]) repr[p] = p;
        if (!repr[np]) repr[np] = np;
        if (repr[p] == repr[np]) {
            /* p <-> ~p -> UNSAT! Unit p is RUP since ~p -> p. */
            proof_add_clause(s, &p, 1);
            s->stats.equiv_conflicts++;
            s->base_unsat = true;
            s->result = FALSE;
            free(repr);
            return false;
        }
        /* Never substitute frozen variables; otherwise prefer frozen representatives consistently. */
        if (solver_is_frozen(s, v)) {
            repr[p] = p;
            repr[np] = np;
        } else {
            bool fp = solver_is_frozen(s, var(repr[p]));
            bool fn = solver_is_frozen(s, var(repr[np]));
            if ((fp && !fn) || (fp == fn && var(repr[p]) < var(repr[np]))) {
                repr[np] = neg(repr[p]);
            } else {
                repr[p] = neg(repr[np]);
            }
        }
        if (repr[p] != p) substituted++;
    }

    if (!substituted) { free(repr); return true; }

    /* Record reconstruction witnesses for all substituted variables. */
    elim_init(s);
    if (s->error) { free(repr); return false; }
    for (Var v = 1; v <= s->num_vars; ++v) {
        Lit p = mkLit(v, false), r = repr[p];
        if (p == r) continue;
        if (s->elim->eliminated[v]) continue;
        Lit witness[] = {neg(p), r, 0, p, neg(r), 0};
        if (!elim_save(s, v, witness, 6)) { free(repr); return false; }
        s->elim->eliminated[v] = true;
    }

    /* Phase A: Rewrite all original and learned clauses in-place, emitting RUP
     * additions BEFORE deleting any old clauses (so binary equivalence chains
     * remain active during all RUP additions!). */
    uint32_t del_cap = 256, del_count = 0;
    CRef *to_delete = malloc((size_t)del_cap * sizeof *to_delete);
    Lit *buf = NULL;
    uint32_t buf_cap = 0;
    if (!to_delete) { free(repr); s->error = true; return false; }

    bool unsat = false;
    for (int pass = 0; pass < 2 && !unsat && !s->error; ++pass) {
        bool learned = (pass == 1);
        CRef *arr = learned ? s->learnts : s->clauses;
        uint32_t count = learned ? s->num_learnts : s->num_clauses;
        for (uint32_t i = 0; i < count; ++i) {
            CRef cr = arr[i];
            if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
            uint32_t size = CLAUSE_SIZE(s->arena, cr);
            const Lit *old = CLAUSE_LITS(s->arena, cr);
            bool changed = false;
            for (uint32_t j = 0; j < size; ++j) {
                if (repr[old[j]] != old[j] || s->values[var(old[j])] != UNDEF) { changed = true; break; }
            }
            if (!changed) continue;

            if (size > buf_cap) {
                Lit *p = realloc(buf, (size_t)size * sizeof *p);
                if (!p) { s->error = true; break; }
                buf = p; buf_cap = size;
            }
            bool taut = false;
            uint32_t k = 0;
            for (uint32_t j = 0; j < size; ++j) {
                Lit ml = repr[old[j]];
                if (lit_is_true(s, ml)) { taut = true; break; }
                if (lit_is_false(s, ml)) continue;
                buf[k++] = ml;
            }
            if (!taut && k > 1) {
                sort_lits_small(buf, k);
                uint32_t u = 0;
                for (uint32_t j = 0; j < k; ++j) {
                    if (u && buf[j] == buf[u - 1]) continue;
                    if (u && buf[j] == neg(buf[u - 1])) { taut = true; break; }
                    buf[u++] = buf[j];
                }
                k = u;
            }
            /* Schedule old clause for deletion after all RUP additions. */
            if (del_count == del_cap) {
                del_cap *= 2;
                CRef *p = realloc(to_delete, (size_t)del_cap * sizeof *p);
                if (!p) { s->error = true; break; }
                to_delete = p;
            }
            to_delete[del_count++] = cr;

            if (taut) {
                arr[i] = INVALID_CLAUSE;
                continue;
            }
            proof_add_clause(s, buf, k);
            if (s->error) break;
            s->stats.equiv_clauses++;
            if (k == 0) {
                arr[i] = INVALID_CLAUSE;
                s->base_unsat = true; s->result = FALSE;
                unsat = true; break;
            }
            if (k == 1) {
                arr[i] = INVALID_CLAUSE;
                if (!enqueue_root_unit(s, buf[0])) { unsat = true; break; }
                continue;
            }
            uint32_t old_lbd = clause_lbd(s->arena, cr);
            uint32_t old_used = CLAUSE_HEADER(s->arena, cr)->used;
            CRef ncr = arena_alloc(s->arena, buf, k, learned);
            if (ncr == INVALID_CLAUSE) { s->error = true; break; }
            if (learned) {
                set_clause_lbd(s->arena, ncr, MIN(old_lbd, k));
                CLAUSE_HEADER(s->arena, ncr)->used = old_used;
            }
            arr[i] = ncr;
        }
    }

    /* Phase B: Delete all replaced/tautological old clauses from proof & arena. */
    for (uint32_t i = 0; i < del_count; ++i) {
        CRef cr = to_delete[i];
        if (!clause_deleted(s->arena, cr)) {
            proof_delete_clause(s, CLAUSE_LITS(s->arena, cr), CLAUSE_SIZE(s->arena, cr));
            arena_delete(s->arena, cr);
        }
    }

    free(to_delete);
    free(buf);
    free(repr);
    s->stats.equiv_variables += substituted;

    if (unsat || s->error) return false;
    solver_rebuild_watches(s);
    if (s->error) return false;
    return inproc_root_clean(s);
}

/* =====================================================================
 * Pass 3: Occurrence Lists + Forward Subsumption & Self-Subsuming Res.
 * ===================================================================== */

typedef struct {
    CRef *clauses;
    uint32_t size, cap;
} InprocOcc;

typedef struct {
    CRef cref;
    uint64_t sig;
    uint32_t size;
    uint32_t slot;
    bool learned;
} CandClause;

static bool occ_push(InprocOcc *o, CRef cr) {
    if (o->size == o->cap) {
        uint32_t ncap = o->cap ? o->cap * 2 : 8;
        CRef *p = realloc(o->clauses, (size_t)ncap * sizeof *p);
        if (!p) return false;
        o->clauses = p; o->cap = ncap;
    }
    o->clauses[o->size++] = cr;
    return true;
}

static int cmp_cand_size(const void *a, const void *b) {
    const CandClause *x = (const CandClause *)a;
    const CandClause *y = (const CandClause *)b;
    if (x->size != y->size) return (x->size > y->size) - (x->size < y->size);
    if (x->learned != y->learned) return (int)x->learned - (int)y->learned;
    return (x->cref > y->cref) - (x->cref < y->cref);
}

static bool inproc_subsume(Solver *s, uint64_t tick_budget) {
    if (s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;
    uint64_t end_work = s->work + tick_budget;
    uint32_t nl = 2 * (s->num_vars + 1);

    /* Collect candidate clauses (irredundant + Tier-1/Tier-2 learned up to size 32). */
    uint32_t max_cands = s->num_clauses + s->num_learnts;
    if (!max_cands) return true;
    CandClause *cands = malloc((size_t)max_cands * sizeof *cands);
    InprocOcc *occs = calloc(nl, sizeof *occs);
    uint8_t *marks = calloc(s->num_vars + 1, sizeof *marks);
    if (!cands || !occs || !marks) {
        free(cands); free(occs); free(marks);
        s->error = true; return false;
    }

    /* Map CRef -> index in cands? Use direct occurrence lists storing indices into cands! */
    uint32_t nc = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 2 || sz > 64) continue;
        cands[nc++] = (CandClause){cr, clause_sig(CLAUSE_LITS(s->arena, cr), sz), sz, i, false};
    }
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        ClauseHeader *h = CLAUSE_HEADER(s->arena, cr);
        if (h->size < 2 || h->size > 32 || h->lbd > 6) continue;
        cands[nc++] = (CandClause){cr, clause_sig(CLAUSE_LITS(s->arena, cr), h->size), h->size, i, true};
    }

    if (nc < 2) { free(cands); free(occs); free(marks); return true; }
    qsort(cands, nc, sizeof *cands, cmp_cand_size);

    /* Populate occurrence lists with indices into cands[] (capped per literal). */
    const uint32_t MAX_OCC_SUB = 512;
    for (uint32_t idx = 0; idx < nc; ++idx) {
        CRef cr = cands[idx].cref;
        uint32_t sz = cands[idx].size;
        const Lit *lits = CLAUSE_LITS(s->arena, cr);
        for (uint32_t j = 0; j < sz; ++j) {
            if (occs[lits[j]].size < MAX_OCC_SUB) {
                if (!occ_push(&occs[lits[j]], idx)) { s->error = true; goto cleanup; }
            }
        }
    }

    bool any_change = false;
    bool unsat = false;
    Lit small_buf[128];

    for (uint32_t ci = 0; ci < nc && s->work < end_work && !unsat && !s->error; ++ci) {
        if ((ci & 63) == 0 && solver_budget_exhausted_now(s)) break;
        CRef cr = cands[ci].cref;
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t csize = CLAUSE_SIZE(s->arena, cr);
        if (csize < 2 || csize > 16) continue; /* Short clauses subsume/strengthen best */
        const Lit *clits = CLAUSE_LITS(s->arena, cr);
        uint64_t csig = cands[ci].sig;

        /* Mark C's literals: 1 = positive, 2 = negative. Find rarest literal for subsumption. */
        Lit min_l = clits[0];
        uint32_t min_occ = occs[min_l].size;
        for (uint32_t j = 0; j < csize; ++j) {
            Lit l = clits[j];
            marks[var(l)] = (uint8_t)(1u + sign(l));
            if (occs[l].size < min_occ) { min_occ = occs[l].size; min_l = l; }
        }

        /* 1. Forward subsumption via occs[min_l]. */
        InprocOcc *sub_list = &occs[min_l];
        for (uint32_t oi = 0; oi < sub_list->size && s->work < end_work; ++oi) {
            uint32_t di = sub_list->clauses[oi];
            if (di == ci) continue;
            CRef dr = cands[di].cref;
            if (dr == INVALID_CLAUSE || clause_deleted(s->arena, dr)) continue;
            if (cands[di].size < csize) continue;
            if ((csig & ~cands[di].sig) != 0) continue; /* 64-bit bloom reject */
            s->work += cands[di].size;

            const Lit *dlits = CLAUSE_LITS(s->arena, dr);
            uint32_t dsize = CLAUSE_SIZE(s->arena, dr);
            uint32_t matched = 0;
            for (uint32_t j = 0; j < dsize; ++j) {
                uint8_t m = marks[var(dlits[j])];
                if (m && m == (uint8_t)(1u + sign(dlits[j]))) matched++;
            }
            if (matched == csize) {
                /* C subsumes D! If C is learned and D is original, promote C to original first! */
                if (cands[ci].learned && !cands[di].learned) {
                    CLAUSE_HEADER(s->arena, cr)->flags &= ~CLAUSE_LEARNED;
                    if (!push_original_clause_ref(s, cr)) { unsat = true; break; }
                    s->learnts[cands[ci].slot] = INVALID_CLAUSE;
                    cands[ci].learned = false;
                    cands[ci].slot = s->num_clauses - 1;
                }
                proof_delete_clause(s, dlits, dsize);
                if (s->lrat) lrat_buffer_delete(s, dr);
                arena_delete(s->arena, dr);
                cands[di].cref = INVALID_CLAUSE;
                s->stats.subsumed_clauses++;
                any_change = true;
            }
        }

        /* 2. Self-subsuming resolution (SSR): check occs[neg(l)] for literals of C (size <= 8). */
        if (!unsat && !s->error && csize <= 8) {
            for (uint32_t li = 0; li < csize && s->work < end_work && !unsat && !s->error; ++li) {
                if (clause_deleted(s->arena, cr)) break;
                Lit pivot = clits[li];
                Lit neg_piv = neg(pivot);
                InprocOcc *ssr_list = &occs[neg_piv];
                if (ssr_list->size > 256) continue;
                uint64_t csig_wo = csig & ~(UINT64_C(1) << (var(pivot) & 63));
                /* Recompute csig_wo accurately if another literal shared bit mod 64 */
                csig_wo = 0;
                for (uint32_t j = 0; j < csize; ++j)
                    if (j != li) csig_wo |= UINT64_C(1) << (var(clits[j]) & 63);

                for (uint32_t oi = 0; oi < ssr_list->size && s->work < end_work; ++oi) {
                    uint32_t di = ssr_list->clauses[oi];
                    if (di == ci) continue;
                    CRef dr = cands[di].cref;
                    if (dr == INVALID_CLAUSE || clause_deleted(s->arena, dr)) continue;
                    if (cands[di].size < csize) continue;
                    if ((csig_wo & ~cands[di].sig) != 0) continue;
                    s->work += cands[di].size;

                    const Lit *dlits = CLAUSE_LITS(s->arena, dr);
                    uint32_t dsize = CLAUSE_SIZE(s->arena, dr);
                    uint32_t matched = 0;
                    bool has_neg_piv = false;
                    for (uint32_t j = 0; j < dsize; ++j) {
                        Lit dl = dlits[j];
                        if (dl == neg_piv) { has_neg_piv = true; continue; }
                        uint8_t m = marks[var(dl)];
                        if (m && m == (uint8_t)(1u + sign(dl))) matched++;
                    }
                    if (has_neg_piv && matched == csize - 1) {
                        /* Strengthen D by removing neg_piv! */
                        uint32_t nsz = dsize - 1;
                        Lit *nbuf = (nsz <= 128) ? small_buf : malloc((size_t)(nsz ? nsz : 1) * sizeof *nbuf);
                        if (!nbuf) { s->error = true; break; }
                        uint32_t k = 0;
                        for (uint32_t j = 0; j < dsize; ++j) {
                            if (dlits[j] != neg_piv) nbuf[k++] = dlits[j];
                        }
                        uint64_t lrat_new_id = s->lrat ? lrat_emit_pair_rup(s, nbuf, k, cr, dr) : 0;
                        proof_add_clause(s, nbuf, k);
                        if (s->error) { if (nbuf != small_buf) free(nbuf); break; }
                        proof_delete_clause(s, dlits, dsize);
                        if (s->lrat) lrat_buffer_delete(s, dr);
                        uint32_t old_lbd = clause_lbd(s->arena, dr);
                        uint32_t old_used = CLAUSE_HEADER(s->arena, dr)->used;
                        bool d_learned = cands[di].learned;
                        arena_delete(s->arena, dr);
                        s->stats.subsumed_clauses++;
                        any_change = true;

                        if (k == 0) {
                            if (nbuf != small_buf) free(nbuf);
                            cands[di].cref = INVALID_CLAUSE;
                            s->base_unsat = true; s->result = FALSE;
                            unsat = true; break;
                        }
                        if (k == 1) {
                            Lit unit = nbuf[0];
                            if (nbuf != small_buf) free(nbuf);
                            cands[di].cref = INVALID_CLAUSE;
                            if (!inproc_finish_unit_lrat(s, unit, lrat_new_id)) { unsat = true; break; }
                            continue;
                        }
                        CRef ncr = arena_alloc(s->arena, nbuf, k, d_learned);
                        /* Note: arena_alloc might realloc arena->memory -> refresh clits! */
                        clits = CLAUSE_LITS(s->arena, cr);
                        if (ncr == INVALID_CLAUSE) {
                            if (nbuf != small_buf) free(nbuf);
                            s->error = true; break;
                        }
                        if (s->lrat && lrat_new_id) lrat_set_cref_id(s, ncr, lrat_new_id);
                        if (d_learned) {
                            set_clause_lbd(s->arena, ncr, MIN(old_lbd, k));
                            CLAUSE_HEADER(s->arena, ncr)->used = old_used;
                            s->learnts[cands[di].slot] = ncr;
                        } else {
                            s->clauses[cands[di].slot] = ncr;
                        }
                        cands[di].cref = ncr;
                        cands[di].size = k;
                        cands[di].sig = clause_sig(nbuf, k);
                        if (nbuf != small_buf) free(nbuf);
                    }
                }
            }
        }

        /* Clear marks for C. */
        clits = CLAUSE_LITS(s->arena, cr);
        for (uint32_t j = 0; j < csize; ++j) marks[var(clits[j])] = 0;
    }

cleanup:
    for (uint32_t l = 0; l < nl; ++l) free(occs[l].clauses);
    free(occs);
    free(cands);
    free(marks);

    if (unsat || s->error) return false;
    if (any_change) {
        solver_rebuild_watches(s);
        if (s->error) return false;
        return inproc_root_clean(s);
    }
    return true;
}

/* =====================================================================
 * Pass 4: Gate-Aware Bounded Variable Elimination (BVE)
 * ===================================================================== */

typedef struct {
    Var *vars;
    uint32_t *pos;
    uint64_t *score;
    uint32_t size, cap;
} ElimQueue;

static void eq_sift_up(ElimQueue *q, uint32_t i) {
    Var v = q->vars[i];
    uint64_t sc = q->score[v];
    while (i > 0) {
        uint32_t p = (i - 1) >> 1;
        Var pv = q->vars[p];
        if (q->score[pv] <= sc) break;
        q->vars[i] = pv; q->pos[pv] = i;
        i = p;
    }
    q->vars[i] = v; q->pos[v] = i;
}

static void eq_sift_down(ElimQueue *q, uint32_t i) {
    Var v = q->vars[i];
    uint64_t sc = q->score[v];
    while ((i << 1) + 1 < q->size) {
        uint32_t c = (i << 1) + 1;
        if (c + 1 < q->size && q->score[q->vars[c + 1]] < q->score[q->vars[c]]) c++;
        if (sc <= q->score[q->vars[c]]) break;
        Var cv = q->vars[c];
        q->vars[i] = cv; q->pos[cv] = i;
        i = c;
    }
    q->vars[i] = v; q->pos[v] = i;
}

static void eq_push_or_update(ElimQueue *q, Var v, uint64_t sc) {
    q->score[v] = sc;
    uint32_t p = q->pos[v];
    if (p == UINT32_MAX) {
        p = q->size++;
        q->vars[p] = v; q->pos[v] = p;
        eq_sift_up(q, p);
    } else {
        eq_sift_up(q, p);
        eq_sift_down(q, q->pos[v]);
    }
}

static Var eq_pop(ElimQueue *q) {
    Var top = q->vars[0];
    q->pos[top] = UINT32_MAX;
    if (--q->size == 0) return top;
    Var last = q->vars[q->size];
    q->vars[0] = last; q->pos[last] = 0;
    eq_sift_down(q, 0);
    return top;
}

static void clean_inproc_occ(const Solver *s, InprocOcc *o) {
    uint32_t out = 0;
    for (uint32_t i = 0; i < o->size; ++i) {
        CRef cr = o->clauses[i];
        if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr)) o->clauses[out++] = cr;
    }
    o->size = out;
}

/* Try to extract an equivalence or AND gate defining v (or ~v).
 * Marks gate clauses in gate_pos[0..pos_occ->size-1] and gate_neg[0..neg_occ->size-1].
 * Returns true if a complete gate was found. */
static bool extract_gate(Solver *s, Var v, const InprocOcc *pos_occ, const InprocOcc *neg_occ,
                         uint8_t *gate_pos, uint8_t *gate_neg, uint8_t *lit_seen) {
    Lit pos_v = mkLit(v, false);
    Lit neg_v = mkLit(v, true);

    /* 1. Try AND gate: one long clause (v | ~a1 | ... | ~ak) in pos_occ
     *    matched by binaries (~v | a1), ..., (~v | ak) in neg_occ. */
    if (neg_occ->size >= 1 && pos_occ->size >= 1) {
        /* Mark all literals appearing in binary clauses of neg_occ: (~v | l). */
        uint32_t bin_count = 0;
        for (uint32_t j = 0; j < neg_occ->size; ++j) {
            CRef cr = neg_occ->clauses[j];
            if (CLAUSE_SIZE(s->arena, cr) == 2) {
                const Lit *ls = CLAUSE_LITS(s->arena, cr);
                Lit other = (ls[0] == neg_v) ? ls[1] : ls[0];
                lit_seen[var(other)] |= (uint8_t)(1u << sign(other));
                bin_count++;
            }
        }
        if (bin_count >= 1) {
            bool found = false;
            for (uint32_t i = 0; i < pos_occ->size; ++i) {
                CRef cr = pos_occ->clauses[i];
                uint32_t sz = CLAUSE_SIZE(s->arena, cr);
                if (sz < 2 || sz > 32) continue;
                const Lit *ls = CLAUSE_LITS(s->arena, cr);
                bool all_match = true;
                for (uint32_t k = 0; k < sz; ++k) {
                    if (ls[k] == pos_v) continue;
                    Lit need = neg(ls[k]);
                    if (!(lit_seen[var(need)] & (1u << sign(need)))) { all_match = false; break; }
                }
                if (all_match) {
                    memset(gate_pos, 0, pos_occ->size);
                    memset(gate_neg, 0, neg_occ->size);
                    gate_pos[i] = 1;
                    /* Mark exact matching binary clauses in neg_occ. */
                    for (uint32_t k = 0; k < sz; ++k) {
                        if (ls[k] == pos_v) continue;
                        Lit need = neg(ls[k]);
                        for (uint32_t j = 0; j < neg_occ->size; ++j) {
                            CRef bcr = neg_occ->clauses[j];
                            if (CLAUSE_SIZE(s->arena, bcr) != 2) continue;
                            const Lit *bls = CLAUSE_LITS(s->arena, bcr);
                            Lit other = (bls[0] == neg_v) ? bls[1] : bls[0];
                            if (other == need) { gate_neg[j] = 1; break; }
                        }
                    }
                    found = true;
                    break;
                }
            }
            /* Clear lit_seen */
            for (uint32_t j = 0; j < neg_occ->size; ++j) {
                CRef cr = neg_occ->clauses[j];
                if (CLAUSE_SIZE(s->arena, cr) == 2) {
                    const Lit *ls = CLAUSE_LITS(s->arena, cr);
                    Lit other = (ls[0] == neg_v) ? ls[1] : ls[0];
                    lit_seen[var(other)] = 0;
                }
            }
            if (found) return true;
        }
    }

    /* 2. Symmetric AND gate: one clause (~v | ~a1 | ... | ~ak) in neg_occ
     *    matched by binaries (v | a1), ..., (v | ak) in pos_occ. */
    if (pos_occ->size >= 1 && neg_occ->size >= 1) {
        uint32_t bin_count = 0;
        for (uint32_t i = 0; i < pos_occ->size; ++i) {
            CRef cr = pos_occ->clauses[i];
            if (CLAUSE_SIZE(s->arena, cr) == 2) {
                const Lit *ls = CLAUSE_LITS(s->arena, cr);
                Lit other = (ls[0] == pos_v) ? ls[1] : ls[0];
                lit_seen[var(other)] |= (uint8_t)(1u << sign(other));
                bin_count++;
            }
        }
        if (bin_count >= 1) {
            bool found = false;
            for (uint32_t j = 0; j < neg_occ->size; ++j) {
                CRef cr = neg_occ->clauses[j];
                uint32_t sz = CLAUSE_SIZE(s->arena, cr);
                if (sz < 2 || sz > 32) continue;
                const Lit *ls = CLAUSE_LITS(s->arena, cr);
                bool all_match = true;
                for (uint32_t k = 0; k < sz; ++k) {
                    if (ls[k] == neg_v) continue;
                    Lit need = neg(ls[k]);
                    if (!(lit_seen[var(need)] & (1u << sign(need)))) { all_match = false; break; }
                }
                if (all_match) {
                    memset(gate_pos, 0, pos_occ->size);
                    memset(gate_neg, 0, neg_occ->size);
                    gate_neg[j] = 1;
                    for (uint32_t k = 0; k < sz; ++k) {
                        if (ls[k] == neg_v) continue;
                        Lit need = neg(ls[k]);
                        for (uint32_t i = 0; i < pos_occ->size; ++i) {
                            CRef bcr = pos_occ->clauses[i];
                            if (CLAUSE_SIZE(s->arena, bcr) != 2) continue;
                            const Lit *bls = CLAUSE_LITS(s->arena, bcr);
                            Lit other = (bls[0] == pos_v) ? bls[1] : bls[0];
                            if (other == need) { gate_pos[i] = 1; break; }
                        }
                    }
                    found = true;
                    break;
                }
            }
            for (uint32_t i = 0; i < pos_occ->size; ++i) {
                CRef cr = pos_occ->clauses[i];
                if (CLAUSE_SIZE(s->arena, cr) == 2) {
                    const Lit *ls = CLAUSE_LITS(s->arena, cr);
                    Lit other = (ls[0] == pos_v) ? ls[1] : ls[0];
                    lit_seen[var(other)] = 0;
                }
            }
            if (found) return true;
        }
    }

    /* 3. Try ITE / XOR gate (4 ternary clauses):
     *    pos_occ has (v | a | b) and (v | ~a | c),
     *    neg_occ has (~v | a | ~b) and (~v | ~a | ~c). */
    if (pos_occ->size >= 2 && neg_occ->size >= 2 && pos_occ->size <= 64 && neg_occ->size <= 64) {
        for (uint32_t i1 = 0; i1 < pos_occ->size; ++i1) {
            CRef c1 = pos_occ->clauses[i1];
            if (CLAUSE_SIZE(s->arena, c1) != 3) continue;
            const Lit *l1 = CLAUSE_LITS(s->arena, c1);
            Lit p1[2]; uint32_t k = 0;
            for (uint32_t t = 0; t < 3; ++t) if (l1[t] != pos_v) p1[k++] = l1[t];
            if (k != 2) continue;
            /* Try either p1[0] or p1[1] as the condition literal cond. */
            for (int ci = 0; ci < 2; ++ci) {
                Lit cond = p1[ci], b_lit = p1[1 - ci];
                /* Look for (~v | cond | ~b_lit) in neg_occ */
                uint32_t j1 = UINT32_MAX;
                for (uint32_t j = 0; j < neg_occ->size; ++j) {
                    CRef cn = neg_occ->clauses[j];
                    if (CLAUSE_SIZE(s->arena, cn) != 3) continue;
                    const Lit *ln = CLAUSE_LITS(s->arena, cn);
                    bool hc = false, hb = false;
                    for (uint32_t t = 0; t < 3; ++t) {
                        if (ln[t] == cond) hc = true;
                        else if (ln[t] == neg(b_lit)) hb = true;
                    }
                    if (hc && hb) { j1 = j; break; }
                }
                if (j1 == UINT32_MAX) continue;
                /* Look for (v | ~cond | c_lit) in pos_occ and (~v | ~cond | ~c_lit) in neg_occ */
                Lit ncond = neg(cond);
                for (uint32_t i2 = i1 + 1; i2 < pos_occ->size; ++i2) {
                    CRef c2 = pos_occ->clauses[i2];
                    if (CLAUSE_SIZE(s->arena, c2) != 3) continue;
                    const Lit *l2 = CLAUSE_LITS(s->arena, c2);
                    bool hnc = false; Lit c_lit = LIT_UNDEF;
                    for (uint32_t t = 0; t < 3; ++t) {
                        if (l2[t] == pos_v) continue;
                        if (l2[t] == ncond) hnc = true;
                        else c_lit = l2[t];
                    }
                    if (!hnc || c_lit == LIT_UNDEF) continue;
                    for (uint32_t j2 = 0; j2 < neg_occ->size; ++j2) {
                        if (j2 == j1) continue;
                        CRef cn2 = neg_occ->clauses[j2];
                        if (CLAUSE_SIZE(s->arena, cn2) != 3) continue;
                        const Lit *ln2 = CLAUSE_LITS(s->arena, cn2);
                        bool hnc2 = false, hnc_lit = false;
                        for (uint32_t t = 0; t < 3; ++t) {
                            if (ln2[t] == ncond) hnc2 = true;
                            else if (ln2[t] == neg(c_lit)) hnc_lit = true;
                        }
                        if (hnc2 && hnc_lit) {
                            memset(gate_pos, 0, pos_occ->size);
                            memset(gate_neg, 0, neg_occ->size);
                            gate_pos[i1] = 1; gate_pos[i2] = 1;
                            gate_neg[j1] = 1; gate_neg[j2] = 1;
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

/* Semantic definition extraction via tiny bounded core-tracking CDCL (Kissat definition.c).
 * Checks whether the stripped occurrence clauses (P \ {v}) U (N \ {~v}) are UNSAT;
 * if so, returns the exact UNSAT core in gate_pos/gate_neg (allowing R_pos x R_neg
 * resolvents to be skipped while retaining G_pos x G_neg for full soundness!). */
static bool extract_semantic_gate(Solver *s, Var v, const InprocOcc *pos_occ, const InprocOcc *neg_occ,
                                  uint8_t *gate_pos, uint8_t *gate_neg) {
    uint32_t np = pos_occ->size, nn = neg_occ->size;
    if (np < 2u || nn < 2u || np + nn > 64u) return false;

    enum { DK_MAX_VARS = 128, DK_MAX_CLS = 192, DK_POOL = 2048 };
    Var l2g[DK_MAX_VARS + 1];
    uint32_t nvars = 0;

    uint16_t pool[DK_POOL];
    uint32_t pool_used = 0;
    uint32_t coff[DK_MAX_CLS];
    uint16_t csz[DK_MAX_CLS];
    uint64_t cmask[DK_MAX_CLS];
    uint32_t ncls = 0;

    int8_t val[DK_MAX_VARS + 1];
    uint32_t lvl[DK_MAX_VARS + 1];
    uint32_t rsn[DK_MAX_VARS + 1];
    uint64_t rmask0[DK_MAX_VARS + 1];
    uint8_t seen[DK_MAX_VARS + 1];
    uint16_t trail[DK_MAX_VARS + 2];
    uint32_t tlim[DK_MAX_VARS + 2];
    uint32_t tsz = 0, qhead = 0, dlvl = 0;

    memset(val, 0, sizeof val);
    memset(lvl, 0, sizeof lvl);
    memset(rmask0, 0, sizeof rmask0);
    memset(seen, 0, sizeof seen);

    bool unsat0 = false;
    uint64_t core_mask = 0;

    /* Helper macro to map global Var -> local 1..DK_MAX_VARS. */
    for (uint32_t side = 0; side < 2u && !unsat0; ++side) {
        const InprocOcc *occ = side ? neg_occ : pos_occ;
        uint32_t base_bit = side ? np : 0u;
        for (uint32_t ci = 0; ci < occ->size && !unsat0; ++ci) {
            CRef cr = occ->clauses[ci];
            uint32_t sz = CLAUSE_SIZE(s->arena, cr);
            if (sz > 16u) continue;
            const Lit *ls = CLAUSE_LITS(s->arena, cr);
            uint16_t loc[16];
            uint32_t ln = 0;
            bool skip = false;
            for (uint32_t k = 0; k < sz; ++k) {
                Lit l = ls[k];
                Var gv = var(l);
                if (gv == v) continue;
                lbool lv = lxor(s->values[gv], sign(l));
                if (lv == TRUE) { skip = true; break; }
                if (lv == FALSE) continue;
                uint32_t lv_id = 0;
                for (uint32_t m = 1u; m <= nvars; ++m) {
                    if (l2g[m] == gv) { lv_id = m; break; }
                }
                if (!lv_id) {
                    if (nvars >= DK_MAX_VARS) return false;
                    lv_id = ++nvars;
                    l2g[lv_id] = gv;
                }
                uint16_t ll = (uint16_t)((lv_id << 1) | (uint32_t)sign(l));
                bool dup = false;
                for (uint32_t m = 0; m < ln; ++m) {
                    if (loc[m] == ll) { dup = true; break; }
                    if (loc[m] == (ll ^ 1u)) { skip = true; break; }
                }
                if (skip) break;
                if (!dup) loc[ln++] = ll;
            }
            if (skip) continue;
            uint64_t bit = UINT64_C(1) << (base_bit + ci);
            if (ln == 0) { unsat0 = true; core_mask = bit; break; }
            if (ln == 1u) {
                uint32_t lv_id = loc[0] >> 1;
                int8_t want = (loc[0] & 1u) ? (int8_t)-1 : (int8_t)1;
                if (val[lv_id] == -want) {
                    unsat0 = true;
                    core_mask = bit | rmask0[lv_id];
                    break;
                }
                if (val[lv_id] == 0) {
                    val[lv_id] = want;
                    lvl[lv_id] = 0;
                    rsn[lv_id] = UINT32_MAX;
                    rmask0[lv_id] = bit;
                    trail[tsz++] = loc[0];
                }
                continue;
            }
            if (ncls >= DK_MAX_CLS || pool_used + ln > DK_POOL) return false;
            coff[ncls] = pool_used;
            csz[ncls] = (uint16_t)ln;
            cmask[ncls] = bit;
            memcpy(pool + pool_used, loc, (size_t)ln * sizeof(uint16_t));
            pool_used += ln;
            ncls++;
        }
    }

    if (!unsat0 && nvars == 0) return false;

    uint32_t conflicts = 0;
    uint16_t learnt[DK_MAX_VARS + 2];

    while (!unsat0) {
        /* Simple scan propagation (ncls <= 192, ultra-fast in L1 cache!). */
        uint32_t confl = UINT32_MAX;
        while (qhead <= tsz && confl == UINT32_MAX) {
            qhead = tsz;
            bool progressed = false;
            for (uint32_t ci = 0; ci < ncls; ++ci) {
                const uint16_t *ls = pool + coff[ci];
                uint32_t sz = csz[ci];
                uint32_t undef_cnt = 0;
                uint16_t undef_lit = 0;
                bool sat = false;
                for (uint32_t k = 0; k < sz; ++k) {
                    uint16_t l = ls[k];
                    int8_t lv = (l & 1u) ? (int8_t)-val[l >> 1] : val[l >> 1];
                    if (lv > 0) { sat = true; break; }
                    if (lv == 0) { undef_cnt++; undef_lit = l; if (undef_cnt > 1u) break; }
                }
                if (sat || undef_cnt > 1u) continue;
                if (undef_cnt == 0) { confl = ci; break; }
                uint32_t u = undef_lit >> 1;
                val[u] = (undef_lit & 1u) ? (int8_t)-1 : (int8_t)1;
                lvl[u] = dlvl;
                rsn[u] = ci;
                if (dlvl == 0) {
                    uint64_t m0 = cmask[ci];
                    for (uint32_t k = 0; k < sz; ++k) {
                        uint32_t uv = ls[k] >> 1;
                        if (uv != u) m0 |= rmask0[uv];
                    }
                    rmask0[u] = m0;
                }
                trail[tsz++] = undef_lit;
                progressed = true;
            }
            s->work += ncls;
            if (!progressed) break;
        }

        if (confl != UINT32_MAX) {
            if (dlvl == 0) {
                unsat0 = true;
                core_mask = cmask[confl];
                const uint16_t *ls = pool + coff[confl];
                for (uint32_t k = 0; k < csz[confl]; ++k) core_mask |= rmask0[ls[k] >> 1];
                break;
            }
            if (++conflicts > 64u || ncls >= DK_MAX_CLS) return false;

            /* 1-UIP conflict analysis with exact bitmask core propagation! */
            uint32_t lsz = 1u, pathC = 0;
            uint32_t idx = tsz - 1u;
            uint32_t cidx = confl;
            uint64_t lmask = 0;
            uint16_t p = 0;
            do {
                lmask |= cmask[cidx];
                const uint16_t *ls = pool + coff[cidx];
                uint32_t sz = csz[cidx];
                for (uint32_t k = 0; k < sz; ++k) {
                    uint16_t q = ls[k];
                    uint32_t qv = q >> 1;
                    if (cidx != confl && qv == (p >> 1)) continue;
                    if (lvl[qv] == 0) {
                        lmask |= rmask0[qv];
                        continue;
                    }
                    if (!seen[qv]) {
                        seen[qv] = 1;
                        if (lvl[qv] == dlvl) pathC++;
                        else learnt[lsz++] = q;
                    }
                }
                while (!seen[trail[idx] >> 1]) idx--;
                p = trail[idx--];
                seen[p >> 1] = 0;
                cidx = rsn[p >> 1];
                pathC--;
            } while (pathC > 0);
            learnt[0] = p ^ 1u;
            for (uint32_t k = 1u; k < lsz; ++k) seen[learnt[k] >> 1] = 0;

            uint32_t bt = 0;
            for (uint32_t k = 1u; k < lsz; ++k) {
                if (lvl[learnt[k] >> 1] > bt) bt = lvl[learnt[k] >> 1];
            }
            /* Backtrack to bt */
            while (dlvl > bt) {
                uint32_t lim = tlim[dlvl--];
                while (tsz > lim) { val[trail[--tsz] >> 1] = 0; }
            }
            qhead = tsz;

            if (lsz == 1u) {
                uint32_t u = learnt[0] >> 1;
                int8_t want = (learnt[0] & 1u) ? (int8_t)-1 : (int8_t)1;
                if (val[u] == -want) {
                    unsat0 = true;
                    core_mask = lmask | rmask0[u];
                    break;
                }
                if (val[u] == 0) {
                    val[u] = want;
                    lvl[u] = 0;
                    rsn[u] = UINT32_MAX;
                    rmask0[u] = lmask;
                    trail[tsz++] = learnt[0];
                }
            } else {
                if (pool_used + lsz > DK_POOL) return false;
                uint32_t nc = ncls++;
                coff[nc] = pool_used;
                csz[nc] = (uint16_t)lsz;
                cmask[nc] = lmask;
                memcpy(pool + pool_used, learnt, (size_t)lsz * sizeof(uint16_t));
                pool_used += lsz;
                uint32_t u = learnt[0] >> 1;
                val[u] = (learnt[0] & 1u) ? (int8_t)-1 : (int8_t)1;
                lvl[u] = dlvl;
                rsn[u] = nc;
                trail[tsz++] = learnt[0];
            }
            continue;
        }

        /* Pick first unassigned local variable. */
        uint32_t pick = 0;
        for (uint32_t u = 1u; u <= nvars; ++u) {
            if (val[u] == 0) { pick = u; break; }
        }
        if (!pick) return false; /* SAT -> no semantic definition! */
        tlim[++dlvl] = tsz;
        val[pick] = 1;
        lvl[pick] = dlvl;
        rsn[pick] = UINT32_MAX;
        trail[tsz++] = (uint16_t)(pick << 1);
    }

    if (!unsat0 || !core_mask) return false;

    uint32_t gp_cnt = 0, gn_cnt = 0;
    memset(gate_pos, 0, np);
    memset(gate_neg, 0, nn);
    for (uint32_t i = 0; i < np; ++i) {
        if ((core_mask >> i) & 1u) { gate_pos[i] = 1; gp_cnt++; }
    }
    for (uint32_t j = 0; j < nn; ++j) {
        if ((core_mask >> (np + j)) & 1u) { gate_neg[j] = 1; gn_cnt++; }
    }
    /* Useful only if at least one non-gate clause exists on BOTH sides so R x R skips work! */
    return gp_cnt < np && gn_cnt < nn;
}

/* Resolve clauses ca and cb on pivot v into out_buf (size <= max_len).
 * Returns -1 if tautology or satisfied, -2 if exceeds max_len, else resolvent length. */
static int fast_resolve(Solver *s, CRef ca, CRef cb, Var pivot,
                        Lit *out_buf, uint32_t max_len) {
    const Lit *a = CLAUSE_LITS(s->arena, ca);
    uint32_t na = CLAUSE_SIZE(s->arena, ca);
    const Lit *b = CLAUSE_LITS(s->arena, cb);
    uint32_t nb = CLAUSE_SIZE(s->arena, cb);
    uint8_t *seen = s->seen;
    Var *touched = s->minimize_touched;
    uint32_t tc = 0, out_n = 0;
    int status = 0;

    for (unsigned part = 0; part < 2 && status == 0; ++part) {
        const Lit *ls = part ? b : a;
        uint32_t sz = part ? nb : na;
        for (uint32_t i = 0; i < sz; ++i) {
            Lit l = ls[i];
            Var u = var(l);
            if (u == pivot) continue;
            if (lit_is_true(s, l)) { status = -1; break; }
            if (lit_is_false(s, l)) continue;
            uint8_t mark = (uint8_t)(1u + sign(l));
            if (seen[u]) {
                if (seen[u] != mark) { status = -1; break; }
                continue;
            }
            if (out_n >= max_len) { status = -2; break; }
            seen[u] = mark;
            touched[tc++] = u;
            if (out_buf) out_buf[out_n] = l;
            out_n++;
        }
    }
    for (uint32_t i = 0; i < tc; ++i) seen[touched[i]] = 0;
    return status < 0 ? status : (int)out_n;
}

static bool inproc_elim(Solver *s, uint64_t tick_budget, uint32_t grow_bound) {
    if (s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;
    elim_init(s);
    if (s->error) return false;

    uint64_t end_work = s->work + tick_budget;
    uint32_t nl = 2 * (s->num_vars + 1);

    /* Build irredundant occurrence lists + exact live occurrence counts for cascading BVE. */
    InprocOcc *occs = calloc(nl, sizeof *occs);
    uint32_t *live_occ = calloc(nl, sizeof *live_occ);
    ElimQueue q = {
        .vars = malloc((size_t)(s->num_vars + 1) * sizeof(Var)),
        .pos = malloc((size_t)(s->num_vars + 1) * sizeof(uint32_t)),
        .score = calloc(s->num_vars + 1, sizeof(uint64_t)),
        .size = 0, .cap = s->num_vars + 1
    };
    uint8_t *lit_seen = calloc(s->num_vars + 1, sizeof *lit_seen);
    uint8_t *gate_pos = malloc(128);
    uint8_t *gate_neg = malloc(128);
    Lit *stage_lits = malloc(4096 * sizeof(Lit));
    uint32_t stage_cap = 4096;
    uint32_t *stage_off = malloc(256 * sizeof(uint32_t));
    CRef *stage_ca = malloc(256 * sizeof(CRef));
    CRef *stage_cb = malloc(256 * sizeof(CRef));
    uint32_t stage_off_cap = 256;

    if (!occs || !live_occ || !q.vars || !q.pos || !q.score || !lit_seen || !gate_pos || !gate_neg ||
        !stage_lits || !stage_off || !stage_ca || !stage_cb) {
        free(occs); free(live_occ); free(q.vars); free(q.pos); free(q.score);
        free(lit_seen); free(gate_pos); free(gate_neg); free(stage_lits);
        free(stage_off); free(stage_ca); free(stage_cb);
        s->error = true; return false;
    }
    for (Var v = 0; v <= s->num_vars; ++v) q.pos[v] = UINT32_MAX;

    /* Index all active irredundant clauses. */
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        for (uint32_t j = 0; j < sz; ++j) {
            if (!occ_push(&occs[ls[j]], cr)) { s->error = true; goto done; }
            live_occ[ls[j]]++;
        }
    }

    const uint32_t MAX_OCC_GATE = 100;
    const uint32_t MAX_OCC_PLAIN = 20;
    const uint32_t MAX_RES_LEN = 100;

    /* Enqueue candidate variables ordered by pos * neg. */
    for (Var v = 1; v <= s->num_vars; ++v) {
        if (s->values[v] != UNDEF || s->elim->eliminated[v] || gauss_var_protected(s, v) || solver_is_frozen(s, v)) continue;
        if (s->factor_original_vars && v > s->factor_original_vars) continue;
        uint32_t p = occs[mkLit(v, false)].size;
        uint32_t n = occs[mkLit(v, true)].size;
        if (p == 0 && n == 0) continue;
        if (p <= MAX_OCC_GATE && n <= MAX_OCC_GATE) {
            eq_push_or_update(&q, v, (uint64_t)p * n);
        }
    }

    bool any_elim = false;
    bool unsat = false;
    uint32_t vars_elim_this_round = 0;

    while (q.size > 0 && s->work < end_work && !unsat && !s->error) {
        if ((vars_elim_this_round & 15) == 0 && solver_budget_exhausted_now(s)) break;
        Var v = eq_pop(&q);
        if (s->values[v] != UNDEF || s->elim->eliminated[v] || gauss_var_protected(s, v) || solver_is_frozen(s, v)) continue;
        if (s->factor_original_vars && v > s->factor_original_vars) continue;

        Lit pos_v = mkLit(v, false), neg_v = mkLit(v, true);
        InprocOcc *po = &occs[pos_v];
        InprocOcc *no = &occs[neg_v];
        clean_inproc_occ(s, po);
        clean_inproc_occ(s, no);
        uint32_t np = po->size, nn = no->size;
        if (np == 0 && nn == 0) continue;
        if (np > MAX_OCC_GATE || nn > MAX_OCC_GATE) continue;

        /* Pure literal fast path (0 resolvents!). */
        if (np == 0 || nn == 0) {
            Lit def_lit = (nn == 0) ? pos_v : neg_v;
            Lit rec[2] = {def_lit, 0};
            if (!elim_save(s, v, rec, 2)) break;
            InprocOcc *active = (np > 0) ? po : no;
            for (uint32_t i = 0; i < active->size; ++i) {
                CRef cr = active->clauses[i];
                if (!clause_deleted(s->arena, cr)) {
                    uint32_t sz = CLAUSE_SIZE(s->arena, cr);
                    const Lit *ls = CLAUSE_LITS(s->arena, cr);
                    proof_delete_clause(s, ls, sz);
                    if (s->lrat) lrat_buffer_delete(s, cr);
                    for (uint32_t k = 0; k < sz; ++k) {
                        Lit kl = ls[k];
                        if (live_occ[kl] > 0) live_occ[kl]--;
                        Var u = var(kl);
                        if (u != v && !s->elim->eliminated[u] && !solver_is_frozen(s, u) && s->values[u] == UNDEF) {
                            uint32_t up = live_occ[mkLit(u, false)];
                            uint32_t un = live_occ[mkLit(u, true)];
                            if (up <= MAX_OCC_GATE && un <= MAX_OCC_GATE)
                                eq_push_or_update(&q, u, (uint64_t)up * un);
                        }
                    }
                    arena_delete(s->arena, cr);
                    s->elim->clauses_removed++;
                }
            }
            live_occ[pos_v] = live_occ[neg_v] = 0;
            s->elim->eliminated[v] = true;
            s->elim->vars_eliminated++;
            vars_elim_this_round++;
            any_elim = true;
            continue;
        }

        /* Try syntactic gate extraction first, then bounded semantic Mini-CDCL definition! */
        uint8_t gate_kind = extract_gate(s, v, po, no, gate_pos, gate_neg, lit_seen) ? 1u : 0u;
        if (!gate_kind && np >= 2u && nn >= 2u && np + nn <= 64u &&
            np * nn > np + nn + grow_bound && s->work < end_work) {
            if (extract_semantic_gate(s, v, po, no, gate_pos, gate_neg)) {
                gate_kind = 2u;
            }
        }
        bool has_gate = (gate_kind != 0u);
        if (!has_gate && (np > MAX_OCC_PLAIN || nn > MAX_OCC_PLAIN)) continue;

        /* Stage all resolvents into flat buffer, checking clause budget. */
        uint32_t max_res = np + nn + grow_bound;
        uint32_t nr = 0;
        uint32_t stage_used = 0;
        bool too_many = false;

        for (uint32_t i = 0; i < np && !too_many; ++i) {
            CRef ca = po->clauses[i];
            for (uint32_t j = 0; j < nn && !too_many; ++j) {
                /* Syntactic gates: skip GxG (tautologies) and RxR (redundant).
                 * Semantic gates: skip RxR (redundant), KEEP GxG (may contain non-tautologies)! */
                if (gate_kind == 1u && (gate_pos[i] == gate_neg[j])) continue;
                if (gate_kind == 2u && (!gate_pos[i] && !gate_neg[j])) continue;
                CRef cb = no->clauses[j];
                s->work += CLAUSE_SIZE(s->arena, ca) + CLAUSE_SIZE(s->arena, cb);

                if (stage_used + MAX_RES_LEN + 1 > stage_cap) {
                    stage_cap = stage_cap * 2 + MAX_RES_LEN + 1;
                    Lit *p = realloc(stage_lits, (size_t)stage_cap * sizeof *p);
                    if (!p) { s->error = true; too_many = true; break; }
                    stage_lits = p;
                }
                int rlen = fast_resolve(s, ca, cb, v, stage_lits + stage_used, MAX_RES_LEN);
                if (rlen == -1) continue; /* Tautology / satisfied */
                if (rlen == -2) { too_many = true; break; }
                if (nr >= max_res) { too_many = true; break; }
                if (nr + 2 > stage_off_cap) {
                    stage_off_cap *= 2;
                    uint32_t *p = realloc(stage_off, (size_t)stage_off_cap * sizeof *p);
                    CRef *pa = realloc(stage_ca, (size_t)stage_off_cap * sizeof *pa);
                    CRef *pb = realloc(stage_cb, (size_t)stage_off_cap * sizeof *pb);
                    if (!p || !pa || !pb) {
                        if (p) stage_off = p;
                        if (pa) stage_ca = pa;
                        if (pb) stage_cb = pb;
                        s->error = true; too_many = true; break;
                    }
                    stage_off = p;
                    stage_ca = pa;
                    stage_cb = pb;
                }
                stage_ca[nr] = ca;
                stage_cb[nr] = cb;
                stage_off[nr++] = stage_used;
                stage_used += (uint32_t)rlen;
                stage_off[nr] = stage_used;
            }
        }
        if (too_many || s->error) continue;

        /* Save reconstruction clauses:
         * - For gate BVE: MUST save ONE polarity of the GATE clauses ONLY!
         * - For plain BVE: save the smaller polarity of ALL clauses! */
        uint32_t save_words = 2;
        bool save_neg;
        if (has_gate) {
            uint32_t gp_w = 0, gn_w = 0;
            for (uint32_t i = 0; i < np; ++i) if (gate_pos[i]) gp_w += CLAUSE_SIZE(s->arena, po->clauses[i]) + 1;
            for (uint32_t j = 0; j < nn; ++j) if (gate_neg[j]) gn_w += CLAUSE_SIZE(s->arena, no->clauses[j]) + 1;
            save_neg = (gn_w < gp_w);
            save_words += save_neg ? gn_w : gp_w;
        } else {
            uint32_t p_w = 0, n_w = 0;
            for (uint32_t i = 0; i < np; ++i) p_w += CLAUSE_SIZE(s->arena, po->clauses[i]) + 1;
            for (uint32_t j = 0; j < nn; ++j) n_w += CLAUSE_SIZE(s->arena, no->clauses[j]) + 1;
            save_neg = (n_w < p_w);
            save_words += save_neg ? n_w : p_w;
        }

        Lit *saved = malloc((size_t)save_words * sizeof *saved);
        if (!saved) { s->error = true; break; }
        uint32_t sw = 0;
        saved[sw++] = mkLit(v, !save_neg); /* Default satisfies omitted polarity */
        saved[sw++] = 0;
        InprocOcc *kept_occ = save_neg ? no : po;
        uint8_t *kept_gate = save_neg ? gate_neg : gate_pos;
        for (uint32_t i = 0; i < kept_occ->size; ++i) {
            if (has_gate && !kept_gate[i]) continue;
            CRef cr = kept_occ->clauses[i];
            uint32_t sz = CLAUSE_SIZE(s->arena, cr);
            memcpy(saved + sw, CLAUSE_LITS(s->arena, cr), (size_t)sz * sizeof(Lit));
            sw += sz;
            saved[sw++] = 0;
        }
        bool saved_ok = elim_save(s, v, saved, sw);
        free(saved);
        if (!saved_ok) break;

        /* Emit RUP proofs & add all resolvents BEFORE deleting old clauses! */
        for (uint32_t r = 0; r < nr && !unsat && !s->error; ++r) {
            Lit *rlits = stage_lits + stage_off[r];
            uint32_t rsz = stage_off[r + 1] - stage_off[r];
            uint64_t lrat_new_id = s->lrat ? lrat_emit_pair_rup(s, rlits, rsz, stage_ca[r], stage_cb[r]) : 0;
            proof_add_clause(s, rlits, rsz);
            if (s->error) break;
            if (rsz == 0) {
                s->base_unsat = true; s->result = FALSE;
                unsat = true; break;
            }
            if (rsz == 1) {
                if (!inproc_finish_unit_lrat(s, rlits[0], lrat_new_id)) { unsat = true; break; }
                s->elim->resolvents_added++;
                continue;
            }
            CRef ncr = arena_alloc(s->arena, rlits, rsz, false);
            if (ncr == INVALID_CLAUSE) { s->error = true; break; }
            if (s->lrat && lrat_new_id) lrat_set_cref_id(s, ncr, lrat_new_id);
            if (!push_original_clause_ref(s, ncr)) break;
            for (uint32_t k = 0; k < rsz; ++k) {
                if (!occ_push(&occs[rlits[k]], ncr)) { s->error = true; break; }
                live_occ[rlits[k]]++;
            }
            s->elim->resolvents_added++;
        }
        if (unsat || s->error) break;

        /* Delete all removed original clauses containing v, update live_occ, and cascade! */
        for (int pol = 0; pol < 2; ++pol) {
            InprocOcc *o = pol ? no : po;
            for (uint32_t i = 0; i < o->size; ++i) {
                CRef cr = o->clauses[i];
                if (clause_deleted(s->arena, cr)) continue;
                uint32_t sz = CLAUSE_SIZE(s->arena, cr);
                const Lit *ls = CLAUSE_LITS(s->arena, cr);
                proof_delete_clause(s, ls, sz);
                if (s->lrat) lrat_buffer_delete(s, cr);
                for (uint32_t k = 0; k < sz; ++k) {
                    if (live_occ[ls[k]] > 0) live_occ[ls[k]]--;
                }
                for (uint32_t k = 0; k < sz; ++k) {
                    Var u = var(ls[k]);
                    if (u != v && !s->elim->eliminated[u] && !solver_is_frozen(s, u) && s->values[u] == UNDEF) {
                        uint32_t up = live_occ[mkLit(u, false)];
                        uint32_t un = live_occ[mkLit(u, true)];
                        if (up <= MAX_OCC_GATE && un <= MAX_OCC_GATE) {
                            eq_push_or_update(&q, u, (uint64_t)up * un);
                        }
                    }
                }
                arena_delete(s->arena, cr);
                s->elim->clauses_removed++;
            }
            o->size = 0;
        }
        live_occ[pos_v] = live_occ[neg_v] = 0;
        s->elim->eliminated[v] = true;
        s->elim->vars_eliminated++;
        vars_elim_this_round++;
        any_elim = true;
    }

    /* Delete any learned clauses that reference an eliminated variable! */
    if (any_elim && !unsat && !s->error) {
        for (uint32_t i = 0; i < s->num_learnts; ++i) {
            CRef cr = s->learnts[i];
            if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
            uint32_t sz = CLAUSE_SIZE(s->arena, cr);
            const Lit *ls = CLAUSE_LITS(s->arena, cr);
            bool has_elim = false;
            for (uint32_t k = 0; k < sz; ++k) {
                if (s->elim->eliminated[var(ls[k])]) { has_elim = true; break; }
            }
            if (has_elim) {
                proof_delete_clause(s, ls, sz);
                if (s->lrat) lrat_buffer_delete(s, cr);
                arena_delete(s->arena, cr);
                s->learnts[i] = INVALID_CLAUSE;
            }
        }
    }

done:
    for (uint32_t l = 0; l < nl; ++l) free(occs[l].clauses);
    free(occs);
    free(live_occ);
    free(q.vars); free(q.pos); free(q.score);
    free(lit_seen); free(gate_pos); free(gate_neg);
    free(stage_lits); free(stage_off); free(stage_ca); free(stage_cb);

    if (unsat || s->error) return false;
    if (any_elim) {
        solver_rebuild_watches(s);
        if (s->error) return false;
        return inproc_root_clean(s);
    }
    return true;
}

/* =====================================================================
 * Pass 5: O(n) Prefix-Sharing Clause Vivification
 * ===================================================================== */

typedef struct {
    CRef cref;
    uint32_t slot;
    Lit key[3];
    bool learned;
} VivCand;

static int cmp_viv_cand(const void *a, const void *b) {
    const VivCand *x = (const VivCand *)a;
    const VivCand *y = (const VivCand *)b;
    for (int k = 0; k < 3; ++k) {
        if (x->key[k] != y->key[k]) return (x->key[k] > y->key[k]) - (x->key[k] < y->key[k]);
    }
    return (x->cref > y->cref) - (x->cref < y->cref);
}

/* Collect all decision literals (level > 0) responsible for a conflict or implied literal. */
static uint32_t vivify_analyze(Solver *s, CRef confl, Lit implied, Lit *out) {
    uint8_t *seen = s->seen;
    Var *touched = s->minimize_touched;
    uint32_t tc = 0, out_n = 0;

    if (implied != LIT_UNDEF) {
        out[out_n++] = implied;
        Var iv = var(implied);
        seen[iv] = 1;
        touched[tc++] = iv;
    } else if (confl == BINARY_CONFLICT) {
        for (int i = 0; i < 2; ++i) {
            Var v = var(s->binary_conflict_lits[i]);
            if (!seen[v] && s->vars[v].level > 0) { seen[v] = 1; touched[tc++] = v; }
        }
    } else if (confl != INVALID_CLAUSE) {
        uint32_t sz = CLAUSE_SIZE(s->arena, confl);
        const Lit *ls = CLAUSE_LITS(s->arena, confl);
        for (uint32_t i = 0; i < sz; ++i) {
            Var v = var(ls[i]);
            if (!seen[v] && s->vars[v].level > 0) { seen[v] = 1; touched[tc++] = v; }
        }
    }

    for (uint32_t idx = s->trail_size; idx > 0;) {
        Lit p = s->trail[--idx].lit;
        Var v = var(p);
        if (!seen[v]) continue;
        if (s->vars[v].level == 0) continue;

        CRef reason = s->vars[v].reason;
        Lit bin = s->binary_reasons[v];
        if (bin != LIT_UNDEF) {
            Var u = var(bin);
            if (!seen[u] && s->vars[u].level > 0) { seen[u] = 1; touched[tc++] = u; }
        } else if (reason != INVALID_CLAUSE) {
            uint32_t sz = CLAUSE_SIZE(s->arena, reason);
            const Lit *ls = CLAUSE_LITS(s->arena, reason);
            for (uint32_t j = 0; j < sz; ++j) {
                Var u = var(ls[j]);
                if (u != v && !seen[u] && s->vars[u].level > 0) { seen[u] = 1; touched[tc++] = u; }
            }
        } else {
            /* Decision variable: its negated assignment belongs in the shortened clause! */
            if (implied == LIT_UNDEF || v != var(implied)) {
                out[out_n++] = neg(p);
            }
        }
    }

    for (uint32_t i = 0; i < tc; ++i) seen[touched[i]] = 0;
    return out_n;
}

static bool inproc_vivify(Solver *s, uint64_t tick_budget) {
    if (s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;
    uint32_t nl = 2 * (s->num_vars + 1);
    uint32_t max_c = s->num_clauses + s->num_learnts;
    if (!max_c) return true;
    uint32_t *lit_freq = calloc(nl, sizeof *lit_freq);
    if (!lit_freq) { s->error = true; return false; }

    VivCand *cands = malloc((size_t)max_c * sizeof *cands);
    if (!cands) { free(lit_freq); s->error = true; return false; }

    uint32_t nc = 0;
    /* Collect irredundant clauses (size 3..64) and Tier-1/Tier-2 learned clauses (size 3..32). */
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        ClauseHeader *h = CLAUSE_HEADER(s->arena, cr);
        if (h->size < 3 || h->size > 64 || h->vivified) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        for (uint32_t j = 0; j < h->size; ++j) lit_freq[ls[j]]++;
        cands[nc++] = (VivCand){cr, i, {0, 0, 0}, false};
    }
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        ClauseHeader *h = CLAUSE_HEADER(s->arena, cr);
        if (h->size < 3 || h->size > 32 || h->lbd > 6 || h->vivified) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        for (uint32_t j = 0; j < h->size; ++j) lit_freq[ls[j]]++;
        cands[nc++] = (VivCand){cr, i, {0, 0, 0}, true};
    }

    if (nc == 0) { free(cands); free(lit_freq); return true; }

    /* Sort each candidate's literals (non-watched tail or key preview) by descending frequency.
     * To avoid invalidating watches in-place, just compute top-3 key literals for prefix sorting! */
    for (uint32_t i = 0; i < nc; ++i) {
        CRef cr = cands[i].cref;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        Lit top[3] = {0, 0, 0};
        uint32_t tf[3] = {0, 0, 0};
        for (uint32_t j = 0; j < sz; ++j) {
            Lit l = ls[j];
            uint32_t f = lit_freq[l];
            for (int k = 0; k < 3; ++k) {
                if (f > tf[k] || (f == tf[k] && l > top[k])) {
                    for (int m = 2; m > k; --m) { top[m] = top[m - 1]; tf[m] = tf[m - 1]; }
                    top[k] = l; tf[k] = f; break;
                }
            }
        }
        cands[i].key[0] = top[0]; cands[i].key[1] = top[1]; cands[i].key[2] = top[2];
    }
    qsort(cands, nc, sizeof *cands, cmp_viv_cand);

    /* Save solver mode flags: run vivification cleanly with non-chrono levels & no phase pollution. */
    bool saved_chrono = s->opts.chrono;
    bool saved_phase = s->opts.phase_saving;
    uint64_t saved_wl = s->work_limit;
    s->opts.chrono = false;
    s->opts.phase_saving = false;
    uint64_t end_work = s->work + tick_budget;
    s->work_limit = end_work;

    Lit lbuf[128], out_buf[128], assumed[128];
    uint32_t assumed_n = 0;
    bool unsat = false;
    bool any_change = false;

    for (uint32_t ci = 0; ci < nc && s->work < end_work && !unsat && !s->error && !s->interrupted; ++ci) {
        if ((ci & 31) == 0 && solver_budget_exhausted_now(s)) break;
        CRef cr = cands[ci].cref;
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 3 || sz > 64) continue;

        /* Copy clause literals and sort by descending frequency for prefix sharing. */
        memcpy(lbuf, CLAUSE_LITS(s->arena, cr), (size_t)sz * sizeof(Lit));
        for (uint32_t i = 1; i < sz; ++i) {
            Lit key = lbuf[i];
            uint32_t kf = lit_freq[key];
            uint32_t j = i;
            while (j > 0 && (lit_freq[lbuf[j - 1]] < kf ||
                             (lit_freq[lbuf[j - 1]] == kf && lbuf[j - 1] < key))) {
                lbuf[j] = lbuf[j - 1]; --j;
            }
            lbuf[j] = key;
        }

        /* Reuse shared prefix of assumptions from previous clause!
         * Guard against circularity: if cr itself propagated a literal on the
         * current trail, backtrack strictly before that literal's level. */
        uint32_t shared = 0;
        while (shared < assumed_n && shared < sz && assumed[shared] == lbuf[shared]) shared++;
        {
            Lit w0 = CLAUSE_LITS(s->arena, cr)[0];
            if (s->values[var(w0)] != UNDEF && s->vars[var(w0)].reason == cr) {
                Level rl = s->vars[var(w0)].level;
                if (rl > 0 && rl - 1 < shared) shared = rl - 1;
            }
        }
        if (shared < s->decision_level) {
            solver_backtrack(s, shared);
            assumed_n = shared;
        }

        /* Hide C itself during propagation so C cannot imply its own literals. */
        s->ignore_clause = cr;

        bool shortened = false;
        bool subsumed_sat = false;
        uint32_t new_n = 0;
        uint32_t dropped_false = 0;
        CRef lrat_trigger = INVALID_CLAUSE;

        for (uint32_t i = shared; i < sz; ++i) {
            Lit l = lbuf[i];
            if (lit_is_false(s, l)) {
                /* Either root-false or implied false by earlier assumptions -> redundant! */
                dropped_false++;
                continue;
            }
            if (lit_is_true(s, l)) {
                if (s->vars[var(l)].level == 0) {
                    subsumed_sat = true;
                } else {
                    CRef r = s->vars[var(l)].reason;
                    if (!s->lrat || (r != INVALID_CLAUSE && r != BINARY_CONFLICT)) {
                        new_n = vivify_analyze(s, INVALID_CLAUSE, l, out_buf);
                        shortened = (new_n < sz);
                        if (shortened) lrat_trigger = r;
                    }
                }
                break;
            }
            /* Assume neg(l) at next decision level and propagate. */
            s->decision_level++;
            s->trail_lims[s->decision_level] = s->trail_size;
            Var v = var(l);
            s->values[v] = (uint8_t)(1u + sign(l)); /* neg(l) is TRUE -> l is FALSE */
            s->vars[v].level = s->decision_level;
            s->vars[v].reason = INVALID_CLAUSE;
            s->binary_reasons[v] = LIT_UNDEF;
            s->vars[v].trail_pos = s->trail_size;
            s->trail[s->trail_size++].lit = neg(l);
            assumed[assumed_n++] = l;

            CRef confl = solver_propagate(s);
            if (s->error || s->interrupted || s->work >= end_work) break;
            if (confl != INVALID_CLAUSE) {
                new_n = vivify_analyze(s, confl, LIT_UNDEF, out_buf);
                shortened = (new_n < sz);
                if (shortened) lrat_trigger = confl;
                break;
            }
        }

        s->ignore_clause = INVALID_CLAUSE;

        if (!shortened && !subsumed_sat && dropped_false > 0 && !s->interrupted && s->work < end_work) {
            /* Dropped false literals: rebuild shortened clause from remaining literals. */
            /* Re-verify: collect all literals of lbuf not falsified at level 0 or redundant. */
            new_n = 0;
            for (uint32_t i = 0; i < sz; ++i) {
                /* Keep literals that were assumed (in assumed[0..assumed_n-1]). */
                for (uint32_t a = 0; a < assumed_n; ++a) {
                    if (assumed[a] == lbuf[i]) { out_buf[new_n++] = lbuf[i]; break; }
                }
            }
            if (new_n < sz) {
                shortened = true;
                lrat_trigger = cr;
            }
        }

        if (subsumed_sat) {
            solver_backtrack(s, 0);
            assumed_n = 0;
            proof_delete_clause(s, CLAUSE_LITS(s->arena, cr), sz);
            if (s->lrat) lrat_buffer_delete(s, cr);
            watch_remove_clause(s->watches, s->arena, cr);
            arena_delete(s->arena, cr);
            if (cands[ci].learned) s->learnts[cands[ci].slot] = INVALID_CLAUSE;
            else s->clauses[cands[ci].slot] = INVALID_CLAUSE;
            any_change = true;
        } else if (shortened) {
            /* Deduplicate out_buf cleanly BEFORE backtracking so LRAT can trace live trail reasons! */
            if (new_n > 1) {
                sort_lits_small(out_buf, new_n);
                uint32_t u = 0;
                for (uint32_t j = 0; j < new_n; ++j) {
                    if (u && out_buf[j] == out_buf[u - 1]) continue;
                    out_buf[u++] = out_buf[j];
                }
                new_n = u;
            }
            uint64_t lrat_new_id = 0;
            if (new_n < sz && s->lrat) {
                lrat_new_id = lrat_emit_learned(s, lrat_trigger, out_buf, new_n);
                if (s->error) { solver_backtrack(s, 0); break; }
            }
            solver_backtrack(s, 0);
            assumed_n = 0;
            if (new_n < sz) {
                proof_add_clause(s, out_buf, new_n);
                if (s->error) break;
                proof_delete_clause(s, CLAUSE_LITS(s->arena, cr), sz);
                if (s->lrat) lrat_buffer_delete(s, cr);
                uint32_t old_lbd = clause_lbd(s->arena, cr);
                uint32_t old_used = CLAUSE_HEADER(s->arena, cr)->used;
                watch_remove_clause(s->watches, s->arena, cr);
                arena_delete(s->arena, cr);
                s->stats.minimized_literals += sz - new_n;
                any_change = true;
                if (new_n == 0) {
                    s->base_unsat = true; s->result = FALSE;
                    unsat = true; break;
                } else if (new_n == 1) {
                    if (cands[ci].learned) s->learnts[cands[ci].slot] = INVALID_CLAUSE;
                    else s->clauses[cands[ci].slot] = INVALID_CLAUSE;
                    if (!inproc_finish_unit_lrat(s, out_buf[0], lrat_new_id) || !root_propagate(s)) {
                        unsat = true; break;
                    }
                } else {
                    CRef ncr = arena_alloc(s->arena, out_buf, new_n, cands[ci].learned);
                    if (ncr == INVALID_CLAUSE) { s->error = true; break; }
                    if (s->lrat && lrat_new_id) lrat_set_cref_id(s, ncr, lrat_new_id);
                    CLAUSE_HEADER(s->arena, ncr)->vivified = 1;
                    if (cands[ci].learned) {
                        set_clause_lbd(s->arena, ncr, MIN(old_lbd, new_n));
                        CLAUSE_HEADER(s->arena, ncr)->used = old_used;
                        s->learnts[cands[ci].slot] = ncr;
                        CRef w = (new_n == 2) ? arena_binary_watch_ref(ncr) : ncr;
                        watch_add(s->watches, out_buf[0], w, out_buf[1]);
                        watch_add(s->watches, out_buf[1], w, out_buf[0]);
                    } else {
                        s->clauses[cands[ci].slot] = ncr;
                        CRef w = (new_n == 2) ? (s->lrat ? arena_binary_watch_ref(ncr) : INVALID_CLAUSE) : ncr;
                        watch_add(s->watches, out_buf[0], w, out_buf[1]);
                        watch_add(s->watches, out_buf[1], w, out_buf[0]);
                    }
                    if (s->watches->failed) { s->error = true; break; }
                }
            }
        } else if (!s->interrupted && s->work < end_work) {
            CLAUSE_HEADER(s->arena, cr)->vivified = 1;
        }
    }

    s->ignore_clause = INVALID_CLAUSE;
    solver_backtrack(s, 0);
    s->opts.chrono = saved_chrono;
    s->opts.phase_saving = saved_phase;
    s->work_limit = saved_wl;

    free(cands);
    free(lit_freq);

    if (unsat || s->error) return false;
    if (any_change) {
        solver_rebuild_watches(s);
        if (s->error) return false;
        return inproc_root_clean(s);
    }
    return true;
}

/* =====================================================================
 * Pass 6: Root-Prioritized Failed-Literal Probing + Hyper-Binary Res.
 * ===================================================================== */

typedef struct {
    Lit lit;
    uint32_t degree;
} ProbeCand;

static int cmp_probe_cand(const void *a, const void *b) {
    const ProbeCand *x = (const ProbeCand *)a;
    const ProbeCand *y = (const ProbeCand *)b;
    if (x->degree != y->degree) return (y->degree > x->degree) - (y->degree < x->degree);
    return (x->lit > y->lit) - (x->lit < y->lit);
}

static bool has_binary_watch(Solver *s, Lit a, Lit b) {
    WatchList *wl = watch_list(s->watches, a);
    for (uint32_t i = 0; i < wl->size; ++i) {
        Watch w = wl->watches[i];
        if ((is_binary_watch(w) || is_arena_binary_watch(w)) && w.blocker == b) return true;
    }
    return false;
}

static bool inproc_probe_hbr(Solver *s, uint64_t tick_budget) {
    if (s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;
    uint32_t nl = 2 * (s->num_vars + 1);
    ProbeCand *cands = malloc((size_t)nl * sizeof *cands);
    if (!cands) { s->error = true; return false; }

    /* Score each unassigned literal u by outgoing binary implications (~u watches).
     * Favor roots (literals with 0 incoming binary edges) with a bonus! */
    uint32_t nc = 0;
    for (Lit u = 2; u < nl; ++u) {
        if (s->values[var(u)] != UNDEF || (s->elim && s->elim->eliminated[var(u)])) continue;
        WatchList *out_wl = watch_list(s->watches, neg(u));
        uint32_t out_bin = 0;
        for (uint32_t i = 0; i < out_wl->size; ++i) {
            Watch w = out_wl->watches[i];
            if ((is_binary_watch(w) || is_arena_binary_watch(w)) && s->values[var(w.blocker)] == UNDEF)
                out_bin++;
        }
        if (out_bin == 0) continue;
        WatchList *in_wl = watch_list(s->watches, u);
        uint32_t in_bin = 0;
        for (uint32_t i = 0; i < in_wl->size; ++i) {
            Watch w = in_wl->watches[i];
            if ((is_binary_watch(w) || is_arena_binary_watch(w)) && s->values[var(w.blocker)] == UNDEF) {
                in_bin++; break;
            }
        }
        uint32_t score = out_bin + (in_bin == 0 ? 100000u : 0u);
        cands[nc++] = (ProbeCand){u, score};
    }

    if (nc == 0) { free(cands); return true; }
    qsort(cands, nc, sizeof *cands, cmp_probe_cand);

    bool saved_chrono = s->opts.chrono;
    bool saved_phase = s->opts.phase_saving;
    uint64_t saved_wl = s->work_limit;
    s->opts.chrono = false;
    s->opts.phase_saving = false;
    uint64_t end_work = s->work + tick_budget;
    s->work_limit = end_work;

    bool unsat = false;
    uint32_t hbr_added = 0;
    const uint32_t MAX_HBR_PER_ROUND = 2000;
    Lit hbr_buf[32];
    Lit both_buf[32];
    uint32_t *imp_stamp = calloc(nl, sizeof *imp_stamp);
    uint8_t *pair_done = calloc((size_t)(s->num_vars + 1), 1);
    if (!imp_stamp || !pair_done) {
        free(imp_stamp); free(pair_done); free(cands);
        s->opts.chrono = saved_chrono; s->opts.phase_saving = saved_phase; s->work_limit = saved_wl;
        s->error = true; return false;
    }
    uint32_t cur_stamp = 0;

    uint32_t max_probes = MIN(nc, 4096u);
    for (uint32_t ci = 0; ci < max_probes && s->work < end_work && !unsat && !s->error && !s->interrupted; ++ci) {
        Lit u = cands[ci].lit;
        if (s->values[var(u)] != UNDEF) continue;

        uint32_t root_trail = s->trail_size;
        s->decision_level = 1;
        s->trail_lims[1] = root_trail;
        Var uv = var(u);
        s->values[uv] = (uint8_t)(2u - sign(u));
        s->vars[uv].level = 1;
        s->vars[uv].reason = INVALID_CLAUSE;
        s->binary_reasons[uv] = LIT_UNDEF;
        s->vars[uv].trail_pos = root_trail;
        s->trail[s->trail_size++].lit = u;

        CRef confl = solver_propagate(s);
        if (s->error || s->interrupted || s->work >= end_work) {
            solver_backtrack(s, 0);
            break;
        }

        if (confl != INVALID_CLAUSE) {
            /* Failed literal! neg(u) is a RUP root unit. */
            solver_backtrack(s, 0);
            Lit unit = neg(u);
            proof_add_clause(s, &unit, 1);
            if (s->error || !enqueue_root_unit(s, unit) || !root_propagate(s)) {
                unsat = true; break;
            }
        } else {
            /* Collect hyper-binary resolvents (~u | q) for literals q implied by long clauses. */
            uint32_t n_hbr = 0;
            uint32_t prop_cnt = s->trail_size - (root_trail + 1);
            /* Run double-lookahead pair intersection on small/medium formulas (<= 5k vars)
             * during later rounds so preprocessing HBR on large planning/circuit instances
             * is completely undisturbed. */
            bool try_pair = (s->num_vars <= 5000u && s->inprocess_rounds >= 2 && ci < 256u &&
                             !pair_done[uv] && prop_cnt >= 4 && prop_cnt <= 128 && cur_stamp < UINT32_MAX - 2u);
            if (try_pair) {
                cur_stamp++;
                for (uint32_t t = root_trail + 1; t < s->trail_size; ++t) {
                    imp_stamp[s->trail[t].lit] = cur_stamp;
                }
            }
            if (hbr_added < MAX_HBR_PER_ROUND) {
                for (uint32_t t = root_trail + 1; t < s->trail_size && n_hbr < 32; ++t) {
                    Lit q = s->trail[t].lit;
                    Var qv = var(q);
                    if (s->binary_reasons[qv] == LIT_UNDEF && s->vars[qv].reason != INVALID_CLAUSE) {
                        if (!has_binary_watch(s, neg(u), q)) {
                            hbr_buf[n_hbr++] = q;
                        }
                    }
                }
            }
            solver_backtrack(s, 0);
            for (uint32_t k = 0; k < n_hbr && hbr_added < MAX_HBR_PER_ROUND && !s->error; ++k) {
                Lit pair[2] = {neg(u), hbr_buf[k]};
                proof_add_clause(s, pair, 2);
                if (s->error) break;
                if (s->num_learnts == s->learnts_size) {
                    uint32_t cap = s->learnts_size ? s->learnts_size * 2 : 64;
                    CRef *p = realloc(s->learnts, (size_t)cap * sizeof *p);
                    if (!p) { s->error = true; break; }
                    s->learnts = p; s->learnts_size = cap;
                }
                CRef cr = arena_alloc(s->arena, pair, 2, true);
                if (cr == INVALID_CLAUSE) { s->error = true; break; }
                set_clause_lbd(s->arena, cr, 2);
                CLAUSE_HEADER(s->arena, cr)->hyper = 1;
                s->learnts[s->num_learnts++] = cr;
                CRef wref = arena_binary_watch_ref(cr);
                watch_add(s->watches, pair[0], wref, pair[1]);
                watch_add(s->watches, pair[1], wref, pair[0]);
                if (s->watches->failed) { s->error = true; break; }
                hbr_added++;
            }

            /* Probing intersection (double lookahead): probe ~u and harvest backbone units
             * q implied by BOTH u and ~u (100% RUP-certified via (~u | q), (q))! */
            if (try_pair && !s->error && !s->interrupted && s->work < end_work && s->values[uv] == UNDEF) {
                pair_done[uv] = 1;
                Lit nu = neg(u);
                uint32_t r2 = s->trail_size;
                s->decision_level = 1;
                s->trail_lims[1] = r2;
                s->values[uv] = (uint8_t)(2u - sign(nu));
                s->vars[uv].level = 1;
                s->vars[uv].reason = INVALID_CLAUSE;
                s->binary_reasons[uv] = LIT_UNDEF;
                s->vars[uv].trail_pos = r2;
                s->trail[s->trail_size++].lit = nu;

                CRef c2 = solver_propagate(s);
                if (s->error || s->interrupted || s->work >= end_work) {
                    solver_backtrack(s, 0);
                    break;
                }
                if (c2 != INVALID_CLAUSE) {
                    solver_backtrack(s, 0);
                    proof_add_clause(s, &u, 1);
                    if (s->error || !enqueue_root_unit(s, u) || !root_propagate(s)) {
                        unsat = true; break;
                    }
                } else {
                    uint32_t n_both = 0;
                    for (uint32_t t = r2 + 1; t < s->trail_size && n_both < 32; ++t) {
                        Lit q = s->trail[t].lit;
                        if (imp_stamp[q] == cur_stamp) both_buf[n_both++] = q;
                    }
                    solver_backtrack(s, 0);
                    for (uint32_t b = 0; b < n_both && !unsat && !s->error; ++b) {
                        Lit q = both_buf[b];
                        if (lit_is_true(s, q)) continue;
                        Lit bridge[2] = {neg(u), q};
                        proof_add_clause(s, bridge, 2);
                        if (s->error) break;
                        proof_add_clause(s, &q, 1);
                        if (s->error) break;
                        proof_delete_clause(s, bridge, 2);
                        if (!enqueue_root_unit(s, q) || !root_propagate(s)) {
                            unsat = true; break;
                        }
                    }
                }
            }
        }
    }

    solver_backtrack(s, 0);
    s->opts.chrono = saved_chrono;
    s->opts.phase_saving = saved_phase;
    s->work_limit = saved_wl;
    free(imp_stamp);
    free(pair_done);
    free(cands);
    return !unsat && !s->error;
}

/* Pass 7A: GF(2) XOR extraction, root RREF, and in-search Bird/Han-Jiang
 * dynamic Gaussian elimination now live in src/gauss.c (gauss_extract_and_init). */

/* =====================================================================
 * Pass 2c: Iterative Fixpoint Multi-Gate Congruence Closure (100% RUP-Sound)
 *
 * Extracts:
 *   - N-ary AND gates (k = 2..8): out <-> a_1 & ... & a_k
 *   - 3-input Majority gates (MAJ3, full-adder carry): out <-> maj(a, b, c)
 *   - 3-input XOR gates (XOR3, full-adder sum): out <-> a ^ b ^ c
 *
 * Runs an iterative fixpoint union-find closure seeded from live binary
 * equivalences, propagating discovered equivalences and root units across
 * gate levels until fixpoint or budget. Every emitted unit/binary is
 * independently RUP-checked via solver_add_rup_clause (using 1-split RUP for
 * MAJ3 and 2-split RUP for XOR3).
 * ===================================================================== */

enum {
    NCONG_AND  = 0,
    NCONG_MAJ3 = 1,
    NCONG_XOR3 = 2,
    NCONG_XOR2 = 3,
    NCONG_DEAD = 4
};

typedef struct {
    Lit out;
    Lit in[8];
    uint8_t k;
    uint8_t kind;
} NaryGate;

typedef struct {
    Var v[3];
    uint8_t mask;
} TernCell;

typedef struct {
    Var v[4];
    uint16_t mask;
} QuadCell;

typedef struct {
    Lit a, b;
} MajPair;

static uint64_t hash_nary_gate(uint8_t kind, const Lit *in, uint8_t k) {
    uint64_t h = ((uint64_t)kind * 31u + (uint64_t)k) * UINT64_C(0x9e3779b97f4a7c15);
    for (uint8_t i = 0; i < k; ++i) {
        h ^= (uint64_t)in[i] + UINT64_C(0x9e3779b97f4a7c15) + (h << 6) + (h >> 2);
    }
    h ^= h >> 30; h *= UINT64_C(0xbf58476d1ce4e5b9);
    h ^= h >> 27; h *= UINT64_C(0x94d049bb133111eb);
    return h ^ (h >> 31);
}

static uint64_t mix64_cong(uint64_t x) {
    x ^= x >> 30; x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27; x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static bool add_rup_binary(Solver *s, Lit a, Lit b) {
    if (has_binary_watch(s, a, b)) return true;
    Lit pair[2] = {a, b};
    proof_add_clause(s, pair, 2);
    if (s->error) return false;
    if (s->num_learnts == s->learnts_size) {
        uint32_t ncap = s->learnts_size ? s->learnts_size * 2u : 64u;
        CRef *ptr = realloc(s->learnts, (size_t)ncap * sizeof *ptr);
        if (!ptr) { s->error = true; return false; }
        s->learnts = ptr; s->learnts_size = ncap;
    }
    CRef cr = arena_alloc(s->arena, pair, 2, true);
    if (cr == INVALID_CLAUSE) { s->error = true; return false; }
    set_clause_lbd(s->arena, cr, 2);
    s->learnts[s->num_learnts++] = cr;
    CRef wref = arena_binary_watch_ref(cr);
    watch_add(s->watches, pair[0], wref, pair[1]);
    watch_add(s->watches, pair[1], wref, pair[0]);
    if (s->watches->failed) { s->error = true; return false; }
    return true;
}

static bool ncong_rup_split1(Solver *s, const Lit *lits, uint32_t n, Lit split) {
    if (solver_add_rup_clause(s, lits, n)) return true;
    if (s->error || s->result == FALSE || split == LIT_UNDEF || solver_budget_exhausted_now(s)) return false;
    Lit cond[4];
    memcpy(cond, lits, (size_t)n * sizeof *lits);
    cond[n] = split;
    if (!solver_add_rup_clause(s, cond, n + 1u)) return false;
    cond[n] = neg(split);
    if (!solver_add_rup_clause(s, cond, n + 1u)) return false;
    return solver_add_rup_clause(s, lits, n);
}

static bool ncong_rup_split2(Solver *s, const Lit *lits, uint32_t n, Lit s1, Lit s2) {
    if (ncong_rup_split1(s, lits, n, s1)) return true;
    if (s->error || s->result == FALSE || s2 == LIT_UNDEF || solver_budget_exhausted_now(s)) return false;
    Lit cond[4];
    memcpy(cond, lits, (size_t)n * sizeof *lits);
    cond[n] = s2;
    if (!ncong_rup_split1(s, cond, n + 1u, s1)) return false;
    cond[n] = neg(s2);
    if (!ncong_rup_split1(s, cond, n + 1u, s1)) return false;
    return solver_add_rup_clause(s, lits, n);
}

static bool ncong_emit_clause(Solver *s, const Lit *lits, uint32_t n, Lit s1, Lit s2) {
    if (s2 != LIT_UNDEF) return ncong_rup_split2(s, lits, n, s1, s2);
    if (s1 != LIT_UNDEF) return ncong_rup_split1(s, lits, n, s1);
    return solver_add_rup_clause(s, lits, n);
}

static Lit ncong_uf_find(Lit *parent, Lit l) {
    Lit root = l;
    while (parent[var(root)] != mkLit(var(root), false)) {
        root = parent[var(root)] ^ sign(root);
    }
    while (var(l) != var(root)) {
        Lit next = parent[var(l)] ^ sign(l);
        parent[var(l)] = root ^ sign(l);
        l = next;
    }
    return root;
}

static bool ncong_merge(Solver *s, Lit *parent, Lit a, Lit b, Lit s1, Lit s2, uint32_t *changes) {
    a = ncong_uf_find(parent, a);
    b = ncong_uf_find(parent, b);
    if (a == b) return true;
    if (a == neg(b)) {
        Lit u = a;
        if (!ncong_emit_clause(s, &u, 1u, s1, s2)) return false;
        if (s->result != FALSE) {
            Lit nu = neg(a);
            (void)ncong_emit_clause(s, &nu, 1u, s1, s2);
        }
        (*changes)++;
        return true;
    }
    Lit fwd[2] = {neg(a), b};
    Lit rev[2] = {neg(b), a};
    if (!ncong_emit_clause(s, fwd, 2u, s1, s2) || s->result == FALSE || s->error) return false;
    if (!ncong_emit_clause(s, rev, 2u, s1, s2) || s->result == FALSE || s->error) return false;
    if (var(a) > var(b)) parent[var(a)] = b ^ sign(a);
    else parent[var(b)] = a ^ sign(b);
    (*changes)++;
    return true;
}

static bool ncong_emit_unit(Solver *s, Lit u, Lit s1, Lit s2, uint32_t *changes) {
    if (lxor(s->values[var(u)], sign(u)) == TRUE) return true;
    if (!ncong_emit_clause(s, &u, 1u, s1, s2)) return false;
    (*changes)++;
    return true;
}

static bool ncong_push_gate(NaryGate **gates, uint32_t *ngates, uint32_t *cap,
                            uint8_t kind, Lit out, const Lit *in, uint8_t k, Solver *s) {
    if (*ngates >= 131072u) return true;
    if (*ngates == *cap) {
        uint32_t ncap = (*cap) * 2u;
        if (ncap > 131072u) ncap = 131072u;
        NaryGate *p = realloc(*gates, (size_t)ncap * sizeof *p);
        if (!p) { s->error = true; return false; }
        *gates = p;
        *cap = ncap;
    }
    NaryGate *g = &(*gates)[(*ngates)++];
    g->out = out;
    g->k = k;
    g->kind = kind;
    memcpy(g->in, in, (size_t)k * sizeof(Lit));
    return true;
}

static bool inproc_nary_congruence(Solver *s, uint64_t tick_budget, uint32_t *out_merges) {
    if (out_merges) *out_merges = 0;
    if (!s->num_vars || !s->num_clauses || s->error || s->result == FALSE ||
        solver_budget_exhausted_now(s)) return true;

    /* Count live clauses to gate ceiling on active formula size rather than stale slots. */
    uint32_t live_clauses = 0, n_tern = 0, n_quad = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        live_clauses++;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz == 3u) n_tern++;
        else if (sz == 4u) n_quad++;
    }
    if (live_clauses == 0 || live_clauses > 600000u) return true;

    uint64_t saved_wl = s->work_limit;
    uint64_t end_work = s->work + tick_budget;
    s->work_limit = end_work;

    uint32_t nv = s->num_vars;
    Lit *parent = malloc(((size_t)nv + 1u) * sizeof *parent);
    uint32_t cap = 256u, ngates = 0;
    NaryGate *gates = malloc((size_t)cap * sizeof *gates);
    if (!parent || !gates) {
        free(parent); free(gates);
        s->work_limit = saved_wl;
        s->error = true;
        return false;
    }
    for (Var v = 0; v <= nv; ++v) parent[v] = mkLit(v, false);

    /* Seed union-find with all existing binary equivalences: (a | b) and (~a | ~b). */
    for (uint32_t i = 0; i < s->num_clauses && s->work < end_work; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr) || CLAUSE_SIZE(s->arena, cr) != 2u) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        if (s->values[var(ls[0])] != UNDEF || s->values[var(ls[1])] != UNDEF) continue;
        if (has_binary_watch(s, neg(ls[0]), neg(ls[1]))) {
            Lit a = ncong_uf_find(parent, ls[0]);
            Lit b = ncong_uf_find(parent, neg(ls[1]));
            if (a != b && a != neg(b)) {
                if (var(a) > var(b)) parent[var(a)] = b ^ sign(a);
                else parent[var(b)] = a ^ sign(b);
            }
        }
    }

    /* Optional hash tables for MAJ3 (ternary) and XOR3 (quaternary) extraction. */
    size_t tcap = 0, qcap = 0;
    TernCell *tcells = NULL;
    QuadCell *qcells = NULL;
    if (n_tern >= 6u && n_tern <= 250000u) {
        tcap = 64u;
        while (tcap < (size_t)n_tern * 2u) tcap <<= 1;
        tcells = calloc(tcap, sizeof *tcells);
    }
    if (n_quad >= 8u && n_quad <= 200000u) {
        qcap = 64u;
        while (qcap < (size_t)n_quad * 2u) qcap <<= 1;
        qcells = calloc(qcap, sizeof *qcells);
    }

    bool unsat = false;
    uint32_t total_changes = 0;

    /* 1. Extract N-ary AND (k = 2..8), index ternary & quaternary clauses. */
    for (uint32_t i = 0; i < s->num_clauses && s->work < end_work && !unsat && !s->error; ++i) {
        if ((i & 1023u) == 0 && solver_budget_exhausted_now(s)) break;
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 3u || sz > 9u) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool skip = false;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { skip = true; break; }
        }
        if (skip) continue;
        s->work += sz;

        /* Index ternary clause for MAJ3 extraction. */
        if (sz == 3u && tcells) {
            Lit sl[3] = {ls[0], ls[1], ls[2]};
            if (var(sl[0]) > var(sl[1])) { Lit t = sl[0]; sl[0] = sl[1]; sl[1] = t; }
            if (var(sl[1]) > var(sl[2])) { Lit t = sl[1]; sl[1] = sl[2]; sl[2] = t; }
            if (var(sl[0]) > var(sl[1])) { Lit t = sl[0]; sl[0] = sl[1]; sl[1] = t; }
            Var v0 = var(sl[0]), v1 = var(sl[1]), v2 = var(sl[2]);
            uint8_t bit = (uint8_t)(1u << (sign(sl[0]) | (sign(sl[1]) << 1) | (sign(sl[2]) << 2)));
            size_t pos = (size_t)(mix64_cong((uint64_t)v0 ^ ((uint64_t)v1 << 21) ^ ((uint64_t)v2 << 42)) & (tcap - 1u));
            for (;;) {
                if (!tcells[pos].v[0]) {
                    tcells[pos].v[0] = v0; tcells[pos].v[1] = v1; tcells[pos].v[2] = v2;
                    tcells[pos].mask = bit;
                    break;
                }
                if (tcells[pos].v[0] == v0 && tcells[pos].v[1] == v1 && tcells[pos].v[2] == v2) {
                    tcells[pos].mask |= bit;
                    break;
                }
                pos = (pos + 1u) & (tcap - 1u);
            }
        }

        /* Index quaternary clause for XOR3 extraction. */
        if (sz == 4u && qcells) {
            Lit sl[4] = {ls[0], ls[1], ls[2], ls[3]};
            for (unsigned a = 1; a < 4; ++a) {
                Lit key = sl[a];
                unsigned b = a;
                while (b > 0 && var(sl[b - 1]) > var(key)) { sl[b] = sl[b - 1]; --b; }
                sl[b] = key;
            }
            Var v0 = var(sl[0]), v1 = var(sl[1]), v2 = var(sl[2]), v3 = var(sl[3]);
            uint16_t bit = (uint16_t)(1u << (sign(sl[0]) | (sign(sl[1]) << 1) | (sign(sl[2]) << 2) | (sign(sl[3]) << 3)));
            size_t pos = (size_t)(mix64_cong((uint64_t)v0 ^ ((uint64_t)v1 << 16) ^ ((uint64_t)v2 << 32) ^ ((uint64_t)v3 << 48)) & (qcap - 1u));
            for (;;) {
                if (!qcells[pos].v[0]) {
                    qcells[pos].v[0] = v0; qcells[pos].v[1] = v1; qcells[pos].v[2] = v2; qcells[pos].v[3] = v3;
                    qcells[pos].mask = bit;
                    break;
                }
                if (qcells[pos].v[0] == v0 && qcells[pos].v[1] == v1 &&
                    qcells[pos].v[2] == v2 && qcells[pos].v[3] == v3) {
                    qcells[pos].mask |= bit;
                    break;
                }
                pos = (pos + 1u) & (qcap - 1u);
            }
        }

        /* Extract N-ary AND gate (k = 2..8): (out | ~a1 | ... | ~ak) + (~out | ai). */
        if (sz >= 4u || (sz == 3u && ngates < 65536u)) {
            for (uint32_t oi = 0; oi < sz; ++oi) {
                Lit out = ls[oi];
                Lit nout = neg(out);
                if (watch_list(s->watches, nout)->size < sz - 1u) continue;
                bool all_bin = true;
                Lit inputs[8];
                uint8_t k = 0;
                for (uint32_t m = 0; m < sz; ++m) {
                    if (m == oi) continue;
                    Lit inp = neg(ls[m]);
                    if (!has_binary_watch(s, nout, inp)) { all_bin = false; break; }
                    inputs[k++] = inp;
                }
                if (!all_bin) continue;
                if (!ncong_push_gate(&gates, &ngates, &cap, NCONG_AND, out, inputs, k, s)) {
                    unsat = true; break;
                }
                break;
            }
        }
    }

    /* 2. Extract 3-input Majority gates (MAJ3) from complementary ternary pairs. */
    if (tcells && !unsat && !s->error && s->work < end_work && !solver_budget_exhausted_now(s)) {
        uint32_t nl = 2u * (nv + 1u);
        uint8_t *maj_deg = calloc(nl, 1);
        MajPair *maj_adj = malloc((size_t)nl * 12u * sizeof *maj_adj);
        if (maj_deg && maj_adj) {
            for (size_t i = 0; i < tcap; ++i) {
                if (!tcells[i].v[0]) continue;
                uint8_t mask = tcells[i].mask;
                for (unsigned m = 0; m < 4u; ++m) {
                    if ((mask & (1u << m)) && (mask & (1u << (m ^ 7u)))) {
                        Lit tl[3];
                        for (unsigned j = 0; j < 3u; ++j) tl[j] = mkLit(tcells[i].v[j], (m >> j) & 1u);
                        for (unsigned j = 0; j < 3u; ++j) {
                            Lit out = tl[j];
                            Lit a = tl[(j + 1u) % 3u], b = tl[(j + 2u) % 3u];
                            if (a > b) { Lit t = a; a = b; b = t; }
                            if (maj_deg[out] < 12u) {
                                maj_adj[(size_t)out * 12u + maj_deg[out]++] = (MajPair){a, b};
                            }
                            Lit nout = neg(out);
                            Lit na = neg(a), nb = neg(b);
                            if (na > nb) { Lit t = na; na = nb; nb = t; }
                            if (maj_deg[nout] < 12u) {
                                maj_adj[(size_t)nout * 12u + maj_deg[nout]++] = (MajPair){na, nb};
                            }
                        }
                    }
                }
            }
            for (Lit out = 2u; out < nl && !s->error && s->work < end_work; ++out) {
                uint8_t d = maj_deg[out];
                if (d < 3u) continue;
                const MajPair *ps = &maj_adj[(size_t)out * 12u];
                for (uint8_t p1 = 0; p1 < d; ++p1) {
                    for (uint8_t p2 = p1 + 1u; p2 < d; ++p2) {
                        Lit shared = LIT_UNDEF, x = LIT_UNDEF, y = LIT_UNDEF;
                        if (ps[p1].a == ps[p2].a) { shared = ps[p1].a; x = ps[p1].b; y = ps[p2].b; }
                        else if (ps[p1].a == ps[p2].b) { shared = ps[p1].a; x = ps[p1].b; y = ps[p2].a; }
                        else if (ps[p1].b == ps[p2].a) { shared = ps[p1].b; x = ps[p1].a; y = ps[p2].b; }
                        else if (ps[p1].b == ps[p2].b) { shared = ps[p1].b; x = ps[p1].a; y = ps[p2].a; }
                        if (shared == LIT_UNDEF || x == y) continue;
                        if (x > y) { Lit t = x; x = y; y = t; }
                        for (uint8_t p3 = p2 + 1u; p3 < d; ++p3) {
                            if (ps[p3].a == x && ps[p3].b == y) {
                                Lit ins[3] = {neg(shared), neg(x), neg(y)};
                                (void)ncong_push_gate(&gates, &ngates, &cap, NCONG_MAJ3, out, ins, 3u, s);
                                break;
                            }
                        }
                    }
                }
            }
        }
        free(maj_deg);
        free(maj_adj);
    }
    free(tcells);
    tcells = NULL;

    /* 3. Extract 3-input XOR gates (XOR3) from complete 8-clause quaternary parity blocks. */
    if (qcells && !unsat && !s->error && s->work < end_work && !solver_budget_exhausted_now(s)) {
        for (size_t i = 0; i < qcap && !s->error; ++i) {
            if (!qcells[i].v[0]) continue;
            uint16_t mask = qcells[i].mask;
            for (unsigned parity = 0; parity < 2u; ++parity) {
                uint16_t want = parity ? 0x9669u : 0x6996u;
                if ((mask & want) != want) continue;
                for (unsigned j = 0; j < 4u; ++j) {
                    Lit out = mkLit(qcells[i].v[j], parity != 0);
                    Lit ins[3];
                    uint8_t k = 0;
                    for (unsigned m = 0; m < 4u; ++m) {
                        if (m != j) ins[k++] = mkLit(qcells[i].v[m], false);
                    }
                    if (!ncong_push_gate(&gates, &ngates, &cap, NCONG_XOR3, out, ins, 3u, s)) break;
                }
            }
        }
    }
    free(qcells);
    qcells = NULL;

    /* 4. Iterative fixpoint congruence closure across ALL extracted gates! */
    if (!unsat && !s->error && ngates >= 2u && !solver_budget_exhausted_now(s)) {
        size_t hcap = 64u;
        while (hcap < (size_t)ngates * 2u) hcap <<= 1;
        uint32_t *ht = malloc(hcap * sizeof *ht);
        if (!ht) {
            free(parent); free(gates);
            s->work_limit = saved_wl;
            s->error = true;
            return false;
        }

        for (uint32_t iter = 0; iter < 8u && !unsat && !s->error && s->work < end_work; ++iter) {
            if (solver_budget_exhausted_now(s)) break;
            uint32_t before = total_changes;
            memset(ht, 0, hcap * sizeof *ht);

            for (uint32_t gi = 0; gi < ngates && !unsat && !s->error && s->work < end_work; ++gi) {
                if ((gi & 255u) == 0 && solver_budget_exhausted_now(s)) break;
                NaryGate *g = &gates[gi];
                if (g->kind == NCONG_DEAD) continue;
                s->work += g->k + 2u;

                g->out = ncong_uf_find(parent, g->out);
                for (uint8_t j = 0; j < g->k; ++j) g->in[j] = ncong_uf_find(parent, g->in[j]);

                if (g->kind == NCONG_AND) {
                    Lit nins[8];
                    uint8_t nk = 0;
                    bool force_false = false;
                    for (uint8_t j = 0; j < g->k; ++j) {
                        Lit l = g->in[j];
                        lbool v = lxor(s->values[var(l)], sign(l));
                        if (v == FALSE || l == neg(g->out)) { force_false = true; break; }
                        if (v == TRUE) continue;
                        nins[nk++] = l;
                    }
                    if (!force_false) {
                        sort_lits_small(nins, nk);
                        uint8_t u = 0;
                        for (uint8_t j = 0; j < nk; ++j) {
                            if (u > 0 && nins[j] == nins[u - 1u]) continue;
                            if (u > 0 && nins[j] == neg(nins[u - 1u])) { force_false = true; break; }
                            nins[u++] = nins[j];
                        }
                        nk = u;
                    }
                    if (force_false) {
                        if (ncong_emit_unit(s, neg(g->out), LIT_UNDEF, LIT_UNDEF, &total_changes)) g->kind = NCONG_DEAD;
                        if (s->result == FALSE) unsat = true;
                        continue;
                    }
                    if (nk == 0) {
                        if (ncong_emit_unit(s, g->out, LIT_UNDEF, LIT_UNDEF, &total_changes)) g->kind = NCONG_DEAD;
                        if (s->result == FALSE) unsat = true;
                        continue;
                    }
                    if (nk == 1u) {
                        if (ncong_merge(s, parent, g->out, nins[0], LIT_UNDEF, LIT_UNDEF, &total_changes)) g->kind = NCONG_DEAD;
                        if (s->result == FALSE) unsat = true;
                        continue;
                    }
                    g->k = nk;
                    memcpy(g->in, nins, (size_t)nk * sizeof(Lit));
                } else if (g->kind == NCONG_MAJ3) {
                    Lit a = g->in[0], b = g->in[1], c = g->in[2];
                    lbool va = lxor(s->values[var(a)], sign(a));
                    lbool vb = lxor(s->values[var(b)], sign(b));
                    lbool vc = lxor(s->values[var(c)], sign(c));
                    if (va != UNDEF || vb != UNDEF || vc != UNDEF) {
                        Lit x = (va != UNDEF) ? b : (vb != UNDEF) ? a : a;
                        Lit y = (va != UNDEF) ? c : (vb != UNDEF) ? c : b;
                        lbool cv = (va != UNDEF) ? va : (vb != UNDEF) ? vb : vc;
                        if (cv == TRUE) { g->out = neg(g->out); x = neg(x); y = neg(y); }
                        g->in[0] = x; g->in[1] = y; g->k = 2u; g->kind = NCONG_AND;
                        sort_lits_small(g->in, 2u);
                        gi--; continue;
                    }
                    Lit eq_lit = LIT_UNDEF, sp = LIT_UNDEF;
                    if (a == b) { eq_lit = a; sp = c; }
                    else if (a == c) { eq_lit = a; sp = b; }
                    else if (b == c) { eq_lit = b; sp = a; }
                    else if (a == neg(b)) { eq_lit = c; sp = a; }
                    else if (a == neg(c)) { eq_lit = b; sp = a; }
                    else if (b == neg(c)) { eq_lit = a; sp = b; }
                    if (eq_lit != LIT_UNDEF) {
                        if (ncong_merge(s, parent, g->out, eq_lit, sp, LIT_UNDEF, &total_changes)) g->kind = NCONG_DEAD;
                        if (s->result == FALSE) unsat = true;
                        continue;
                    }
                    unsigned negs = (unsigned)sign(a) + (unsigned)sign(b) + (unsigned)sign(c);
                    if (negs >= 2u) {
                        g->out = neg(g->out);
                        a = neg(a); b = neg(b); c = neg(c);
                    }
                    g->in[0] = a; g->in[1] = b; g->in[2] = c;
                    sort_lits_small(g->in, 3u);
                } else if (g->kind == NCONG_XOR3 || g->kind == NCONG_XOR2) {
                    Lit nins[3];
                    uint8_t nk = 0;
                    Lit s1 = g->in[0], s2 = (g->k >= 3u) ? g->in[1] : LIT_UNDEF;
                    for (uint8_t j = 0; j < g->k; ++j) {
                        Lit l = g->in[j];
                        lbool v = lxor(s->values[var(l)], sign(l));
                        if (v != UNDEF) {
                            if (v == TRUE) g->out = neg(g->out);
                            continue;
                        }
                        if (sign(l)) { g->out = neg(g->out); l = neg(l); }
                        nins[nk++] = l;
                    }
                    sort_lits_small(nins, nk);
                    uint8_t u = 0;
                    for (uint8_t j = 0; j < nk; ) {
                        if (j + 1u < nk && nins[j] == nins[j + 1u]) { j += 2u; }
                        else { nins[u++] = nins[j++]; }
                    }
                    nk = u;
                    if (nk == 0) {
                        if (ncong_emit_unit(s, neg(g->out), s1, s2, &total_changes)) g->kind = NCONG_DEAD;
                        if (s->result == FALSE) unsat = true;
                        continue;
                    }
                    if (nk == 1u) {
                        if (ncong_merge(s, parent, g->out, nins[0], s1, s2, &total_changes)) g->kind = NCONG_DEAD;
                        if (s->result == FALSE) unsat = true;
                        continue;
                    }
                    g->k = nk;
                    g->kind = (nk == 3u) ? NCONG_XOR3 : NCONG_XOR2;
                    memcpy(g->in, nins, (size_t)nk * sizeof(Lit));
                }

                size_t pos = (size_t)(hash_nary_gate(g->kind, g->in, g->k) & (hcap - 1u));
                for (;;) {
                    uint32_t slot = ht[pos];
                    if (!slot) { ht[pos] = gi + 1u; break; }
                    NaryGate *h = &gates[slot - 1u];
                    if (h->kind == g->kind && h->k == g->k &&
                        memcmp(h->in, g->in, (size_t)g->k * sizeof(Lit)) == 0) {
                        if (g->out != h->out) {
                            Lit sp1 = LIT_UNDEF, sp2 = LIT_UNDEF;
                            if (g->kind == NCONG_MAJ3 || g->kind == NCONG_XOR2) sp1 = g->in[0];
                            else if (g->kind == NCONG_XOR3) { sp1 = g->in[0]; sp2 = g->in[1]; }
                            if (ncong_merge(s, parent, g->out, h->out, sp1, sp2, &total_changes)) {
                                g->kind = NCONG_DEAD;
                            }
                            if (s->result == FALSE) unsat = true;
                        } else {
                            g->kind = NCONG_DEAD;
                        }
                        break;
                    }
                    pos = (pos + 1u) & (hcap - 1u);
                }
            }
            if (total_changes == before) break;
        }
        free(ht);
    }

    free(parent);
    free(gates);
    s->work_limit = saved_wl;
    if (out_merges) *out_merges = total_changes;
    if (unsat || s->error || s->result == FALSE) return false;
    return true;
}

/* =====================================================================
 * Pass 2d: Model-Guided Global Backbone Extraction (100% RUP-Sound)
 *
 * Probes high-activity unassigned variables (including those with zero
 * binary watches, missed by inproc_probe_hbr) using the negation of their
 * best/saved polarity to harvest RUP-certified global backbone units.
 * ===================================================================== */

typedef struct {
    Var v;
    double score;
} BackboneCand;

static int cmp_bb_cand(const void *a, const void *b) {
    double sa = ((const BackboneCand *)a)->score;
    double sb = ((const BackboneCand *)b)->score;
    if (sa != sb) return (sb > sa) - (sb < sa);
    Var va = ((const BackboneCand *)a)->v;
    Var vb = ((const BackboneCand *)b)->v;
    return (va > vb) - (va < vb);
}

static bool inproc_backbone(Solver *s, uint64_t tick_budget) {
    if (!s->num_vars || s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;

    uint32_t nv = s->num_vars;
    BackboneCand *cands = malloc((size_t)nv * sizeof *cands);
    if (!cands) { s->error = true; return false; }
    uint32_t nc = 0;
    for (Var v = 1; v <= nv; ++v) {
        if (s->values[v] != UNDEF || (s->elim && s->elim->eliminated[v])) continue;
        cands[nc++] = (BackboneCand){v, s->vars[v].activity};
    }
    if (nc == 0) { free(cands); return true; }
    qsort(cands, nc, sizeof *cands, cmp_bb_cand);

    bool saved_chrono = s->opts.chrono;
    bool saved_phase = s->opts.phase_saving;
    uint64_t saved_wl = s->work_limit;
    s->opts.chrono = false;
    s->opts.phase_saving = false;
    uint64_t end_work = s->work + tick_budget;
    s->work_limit = end_work;

    bool unsat = false;
    uint32_t max_test = MIN(nc, 512u);
    for (uint32_t i = 0; i < max_test && s->work < end_work && !unsat && !s->error && !s->interrupted; ++i) {
        Var v = cands[i].v;
        if (s->values[v] != UNDEF) continue;
        /* Pick likely-TRUE polarity from best_phase / saved polarity, and probe its NEGATION! */
        bool likely_true = s->vars[v].polarity;
        if (s->rephase.best_phase && s->rephase.best_phase[v] != UNDEF) {
            likely_true = (s->rephase.best_phase[v] == TRUE);
        }
        Lit probe = mkLit(v, likely_true); /* If likely_true, probe ~v (sign=1); else probe v (sign=0) */
        uint32_t root_trail = s->trail_size;
        s->decision_level = 1;
        s->trail_lims[1] = root_trail;
        s->values[v] = (uint8_t)(2u - sign(probe));
        s->vars[v].level = 1;
        s->vars[v].reason = INVALID_CLAUSE;
        s->binary_reasons[v] = LIT_UNDEF;
        s->vars[v].trail_pos = root_trail;
        s->trail[s->trail_size++].lit = probe;

        CRef confl = solver_propagate(s);
        solver_backtrack(s, 0);
        if (s->error || s->interrupted || s->work >= end_work) break;
        if (confl != INVALID_CLAUSE) {
            Lit unit = neg(probe);
            proof_add_clause(s, &unit, 1);
            if (s->error || !enqueue_root_unit(s, unit) || !root_propagate(s)) {
                unsat = true; break;
            }
        }
    }

    s->opts.chrono = saved_chrono;
    s->opts.phase_saving = saved_phase;
    s->work_limit = saved_wl;
    free(cands);
    return !unsat && !s->error;
}

/* =====================================================================
 * Pass 7B: Transitive Reduction of the Binary Implication Graph
 *
 * Deletes transitively redundant binary clauses (RUP-valid!) and catches
 * failed literals (u => ... => ~u) in O(1) bounded BFS per literal.
 * ===================================================================== */

typedef struct {
    Lit to;
    CRef cref;
    uint8_t learned;
} BinEdge;

static bool inproc_transitive(Solver *s, uint64_t tick_budget) {
    if (!s->num_vars || s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;

    uint32_t nl = 2 * (s->num_vars + 1);
    uint32_t *head = calloc((size_t)nl + 1, sizeof *head);
    if (!head) { s->error = true; return false; }

    /* Count binary clauses (irredundant first, then learned). */
    uint32_t n_edges = 0;
    for (uint32_t pass = 0; pass < 2; ++pass) {
        CRef *arr = pass ? s->learnts : s->clauses;
        uint32_t cnt = pass ? s->num_learnts : s->num_clauses;
        for (uint32_t i = 0; i < cnt; ++i) {
            CRef cr = arr[i];
            if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
            if (CLAUSE_SIZE(s->arena, cr) != 2) continue;
            const Lit *ls = CLAUSE_LITS(s->arena, cr);
            if (s->values[var(ls[0])] != UNDEF || s->values[var(ls[1])] != UNDEF) continue;
            /* Clause (a | b) gives edges ~a -> b and ~b -> a. */
            head[neg(ls[0]) + 1]++;
            head[neg(ls[1]) + 1]++;
            n_edges += 2;
        }
    }
    if (n_edges < 4) { free(head); return true; }

    for (uint32_t l = 1; l <= nl; ++l) head[l] += head[l - 1];
    BinEdge *edges = malloc((size_t)n_edges * sizeof *edges);
    uint32_t *cur = malloc((size_t)(nl + 1) * sizeof *cur);
    uint32_t *seen_any = calloc(nl, sizeof *seen_any);
    uint32_t *seen_irr = calloc(nl, sizeof *seen_irr);
    Lit *bfs_q = malloc(128 * sizeof *bfs_q);
    uint8_t *bfs_irr = malloc(128 * sizeof *bfs_irr);
    if (!edges || !cur || !seen_any || !seen_irr || !bfs_q || !bfs_irr) {
        free(head); free(edges); free(cur); free(seen_any); free(seen_irr); free(bfs_q); free(bfs_irr);
        s->error = true; return false;
    }
    memcpy(cur, head, (size_t)(nl + 1) * sizeof *cur);

    for (uint32_t pass = 0; pass < 2; ++pass) {
        CRef *arr = pass ? s->learnts : s->clauses;
        uint32_t cnt = pass ? s->num_learnts : s->num_clauses;
        for (uint32_t i = 0; i < cnt; ++i) {
            CRef cr = arr[i];
            if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
            if (CLAUSE_SIZE(s->arena, cr) != 2) continue;
            const Lit *ls = CLAUSE_LITS(s->arena, cr);
            if (s->values[var(ls[0])] != UNDEF || s->values[var(ls[1])] != UNDEF) continue;
            edges[cur[neg(ls[0])]++] = (BinEdge){ls[1], cr, (uint8_t)pass};
            edges[cur[neg(ls[1])]++] = (BinEdge){ls[0], cr, (uint8_t)pass};
        }
    }
    free(cur);

    uint64_t end_work = s->work + tick_budget;
    uint32_t stamp = 0;
    bool deleted_any = false, unsat = false;
    uint32_t units_found = 0;

    for (Lit u = 2; u < nl && s->work < end_work && !unsat && !s->error && !s->interrupted; ++u) {
        if (s->values[var(u)] != UNDEF) continue;
        uint32_t bgn = head[u], end = head[u + 1];
        if (end - bgn < 2) continue;
        if (++stamp == 0) {
            memset(seen_any, 0, nl * sizeof *seen_any);
            memset(seen_irr, 0, nl * sizeof *seen_irr);
            stamp = 1;
        }
        seen_any[u] = stamp;
        seen_irr[u] = stamp;

        /* Edges are stored irredundant-first! Process direct edges of u one by one. */
        bool failed = false;
        for (uint32_t ei = bgn; ei < end && s->work < end_work; ++ei) {
            BinEdge e = edges[ei];
            if (clause_deleted(s->arena, e.cref)) continue;
            Lit target = e.to;
            if (s->values[var(target)] != UNDEF) continue;

            /* If target is already reachable without using edge e, e is transitively redundant! */
            bool red = e.learned ? (seen_any[target] == stamp) : (seen_irr[target] == stamp);
            if (red) {
                proof_delete_clause(s, CLAUSE_LITS(s->arena, e.cref), 2);
                arena_delete(s->arena, e.cref);
                s->stats.subsumed_clauses++;
                deleted_any = true;
                continue;
            }

            /* Keep edge e and expand bounded BFS from target (<= 64 pops). */
            uint32_t qh = 0, qt = 0;
            uint8_t start_irr = e.learned ? 0u : 1u;
            if (seen_any[target] != stamp) {
                seen_any[target] = stamp;
                if (start_irr) seen_irr[target] = stamp;
                bfs_q[qt] = target; bfs_irr[qt] = start_irr; qt++;
            } else if (start_irr && seen_irr[target] != stamp) {
                seen_irr[target] = stamp;
                bfs_q[qt] = target; bfs_irr[qt] = 1u; qt++;
            }
            if (target == neg(u)) { failed = true; break; }

            while (qh < qt && qt < 64) {
                Lit cur_l = bfs_q[qh];
                uint8_t cur_irr = bfs_irr[qh];
                qh++;
                uint32_t nb = head[cur_l], ne = head[cur_l + 1];
                uint32_t limit = (ne - nb > 32) ? nb + 32 : ne;
                for (uint32_t j = nb; j < limit; ++j) {
                    s->work++;
                    BinEdge ne_edge = edges[j];
                    if (clause_deleted(s->arena, ne_edge.cref)) continue;
                    Lit nxt = ne_edge.to;
                    if (nxt == neg(u)) { failed = true; break; }
                    if (s->values[var(nxt)] != UNDEF) continue;
                    uint8_t nxt_irr = (cur_irr && !ne_edge.learned) ? 1u : 0u;
                    if (seen_any[nxt] != stamp) {
                        seen_any[nxt] = stamp;
                        if (nxt_irr) seen_irr[nxt] = stamp;
                        if (qt < 64) { bfs_q[qt] = nxt; bfs_irr[qt] = nxt_irr; qt++; }
                    } else if (nxt_irr && seen_irr[nxt] != stamp) {
                        seen_irr[nxt] = stamp;
                        if (qt < 64) { bfs_q[qt] = nxt; bfs_irr[qt] = 1u; qt++; }
                    }
                }
                if (failed) break;
            }
            if (failed) break;
        }
        if (failed) {
            Lit unit = neg(u);
            proof_add_clause(s, &unit, 1);
            if (s->error || !enqueue_root_unit(s, unit) || !root_propagate(s)) {
                unsat = true; break;
            }
            units_found++;
        }
    }

    free(head); free(edges); free(seen_any); free(seen_irr); free(bfs_q); free(bfs_irr);
    if (unsat || s->error) return false;
    if (deleted_any || units_found) {
        solver_rebuild_watches(s);
        if (s->error) return false;
        if (units_found) return inproc_root_clean(s);
    }
    return true;
}

/* =====================================================================
 * Pass 7C: Embedded Mini-Kitten CDCL SAT Sweeper (DRAT/RUP-Sound)
 *
 * Extracts local COI neighborhoods (<= 192 vars, <= 768 irredundant clauses),
 * generates 16 exact satisfying models via random-phase Kitten solves to
 * partition variables by exact semantic signatures, and proves backbones &
 * equivalences with bounded CDCL + counterexample signature refinement.
 * Every proved unit/binary is backed by Kitten's RUP proof lemmas!
 * ===================================================================== */

enum {
    KITTEN_MAX_VARS    = 256,
    KITTEN_MAX_LITS    = 2 * (KITTEN_MAX_VARS + 1),
    KITTEN_MAX_ORIG    = 1024,
    KITTEN_MAX_CLS     = 3072,
    KITTEN_POOL_CAP    = 65536
};
#define KITTEN_NO_CLAUSE UINT32_MAX

typedef struct {
    uint32_t off;
    uint32_t size;
    uint8_t learned;
} KClause;

typedef struct {
    uint32_t cidx;
    uint16_t blocker;
} KWatch;

typedef struct {
    uint32_t nv;
    Var l2g[KITTEN_MAX_VARS + 1];
    uint32_t num_orig, num_cls, proof_emitted;
    uint32_t pool_used;
    uint16_t pool[KITTEN_POOL_CAP];
    KClause cls[KITTEN_MAX_CLS];

    /* Watches (dynamic arrays per local literal) */
    KWatch *wdata[KITTEN_MAX_LITS];
    uint32_t wsize[KITTEN_MAX_LITS];
    uint32_t wcap[KITTEN_MAX_LITS];

    /* CDCL state */
    int8_t val[KITTEN_MAX_VARS + 1];       /* 0=UNDEF, 1=TRUE, -1=FALSE */
    uint32_t level[KITTEN_MAX_VARS + 1];
    uint32_t reason[KITTEN_MAX_VARS + 1];
    uint8_t seen[KITTEN_MAX_VARS + 1];
    uint8_t phase[KITTEN_MAX_VARS + 1];
    uint32_t stamp[KITTEN_MAX_VARS + 1];
    uint32_t bump_clock;

    uint16_t trail[KITTEN_MAX_VARS + 2];
    uint32_t trail_lim[KITTEN_MAX_VARS + 2];
    uint32_t trail_sz, qhead, dlevel;
    uint32_t rng;
    bool unsat0;
    bool oom;
} Kitten;

static inline int8_t k_lit_val(const Kitten *k, uint16_t l) {
    int8_t v = k->val[l >> 1];
    return (l & 1u) ? (int8_t)-v : v;
}

static bool k_watch_push(Kitten *k, uint16_t l, uint32_t cidx, uint16_t blocker) {
    if (k->wsize[l] == k->wcap[l]) {
        uint32_t nc = k->wcap[l] ? k->wcap[l] * 2 : 8;
        KWatch *p = realloc(k->wdata[l], (size_t)nc * sizeof *p);
        if (!p) { k->oom = true; return false; }
        k->wdata[l] = p;
        k->wcap[l] = nc;
    }
    k->wdata[l][k->wsize[l]++] = (KWatch){cidx, blocker};
    return true;
}

static void k_init(Kitten *k, uint32_t seed) {
    memset(k, 0, sizeof *k);
    k->rng = seed ? seed : 0x12345678u;
}

static void k_free(Kitten *k) {
    for (uint32_t l = 0; l < KITTEN_MAX_LITS; ++l) free(k->wdata[l]);
}

static void k_assign(Kitten *k, uint16_t l, uint32_t r) {
    uint32_t v = l >> 1;
    k->val[v] = (l & 1u) ? (int8_t)-1 : (int8_t)1;
    k->level[v] = k->dlevel;
    k->reason[v] = r;
    k->phase[v] = (uint8_t)(l & 1u);
    k->trail[k->trail_sz++] = l;
}

static void k_backtrack(Kitten *k, uint32_t target) {
    if (k->dlevel <= target) return;
    uint32_t lim = k->trail_lim[target + 1];
    while (k->trail_sz > lim) {
        uint16_t l = k->trail[--k->trail_sz];
        k->val[l >> 1] = 0;
    }
    k->qhead = k->trail_sz;
    k->dlevel = target;
}

static bool k_add_clause(Kitten *k, const uint16_t *ls, uint32_t sz, bool learned) {
    if (sz == 0) { k->unsat0 = true; return true; }
    if (sz == 1) {
        int8_t v = k_lit_val(k, ls[0]);
        if (v < 0) { k->unsat0 = true; return true; }
        if (v == 0) {
            if (k->num_cls < KITTEN_MAX_CLS && k->pool_used + 1 <= KITTEN_POOL_CAP) {
                uint32_t cidx = k->num_cls++;
                k->cls[cidx] = (KClause){k->pool_used, 1, (uint8_t)learned};
                k->pool[k->pool_used++] = ls[0];
            }
            k_assign(k, ls[0], KITTEN_NO_CLAUSE);
        }
        return true;
    }
    if (k->num_cls >= KITTEN_MAX_CLS || k->pool_used + sz > KITTEN_POOL_CAP) return false;
    uint32_t cidx = k->num_cls++;
    uint32_t off = k->pool_used;
    memcpy(k->pool + off, ls, (size_t)sz * sizeof(uint16_t));
    k->pool_used += sz;
    k->cls[cidx] = (KClause){off, sz, (uint8_t)learned};
    if (!k_watch_push(k, ls[0], cidx, ls[1]) || !k_watch_push(k, ls[1], cidx, ls[0])) return false;
    return true;
}

static uint32_t k_propagate(Kitten *k) {
    while (k->qhead < k->trail_sz) {
        uint16_t p = k->trail[k->qhead++];
        uint16_t fl = p ^ 1u;
        KWatch *ws = k->wdata[fl];
        uint32_t n = k->wsize[fl], out = 0, i = 0;
        while (i < n) {
            KWatch w = ws[i++];
            if (k_lit_val(k, w.blocker) > 0) { ws[out++] = w; continue; }
            KClause *c = &k->cls[w.cidx];
            uint16_t *ls = k->pool + c->off;
            if (ls[0] == fl) { ls[0] = ls[1]; ls[1] = fl; }
            uint16_t other = ls[0];
            if (k_lit_val(k, other) > 0) {
                ws[out++] = (KWatch){w.cidx, other};
                continue;
            }
            bool found = false;
            for (uint32_t j = 2; j < c->size; ++j) {
                if (k_lit_val(k, ls[j]) >= 0) {
                    ls[1] = ls[j]; ls[j] = fl;
                    k_watch_push(k, ls[1], w.cidx, other);
                    found = true;
                    break;
                }
            }
            if (found) continue;
            ws[out++] = (KWatch){w.cidx, other};
            if (k_lit_val(k, other) < 0) {
                while (i < n) ws[out++] = ws[i++];
                k->wsize[fl] = out;
                return w.cidx;
            }
            k_assign(k, other, w.cidx);
        }
        k->wsize[fl] = out;
    }
    return KITTEN_NO_CLAUSE;
}

/* Returns: 1 = SAT, -1 = UNSAT, 0 = budget/unknown. */
static int k_solve(Kitten *k, const uint16_t *assumps, uint32_t na,
                   uint32_t max_conflicts, bool random_phases, uint64_t *work_acc) {
    if (k->unsat0) return -1;
    k_backtrack(k, 0);
    if (k_propagate(k) != KITTEN_NO_CLAUSE) { k->unsat0 = true; return -1; }

    uint16_t learnt[KITTEN_MAX_VARS + 2];
    uint32_t conflicts = 0;

    for (;;) {
        uint32_t confl = k_propagate(k);
        *work_acc += 4;
        if (confl != KITTEN_NO_CLAUSE) {
            if (k->dlevel == 0) { k->unsat0 = true; return -1; }
            if (k->dlevel <= na) {
                /* Conflict derived purely under assumptions (via RUP propagation!). */
                k_backtrack(k, 0);
                return -1;
            }
            if (++conflicts >= max_conflicts || k->num_cls + 2 >= KITTEN_MAX_CLS) {
                k_backtrack(k, 0);
                return 0;
            }
            /* 1-UIP conflict analysis */
            uint32_t lsz = 1, pathC = 0;
            uint32_t idx = k->trail_sz - 1;
            uint32_t cidx = confl;
            uint16_t p = 0;
            do {
                KClause *c = &k->cls[cidx];
                const uint16_t *ls = k->pool + c->off;
                uint32_t start = (cidx == confl) ? 0u : 1u;
                for (uint32_t i = start; i < c->size; ++i) {
                    uint16_t q = ls[i];
                    uint32_t qv = q >> 1;
                    if (!k->seen[qv] && k->level[qv] > 0) {
                        k->seen[qv] = 1;
                        k->stamp[qv] = ++k->bump_clock;
                        if (k->level[qv] == k->dlevel) pathC++;
                        else learnt[lsz++] = q;
                    }
                }
                while (!k->seen[k->trail[idx] >> 1]) idx--;
                p = k->trail[idx--];
                k->seen[p >> 1] = 0;
                cidx = k->reason[p >> 1];
                pathC--;
            } while (pathC > 0);
            learnt[0] = p ^ 1u;
            for (uint32_t i = 1; i < lsz; ++i) k->seen[learnt[i] >> 1] = 0;

            uint32_t bt = 0;
            if (lsz > 1) {
                uint32_t best = 1;
                bt = k->level[learnt[1] >> 1];
                for (uint32_t i = 2; i < lsz; ++i) {
                    if (k->level[learnt[i] >> 1] > bt) {
                        bt = k->level[learnt[i] >> 1];
                        best = i;
                    }
                }
                uint16_t t = learnt[1]; learnt[1] = learnt[best]; learnt[best] = t;
            }
            k_backtrack(k, bt);
            if (!k_add_clause(k, learnt, lsz, true)) { k_backtrack(k, 0); return 0; }
            if (lsz > 1) k_assign(k, learnt[0], k->num_cls - 1);
            continue;
        }

        /* Assign next assumption if any remain. */
        if (k->dlevel < na) {
            uint16_t a = assumps[k->dlevel];
            int8_t av = k_lit_val(k, a);
            if (av < 0) { k_backtrack(k, 0); return -1; }
            k->dlevel++;
            k->trail_lim[k->dlevel] = k->trail_sz;
            if (av == 0) k_assign(k, a, KITTEN_NO_CLAUSE);
            continue;
        }

        /* Pick next unassigned decision variable (highest bump stamp or random). */
        uint32_t best_v = 0;
        if (random_phases && conflicts == 0) {
            uint32_t start = 1u + (bsat_random(&k->rng) % k->nv);
            for (uint32_t t = 0; t < k->nv; ++t) {
                uint32_t v = 1u + ((start - 1u + t) % k->nv);
                if (k->val[v] == 0) { best_v = v; break; }
            }
        } else {
            uint32_t best_st = 0;
            for (uint32_t v = 1; v <= k->nv; ++v) {
                if (k->val[v] == 0 && (best_v == 0 || k->stamp[v] > best_st)) {
                    best_v = v; best_st = k->stamp[v];
                }
            }
        }
        if (best_v == 0) return 1; /* All local variables assigned -> SAT! */
        uint8_t sgn = random_phases ? (uint8_t)(bsat_random(&k->rng) & 1u) : k->phase[best_v];
        k->dlevel++;
        k->trail_lim[k->dlevel] = k->trail_sz;
        k_assign(k, (uint16_t)((best_v << 1) | sgn), KITTEN_NO_CLAUSE);
    }
}

static void k_flush_proof_lemmas(Solver *s, Kitten *k) {
    if (!s->proof_file && !s->proof_journal) { k->proof_emitted = k->num_cls; return; }
    Lit gbuf[KITTEN_MAX_VARS + 2];
    while (k->proof_emitted < k->num_cls && !s->error) {
        KClause *c = &k->cls[k->proof_emitted++];
        if (!c->learned) continue;
        const uint16_t *ls = k->pool + c->off;
        for (uint32_t j = 0; j < c->size; ++j) {
            gbuf[j] = mkLit(k->l2g[ls[j] >> 1], (ls[j] & 1u) != 0);
        }
        proof_add_clause(s, gbuf, c->size);
    }
}

static void k_delete_proof_lemmas(Solver *s, Kitten *k) {
    if (!s->proof_file) return;
    Lit gbuf[KITTEN_MAX_VARS + 2];
    for (uint32_t idx = k->proof_emitted; idx > k->num_orig && !s->error; --idx) {
        KClause *c = &k->cls[idx - 1];
        if (!c->learned || c->size <= 1) continue; /* Never delete emitted units! */
        const uint16_t *ls = k->pool + c->off;
        for (uint32_t j = 0; j < c->size; ++j) {
            gbuf[j] = mkLit(k->l2g[ls[j] >> 1], (ls[j] & 1u) != 0);
        }
        proof_delete_clause(s, gbuf, c->size);
    }
}

static bool inproc_sweep(Solver *s, uint64_t tick_budget) {
    if (!s->num_vars || s->num_vars > 200000 || !s->num_clauses || s->num_clauses > 300000 ||
        s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;

    /* Build compact irredundant variable->clause CSR (clauses of size 2..10). */
    uint32_t nv = s->num_vars;
    uint32_t *vhead = calloc((size_t)nv + 2, sizeof *vhead);
    uint16_t *g2l = calloc((size_t)nv + 1, sizeof *g2l);
    uint8_t *swept = calloc((size_t)nv + 1, 1);
    if (!vhead || !g2l || !swept) { free(vhead); free(g2l); free(swept); s->error = true; return false; }

    uint32_t total_occ = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 2 || sz > 10) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool skip = false;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { skip = true; break; }
        }
        if (skip) continue;
        for (uint32_t j = 0; j < sz; ++j) vhead[var(ls[j]) + 1]++;
        total_occ += sz;
    }
    if (total_occ == 0) { free(vhead); free(g2l); free(swept); return true; }
    for (uint32_t v = 1; v <= nv + 1; ++v) vhead[v] += vhead[v - 1];

    uint32_t *vocc = malloc((size_t)total_occ * sizeof *vocc);
    uint32_t *vcur = malloc((size_t)(nv + 2) * sizeof *vcur);
    uint8_t *cls_used = calloc(s->num_clauses, 1);
    uint32_t *used_cls_list = malloc(KITTEN_MAX_ORIG * sizeof *used_cls_list);
    Kitten *k = malloc(sizeof *k);
    if (!vocc || !vcur || !cls_used || !used_cls_list || !k) {
        free(vhead); free(g2l); free(swept); free(vocc); free(vcur);
        free(cls_used); free(used_cls_list); free(k);
        s->error = true; return false;
    }
    memcpy(vcur, vhead, (size_t)(nv + 2) * sizeof *vcur);
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 2 || sz > 10) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool skip = false;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { skip = true; break; }
        }
        if (skip) continue;
        for (uint32_t j = 0; j < sz; ++j) vocc[vcur[var(ls[j])]++] = i;
    }
    free(vcur);

    uint64_t end_work = s->work + tick_budget;
    uint32_t env_count = 0;
    const uint32_t MAX_ENVS = 40;
    bool unsat = false, added_any = false;
    uint32_t seed_rng = s->random_state ^ (uint32_t)(s->inprocess_rounds * 2654435761u + 13u);

    /* Phase A: Global Gate-DAG 64-bit random simulation for cross-cone equivalence
     * discovery (key for miters & multiplier equivalence where cones are >5 hops apart). */
    if (nv <= 50000 && s->num_clauses <= 150000 && s->work < end_work) {
        typedef struct { Var out; Lit a, b; uint8_t kind; /* 1=AND, 2=XOR */ uint8_t out_neg; } SimGate;
        SimGate *sg = calloc((size_t)nv + 1u, sizeof *sg);
        uint32_t *indeg = calloc((size_t)nv + 1u, sizeof *indeg);
        if (sg && indeg) {
            uint32_t n_sg = 0;
            /* Fast extraction of 2-input AND gates (out <-> a & b) from ternary clauses. */
            for (uint32_t i = 0; i < s->num_clauses; ++i) {
                CRef cr = s->clauses[i];
                if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
                if (CLAUSE_SIZE(s->arena, cr) != 3) continue;
                const Lit *ls = CLAUSE_LITS(s->arena, cr);
                if (s->values[var(ls[0])] != UNDEF || s->values[var(ls[1])] != UNDEF ||
                    s->values[var(ls[2])] != UNDEF) continue;
                for (int oi = 0; oi < 3; ++oi) {
                    Lit out = ls[oi];
                    Var ov = var(out);
                    if (sg[ov].kind) continue;
                    Lit a = neg(ls[(oi + 1) % 3]);
                    Lit b = neg(ls[(oi + 2) % 3]);
                    if (has_binary_watch(s, neg(out), a) && has_binary_watch(s, neg(out), b)) {
                        sg[ov] = (SimGate){ov, a, b, 1u, (uint8_t)sign(out)};
                        n_sg++;
                        break;
                    }
                }
            }
            if (n_sg >= 16u) {
                /* Build fanout CSR for topological 64-bit bit-parallel simulation. */
                uint32_t *fhead = calloc((size_t)nv + 2u, sizeof *fhead);
                uint64_t *gsig = malloc(((size_t)nv + 1u) * sizeof *gsig);
                Var *tq = malloc(((size_t)nv + 1u) * sizeof *tq);
                if (fhead && gsig && tq) {
                    for (Var v = 1; v <= nv; ++v) {
                        if (sg[v].kind) {
                            fhead[var(sg[v].a) + 1u]++;
                            fhead[var(sg[v].b) + 1u]++;
                            indeg[v] = 2u;
                        }
                    }
                    for (Var v = 1; v <= nv + 1u; ++v) fhead[v] += fhead[v - 1u];
                    uint32_t *fcur = malloc(((size_t)nv + 2u) * sizeof *fcur);
                    Var *fedge = malloc((size_t)(fhead[nv + 1u] ? fhead[nv + 1u] : 1u) * sizeof *fedge);
                    if (fcur && fedge) {
                        memcpy(fcur, fhead, ((size_t)nv + 2u) * sizeof *fcur);
                        for (Var v = 1; v <= nv; ++v) {
                            if (sg[v].kind) {
                                fedge[fcur[var(sg[v].a)]++] = v;
                                fedge[fcur[var(sg[v].b)]++] = v;
                            }
                        }
                        uint32_t tqh = 0, tqt = 0;
                        for (Var v = 1; v <= nv; ++v) {
                            uint64_t rlo = bsat_random(&seed_rng);
                            uint64_t rhi = bsat_random(&seed_rng);
                            gsig[v] = (rhi << 32) | rlo;
                            if (indeg[v] == 0) tq[tqt++] = v;
                        }
                        while (tqh < tqt) {
                            Var u = tq[tqh++];
                            for (uint32_t ei = fhead[u]; ei < fhead[u + 1u]; ++ei) {
                                Var w = fedge[ei];
                                if (--indeg[w] == 0) {
                                    uint64_t sa = sign(sg[w].a) ? ~gsig[var(sg[w].a)] : gsig[var(sg[w].a)];
                                    uint64_t sb = sign(sg[w].b) ? ~gsig[var(sg[w].b)] : gsig[var(sg[w].b)];
                                    uint64_t res = (sg[w].kind == 1u) ? (sa & sb) : (sa ^ sb);
                                    gsig[w] = sg[w].out_neg ? ~res : res;
                                    tq[tqt++] = w;
                                }
                            }
                        }
                        /* Hash gate outputs by canonical 64-bit signature to find cross-cone pairs! */
                        size_t shcap = 64u;
                        while (shcap < (size_t)n_sg * 2u) shcap <<= 1;
                        Var *sht = calloc(shcap, sizeof *sht);
                        if (sht) {
                            uint32_t cross_tried = 0;
                            for (Var v = 1; v <= nv && cross_tried < 12u && s->work < end_work &&
                                                !unsat && !s->error && !s->interrupted; ++v) {
                                if (!sg[v].kind || s->values[v] != UNDEF) continue;
                                uint64_t sv = gsig[v];
                                if (sv == 0 || sv == ~UINT64_C(0)) continue;
                                uint64_t canon = (sv & 1u) ? ~sv : sv;
                                size_t pos = (size_t)((canon ^ (canon >> 33)) * UINT64_C(0xff51afd7ed558ccd)) & (shcap - 1u);
                                Var partner = 0;
                                for (;;) {
                                    if (!sht[pos]) { sht[pos] = v; break; }
                                    Var u = sht[pos];
                                    uint64_t su = gsig[u];
                                    if (su == sv || su == ~sv) { partner = u; break; }
                                    pos = (pos + 1u) & (shcap - 1u);
                                }
                                if (!partner || s->values[partner] != UNDEF) continue;
                                bool opp = (gsig[partner] != gsig[v]);
                                Lit ga = mkLit(partner, false), gb = mkLit(v, opp);
                                if (has_binary_watch(s, neg(ga), gb) && has_binary_watch(s, neg(gb), ga)) continue;

                                /* Build dual-cone Kitten environment seeded from BOTH partner and v! */
                                cross_tried++;
                                k_init(k, bsat_random(&seed_rng));
                                k->nv = 2;
                                k->l2g[1] = partner; g2l[partner] = 1;
                                k->l2g[2] = v;       g2l[v] = 2;
                                uint32_t n_ucls = 0, qh2 = 1;
                                while (qh2 <= k->nv && n_ucls < KITTEN_MAX_ORIG) {
                                    Var cur_v = k->l2g[qh2++];
                                    uint32_t ob = vhead[cur_v], oe = vhead[cur_v + 1u];
                                    if (oe - ob > 48u && qh2 > 3u) continue;
                                    for (uint32_t oi = ob; oi < oe && n_ucls < KITTEN_MAX_ORIG; ++oi) {
                                        uint32_t ci = vocc[oi];
                                        if (cls_used[ci]) continue;
                                        CRef cr = s->clauses[ci];
                                        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
                                        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
                                        const Lit *ls = CLAUSE_LITS(s->arena, cr);
                                        uint32_t need_new = 0; bool has_asgn = false;
                                        for (uint32_t j = 0; j < sz; ++j) {
                                            Var u2 = var(ls[j]);
                                            if (s->values[u2] != UNDEF) { has_asgn = true; break; }
                                            if (!g2l[u2]) need_new++;
                                        }
                                        if (has_asgn || k->nv + need_new > KITTEN_MAX_VARS) continue;
                                        uint16_t klits[12];
                                        for (uint32_t j = 0; j < sz; ++j) {
                                            Var u2 = var(ls[j]);
                                            uint16_t lv = g2l[u2];
                                            if (!lv) { lv = (uint16_t)(++k->nv); g2l[u2] = lv; k->l2g[lv] = u2; }
                                            klits[j] = (uint16_t)((lv << 1) | sign(ls[j]));
                                        }
                                        cls_used[ci] = 1;
                                        used_cls_list[n_ucls++] = ci;
                                        if (!k_add_clause(k, klits, sz, false)) break;
                                    }
                                }
                                k->num_orig = k->num_cls;
                                k->proof_emitted = k->num_cls;
                                uint16_t ka = (uint16_t)(1u << 1);
                                uint16_t kb = (uint16_t)((2u << 1) | (opp ? 1u : 0u));
                                uint16_t a1[2] = {ka, (uint16_t)(kb ^ 1u)};
                                uint16_t a2[2] = {(uint16_t)(ka ^ 1u), kb};
                                int r1 = has_binary_watch(s, neg(ga), gb) ? -1 : k_solve(k, a1, 2, 128, false, &s->work);
                                int r2 = (r1 < 0) ? (has_binary_watch(s, neg(gb), ga) ? -1 : k_solve(k, a2, 2, 128, false, &s->work)) : 0;
                                if (r1 < 0 && r2 < 0) {
                                    k_flush_proof_lemmas(s, k);
                                    if (!add_rup_binary(s, neg(ga), gb) || !add_rup_binary(s, neg(gb), ga)) {
                                        /* s->error set inside add_rup_binary */
                                    } else {
                                        added_any = true;
                                    }
                                }
                                if (k->oom) s->error = true;
                                if (!unsat && !s->error) k_delete_proof_lemmas(s, k);
                                for (uint32_t i = 0; i < n_ucls; ++i) cls_used[used_cls_list[i]] = 0;
                                for (uint32_t i = 1; i <= k->nv; ++i) g2l[k->l2g[i]] = 0;
                                k_free(k);
                            }
                            free(sht);
                        }
                    }
                    free(fcur); free(fedge);
                }
                free(fhead); free(gsig); free(tq);
            }
        }
        free(sg); free(indeg);
    }

    /* Rotate starting variable across inprocessing rounds so all cones get swept. */
    uint32_t start_v = (s->inprocess_rounds > 1 && nv > 1) ? (1u + (bsat_random(&seed_rng) % nv)) : 1u;

    for (uint32_t step = 0; step < nv && env_count < MAX_ENVS && s->work < end_work &&
                            !unsat && !s->error && !s->interrupted; ++step) {
        Var root = 1u + ((start_v - 1u + step) % nv);
        if (swept[root] || s->values[root] != UNDEF || (s->elim && s->elim->eliminated[root])) continue;
        uint32_t rdeg = vhead[root + 1] - vhead[root];
        if (rdeg < 2 || rdeg > 32) continue;

        /* Extract local BFS neighborhood around root (<= KITTEN_MAX_VARS vars, <= KITTEN_MAX_ORIG clauses). */
        k_init(k, bsat_random(&seed_rng));
        k->nv = 1;
        k->l2g[1] = root;
        g2l[root] = 1;
        swept[root] = 1;

        uint32_t n_ucls = 0;
        uint32_t qh = 1;
        while (qh <= k->nv && n_ucls < KITTEN_MAX_ORIG) {
            Var cur_v = k->l2g[qh++];
            uint32_t ob = vhead[cur_v], oe = vhead[cur_v + 1];
            if (oe - ob > 48 && qh > 2) continue; /* Skip high-fanout hub vars on deeper hops */
            for (uint32_t oi = ob; oi < oe && n_ucls < KITTEN_MAX_ORIG; ++oi) {
                uint32_t ci = vocc[oi];
                if (cls_used[ci]) continue;
                CRef cr = s->clauses[ci];
                if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
                uint32_t sz = CLAUSE_SIZE(s->arena, cr);
                const Lit *ls = CLAUSE_LITS(s->arena, cr);
                /* Check if adding this clause fits in KITTEN_MAX_VARS. */
                uint32_t need_new = 0;
                bool has_assigned = false;
                for (uint32_t j = 0; j < sz; ++j) {
                    Var u = var(ls[j]);
                    if (s->values[u] != UNDEF) { has_assigned = true; break; }
                    if (!g2l[u]) need_new++;
                }
                if (has_assigned || k->nv + need_new > KITTEN_MAX_VARS) continue;
                uint16_t klits[12];
                for (uint32_t j = 0; j < sz; ++j) {
                    Var u = var(ls[j]);
                    uint16_t lv = g2l[u];
                    if (!lv) {
                        lv = (uint16_t)(++k->nv);
                        g2l[u] = lv;
                        k->l2g[lv] = u;
                        swept[u] = 1;
                    }
                    klits[j] = (uint16_t)((lv << 1) | sign(ls[j]));
                }
                cls_used[ci] = 1;
                used_cls_list[n_ucls++] = ci;
                if (!k_add_clause(k, klits, sz, false)) break;
            }
        }
        k->num_orig = k->num_cls;
        k->proof_emitted = k->num_cls;

        if (k->nv < 4 || n_ucls < 4) {
            for (uint32_t i = 0; i < n_ucls; ++i) cls_used[used_cls_list[i]] = 0;
            for (uint32_t i = 1; i <= k->nv; ++i) g2l[k->l2g[i]] = 0;
            k_free(k);
            continue;
        }
        env_count++;

        /* Step 1: Generate up to 16 exact satisfying models of the local environment! */
        uint64_t sig[KITTEN_MAX_VARS + 1];
        memset(sig, 0, sizeof sig);
        uint32_t n_models = 0;
        bool env_unsat = false;
        for (uint32_t m = 0; m < 16 && s->work < end_work; ++m) {
            int r = k_solve(k, NULL, 0, 128, true, &s->work);
            if (r < 0) { env_unsat = true; break; }
            if (r == 0) break;
            uint64_t bit = UINT64_C(1) << n_models;
            for (uint32_t lv = 1; lv <= k->nv; ++lv) {
                if (k->val[lv] > 0) sig[lv] |= bit;
            }
            n_models++;
        }

        if (env_unsat) {
            k_flush_proof_lemmas(s, k);
            proof_add_clause(s, NULL, 0);
            s->base_unsat = true; s->result = FALSE;
            unsat = true;
        } else if (n_models >= 4) {
            uint64_t full_mask = (n_models == 64) ? ~UINT64_C(0) : ((UINT64_C(1) << n_models) - 1u);
            uint32_t next_bit = n_models;

            /* Step 2: Check constant backbone candidates (sig == 0 or sig == full_mask). */
            for (uint32_t lv = 1; lv <= k->nv && s->work < end_work && !unsat && !s->error; ++lv) {
                uint64_t sv = sig[lv] & full_mask;
                if (sv != 0 && sv != full_mask) continue;
                bool expect_true = (sv == full_mask);
                uint16_t neg_cand = (uint16_t)((lv << 1) | (expect_true ? 1u : 0u));
                int r = k_solve(k, &neg_cand, 1, 96, false, &s->work);
                if (r > 0) {
                    /* Counterexample! Refine ALL local signatures with this real model! */
                    uint32_t b = (next_bit < 64) ? next_bit++ : (bsat_random(&seed_rng) & 63u);
                    uint64_t bmask = UINT64_C(1) << b;
                    full_mask |= bmask;
                    for (uint32_t u = 1; u <= k->nv; ++u) {
                        if (k->val[u] > 0) sig[u] |= bmask;
                        else sig[u] &= ~bmask;
                    }
                } else if (r < 0) {
                    /* Proved backbone unit! */
                    Lit unit = mkLit(k->l2g[lv], !expect_true);
                    k_flush_proof_lemmas(s, k);
                    proof_add_clause(s, &unit, 1);
                    if (s->error || !enqueue_root_unit(s, unit) || !root_propagate(s)) {
                        unsat = true; break;
                    }
                    uint16_t ku = neg_cand ^ 1u;
                    k_add_clause(k, &ku, 1, false);
                    added_any = true;
                }
            }

            /* Step 3: Check equivalence pairs (lv1 <-> lv2 or lv1 <-> ~lv2) with matching signatures! */
            uint32_t pair_probes = 0;
            for (uint32_t a = 1; a <= k->nv && pair_probes < 48 && s->work < end_work &&
                                 !unsat && !s->error; ++a) {
                if (s->values[k->l2g[a]] != UNDEF) continue;
                uint64_t sa = sig[a] & full_mask;
                if (sa == 0 || sa == full_mask) continue;
                for (uint32_t b = a + 1; b <= k->nv && pair_probes < 48 && s->work < end_work &&
                                     !unsat && !s->error; ++b) {
                    if (s->values[k->l2g[b]] != UNDEF) continue;
                    uint64_t sb = sig[b] & full_mask;
                    bool same = (sa == sb);
                    bool opp  = (sa == (sb ^ full_mask));
                    if (!same && !opp) continue;

                    uint16_t ka = (uint16_t)(a << 1);
                    uint16_t kb = (uint16_t)((b << 1) | (opp ? 1u : 0u));
                    Lit ga = mkLit(k->l2g[a], false);
                    Lit gb = mkLit(k->l2g[b], opp);
                    if (has_binary_watch(s, neg(ga), gb) && has_binary_watch(s, neg(gb), ga)) continue;

                    pair_probes++;
                    /* Test direction 1: {ka, ~kb} -> proves (~ga | gb). */
                    uint16_t a1[2] = {ka, (uint16_t)(kb ^ 1u)};
                    int r1 = has_binary_watch(s, neg(ga), gb) ? -1 : k_solve(k, a1, 2, 96, false, &s->work);
                    if (r1 > 0) {
                        uint32_t bit = (next_bit < 64) ? next_bit++ : (bsat_random(&seed_rng) & 63u);
                        uint64_t bmask = UINT64_C(1) << bit;
                        full_mask |= bmask;
                        for (uint32_t u = 1; u <= k->nv; ++u) {
                            if (k->val[u] > 0) sig[u] |= bmask;
                            else sig[u] &= ~bmask;
                        }
                        sa = sig[a] & full_mask;
                        continue;
                    }
                    if (r1 == 0) continue;

                    /* Test direction 2: {~ka, kb} -> proves (~gb | ga). */
                    uint16_t a2[2] = {(uint16_t)(ka ^ 1u), kb};
                    int r2 = has_binary_watch(s, neg(gb), ga) ? -1 : k_solve(k, a2, 2, 96, false, &s->work);
                    if (r2 > 0) {
                        uint32_t bit = (next_bit < 64) ? next_bit++ : (bsat_random(&seed_rng) & 63u);
                        uint64_t bmask = UINT64_C(1) << bit;
                        full_mask |= bmask;
                        for (uint32_t u = 1; u <= k->nv; ++u) {
                            if (k->val[u] > 0) sig[u] |= bmask;
                            else sig[u] &= ~bmask;
                        }
                        sa = sig[a] & full_mask;
                        continue;
                    }
                    if (r2 == 0) continue;

                    /* BOTH directions UNSAT -> proved ga <-> gb!! Emit RUP proof lemmas + binaries. */
                    k_flush_proof_lemmas(s, k);
                    Lit pairs[2][2] = {{neg(ga), gb}, {neg(gb), ga}};
                    uint16_t kpairs[2][2] = {{(uint16_t)(ka ^ 1u), kb}, {(uint16_t)(kb ^ 1u), ka}};
                    for (int p = 0; p < 2 && !s->error; ++p) {
                        if (has_binary_watch(s, pairs[p][0], pairs[p][1])) continue;
                        proof_add_clause(s, pairs[p], 2);
                        if (s->error) break;
                        if (s->num_learnts == s->learnts_size) {
                            uint32_t ncap = s->learnts_size ? s->learnts_size * 2 : 64;
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
                        k_add_clause(k, kpairs[p], 2, false);
                        added_any = true;
                    }
                }
            }
        }

        if (k->oom) s->error = true;
        if (!unsat && !s->error) k_delete_proof_lemmas(s, k);
        for (uint32_t i = 0; i < n_ucls; ++i) cls_used[used_cls_list[i]] = 0;
        for (uint32_t i = 1; i <= k->nv; ++i) g2l[k->l2g[i]] = 0;
        k_free(k);
    }

    free(vhead); free(g2l); free(swept); free(vocc);
    free(cls_used); free(used_cls_list); free(k);

    if (unsat || s->error) return false;
    if (added_any && !solver_budget_exhausted(s)) {
        /* Immediately collapse newly proven equivalences via Tarjan SCC! */
        return inproc_decompose(s);
    }
    return true;
}

/* =====================================================================
 * Pass 8: Kissat-style ProbSAT Phase Seeding (Polynomial Break Weights)
 *
 * Uses precomputed ProbSAT polynomial break-count weights (1 + brk)^(-2.3)
 * with zero-break fast path to find low-energy partial assignments and
 * seed both saved polarities and Stable-mode target phases (best_phase).
 * ===================================================================== */

static bool inproc_walk_seed_phases(Solver *s, uint32_t max_flips, uint64_t tick_budget) {
    if (!s->opts.phase_saving || !s->opts.rephase || !s->rephase.best_phase) return false;
    if (s->num_vars == 0 || s->num_vars > 250000 || s->num_clauses == 0 || s->num_clauses > 700000) return false;
    if (tick_budget == 0 || solver_budget_exhausted_now(s)) return false;

    uint32_t nv = s->num_vars;
    uint32_t nc = s->num_clauses;
    uint32_t live_vars = 0, live_cls = 0;
    for (Var v = 1; v <= nv; ++v) {
        if (s->values[v] == UNDEF && (!s->elim || !s->elim->eliminated[v])) live_vars++;
    }
    if (live_vars == 0 || live_vars > 150000u) return false;
    for (uint32_t i = 0; i < nc; ++i) {
        CRef cr = s->clauses[i];
        if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr)) live_cls++;
    }
    if (live_cls == 0 || live_cls > 450000u) return false;

    uint64_t end_work = s->work + tick_budget;

    /* Precomputed ProbSAT weights: w[b] = (1 + b)^(-2.3) for b = 0..63. */
    static const double probsat_w[64] = {
        1.000000, 0.203063, 0.079914, 0.041235, 0.024675, 0.016224, 0.011386, 0.008373,
        0.006378, 0.004998, 0.004008, 0.003276, 0.002721, 0.002291, 0.001952, 0.001681,
        0.001461, 0.001280, 0.001129, 0.001003, 0.000896, 0.000805, 0.000726, 0.000658,
        0.000599, 0.000547, 0.000501, 0.000461, 0.000425, 0.000394, 0.000365, 0.000340,
        0.000317, 0.000296, 0.000277, 0.000260, 0.000244, 0.000230, 0.000217, 0.000205,
        0.000194, 0.000183, 0.000174, 0.000165, 0.000157, 0.000149, 0.000142, 0.000136,
        0.000129, 0.000124, 0.000118, 0.000113, 0.000108, 0.000104, 0.000100, 0.000096,
        0.000092, 0.000088, 0.000085, 0.000082, 0.000079, 0.000076, 0.000073, 0.000071
    };

    uint8_t *assign = malloc((size_t)(nv + 1));
    uint8_t *best_assign = malloc((size_t)(nv + 1));
    uint32_t *sat_cnt = calloc(nc, sizeof *sat_cnt);
    uint32_t *unsat_list = malloc((size_t)nc * sizeof *unsat_list);
    uint32_t *unsat_pos = malloc((size_t)nc * sizeof *unsat_pos);
    uint32_t *occ_head = calloc((size_t)(2 * (nv + 1)) + 1, sizeof *occ_head);
    if (!assign || !best_assign || !sat_cnt || !unsat_list || !unsat_pos || !occ_head) {
        free(assign); free(best_assign); free(sat_cnt); free(unsat_list); free(unsat_pos); free(occ_head);
        return false;
    }

    uint32_t nl = 2 * (nv + 1);
    uint32_t total_lits = 0;
    for (uint32_t i = 0; i < nc; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        for (uint32_t j = 0; j < sz; ++j) occ_head[ls[j] + 1]++;
        total_lits += sz;
    }
    s->work += total_lits;
    for (uint32_t l = 1; l <= nl; ++l) occ_head[l] += occ_head[l - 1];
    uint32_t *occ_clauses = malloc((size_t)(total_lits ? total_lits : 1) * sizeof *occ_clauses);
    uint32_t *cursor = malloc((size_t)(nl + 1) * sizeof *cursor);
    if (!occ_clauses || !cursor) {
        free(assign); free(best_assign); free(sat_cnt); free(unsat_list); free(unsat_pos);
        free(occ_head); free(occ_clauses); free(cursor);
        return false;
    }
    memcpy(cursor, occ_head, (size_t)(nl + 1) * sizeof *cursor);
    for (uint32_t i = 0; i < nc; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        for (uint32_t j = 0; j < sz; ++j) occ_clauses[cursor[ls[j]]++] = i;
    }
    free(cursor);

    /* Initialize assignment from target/saved polarities + root units. */
    for (Var v = 1; v <= nv; ++v) {
        if (s->values[v] != UNDEF) assign[v] = (s->values[v] == TRUE);
        else if (s->rephase.best_phase[v] != UNDEF) assign[v] = (s->rephase.best_phase[v] == TRUE);
        else assign[v] = s->vars[v].polarity ? 1 : 0;
    }

    uint32_t n_unsat = 0;
    for (uint32_t i = 0; i < nc; ++i) {
        unsat_pos[i] = UINT32_MAX;
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        uint32_t sc = 0;
        for (uint32_t j = 0; j < sz; ++j) {
            if ((assign[var(ls[j])] != 0) == !sign(ls[j])) sc++;
        }
        sat_cnt[i] = sc;
        if (sc == 0 && sz > 0) {
            unsat_pos[i] = n_unsat;
            unsat_list[n_unsat++] = i;
        }
    }

    uint32_t init_unsat = n_unsat;
    uint32_t best_unsat = n_unsat;
    memcpy(best_assign, assign, (size_t)(nv + 1));
    uint32_t rng = s->random_state ^ (uint32_t)(s->inprocess_rounds * 0x9e3779b9u + 7u);
    const uint32_t MAX_FLIPS = max_flips;
    Var cand_vars[64];
    double cand_w[64];

    for (uint32_t flip = 0; flip < MAX_FLIPS && n_unsat > 0 && s->work < end_work; ++flip) {
        if ((flip & 127u) == 0 && solver_budget_exhausted_now(s)) break;
        uint32_t ci = unsat_list[bsat_random(&rng) % n_unsat];
        CRef cr = s->clauses[ci];
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);

        /* ProbSAT selection: pick zero-break move immediately, else sample
         * proportional to (1 + break)^(-2.3) across literals in `ci`. */
        Var chosen = INVALID_VAR;
        uint32_t ncands = 0;
        double total_w = 0.0;
        uint32_t scan_sz = MIN(sz, 64u);
        for (uint32_t k = 0; k < scan_sz; ++k) {
            Var v = var(ls[k]);
            if (s->values[v] != UNDEF) continue;
            Lit true_lit = mkLit(v, !assign[v]);
            uint32_t brk = 0;
            uint32_t ob = occ_head[true_lit], oe = occ_head[true_lit + 1];
            uint32_t ostop = (oe - ob > 512u) ? (ob + 512u) : oe;
            for (uint32_t o = ob; o < ostop; ++o) {
                if (sat_cnt[occ_clauses[o]] == 1) {
                    brk++;
                    if (brk >= 63u) break;
                }
            }
            s->work += (ostop - ob) + 2u;
            if (brk == 0 && ostop == oe) { chosen = v; break; }
            double w = probsat_w[brk];
            cand_vars[ncands] = v;
            cand_w[ncands] = w;
            total_w += w;
            ncands++;
        }
        if (chosen == INVALID_VAR) {
            if (ncands == 0) break;
            double pick = ((double)(bsat_random(&rng) & 0xFFFFFFu) / (double)0x1000000u) * total_w;
            double acc = 0.0;
            chosen = cand_vars[ncands - 1u];
            for (uint32_t k = 0; k < ncands; ++k) {
                acc += cand_w[k];
                if (pick <= acc) { chosen = cand_vars[k]; break; }
            }
        }

        Lit old_true = mkLit(chosen, !assign[chosen]);
        Lit new_true = neg(old_true);
        assign[chosen] ^= 1;

        uint32_t ob0 = occ_head[old_true], oe0 = occ_head[old_true + 1];
        for (uint32_t o = ob0; o < oe0; ++o) {
            uint32_t c = occ_clauses[o];
            if (--sat_cnt[c] == 0) {
                unsat_pos[c] = n_unsat;
                unsat_list[n_unsat++] = c;
            }
        }
        uint32_t ob1 = occ_head[new_true], oe1 = occ_head[new_true + 1];
        for (uint32_t o = ob1; o < oe1; ++o) {
            uint32_t c = occ_clauses[o];
            if (sat_cnt[c]++ == 0) {
                uint32_t pos = unsat_pos[c];
                uint32_t last = unsat_list[--n_unsat];
                unsat_list[pos] = last;
                unsat_pos[last] = pos;
                unsat_pos[c] = UINT32_MAX;
            }
        }
        s->work += (oe0 - ob0) + (oe1 - ob1) + 4u;
        if (n_unsat < best_unsat) {
            best_unsat = n_unsat;
            memcpy(best_assign, assign, (size_t)(nv + 1));
        }
    }

    /* Seed both saved polarities and target phases ONLY when ProbSAT found a
     * genuinely low-energy basin (avoids scrambling initial phases on huge formulas). */
    bool updated = false;
    if (best_unsat == 0 || (best_unsat < init_unsat && best_unsat <= live_cls / 20u)) {
        updated = true;
        for (Var v = 1; v <= nv; ++v) {
            if (s->values[v] == UNDEF) {
                bool pol = (best_assign[v] != 0);
                s->vars[v].polarity = pol;
                if (s->rephase.target_phase) {
                    s->rephase.target_phase[v] = pol ? TRUE : FALSE;
                }
                if (best_unsat == 0 || s->rephase.best_phase[v] == UNDEF) {
                    s->rephase.best_phase[v] = pol ? TRUE : FALSE;
                }
            }
        }
    }

    free(assign); free(best_assign); free(sat_cnt); free(unsat_list);
    free(unsat_pos); free(occ_head); free(occ_clauses);
    return updated;
}

bool solver_walk_rephase(Solver *s, uint32_t max_flips, uint64_t tick_budget) {
    if (!s || s->decision_level || s->error || s->interrupted || s->result == FALSE) return false;
    return inproc_walk_seed_phases(s, max_flips, tick_budget);
}

/* =====================================================================
 * Pass 9: AMO Clique Extraction & Sinz Sequential-Counter Re-encoding
 *
 * On binary-dense formulas (scheduling, coloring, argumentation, battleship),
 * extracts edge-disjoint cliques of size k >= 7 in the irredundant binary
 * conflict graph (where every pair (a_i | a_j) exists, meaning at most one
 * of ~a_0, ..., ~a_{k-1} is true) and replaces the k*(k-1)/2 binary clauses
 * with an exact, arc-consistent Sinz sequential counter (k-1 auxiliary vars,
 * 3k-4 binary clauses). Guarded on non-proof, non-assumption, non-reuse mode.
 * ===================================================================== */

typedef struct {
    Lit other;
    uint32_t cslot;
} AmoEdge;

typedef struct {
    Lit lit;
    uint32_t deg;
} AmoSeed;

static int cmp_amo_edge(const void *a, const void *b) {
    Lit x = ((const AmoEdge *)a)->other;
    Lit y = ((const AmoEdge *)b)->other;
    return (x > y) - (x < y);
}

static int cmp_amo_seed(const void *a, const void *b) {
    uint32_t da = ((const AmoSeed *)a)->deg;
    uint32_t db = ((const AmoSeed *)b)->deg;
    if (da != db) return (db > da) - (db < da);
    Lit la = ((const AmoSeed *)a)->lit;
    Lit lb = ((const AmoSeed *)b)->lit;
    return (la > lb) - (la < lb);
}

static uint32_t amo_find_edge(const AmoEdge *edges, uint32_t lo, uint32_t hi, Lit target) {
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        Lit m = edges[mid].other;
        if (m < target) lo = mid + 1;
        else if (m > target) hi = mid;
        else return mid;
    }
    return UINT32_MAX;
}

bool solver_amo_cliques(Solver *s) {
    if (!s || s->decision_level || s->proof_journal || s->error ||
        s->interrupted || s->result == FALSE || s->num_vars == 0 || s->num_clauses < 500 ||
        s->num_clauses > 250000 || solver_budget_exhausted(s)) return true;

    const uint32_t orig_nv = s->num_vars;
    const uint32_t orig_nc = s->num_clauses;
    const uint32_t nl = 2u * (orig_nv + 1u);

    /* Count live irredundant binary clauses with both literals unassigned. */
    uint32_t bin_cnt = 0;
    for (uint32_t i = 0; i < orig_nc; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        if (CLAUSE_SIZE(s->arena, cr) != 2) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        if (s->values[var(ls[0])] != UNDEF || s->values[var(ls[1])] != UNDEF) continue;
        if (var(ls[0]) == var(ls[1])) continue;
        bin_cnt++;
    }
    /* Only trigger on truly binary-dominated formulas (>= 80% binary clauses,
     * e.g., AMO/scheduling/argumentation/coloring/battleship) so gate-rich
     * planning/verification formulas keep all binaries intact for BVE. */
    if (bin_cnt < 500 || (uint64_t)bin_cnt * 10ULL < (uint64_t)orig_nc * 8ULL) return true;

    uint32_t *head = calloc((size_t)nl + 1u, sizeof *head);
    uint32_t *cur = malloc(((size_t)nl + 1u) * sizeof *cur);
    uint32_t *deg = calloc(nl, sizeof *deg);
    uint8_t *used = calloc(orig_nc, 1);
    AmoEdge *edges = malloc((size_t)bin_cnt * 2u * sizeof *edges);
    if (!head || !cur || !deg || !used || !edges) {
        free(head); free(cur); free(deg); free(used); free(edges);
        s->error = true; return false;
    }

    for (uint32_t i = 0; i < orig_nc; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        if (CLAUSE_SIZE(s->arena, cr) != 2) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        if (s->values[var(ls[0])] != UNDEF || s->values[var(ls[1])] != UNDEF) continue;
        if (var(ls[0]) == var(ls[1])) continue;
        head[ls[0] + 1u]++;
        head[ls[1] + 1u]++;
    }
    for (uint32_t l = 1; l <= nl; ++l) head[l] += head[l - 1u];
    memcpy(cur, head, ((size_t)nl + 1u) * sizeof *cur);

    for (uint32_t i = 0; i < orig_nc; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        if (CLAUSE_SIZE(s->arena, cr) != 2) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        if (s->values[var(ls[0])] != UNDEF || s->values[var(ls[1])] != UNDEF) continue;
        if (var(ls[0]) == var(ls[1])) continue;
        edges[cur[ls[0]]++] = (AmoEdge){ls[1], i};
        edges[cur[ls[1]]++] = (AmoEdge){ls[0], i};
    }
    free(cur);

    /* Sort each adjacency list and deduplicate parallel binary clauses cleanly. */
    AmoSeed *seeds = malloc((size_t)nl * sizeof *seeds);
    uint32_t *mark = calloc(nl, sizeof *mark);
    uint8_t *var_in_clique = calloc((size_t)orig_nv + 1u, 1);
    Lit *cands = malloc((size_t)nl * sizeof *cands);
    Lit *next_cands = malloc((size_t)nl * sizeof *next_cands);
    if (!seeds || !mark || !var_in_clique || !cands || !next_cands) {
        free(head); free(deg); free(used); free(edges);
        free(seeds); free(mark); free(var_in_clique); free(cands); free(next_cands);
        s->error = true; return false;
    }

    uint32_t n_seeds = 0;
    for (Lit l = 2; l < nl; ++l) {
        uint32_t lo = head[l], hi = head[l + 1u];
        if (hi - lo > 1u) qsort(edges + lo, hi - lo, sizeof *edges, cmp_amo_edge);
        /* Count unique neighbors as initial active degree. */
        uint32_t udeg = 0;
        for (uint32_t e = lo; e < hi; ++e) {
            if (e == lo || edges[e].other != edges[e - 1u].other) udeg++;
        }
        deg[l] = udeg;
        if (udeg >= 6u) seeds[n_seeds++] = (AmoSeed){l, udeg};
    }

    if (n_seeds < 7u) {
        free(head); free(deg); free(used); free(edges);
        free(seeds); free(mark); free(var_in_clique); free(cands); free(next_cands);
        return true;
    }
    qsort(seeds, n_seeds, sizeof *seeds, cmp_amo_seed);

    const uint32_t MIN_CLIQUE = 7u;
    const uint32_t MAX_CLIQUE = 128u;
    const uint32_t MAX_AUX_VARS = 4096u;
    uint32_t aux_added = 0;
    uint32_t cliques_encoded = 0;
    uint32_t stamp = 0;
    Lit clique[MAX_CLIQUE];

    for (uint32_t si = 0; si < n_seeds && aux_added < MAX_AUX_VARS && !s->error && !s->interrupted; ++si) {
        if ((si & 63u) == 0 && solver_budget_exhausted(s)) break;
        Lit u = seeds[si].lit;
        if (deg[u] < MIN_CLIQUE - 1u) continue;

        /* Repeat on seed u while it can still form edge-disjoint cliques of size >= MIN_CLIQUE. */
        while (deg[u] >= MIN_CLIQUE - 1u && aux_added < MAX_AUX_VARS && !s->error) {
            uint32_t nc = 0;
            uint32_t lo = head[u], hi = head[u + 1u];
            if (++stamp == 0) { memset(mark, 0, (size_t)nl * sizeof *mark); stamp = 1; }
            for (uint32_t e = lo; e < hi; ++e) {
                if (used[edges[e].cslot]) continue;
                Lit w = edges[e].other;
                if (deg[w] < MIN_CLIQUE - 1u || var(w) == var(u) || mark[w] == stamp) continue;
                mark[w] = stamp;
                cands[nc++] = w;
            }
            if (nc < MIN_CLIQUE - 1u) break;

            uint32_t k = 1;
            clique[0] = u;
            var_in_clique[var(u)] = 1;

            while (nc > 0 && k < MAX_CLIQUE) {
                /* Pick candidate w in cands with highest active degree (tie-break by lit). */
                Lit best_w = cands[0];
                uint32_t best_d = deg[best_w];
                for (uint32_t i = 1; i < nc; ++i) {
                    Lit w = cands[i];
                    if (deg[w] > best_d) { best_d = deg[w]; best_w = w; }
                }
                if (var_in_clique[var(best_w)]) {
                    /* Remove best_w and continue. */
                    uint32_t nnc = 0;
                    for (uint32_t i = 0; i < nc; ++i) {
                        if (var(cands[i]) != var(best_w)) next_cands[nnc++] = cands[i];
                    }
                    memcpy(cands, next_cands, (size_t)nnc * sizeof(Lit));
                    nc = nnc;
                    continue;
                }
                clique[k++] = best_w;
                var_in_clique[var(best_w)] = 1;

                /* Intersect cands with active neighbors of best_w. */
                if (++stamp == 0) { memset(mark, 0, (size_t)nl * sizeof *mark); stamp = 1; }
                uint32_t wlo = head[best_w], whi = head[best_w + 1u];
                for (uint32_t e = wlo; e < whi; ++e) {
                    if (used[edges[e].cslot]) continue;
                    mark[edges[e].other] = stamp;
                }
                uint32_t nnc = 0;
                for (uint32_t i = 0; i < nc; ++i) {
                    Lit c = cands[i];
                    if (c != best_w && !var_in_clique[var(c)] && mark[c] == stamp) {
                        next_cands[nnc++] = c;
                    }
                }
                memcpy(cands, next_cands, (size_t)nnc * sizeof(Lit));
                nc = nnc;
                s->work += (whi - wlo) + nc;
                if (k + nc < MIN_CLIQUE) break;
            }

            for (uint32_t i = 0; i < k; ++i) var_in_clique[var(clique[i])] = 0;
            if (k < MIN_CLIQUE || aux_added + (k - 1u) > MAX_AUX_VARS) break;

            /* Re-encode clique[0..k-1] with Sinz sequential counter (k-1 aux vars, 3k-4 binaries). */
            if (!s->factor_original_vars) s->factor_original_vars = s->num_vars;
            Var sv[MAX_CLIQUE];
            bool ok = true;
            for (uint32_t i = 0; i < k - 1u; ++i) {
                sv[i] = solver_new_var(s);
                if (sv[i] == INVALID_VAR || s->error) { ok = false; break; }
                aux_added++;
            }
            if (!ok) break;

            /* Emit all 3k-4 binary clauses of the Sinz sequential counter.
             * In --proof mode, also emit temporary backward RAT definitions
             * (~sv[0] | ~clique[0]) and (~sv[i] | sv[i-1] | ~clique[i]) so every
             * forward clause is RAT (pivot at index 0) and every AMO blocking clause
             * (clique[i] | ~sv[i-1]) is 1-step RUP from the original clique! */
            Lit bin[2];
            if (s->proof_file) {
                Lit bdef0[2] = {mkLit(sv[0], true), neg(clique[0])};
                proof_add_clause(s, bdef0, 2);
                if (s->error) break;
            }
            bin[0] = mkLit(sv[0], false); bin[1] = clique[0];
            proof_add_clause(s, bin, 2);
            if (s->error) break;
            CRef cr0 = arena_alloc(s->arena, bin, 2, false);
            if (cr0 == INVALID_CLAUSE || !push_original_clause_ref(s, cr0)) { s->error = true; break; }

            for (uint32_t i = 1; i < k - 1u && !s->error; ++i) {
                /* 1. AMO blocking clause (clique[i] | ~sv[i-1]) — 100% RUP! */
                Lit c3[2] = {clique[i], mkLit(sv[i - 1u], true)};
                proof_add_clause(s, c3, 2);
                if (s->error) break;
                /* 2. Introduce fresh sv[i] via backward RAT def + 2 forward RAT binaries! */
                if (s->proof_file) {
                    Lit bdef[3] = {mkLit(sv[i], true), mkLit(sv[i - 1u], false), neg(clique[i])};
                    proof_add_clause(s, bdef, 3);
                    if (s->error) break;
                }
                Lit c1[2] = {mkLit(sv[i], false), clique[i]};
                Lit c2[2] = {mkLit(sv[i], false), mkLit(sv[i - 1u], true)};
                proof_add_clause(s, c1, 2);
                proof_add_clause(s, c2, 2);
                if (s->error) break;
                CRef r1 = arena_alloc(s->arena, c1, 2, false);
                CRef r2 = (r1 != INVALID_CLAUSE) ? arena_alloc(s->arena, c2, 2, false) : INVALID_CLAUSE;
                CRef r3 = (r2 != INVALID_CLAUSE) ? arena_alloc(s->arena, c3, 2, false) : INVALID_CLAUSE;
                if (r1 == INVALID_CLAUSE || r2 == INVALID_CLAUSE || r3 == INVALID_CLAUSE ||
                    !push_original_clause_ref(s, r1) ||
                    !push_original_clause_ref(s, r2) ||
                    !push_original_clause_ref(s, r3)) {
                    s->error = true; break;
                }
            }
            if (s->error) break;
            bin[0] = clique[k - 1u]; bin[1] = mkLit(sv[k - 2u], true);
            proof_add_clause(s, bin, 2);
            if (s->error) break;
            CRef cr_last = arena_alloc(s->arena, bin, 2, false);
            if (cr_last == INVALID_CLAUSE || !push_original_clause_ref(s, cr_last)) { s->error = true; break; }

            /* Delete all original binary clauses (clique[i] | clique[j]) for 0 <= i < j < k. */
            for (uint32_t i = 0; i < k; ++i) {
                Lit a = clique[i];
                uint32_t alo = head[a], ahi = head[a + 1u];
                for (uint32_t j = i + 1u; j < k; ++j) {
                    Lit b = clique[j];
                    uint32_t idx = amo_find_edge(edges, alo, ahi, b);
                    if (idx == UINT32_MAX) continue;
                    /* Back up to first duplicate with other == b, delete all matching slots. */
                    while (idx > alo && edges[idx - 1u].other == b) idx--;
                    bool decremented = false;
                    while (idx < ahi && edges[idx].other == b) {
                        uint32_t cslot = edges[idx].cslot;
                        if (!used[cslot]) {
                            used[cslot] = 1;
                            CRef old_cr = s->clauses[cslot];
                            if (old_cr != INVALID_CLAUSE && !clause_deleted(s->arena, old_cr)) {
                                proof_delete_clause(s, CLAUSE_LITS(s->arena, old_cr),
                                                    CLAUSE_SIZE(s->arena, old_cr));
                                arena_delete(s->arena, old_cr);
                                s->clauses[cslot] = INVALID_CLAUSE;
                            }
                            if (!decremented) {
                                if (deg[a] > 0) deg[a]--;
                                if (deg[b] > 0) deg[b]--;
                                decremented = true;
                            }
                        }
                        idx++;
                    }
                }
            }
            /* Delete temporary backward RAT definitions from the proof file so the
             * proof clause set stays synchronized with s->clauses. */
            if (s->proof_file && !s->error) {
                Lit bdef0[2] = {mkLit(sv[0], true), neg(clique[0])};
                proof_delete_clause(s, bdef0, 2);
                for (uint32_t i = 1; i < k - 1u && !s->error; ++i) {
                    Lit bdef[3] = {mkLit(sv[i], true), mkLit(sv[i - 1u], false), neg(clique[i])};
                    proof_delete_clause(s, bdef, 3);
                }
            }
            cliques_encoded++;
        }
    }

    free(head); free(deg); free(used); free(edges);
    free(seeds); free(mark); free(var_in_clique); free(cands); free(next_cands);

    if (s->error) return false;
    if (cliques_encoded > 0) {
        if (s->num_vars + 2u > s->levels_capacity) {
            uint8_t *seen = calloc((size_t)s->num_vars + 2u, 1);
            if (!seen) { s->error = true; return false; }
            free(s->level_seen);
            s->level_seen = seen;
            s->levels_capacity = s->num_vars + 2u;
        }
        solver_rebuild_watches(s);
        if (s->error) return false;
    }
    return true;
}

/* =====================================================================
 * Main Inprocessing Orchestrator
 * ===================================================================== */

bool solver_inprocess(Solver *s, bool preprocessing) {
    if (!s || s->decision_level || s->error || s->interrupted || s->result == FALSE) {
        return s && s->result != FALSE;
    }

    double started = solver_account_begin(s);
    uint64_t saved_wl = s->work_limit;

    uint64_t scaled = (uint64_t)s->num_clauses * 200ULL;
    uint64_t base_budget = preprocessing ? MAX(25000000ULL, MIN(scaled, 100000000ULL))
                                         : MAX(10000000ULL, MIN(scaled / 2, 30000000ULL));
    /* Only clamp below base_budget if caller set an explicit small preprocess_budget (< 500k). */
    if (preprocessing && s->opts.preprocess_budget > 0 && s->opts.preprocess_budget < 500000ULL) {
        base_budget = s->opts.preprocess_budget;
    }

    /* Reset vivified flags periodically (every 4th inprocessing round) so newly
     * strengthened formulas can be re-vivified. */
    if (!preprocessing && (s->inprocess_rounds & 3) == 0) {
        for (uint32_t i = 0; i < s->num_clauses; ++i) {
            CRef cr = s->clauses[i];
            if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr))
                CLAUSE_HEADER(s->arena, cr)->vivified = 0;
        }
        for (uint32_t i = 0; i < s->num_learnts; ++i) {
            CRef cr = s->learnts[i];
            if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr))
                CLAUSE_HEADER(s->arena, cr)->vivified = 0;
        }
    }

    s->inprocess_rounds++;
    uint32_t grow_bound = (s->inprocess_rounds <= 1) ? 0u :
                          (s->inprocess_rounds == 2) ? 2u :
                          (s->inprocess_rounds == 3) ? 4u : 8u;

    /* Dedicated 100% LRAT-native RUP inprocessing pipeline:
     * root cleanup -> forward subsumption + 2-hint SSR -> gate-aware BVE -> SSR -> vivification -> GC. */
    if (s->lrat) {
        if (!inproc_root_clean(s)) goto finish;
        if (s->opts.subsumption && !solver_budget_exhausted_now(s)) {
            if (!inproc_subsume(s, base_budget / 2)) goto finish;
        }
        if (s->opts.var_elim && !solver_budget_exhausted_now(s)) {
            if (!inproc_elim(s, base_budget, grow_bound)) goto finish;
            if (s->opts.subsumption && !solver_budget_exhausted_now(s)) {
                if (!inproc_subsume(s, base_budget / 3)) goto finish;
            }
        }
        if (!solver_budget_exhausted_now(s)) {
            if (!inproc_vivify(s, base_budget / 2)) goto finish;
        }
        s->work_limit = 0;
        solver_collect_garbage(s);
        goto finish;
    }

    /* 1. Root cleanup */
    if (!inproc_root_clean(s)) goto finish;

    /* 1b. GF(2) XOR extraction, root RREF, and in-search Bird/Han-Jiang matrix init */
    if ((preprocessing || (s->inprocess_rounds & 3) == 1) && !solver_budget_exhausted_now(s)) {
        if (!gauss_extract_and_init(s, true)) goto finish;
    }

    /* 2. Failed-literal probing + Hyper-Binary Resolution (feeds SCC & gates!) */
    if (s->opts.probing && !solver_budget_exhausted_now(s)) {
        if (!inproc_probe_hbr(s, base_budget / 4)) goto finish;
    }

    /* 2b. Periodic inprocessing congruence closure (100% RUP-verified). */
    if (!preprocessing && (s->inprocess_rounds & 3) == 2 &&
        s->num_vars >= 300 && s->num_clauses <= 600000 && !solver_budget_exhausted_now(s)) {
        uint64_t cbud = MAX(base_budget / 4, (uint64_t)s->num_clauses * 40ULL);
        s->work_limit = s->work + MIN(cbud, 25000000ULL);
        solver_congruence_inproc(s);
        s->work_limit = 0;
        if (s->error || s->interrupted || s->result == FALSE) goto finish;
        if (!root_propagate(s)) goto finish;
    }

    /* 2c. Iterative fixpoint N-ary AND + MAJ3 + XOR3 congruence closure (100% RUP-verified) */
    if ((preprocessing || (s->inprocess_rounds & 3) == 1) && !solver_budget_exhausted_now(s)) {
        uint32_t ncong_merges = 0;
        if (!inproc_nary_congruence(s, base_budget / 4, &ncong_merges)) goto finish;
        /* If multi-gate closure found new equivalences, feed them back into 2-input
         * AND/XOR/ITE congruence once so mixed circuits close to a joint fixpoint! */
        if (ncong_merges > 0 && s->num_vars >= 64 && s->num_clauses <= 600000 &&
            !solver_budget_exhausted_now(s)) {
            uint64_t cbud = MAX(base_budget / 6, (uint64_t)s->num_clauses * 40ULL);
            s->work_limit = s->work + MIN(cbud, 25000000ULL);
            solver_congruence_inproc(s);
            s->work_limit = 0;
            if (s->error || s->interrupted || s->result == FALSE) goto finish;
            if (!root_propagate(s)) goto finish;
        }
    }

    /* 2d. Model-guided global backbone extraction on later rounds (100% RUP-verified) */
    if (!preprocessing && (s->inprocess_rounds & 1) == 1 && s->opts.probing && !solver_budget_exhausted_now(s)) {
        if (!inproc_backbone(s, base_budget / 8)) goto finish;
    }

    /* 3. In-place Tarjan SCC equivalence substitution */
    if (!solver_budget_exhausted_now(s)) {
        s->work_limit = s->work + base_budget / 3;
        if (!inproc_decompose(s)) goto finish;
        s->work_limit = 0;
    }

    /* 4. Embedded Mini-Kitten CDCL SAT sweeper (periodic to keep inprocessing lean) */
    if (s->opts.probing && (preprocessing || (s->inprocess_rounds & 3) == 0) &&
        s->num_vars >= 250 && !solver_budget_exhausted_now(s)) {
        if (!inproc_sweep(s, base_budget / 5)) goto finish;
    }

    /* 5. Forward subsumption & self-subsuming resolution (SSR) */
    if (s->opts.subsumption && !solver_budget_exhausted_now(s)) {
        if (!inproc_subsume(s, base_budget / 2)) goto finish;
    }

    /* 6. Gate-aware BVE (cascading with SSR + SCC) */
    if (s->opts.var_elim && !solver_budget_exhausted_now(s)) {
        if (!inproc_elim(s, base_budget, grow_bound)) goto finish;
        if (s->opts.subsumption && !solver_budget_exhausted_now(s)) {
            if (!inproc_subsume(s, base_budget / 3)) goto finish;
        }
        if (!solver_budget_exhausted_now(s)) {
            s->work_limit = s->work + base_budget / 4;
            if (!inproc_decompose(s)) goto finish;
            s->work_limit = 0;
        }
    }

    /* 6b. Transitive reduction of binary implication graph AFTER BVE (preserves all gates for BVE!) */
    if ((preprocessing || (s->inprocess_rounds & 3) == 2) && !solver_budget_exhausted_now(s)) {
        if (!inproc_transitive(s, base_budget / 6)) goto finish;
    }

    /* 7. O(n) prefix-sharing vivification */
    if (!solver_budget_exhausted_now(s)) {
        if (!inproc_vivify(s, base_budget / 2)) goto finish;
    }

    /* 8. Scaled tick-capped ProbSAT phase seeding (active-var/clause gated up to
     * 150k live vars / 450k live clauses; strictly bounded by tick budget & deadline). */
    if (!solver_budget_exhausted_now(s)) {
        if (preprocessing && s->num_vars <= 50000 && s->num_clauses <= 150000) {
            inproc_walk_seed_phases(s, 4000u, base_budget / 6);
        } else if (!preprocessing && (s->inprocess_rounds & 1) == 0) {
            inproc_walk_seed_phases(s, 20000u, base_budget / 4);
        }
    }

    /* 8b. Refresh/sync live GF(2) matrices after SCC/BVE/units */
    if (!gauss_sync_after_inprocess(s)) goto finish;

    /* 9. Compact arena if deletions created significant waste */
    s->work_limit = 0;
    solver_collect_garbage(s);

finish:
    if (s->watches->failed) s->error = true;
    s->work_limit = saved_wl;
    solver_account_end(s, preprocessing ? ACCOUNT_PREPROCESS : ACCOUNT_SIMPLIFY, started);
    return s->result != FALSE && !s->error;
}
