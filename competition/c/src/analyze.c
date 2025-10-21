#include "solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Debug flag
extern int is_debug();

/* ============================================================================
 * CONFLICT ANALYSIS USING 1UIP (First Unique Implication Point)
 * ============================================================================
 * Learns a new clause by analyzing the implication graph
 *
 * Algorithm:
 * 1. Start from conflict clause
 * 2. Resolve with reason clauses of variables at current decision level
 * 3. Stop at 1UIP (exactly one variable from current level remains)
 * 4. The learned clause is the negation of literals in the cut
 */

void solver_analyze_conflict(Solver *solver, Clause *conflict,
                            uint32_t *backtrack_level, Clause **learned_out) {
    if (is_debug()) {
        fprintf(stderr, "analyze_conflict() started, conflict clause size=%u\n", conflict->size);
    }

    static Literal learned_lits[10000];
    static bool seen[10000];
    uint32_t learned_size = 0;

    // Clear seen array
    memset(seen, 0, sizeof(seen));

    uint32_t current_level = solver->current_level;
    uint32_t num_current_level = 0;

    Clause *current_clause = conflict;
    Literal resolve_lit = LIT_UNDEF;

    uint32_t loop_count = 0;

    while (true) {
        loop_count++;
        if (is_debug() && loop_count % 10 == 0) {
            fprintf(stderr, "  analyze loop %u: num_current_level=%u\n", loop_count, num_current_level);
        }

        if (loop_count > 1000) {
            // Safety limit - fallback to chronological backtracking
            *learned_out = NULL;
            *backtrack_level = (solver->current_level > 0) ? solver->current_level - 1 : 0;
            return;
        }
        // Add literals from current clause to resolution
        if (is_debug() && loop_count <= 3) {
            fprintf(stderr, "  Processing clause (size=%u):\n", current_clause->size);
        }

        for (uint32_t i = 0; i < current_clause->size; i++) {
            Literal lit = current_clause->literals[i];
            uint32_t var = LIT_VAR(lit);

            // Skip if already seen (prevents re-adding resolved variables)
            if (seen[var]) {
                if (is_debug() && loop_count <= 3) {
                    fprintf(stderr, "    Skipping x%u (already seen)\n", var);
                }
                continue;
            }

            seen[var] = true;

            // Find this variable's decision level
            uint32_t lit_level = 0;
            for (uint32_t j = 0; j < solver->trail_size; j++) {
                if (solver->trail[j].variable == var) {
                    lit_level = solver->trail[j].level;
                    break;
                }
            }

            if (lit_level == current_level) {
                num_current_level++;
                if (is_debug() && loop_count <= 3) {
                    fprintf(stderr, "    Added x%u at level %u (current), num_current_level=%u\n",
                           var, lit_level, num_current_level);
                }
            } else if (lit_level > 0) {
                // Add to learned clause (these are from earlier levels)
                learned_lits[learned_size++] = lit;
                if (is_debug() && loop_count <= 3) {
                    fprintf(stderr, "    Added x%u at level %u (learned)\n", var, lit_level);
                }
            }
        }

        // Find the next variable to resolve
        // Walk backwards through trail to find most recent current-level assignment
        resolve_lit = LIT_UNDEF;

        if (is_debug() && loop_count == 1) {
            fprintf(stderr, "Trail size: %u, current_level: %u\n", solver->trail_size, current_level);
            fprintf(stderr, "Variables in seen set at current level:\n");
            for (uint32_t v = 1; v <= solver->num_vars; v++) {
                if (seen[v]) {
                    // Find this variable on trail
                    for (uint32_t j = 0; j < solver->trail_size; j++) {
                        if (solver->trail[j].variable == v) {
                            fprintf(stderr, "  x%u at level %u, reason=%p\n",
                                   v, solver->trail[j].level, (void*)solver->trail[j].reason);
                            break;
                        }
                    }
                }
            }
        }

        for (int i = (int)solver->trail_size - 1; i >= 0; i--) {
            Assignment *assign = &solver->trail[i];
            if (assign->level != current_level) {
                continue;
            }

            uint32_t var = assign->variable;

            if (is_debug() && loop_count <= 3) {
                fprintf(stderr, "  Checking x%u (level %u, seen=%d, reason=%p)\n",
                       var, assign->level, seen[var], (void*)assign->reason);
            }

            if (seen[var]) {
                // Get the literal that's FALSE given this assignment
                // When var=V, literal (var, V) is the false literal
                resolve_lit = LIT_FROM_VAR(var, assign->value);
                num_current_level--;
                // NOTE: Don't remove from seen! Python keeps variables in seen forever.
                // This prevents infinite loops when reason clause contains the variable.

                // Bump variable activity (involved in conflict)
                vsids_bump_var(solver->vsids, var, solver->var_inc);

                // Now check if we've reached 1UIP (after decrement)
                if (num_current_level == 0) {
                    // This is the 1UIP - we're done
                    // resolve_lit is set, will be added at position 0 after the loop
                    if (is_debug()) {
                        fprintf(stderr, "Found 1UIP: var=%u\n", var);
                    }
                    break;  // Exit the for loop and the while loop
                }

                // Get reason clause for this assignment and continue resolving
                current_clause = assign->reason;
                if (!current_clause) {
                    // This is a decision variable - shouldn't happen before 1UIP
                    if (is_debug()) {
                        fprintf(stderr, "WARNING: Found decision variable x%u\n", var);
                    }
                    break;
                }

                break;  // Exit the for loop to continue with next clause
            }
        }

        if (resolve_lit == LIT_UNDEF) {
            if (is_debug()) {
                fprintf(stderr, "ERROR: Could not find variable to resolve! num_current_level=%u\n", num_current_level);
            }
            // Either couldn't find a variable to resolve, or found 1UIP
            break;
        }

        if (num_current_level == 0) {
            break;
        }
    }

    // Add the 1UIP literal to learned clause (at position 0)
    // This is the asserting literal
    if (resolve_lit != LIT_UNDEF) {
        // Shift learned_lits to make room at position 0
        for (int i = (int)learned_size; i > 0; i--) {
            learned_lits[i] = learned_lits[i - 1];
        }
        learned_lits[0] = LIT_NEGATE(resolve_lit);
        learned_size++;
    }

    // Compute backtrack level (second highest decision level in learned clause)
    uint32_t max_level = 0;
    uint32_t second_max_level = 0;

    for (uint32_t i = 0; i < learned_size; i++) {
        Literal lit = learned_lits[i];
        uint32_t var = LIT_VAR(lit);

        // Find decision level
        uint32_t lit_level = 0;
        for (uint32_t j = 0; j < solver->trail_size; j++) {
            if (solver->trail[j].variable == var) {
                lit_level = solver->trail[j].level;
                break;
            }
        }

        if (lit_level > max_level) {
            second_max_level = max_level;
            max_level = lit_level;
        } else if (lit_level > second_max_level) {
            second_max_level = lit_level;
        }
    }

    *backtrack_level = second_max_level;

    // Create learned clause
    if (learned_size > 0) {
        *learned_out = clause_create(learned_lits, learned_size, true);
        (*learned_out)->lbd = clause_lbd(*learned_out, solver->trail, solver->trail_size);
    } else {
        *learned_out = NULL;
    }
}
