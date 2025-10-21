#include "solver.h"
#include "dimacs.h"
#include <stdio.h>

static void print_clause(Clause *c) {
    printf("(");
    for (uint32_t i = 0; i < c->size; i++) {
        Literal lit = c->literals[i];
        uint32_t var = LIT_VAR(lit);
        bool neg = LIT_IS_NEG(lit);
        if (i > 0) printf(" | ");
        printf("%s%u", neg ? "¬" : "", var);
    }
    printf(")");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <cnf_file>\n", argv[0]);
        return 1;
    }

    Solver *solver = NULL;
    if (!dimacs_parse_file(argv[1], &solver)) {
        fprintf(stderr, "ERROR: Failed to parse file\n");
        return 1;
    }

    printf("Parsed: %u vars, %u clauses\n", solver->num_vars, solver->num_clauses);

    uint32_t iter = 0;
    uint32_t max_iter = 100;

    while (iter++ < max_iter) {
        printf("\n=== Iteration %u ===\n", iter);
        printf("Trail size: %u, Propagated: %u, Level: %u\n",
               solver->trail_size, solver->propagated_index, solver->current_level);

        // Propagate
        Clause *conflict = solver_propagate(solver);

        if (conflict) {
            printf("CONFLICT: ");
            print_clause(conflict);
            printf("\n");

            if (solver->current_level == 0) {
                printf("\ns UNSATISFIABLE\n");
                return 0;
            }

            // Just backtrack to level 0 for simplicity
            solver->current_level = 0;
            solver->trail_size = 0;
            solver->propagated_index = 0;
            for (uint32_t i = 1; i <= solver->num_vars; i++) {
                solver->assignments[i] = 0;
            }
            continue;
        }

        if (solver->trail_size == solver->num_vars) {
            printf("\ns SATISFIABLE\n");
            solver_print_solution(solver);
            return 0;
        }

        // Make decision
        uint32_t var = vsids_select(solver->vsids, solver->assignments);
        if (var == 0) {
            printf("\ns SATISFIABLE (no more vars)\n");
            return 0;
        }

        solver->current_level++;
        bool value = false;  // Always choose false for simplicity
        solver->assignments[var] = value ? 1 : -1;

        Assignment assign;
        assign.variable = var;
        assign.value = value;
        assign.level = solver->current_level;
        assign.reason = NULL;

        solver->trail[solver->trail_size++] = assign;

        printf("Decision: x%u = %s at level %u\n", var, value ? "TRUE" : "FALSE", solver->current_level);
    }

    printf("\nTIMEOUT\n");
    return 1;
}
