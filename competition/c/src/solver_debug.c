#include "solver.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Copied from solver.c with debug output

extern void solver_assign(Solver *solver, uint32_t var, bool value, Clause *reason);
extern void solver_backtrack(Solver *solver, uint32_t level);
extern void solver_add_clause(Solver *solver, Literal *lits, uint32_t size);

static void print_clause(const char *label, Clause *c) {
    fprintf(stderr, "%s: (", label);
    for (uint32_t i = 0; i < c->size; i++) {
        Literal lit = c->literals[i];
        uint32_t var = LIT_VAR(lit);
        bool neg = LIT_IS_NEG(lit);
        if (i > 0) fprintf(stderr, " | ");
        fprintf(stderr, "%s%u", neg ? "¬" : "", var);
    }
    fprintf(stderr, ")\n");
}

bool solver_solve_debug(Solver *solver) {
    uint32_t iteration = 0;
    uint32_t max_iterations = 10000;

    while (iteration++ < max_iterations) {
        if (iteration % 10 == 0) {
            fprintf(stderr, "\n=== Iteration %u ===\n", iteration);
            fprintf(stderr, "Trail: %u, Propagated: %u, Level: %u\n",
                   solver->trail_size, solver->propagated_index, solver->current_level);
        }

        // Propagate assignments
        Clause *conflict = solver_propagate(solver);

        if (conflict) {
            fprintf(stderr, "\nCONFLICT at iteration %u, level %u\n", iteration, solver->current_level);
            print_clause("Conflict clause", conflict);

            solver->conflicts++;

            if (solver->current_level == 0) {
                fprintf(stderr, "UNSAT - conflict at level 0\n");
                return false;
            }

            uint32_t backtrack_level;
            Clause *learned = NULL;
            solver_analyze_conflict(solver, conflict, &backtrack_level, &learned);

            if (learned) {
                print_clause("Learned clause", learned);
                fprintf(stderr, "Backtrack level: %u\n", backtrack_level);

                // Backtrack
                fprintf(stderr, "Backtracking from %u to %u\n", solver->current_level, backtrack_level);
                solver_backtrack(solver, backtrack_level);

                // Add learned clause
                uint32_t clause_idx = solver->num_clauses;
                solver_add_clause(solver, learned->literals, learned->size);
                solver->learned_clauses++;

                // Assign asserting literal
                if (learned->size >= 1) {
                    uint32_t var = LIT_VAR(learned->literals[0]);
                    bool value = !LIT_IS_NEG(learned->literals[0]);
                    Clause *added_clause = solver->clauses[clause_idx];

                    fprintf(stderr, "Asserting literal: x%u = %s\n", var, value ? "TRUE" : "FALSE");

                    if (solver->assignments[var] == 0) {
                        solver_assign(solver, var, value, added_clause);
                    } else {
                        fprintf(stderr, "WARNING: Asserting literal already assigned! Current value: %d\n",
                               solver->assignments[var]);
                    }
                }

                clause_destroy(learned);
            } else {
                fprintf(stderr, "No learned clause generated\n");
                solver_backtrack(solver, solver->current_level - 1);
            }

        } else {
            // No conflict - make decision or finish
            if (solver->trail_size == solver->num_vars) {
                fprintf(stderr, "\nSAT - all variables assigned\n");
                return true;
            }

            // Select next variable
            uint32_t var = vsids_select(solver->vsids, solver->assignments);
            if (var == 0) {
                fprintf(stderr, "\nSAT - no unassigned variables\n");
                return true;
            }

            bool value = solver->phase_cache[var];

            solver->current_level++;
            solver->level_starts[solver->current_level] = solver->trail_size;

            if (iteration % 10 == 0) {
                fprintf(stderr, "Decision: x%u = %s at level %u\n", var, value ? "TRUE" : "FALSE", solver->current_level);
            }

            solver->assignments[var] = value ? 1 : -1;

            Assignment assign;
            assign.variable = var;
            assign.value = value;
            assign.level = solver->current_level;
            assign.reason = NULL;

            solver->trail[solver->trail_size++] = assign;
            solver->phase_cache[var] = value;
            solver->decisions++;
        }
    }

    fprintf(stderr, "\nTIMEOUT after %u iterations\n", max_iterations);
    return false;
}
