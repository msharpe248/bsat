/* Binary implication SCC substitution. Two iterative graph passes avoid C
 * recursion. A replacement solver is built before committing, so work-budget
 * exhaustion leaves the original solver usable. Proof additions are RUP under
 * the retained original binary graph; original proof clauses need not be deleted.
 */
#include "../include/solver.h"
#include <string.h>

static bool tick(Solver *s) {
    if (s->error || s->interrupted || (s->work_limit && s->work >= s->work_limit)) return false;
    s->work++;
    s->stats.equiv_work++;
    return (s->stats.equiv_work & 1023) || !solver_budget_exhausted(s);
}

static Lit binary_other_literal(const Solver *s, Watch w) {
    if (is_binary_watch(w)) return w.blocker;
    return CLAUSE_SIZE(s->arena, watch_clause(w)) == 2 ? w.blocker : LIT_UNDEF;
}

/* Root-false literals can expose binary clauses inside larger arena records.
   Make these RUP consequences explicit before traversing the binary graph.
   They remain redundant and usable even if a later SCC stage exhausts budget. */
static bool expose_root_binaries(Solver *s, bool *have_binaries) {
    uint32_t original = s->num_clauses;
    for (uint32_t i = 0; i < original; ++i) {
        if (!tick(s)) return false;
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t size = CLAUSE_SIZE(s->arena, cr);
        if (size <= 2) {
            if (size == 2) {
                const Lit *lits = CLAUSE_LITS(s->arena, cr);
                if (s->values[var(lits[0])] == UNDEF && s->values[var(lits[1])] == UNDEF)
                    *have_binaries = true;
            }
            continue;
        }
        const Lit *lits = CLAUSE_LITS(s->arena, cr);
        Lit pair[2];uint32_t open = 0;bool satisfied = false;
        for (uint32_t j = 0; j < size; ++j) {
            if (!tick(s)) return false;
            lbool value = lxor(s->values[var(lits[j])], sign(lits[j]));
            if (value == TRUE) { satisfied = true;break; }
            if (value == UNDEF) {
                if (open == 2) { open++;break; }
                pair[open++] = lits[j];
            }
        }
        if (satisfied || open != 2) continue;
        proof_add_clause(s, pair, 2);
        s->internal_add = true;solver_add_clause(s, pair, 2);s->internal_add = false;
        s->stats.equiv_binaries++;
        *have_binaries = true;
        if (s->error || s->watches->failed) return false;
    }
    return true;
}

