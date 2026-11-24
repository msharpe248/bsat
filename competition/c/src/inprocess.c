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

static bool root_propagate(Solver *s) {
    if (s->result == FALSE || s->error) return false;
    uint64_t saved_limit = s->work_limit;
    s->work_limit = 0;
    CRef c = solver_propagate(s);
    s->work_limit = saved_limit;
    if (c != INVALID_CLAUSE) {
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
            watch_add(s->watches, lits[0], INVALID_CLAUSE, lits[1]);
            watch_add(s->watches, lits[1], INVALID_CLAUSE, lits[0]);
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
            /* Never delete unit clauses from the DRAT proof: later RUP steps depend on root units! */
            if (size > 1) proof_delete_clause(s, lits, size);
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
        proof_add_clause(s, buf, k);
        if (s->error) { if (buf != small_buf) free(buf); return false; }
        proof_delete_clause(s, lits, size);
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
            bool ok = enqueue_root_unit(s, buf[0]);
            if (buf != small_buf) free(buf);
            if (!ok) return false;
            continue;
        }
        CRef ncr = arena_alloc(s->arena, buf, k, learned);
        if (buf != small_buf) free(buf);
        if (ncr == INVALID_CLAUSE) { s->error = true; return false; }
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
                    /* Pop SCC and pick minimum literal (by canonical variable order) as representative. */
                    uint32_t scc_start = ttop;
                    Lit min_lit = u;
                    while (scc_start > 0) {
                        Lit x = tarjan[--scc_start];
                        if (x < min_lit) min_lit = x;
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
        /* Pick the smaller representative consistently between p and ~p. */
        if (var(repr[p]) < var(repr[np])) {
            repr[np] = neg(repr[p]);
        } else {
            repr[p] = neg(repr[np]);
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
        if ((ci & 63) == 0 && solver_budget_exhausted(s)) break;
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
                        proof_add_clause(s, nbuf, k);
                        if (s->error) { if (nbuf != small_buf) free(nbuf); break; }
                        proof_delete_clause(s, dlits, dsize);
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
                            if (!enqueue_root_unit(s, unit)) { unsat = true; break; }
                            continue;
                        }
                        CRef ncr = arena_alloc(s->arena, nbuf, k, d_learned);
                        /* Note: arena_alloc might realloc arena->memory -> refresh clits! */
                        clits = CLAUSE_LITS(s->arena, cr);
                        if (ncr == INVALID_CLAUSE) {
                            if (nbuf != small_buf) free(nbuf);
                            s->error = true; break;
                        }
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
    uint32_t stage_off_cap = 256;

    if (!occs || !live_occ || !q.vars || !q.pos || !q.score || !lit_seen || !gate_pos || !gate_neg || !stage_lits || !stage_off) {
        free(occs); free(live_occ); free(q.vars); free(q.pos); free(q.score);
        free(lit_seen); free(gate_pos); free(gate_neg); free(stage_lits); free(stage_off);
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
        if (s->values[v] != UNDEF || s->elim->eliminated[v]) continue;
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
        if ((vars_elim_this_round & 31) == 0 && solver_budget_exhausted(s)) break;
        Var v = eq_pop(&q);
        if (s->values[v] != UNDEF || s->elim->eliminated[v]) continue;

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
                    for (uint32_t k = 0; k < sz; ++k) {
                        Lit kl = ls[k];
                        if (live_occ[kl] > 0) live_occ[kl]--;
                        Var u = var(kl);
                        if (u != v && !s->elim->eliminated[u] && s->values[u] == UNDEF) {
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

        /* Try gate extraction! */
        bool has_gate = extract_gate(s, v, po, no, gate_pos, gate_neg, lit_seen);
        if (!has_gate && (np > MAX_OCC_PLAIN || nn > MAX_OCC_PLAIN)) continue;

        /* Stage all resolvents into flat buffer, checking clause budget. */
        uint32_t max_res = np + nn + grow_bound;
        uint32_t nr = 0;
        uint32_t stage_used = 0;
        bool too_many = false;

        for (uint32_t i = 0; i < np && !too_many; ++i) {
            CRef ca = po->clauses[i];
            for (uint32_t j = 0; j < nn && !too_many; ++j) {
                /* With a gate, skip GxG (tautologies) and RxR (redundant)! */
                if (has_gate && (gate_pos[i] == gate_neg[j])) continue;
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
                    if (!p) { s->error = true; too_many = true; break; }
                    stage_off = p;
                }
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
            proof_add_clause(s, rlits, rsz);
            if (s->error) break;
            if (rsz == 0) {
                s->base_unsat = true; s->result = FALSE;
                unsat = true; break;
            }
            if (rsz == 1) {
                if (!enqueue_root_unit(s, rlits[0])) { unsat = true; break; }
                s->elim->resolvents_added++;
                continue;
            }
            CRef ncr = arena_alloc(s->arena, rlits, rsz, false);
            if (ncr == INVALID_CLAUSE) { s->error = true; break; }
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
                for (uint32_t k = 0; k < sz; ++k) {
                    if (live_occ[ls[k]] > 0) live_occ[ls[k]]--;
                }
                for (uint32_t k = 0; k < sz; ++k) {
                    Var u = var(ls[k]);
                    if (u != v && !s->elim->eliminated[u] && s->values[u] == UNDEF) {
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
    free(stage_lits); free(stage_off);

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
                    new_n = vivify_analyze(s, INVALID_CLAUSE, l, out_buf);
                    shortened = (new_n < sz);
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
            if (new_n < sz) shortened = true;
        }

        if (subsumed_sat) {
            solver_backtrack(s, 0);
            assumed_n = 0;
            proof_delete_clause(s, CLAUSE_LITS(s->arena, cr), sz);
            arena_delete(s->arena, cr);
            if (cands[ci].learned) s->learnts[cands[ci].slot] = INVALID_CLAUSE;
            else s->clauses[cands[ci].slot] = INVALID_CLAUSE;
            any_change = true;
        } else if (shortened) {
            solver_backtrack(s, 0);
            assumed_n = 0;
            /* Deduplicate out_buf cleanly. */
            if (new_n > 1) {
                sort_lits_small(out_buf, new_n);
                uint32_t u = 0;
                for (uint32_t j = 0; j < new_n; ++j) {
                    if (u && out_buf[j] == out_buf[u - 1]) continue;
                    out_buf[u++] = out_buf[j];
                }
                new_n = u;
            }
            if (new_n < sz) {
                proof_add_clause(s, out_buf, new_n);
                if (s->error) break;
                proof_delete_clause(s, CLAUSE_LITS(s->arena, cr), sz);
                uint32_t old_lbd = clause_lbd(s->arena, cr);
                uint32_t old_used = CLAUSE_HEADER(s->arena, cr)->used;
                arena_delete(s->arena, cr);
                s->stats.minimized_literals += sz - new_n;
                any_change = true;
                if (new_n == 0) {
                    s->base_unsat = true; s->result = FALSE;
                    unsat = true; break;
                } else if (new_n == 1) {
                    if (cands[ci].learned) s->learnts[cands[ci].slot] = INVALID_CLAUSE;
                    else s->clauses[cands[ci].slot] = INVALID_CLAUSE;
                    if (!enqueue_root_unit(s, out_buf[0]) || !root_propagate(s)) { unsat = true; break; }
                } else {
                    CRef ncr = arena_alloc(s->arena, out_buf, new_n, cands[ci].learned);
                    if (ncr == INVALID_CLAUSE) { s->error = true; break; }
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
                        CRef w = (new_n == 2) ? INVALID_CLAUSE : ncr;
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
        }
    }

    solver_backtrack(s, 0);
    s->opts.chrono = saved_chrono;
    s->opts.phase_saving = saved_phase;
    s->work_limit = saved_wl;
    free(cands);
    return !unsat && !s->error;
}

/* =====================================================================
 * Pass 7A: GF(2) XOR Extraction & RREF Gaussian Elimination
 *
 * Extracts exact 2..6-variable XOR parity constraints from live irredundant
 * clauses and computes Reduced Row Echelon Form over GF(2) to derive
 * contradictions (0 = 1), root units (x = 0/1), and equivalences (x ^ y = c).
 * Guarded on non-proof mode so standard DRAT/RUP checks remain 100% sound.
 * ===================================================================== */

typedef struct {
    Var vars[6];
    uint8_t k;
    uint64_t mask;
} XorSlot;

static uint64_t hash_xor_tuple(const Var *vs, uint8_t k) {
    uint64_t h = (uint64_t)k * UINT64_C(0x9e3779b97f4a7c15);
    for (uint8_t i = 0; i < k; ++i) {
        h ^= (uint64_t)vs[i] + UINT64_C(0x9e3779b97f4a7c15) + (h << 6) + (h >> 2);
    }
    h ^= h >> 30; h *= UINT64_C(0xbf58476d1ce4e5b9);
    h ^= h >> 27; h *= UINT64_C(0x94d049bb133111eb);
    return h ^ (h >> 31);
}

static bool inproc_gauss(Solver *s) {
    if (s->proof_file || s->proof_journal) return true;
    if (!s->num_vars || !s->num_clauses || s->num_clauses > 250000 ||
        s->error || s->result == FALSE || solver_budget_exhausted(s)) return true;

    /* Count eligible clauses of size 2..6. */
    uint32_t elig = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz >= 2 && sz <= 6) elig++;
    }
    if (elig < 4) return true;

    /* Build open-addressed hash table grouping clauses by sorted variable tuple. */
    size_t cap = 64;
    while (cap < (size_t)elig * 2 && cap < (1u << 21)) cap <<= 1;
    if (cap < (size_t)elig + 16) return true;
    XorSlot *tab = calloc(cap, sizeof *tab);
    if (!tab) { s->error = true; return false; }

    uint32_t *used_slots = malloc(MIN(cap, (size_t)elig + 1) * sizeof *used_slots);
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
        /* Sort tmp[0..sz-1] by variable ascending. */
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

        size_t pos = (size_t)(hash_xor_tuple(vs, (uint8_t)sz) & (cap - 1));
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
            pos = (pos + 1) & (cap - 1);
        }
    }

    /* Precompute exact even/odd polarity bitmasks for k = 2..6. */
    uint64_t even_need[7] = {0}, odd_need[7] = {0};
    for (uint32_t k = 2; k <= 6; ++k) {
        for (uint32_t m = 0; m < (1u << k); ++m) {
            uint32_t parity = 0, x = m;
            while (x) { parity ^= (x & 1u); x >>= 1; }
            if (parity == 0) even_need[k] |= (UINT64_C(1) << m);
            else odd_need[k] |= (UINT64_C(1) << m);
        }
    }

    /* Collect complete XOR equations (prefer k >= 3; include k == 2 equivs if any k >= 3 exist). */
    typedef struct { Var vars[6]; uint8_t k; uint8_t rhs; } XorEq;
    uint32_t n_ge3 = 0, n_eqs = 0;
    for (uint32_t ui = 0; ui < n_used; ++ui) {
        XorSlot *sl = &tab[used_slots[ui]];
        uint8_t k = sl->k;
        bool has_even = (sl->mask & even_need[k]) == even_need[k];
        bool has_odd  = (sl->mask & odd_need[k]) == odd_need[k];
        if (has_even && has_odd) {
            /* Both parities forbidden -> immediate UNSAT! */
            free(used_slots); free(tab);
            s->base_unsat = true; s->result = FALSE;
            return false;
        }
        if (has_even || has_odd) {
            n_eqs++;
            if (k >= 3) n_ge3++;
        }
    }
    if (n_ge3 < 2 || n_eqs > 8192) {
        free(used_slots); free(tab);
        return true;
    }

    XorEq *eqs = malloc((size_t)n_eqs * sizeof *eqs);
    uint32_t *var_to_col = malloc((size_t)(s->num_vars + 1) * sizeof *var_to_col);
    if (!eqs || !var_to_col) {
        free(eqs); free(var_to_col); free(used_slots); free(tab);
        s->error = true; return false;
    }
    memset(var_to_col, 0xFF, (size_t)(s->num_vars + 1) * sizeof *var_to_col);

    uint32_t eq_count = 0, n_cols = 0;
    for (uint32_t ui = 0; ui < n_used; ++ui) {
        XorSlot *sl = &tab[used_slots[ui]];
        uint8_t k = sl->k;
        bool has_even = (sl->mask & even_need[k]) == even_need[k];
        bool has_odd  = (sl->mask & odd_need[k]) == odd_need[k];
        if (!has_even && !has_odd) continue;
        /* Even forbidden clauses -> odd true count (rhs = 1); odd forbidden -> rhs = 0. */
        XorEq *e = &eqs[eq_count++];
        e->k = k;
        e->rhs = has_even ? 1u : 0u;
        memcpy(e->vars, sl->vars, sizeof e->vars);
        for (uint8_t j = 0; j < k; ++j) {
            Var v = sl->vars[j];
            if (var_to_col[v] == UINT32_MAX) {
                if (n_cols >= 4096) { n_cols = UINT32_MAX; break; }
                var_to_col[v] = n_cols++;
            }
        }
        if (n_cols == UINT32_MAX) break;
    }
    free(used_slots); free(tab);

    if (n_cols == 0 || n_cols == UINT32_MAX || eq_count > 4096) {
        free(eqs); free(var_to_col);
        return true;
    }

    Var *col_to_var = malloc((size_t)n_cols * sizeof *col_to_var);
    uint32_t words = (n_cols + 63u) / 64u;
    uint64_t *mat = calloc((size_t)eq_count * words, sizeof *mat);
    uint8_t *rhs = calloc(eq_count, sizeof *rhs);
    if (!col_to_var || !mat || !rhs) {
        free(col_to_var); free(mat); free(rhs); free(eqs); free(var_to_col);
        s->error = true; return false;
    }
    for (uint32_t i = 0; i < eq_count; ++i) {
        uint64_t *row = mat + (size_t)i * words;
        rhs[i] = eqs[i].rhs;
        for (uint8_t j = 0; j < eqs[i].k; ++j) {
            uint32_t c = var_to_col[eqs[i].vars[j]];
            col_to_var[c] = eqs[i].vars[j];
            row[c >> 6] ^= (UINT64_C(1) << (c & 63u));
        }
    }
    free(eqs);
    free(var_to_col);

    /* Gauss-Jordan RREF over GF(2). */
    uint32_t pivot_row = 0;
    for (uint32_t col = 0; col < n_cols && pivot_row < eq_count; ++col) {
        uint32_t w = col >> 6;
        uint64_t bit = UINT64_C(1) << (col & 63u);
        uint32_t found = eq_count;
        for (uint32_t r = pivot_row; r < eq_count; ++r) {
            if (mat[(size_t)r * words + w] & bit) { found = r; break; }
        }
        if (found == eq_count) continue;
        if (found != pivot_row) {
            uint64_t *ra = mat + (size_t)pivot_row * words;
            uint64_t *rb = mat + (size_t)found * words;
            for (uint32_t k = 0; k < words; ++k) {
                uint64_t t = ra[k]; ra[k] = rb[k]; rb[k] = t;
            }
            uint8_t tr = rhs[pivot_row]; rhs[pivot_row] = rhs[found]; rhs[found] = tr;
        }
        uint64_t *pr = mat + (size_t)pivot_row * words;
        uint8_t prhs = rhs[pivot_row];
        for (uint32_t r = 0; r < eq_count; ++r) {
            if (r == pivot_row) continue;
            uint64_t *rr = mat + (size_t)r * words;
            if (rr[w] & bit) {
                for (uint32_t k = 0; k < words; ++k) rr[k] ^= pr[k];
                rhs[r] ^= prhs;
            }
        }
        pivot_row++;
        s->work += (uint64_t)eq_count * words;
    }

    /* Harvest contradictions, units, and binary equivalences from RREF rows! */
    bool unsat = false;
    bool added_equiv = false;
    bool added_unit = false;
    for (uint32_t r = 0; r < eq_count && !unsat && !s->error; ++r) {
        const uint64_t *rr = mat + (size_t)r * words;
        uint32_t cnt = 0;
        uint32_t c0 = 0, c1 = 0;
        for (uint32_t w = 0; w < words && cnt <= 2; ++w) {
            uint64_t x = rr[w];
            while (x) {
                uint32_t tz = (uint32_t)__builtin_ctzll(x);
                uint32_t col = (w << 6) + tz;
                if (cnt == 0) c0 = col;
                else if (cnt == 1) c1 = col;
                cnt++;
                if (cnt > 2) break;
                x &= x - 1;
            }
        }
        if (cnt == 0) {
            if (rhs[r]) {
                s->base_unsat = true; s->result = FALSE;
                unsat = true;
                break;
            }
        } else if (cnt == 1) {
            Lit unit = mkLit(col_to_var[c0], rhs[r] == 0);
            if (!enqueue_root_unit(s, unit)) { unsat = true; break; }
            added_unit = true;
        } else if (cnt == 2) {
            Lit a = mkLit(col_to_var[c0], false);
            Lit b = mkLit(col_to_var[c1], rhs[r] != 0); /* a == b if rhs==0, else a == ~b */
            Lit pairs[2][2] = {{neg(a), b}, {neg(b), a}};
            for (int p = 0; p < 2 && !s->error; ++p) {
                if (has_binary_watch(s, pairs[p][0], pairs[p][1])) continue;
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
                added_equiv = true;
            }
        }
    }

    free(col_to_var); free(mat); free(rhs);
    if (unsat || s->error) return false;
    if (added_unit && !root_propagate(s)) return false;
    if (added_equiv && !solver_budget_exhausted(s)) return inproc_decompose(s);
    return true;
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
    const uint32_t MAX_ENVS = 32;
    bool unsat = false, added_any = false;
    uint32_t seed_rng = s->random_state ^ (uint32_t)(s->inprocess_rounds * 2654435761u + 13u);

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
 * Pass 8: Lightweight WalkSAT Phase Seeding (for SAT guidance)
 * ===================================================================== */

static void inproc_walk_seed_phases(Solver *s) {
    if (!s->opts.phase_saving || !s->opts.rephase || !s->rephase.best_phase) return;
    if (s->num_vars == 0 || s->num_vars > 50000 || s->num_clauses == 0 || s->num_clauses > 150000) return;
    if (solver_budget_exhausted(s)) return;

    uint32_t nv = s->num_vars;
    uint32_t nc = s->num_clauses;
    uint8_t *assign = malloc((size_t)(nv + 1));
    uint8_t *best_assign = malloc((size_t)(nv + 1));
    uint32_t *sat_cnt = calloc(nc, sizeof *sat_cnt);
    uint32_t *unsat_list = malloc((size_t)nc * sizeof *unsat_list);
    uint32_t *unsat_pos = malloc((size_t)nc * sizeof *unsat_pos);
    uint32_t *occ_head = calloc((size_t)(2 * (nv + 1)) + 1, sizeof *occ_head);
    if (!assign || !best_assign || !sat_cnt || !unsat_list || !unsat_pos || !occ_head) {
        free(assign); free(best_assign); free(sat_cnt); free(unsat_list); free(unsat_pos); free(occ_head);
        s->error = true;
        return;
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
    for (uint32_t l = 1; l <= nl; ++l) occ_head[l] += occ_head[l - 1];
    uint32_t *occ_clauses = malloc((size_t)(total_lits ? total_lits : 1) * sizeof *occ_clauses);
    uint32_t *cursor = malloc((size_t)(nl + 1) * sizeof *cursor);
    if (!occ_clauses || !cursor) {
        free(assign); free(best_assign); free(sat_cnt); free(unsat_list); free(unsat_pos);
        free(occ_head); free(occ_clauses); free(cursor);
        s->error = true;
        return;
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

    /* Initialize assignment from current saved polarities + root units. */
    for (Var v = 1; v <= nv; ++v) {
        if (s->values[v] != UNDEF) assign[v] = (s->values[v] == TRUE);
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

    uint32_t best_unsat = n_unsat;
    memcpy(best_assign, assign, (size_t)(nv + 1));
    uint32_t rng = s->random_state ^ (uint32_t)(s->inprocess_rounds * 0x9e3779b9u + 7u);
    const uint32_t MAX_FLIPS = 15000;

    for (uint32_t flip = 0; flip < MAX_FLIPS && n_unsat > 0; ++flip) {
        if ((flip & 1023) == 0 && solver_budget_exhausted(s)) break;
        uint32_t ci = unsat_list[bsat_random(&rng) % n_unsat];
        CRef cr = s->clauses[ci];
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        const Lit *ls = CLAUSE_LITS(s->arena, cr);

        /* Pick variable in ci with minimum break count (or random with prob 0.35). */
        Var chosen = INVALID_VAR;
        uint32_t best_brk = UINT32_MAX;
        bool do_rand = (bsat_random(&rng) % 100) < 35;
        if (do_rand) {
            uint32_t start = bsat_random(&rng) % sz;
            for (uint32_t k = 0; k < sz; ++k) {
                Var v = var(ls[(start + k) % sz]);
                if (s->values[v] == UNDEF) { chosen = v; break; }
            }
        }
        if (chosen == INVALID_VAR) {
            for (uint32_t k = 0; k < sz; ++k) {
                Var v = var(ls[k]);
                if (s->values[v] != UNDEF) continue;
                Lit true_lit = mkLit(v, !assign[v]);
                uint32_t brk = 0;
                for (uint32_t o = occ_head[true_lit]; o < occ_head[true_lit + 1]; ++o) {
                    if (sat_cnt[occ_clauses[o]] == 1) { brk++; if (brk >= best_brk) break; }
                }
                if (brk < best_brk) { best_brk = brk; chosen = v; if (brk == 0) break; }
            }
        }
        if (chosen == INVALID_VAR) break;

        Lit old_true = mkLit(chosen, !assign[chosen]);
        Lit new_true = neg(old_true);
        assign[chosen] ^= 1;

        for (uint32_t o = occ_head[old_true]; o < occ_head[old_true + 1]; ++o) {
            uint32_t c = occ_clauses[o];
            if (--sat_cnt[c] == 0) {
                unsat_pos[c] = n_unsat;
                unsat_list[n_unsat++] = c;
            }
        }
        for (uint32_t o = occ_head[new_true]; o < occ_head[new_true + 1]; ++o) {
            uint32_t c = occ_clauses[o];
            if (sat_cnt[c]++ == 0) {
                uint32_t pos = unsat_pos[c];
                uint32_t last = unsat_list[--n_unsat];
                unsat_list[pos] = last;
                unsat_pos[last] = pos;
                unsat_pos[c] = UINT32_MAX;
            }
        }
        if (n_unsat < best_unsat) {
            best_unsat = n_unsat;
            memcpy(best_assign, assign, (size_t)(nv + 1));
        }
    }

    /* Seed saved polarities with the best low-unsat assignment found! */
    if (best_unsat < n_unsat || best_unsat <= nc / 20) {
        for (Var v = 1; v <= nv; ++v) {
            if (s->values[v] == UNDEF) s->vars[v].polarity = (best_assign[v] != 0);
        }
    }

    free(assign); free(best_assign); free(sat_cnt); free(unsat_list);
    free(unsat_pos); free(occ_head); free(occ_clauses);
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

    /* 1. Root cleanup */
    if (!inproc_root_clean(s)) goto finish;

    /* 1b. GF(2) XOR extraction & Gaussian elimination (non-proof mode) */
    if ((preprocessing || (s->inprocess_rounds & 3) == 1) && !solver_budget_exhausted(s)) {
        if (!inproc_gauss(s)) goto finish;
    }

    /* 2. Failed-literal probing + Hyper-Binary Resolution (feeds SCC & gates!) */
    if (s->opts.probing && !solver_budget_exhausted(s)) {
        if (!inproc_probe_hbr(s, base_budget / 4)) goto finish;
    }

    /* 3. In-place Tarjan SCC equivalence substitution */
    s->work_limit = s->work + base_budget / 3;
    if (!inproc_decompose(s)) goto finish;
    s->work_limit = 0;

    /* 4. Embedded Mini-Kitten CDCL SAT sweeper (periodic to keep inprocessing lean) */
    if (s->opts.probing && (preprocessing || (s->inprocess_rounds & 3) == 0) &&
        s->num_vars >= 250 && !solver_budget_exhausted(s)) {
        if (!inproc_sweep(s, base_budget / 5)) goto finish;
    }

    /* 5. Forward subsumption & self-subsuming resolution (SSR) */
    if (s->opts.subsumption && !solver_budget_exhausted(s)) {
        if (!inproc_subsume(s, base_budget / 2)) goto finish;
    }

    /* 6. Gate-aware BVE (cascading with SSR + SCC) */
    if (s->opts.var_elim && !solver_budget_exhausted(s)) {
        if (!inproc_elim(s, base_budget, grow_bound)) goto finish;
        if (s->opts.subsumption && !solver_budget_exhausted(s)) {
            if (!inproc_subsume(s, base_budget / 3)) goto finish;
        }
        s->work_limit = s->work + base_budget / 4;
        if (!inproc_decompose(s)) goto finish;
        s->work_limit = 0;
    }

    /* 6b. Transitive reduction of binary implication graph AFTER BVE (preserves all gates for BVE!) */
    if ((preprocessing || (s->inprocess_rounds & 3) == 2) && !solver_budget_exhausted(s)) {
        if (!inproc_transitive(s, base_budget / 6)) goto finish;
    }

    /* 7. O(n) prefix-sharing vivification */
    if (!solver_budget_exhausted(s)) {
        if (!inproc_vivify(s, base_budget / 2)) goto finish;
    }

    /* 8. Lightweight WalkSAT phase seeding (helps SAT instances find models fast) */
    if (!preprocessing && (s->inprocess_rounds & 1) == 0 && !solver_budget_exhausted(s)) {
        inproc_walk_seed_phases(s);
    }

    /* 9. Compact arena if deletions created significant waste */
    s->work_limit = 0;
    solver_collect_garbage(s);

finish:
    s->work_limit = saved_wl;
    solver_account_end(s, preprocessing ? ACCOUNT_PREPROCESS : ACCOUNT_SIMPLIFY, started);
    return s->result != FALSE && !s->error;
}
