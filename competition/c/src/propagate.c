#include "solver.h"
#include <stdio.h>

/* ============================================================================
 * BOOLEAN CONSTRAINT PROPAGATION (BCP) WITH TWO-WATCHED LITERALS
 * ============================================================================
 * This is the performance-critical inner loop of CDCL
 *
 * Key invariant: Each clause watches exactly 2 literals (or 1 for unit clauses)
 * When a watched literal becomes FALSE:
 *   1. Try to find a new unassigned/true literal to watch
 *   2. If found, update watch (no need to revisit this clause)
 *   3. If not found and other watch is unassigned -> unit propagation
 *   4. If not found and other watch is false -> conflict
 */

static inline bool lit_is_true(Solver *solver, Literal lit) {
    uint32_t var = LIT_VAR(lit);
    int8_t assignment = solver->assignments[var];
    if (assignment == 0) return false;
    bool var_value = (assignment == 1);
    bool lit_negated = LIT_IS_NEG(lit);
    return var_value != lit_negated;
}

static inline bool lit_is_false(Solver *solver, Literal lit) {
    uint32_t var = LIT_VAR(lit);
    int8_t assignment = solver->assignments[var];
    if (assignment == 0) return false;
    bool var_value = (assignment == 1);
    bool lit_negated = LIT_IS_NEG(lit);
    return var_value == lit_negated;
}

Clause* solver_propagate(Solver *solver) {
    while (solver->propagated_index < solver->trail_size) {
        Assignment *assign = &solver->trail[solver->propagated_index];

        // Get the literal that just became FALSE
        // When var=X is assigned value=V:
        //   - Literal (X, negated=0) has value V
        //   - Literal (X, negated=1) has value !V
        // So the FALSE literal has negated=V (when V=true, negated literal is false)
        uint32_t var = assign->variable;
        bool value = assign->value;
        Literal false_lit = LIT_FROM_VAR(var, value);  // The literal that's now FALSE

        WatchList *wl = &solver->watch_lists[false_lit];

        // Iterate through clauses watching this literal
        uint32_t i = 0;
        while (i < wl->size) {
            Clause *clause = wl->clauses[i];
            solver->propagations++;

            // Find the watched literals in this clause
            // By convention, we keep watched literals at positions 0 and 1
            Literal *lits = clause->literals;
            Literal w0 = lits[0];
            Literal w1 = lits[1];

            // Make sure false_lit is at position 1 (for efficiency)
            if (w0 == false_lit) {
                lits[0] = w1;
                lits[1] = w0;
                w0 = lits[0];
                w1 = lits[1];
            }

            // Now w1 is the false literal, w0 is the other watched literal

            // Check if w0 is already true (clause satisfied)
            if (lit_is_true(solver, w0)) {
                i++;
                continue;
            }

            // Try to find a new literal to watch
            bool found_new_watch = false;
            for (uint32_t j = 2; j < clause->size; j++) {
                Literal lit = lits[j];

                if (!lit_is_false(solver, lit)) {
                    // Found a new literal to watch (unassigned or true)
                    lits[1] = lit;
                    lits[j] = w1;

                    // Remove from current watch list
                    wl->clauses[i] = wl->clauses[--wl->size];

                    // Add to new watch list
                    WatchList *new_wl = &solver->watch_lists[lit];
                    if (new_wl->size >= new_wl->capacity) {
                        new_wl->capacity = (new_wl->capacity == 0) ? 4 : new_wl->capacity * 2;
                        new_wl->clauses = (Clause**)xrealloc(new_wl->clauses,
                                                            new_wl->capacity * sizeof(Clause*));
                    }
                    new_wl->clauses[new_wl->size++] = clause;

                    found_new_watch = true;
                    break;
                }
            }

            if (found_new_watch) {
                // Don't increment i - we removed current clause
                continue;
            }

            // No new watch found - check w0
            if (lit_is_false(solver, w0)) {
                // Both watches are false - CONFLICT!
                return clause;
            }

            // w0 is unassigned - unit propagation
            uint32_t unit_var = LIT_VAR(w0);
            bool unit_value = !LIT_IS_NEG(w0);

            // Check if already assigned
            if (solver->assignments[unit_var] == 0) {
                // Assign the unit literal
                solver->assignments[unit_var] = unit_value ? 1 : -1;

                Assignment new_assign;
                new_assign.variable = unit_var;
                new_assign.value = unit_value;
                new_assign.level = solver->current_level;
                new_assign.reason = clause;

                solver->trail[solver->trail_size++] = new_assign;
                solver->phase_cache[unit_var] = unit_value;

                // Bump variable activity
                vsids_bump_var(solver->vsids, unit_var, solver->var_inc);
            }

            i++;
        }

        // Always increment - newly assigned units will be processed
        // in subsequent iterations of the outer loop
        solver->propagated_index++;
    }

    return NULL;  // No conflict
}
