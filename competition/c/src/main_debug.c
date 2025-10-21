#include "solver.h"
#include "dimacs.h"
#include <stdio.h>

// Declared in solver_debug.c
extern bool solver_solve_debug(Solver *solver);

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

    fprintf(stderr, "Parsed: %u vars, %u clauses\n\n", solver->num_vars, solver->num_clauses);

    bool sat = solver_solve_debug(solver);

    if (sat) {
        printf("s SATISFIABLE\n");
        solver_print_solution(solver);
    } else {
        printf("s UNSATISFIABLE\n");
    }

    fprintf(stderr, "\nStatistics:\n");
    solver_print_stats(solver);

    solver_destroy(solver);
    return 0;
}
