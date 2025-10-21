#include "solver.h"
#include "dimacs.h"
#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <cnf_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    // Parse DIMACS file
    Solver *solver = NULL;
    if (!dimacs_parse_file(filename, &solver)) {
        fprintf(stderr, "ERROR: Failed to parse '%s'\n", filename);
        return 1;
    }

    fprintf(stderr, "c Parsed CNF: %u variables, %u clauses\n",
            solver->num_vars, solver->num_clauses);

    // Solve with iteration limit
    uint32_t max_iterations = 10000;
    uint32_t iteration = 0;

    while (iteration++ < max_iterations) {
        // Propagate assignments
        Clause *conflict = solver_propagate(solver);

        if (iteration % 100 == 0) {
            fprintf(stderr, "c Iteration %u: trail_size=%u, propagated=%u, level=%u\n",
                    iteration, solver->trail_size, solver->propagated_index, solver->current_level);
        }

        if (conflict) {
            fprintf(stderr, "c Conflict at iteration %u\n", iteration);
            solver->conflicts++;

            if (solver->current_level == 0) {
                printf("s UNSATISFIABLE\n");
                solver_destroy(solver);
                return 0;
            }

            // For now, just backtrack to level 0
            solver->current_level = 0;
            solver->trail_size = 0;
            solver->propagated_index = 0;
            for (uint32_t i = 1; i <= solver->num_vars; i++) {
                solver->assignments[i] = 0;
            }
            continue;
        }

        // Check if done
        if (solver->trail_size == solver->num_vars) {
            printf("s SATISFIABLE\n");
            solver_print_solution(solver);
            solver_destroy(solver);
            return 0;
        }

        // Make decision
        uint32_t var = vsids_select(solver->vsids, solver->assignments);
        if (var == 0) {
            printf("s SATISFIABLE\n");
            solver_print_solution(solver);
            solver_destroy(solver);
            return 0;
        }

        solver->current_level++;
        bool value = solver->phase_cache[var];
        solver->assignments[var] = value ? 1 : -1;

        Assignment assign;
        assign.variable = var;
        assign.value = value;
        assign.level = solver->current_level;
        assign.reason = NULL;

        solver->trail[solver->trail_size++] = assign;
        solver->phase_cache[var] = value;
    }

    fprintf(stderr, "c TIMEOUT after %u iterations\n", max_iterations);
    printf("s UNKNOWN\n");
    solver_destroy(solver);
    return 0;
}
