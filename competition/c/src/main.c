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

    // Solve
    clock_t start = clock();
    bool sat = solver_solve(solver);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    // Print result
    if (sat) {
        printf("s SATISFIABLE\n");
        solver_print_solution(solver);
    } else {
        printf("s UNSATISFIABLE\n");
    }

    // Print statistics
    fprintf(stderr, "c Time:            %.4f seconds\n", elapsed);
    solver_print_stats(solver);

    // Cleanup
    solver_destroy(solver);

    return 0;
}