uint32_t solver_substitute_equivalences(Solver *s) {
    if (!s->num_vars || !s->num_clauses || !s->opts.equiv_budget || s->decision_level || s->num_learnts ||
        s->stats.decisions || s->elim || s->result == FALSE || solver_budget_exhausted(s)) return 0;
    bool have_binaries = false;
    uint32_t original_clauses = s->num_clauses;
    if (!expose_root_binaries(s, &have_binaries) || !have_binaries) return 0;
    if (s->work_limit && s->work_limit-s->work < 2*(uint64_t)s->num_vars) return 0;
    uint32_t nl = 2 * (s->num_vars + 1), count = 0, finished = 0;
    uint8_t *seen = calloc(nl, sizeof *seen);
    uint32_t *next = calloc(nl, sizeof *next);
    Lit *stack = malloc((size_t)nl * sizeof *stack);
    Lit *order = malloc((size_t)nl * sizeof *order);
    Lit *repr = calloc(nl, sizeof *repr);
    Solver *fresh = NULL;
    Lit *clause = NULL;
    uint32_t capacity = 0, rewritten = 0;
    if (!seen || !next || !stack || !order || !repr) { s->error = true; goto done; }

    /* Forward graph: p -> q for the binary clause (~p | q). */
    for (Lit root = 2; root < nl; ++root) {
        if (!tick(s)) goto done;
        if (seen[root] || s->values[var(root)] != UNDEF) continue;
        uint32_t top = 0;
        stack[top++] = root; seen[root] = 1;
        while (top) {
            Lit p = stack[top-1];
            WatchList *wl = watch_list(s->watches, neg(p));
            bool descend = false;
            while (next[p] < wl->size) {
                if (!tick(s)) goto done;
                Lit q = binary_other_literal(s, wl->watches[next[p]++]);
                if (!q || seen[q] || s->values[var(q)] != UNDEF) continue;
                seen[q] = 1; stack[top++] = q; descend = true; break;
            }
            if (!descend) { order[finished++] = p; --top; }
        }
    }
    /* Reverse successors of p are complements of successors of ~p. Binary
       implication graphs are closed under contraposition. */
    while (finished) {
        Lit root = order[--finished];
        if (!tick(s)) goto done;
        if (repr[root]) continue;
        uint32_t top = 0;
        Lit minimum = root;
        repr[root] = root; stack[top++] = root;
        while (top) {
            Lit p = stack[--top];
            if (p < minimum) minimum = p;
            WatchList *wl = watch_list(s->watches, p);
            for (uint32_t i = 0; i < wl->size; ++i) {
                if (!tick(s)) goto done;
                Lit q = binary_other_literal(s, wl->watches[i]);
                if (!q) continue;
                q = neg(q);
                if (repr[q] || s->values[var(q)] != UNDEF) continue;
                repr[q] = root; stack[top++] = q;
            }
        }
        next[root] = minimum;
    }
    for (Lit p = 2; p < nl; ++p) {
        if (!tick(s)) goto done;
        repr[p] = repr[p] ? next[repr[p]] : p;
    }
    for (Var v = 1; v <= s->num_vars; ++v) {
        if (!tick(s)) goto done;
        Lit p = mkLit(v, false);
        if (repr[p] == repr[neg(p)]) {
            /* ~p implies p, so unit p is RUP; p then implies ~p, proving the
               terminal empty clause emitted by the normal solve wrapper. */
            proof_add_clause(s, &p, 1);
            s->stats.equiv_conflicts++; s->result = FALSE; goto done;
        }
        ASSERT(repr[neg(p)] == neg(repr[p]));
        if (repr[p] != p) count++;
    }
    if (!count) goto done;
    free(seen); seen = NULL; free(next); next = NULL;
    free(stack); stack = NULL; free(order); order = NULL;

    SolverOpts opts = s->opts;
    opts.proof_path = NULL; // Never reopen/truncate the caller's proof stream.
    fresh = solver_new_with_opts(&opts);
    if (!fresh) { s->error = true; goto done; }
    fresh->terminate=s->terminate;fresh->terminate_state=s->terminate_state;
    fresh->learn_callback=s->learn_callback;fresh->learn_state=s->learn_state;fresh->learn_max_length=s->learn_max_length;
    fresh->reused_solves=s->reused_solves;
    for (Var v = 1; v <= s->num_vars; ++v) {
        if (!tick(s)) goto done;
        if (!solver_new_var(fresh)) { s->error = true; goto done; }
        fresh->vars[v].polarity = s->vars[v].polarity;
    }
    fresh->internal_add = true;
    /* Probing can establish root units that have no arena record. Preserve
       every root fact before rebuilding and before dropping satisfied clauses. */
    for (Var v = 1; v <= s->num_vars; ++v) {
        if (!tick(s)) goto done;
        if (s->values[v] == UNDEF) continue;
        Lit unit = mkLit(v, s->values[v] == FALSE);
        proof_add_clause(s, &unit, 1);
        solver_add_clause(fresh, &unit, 1);
        if (fresh->error) { s->error = true; goto done; }
    }
    /* Newly exposed binaries duplicate the shortened original clauses. They
       support discovery/proofs, but must not be duplicated in the new core. */
    for (uint32_t i = 0; i < original_clauses; ++i) {
        if (!tick(s)) goto done;
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t size = CLAUSE_SIZE(s->arena, cr);
        if (size > capacity) {
            Lit *p = realloc(clause, (size_t)size * sizeof *p);
            if (!p) { s->error = true; goto done; }
            clause = p; capacity = size;
        }
        const Lit *old = CLAUSE_LITS(s->arena, cr);
        bool changed = false, satisfied = false;
        uint32_t kept = 0;
        for (uint32_t j = 0; j < size; ++j) {
            if (!tick(s)) goto done;
            lbool value = lxor(s->values[var(old[j])], sign(old[j]));
            if (value == TRUE) { satisfied = true;break; }
            if (value == FALSE) { changed = true;continue; }
            clause[kept] = repr[old[j]];
            changed |= clause[kept++] != old[j];
        }
        if (satisfied) continue;
        if (changed) { proof_add_clause(s, clause, kept); rewritten++; }
        solver_add_clause(fresh, clause, kept);
        if (fresh->error || fresh->watches->failed) { s->error = true; goto done; }
    }
    fresh->internal_add = false;
    for (Var v = 1; v <= s->num_vars; ++v) {
        if (!tick(s)) goto done;
        Lit p = mkLit(v, false), r = repr[p];
        if (p == r) continue;
        /* Reconstruct p <-> r after later BVE/BCE records have been restored. */
        Lit witness[] = {neg(p), r, 0, p, neg(r), 0};
        if (!elim_save(fresh, v, witness, 6)) { s->error = true; goto done; }
        fresh->elim->eliminated[v] = true;
    }
    if (solver_budget_exhausted(s)) goto done;
    fresh->opts = s->opts;
    fresh->stats = s->stats;
    if (s->opts.accounting) {
        uint64_t overlap=solver_memory(s).total+solver_memory(fresh).total;
        fresh->accounting.rebuild_overlap_peak=MAX(overlap,s->accounting.rebuild_overlap_peak);
        fresh->accounting.congruence_temporary_peak=MAX(fresh->accounting.congruence_temporary_peak,s->accounting.congruence_temporary_peak);
    }
    for (unsigned phase=0;phase<ACCOUNT_PHASES;++phase) {
        fresh->accounting.seconds[phase] += s->accounting.seconds[phase];
        fresh->accounting.calls[phase] += s->accounting.calls[phase];
    }
    fresh->stats.equiv_variables += count;
    fresh->stats.equiv_clauses += rewritten;
    fresh->work = s->work; fresh->work_limit = s->work_limit;
    fresh->random_state = s->random_state;
    /* Solving allocates level marks before preprocessing. Keep that scratch
       when replacing the solver, or every subsequent LBD is silently zero. */
    free(fresh->level_seen);
    fresh->level_seen = s->level_seen; fresh->levels_capacity = s->levels_capacity;
    s->level_seen = NULL; s->levels_capacity = 0;
    fresh->input = s->input; fresh->input_size = s->input_size;
    fresh->input_capacity = s->input_capacity; fresh->input_clauses = s->input_clauses;
    s->input = NULL; s->input_size = s->input_capacity = 0;
    fresh->proof_file = s->proof_file; s->proof_file = NULL;
    Solver old = *s; *s = *fresh; *fresh = old;
    solver_free(fresh); fresh = NULL;
done:
    free(seen); free(next); free(stack); free(order); free(repr); free(clause);
    solver_free(fresh);
    return s->stats.equiv_variables ? count : 0;
}
