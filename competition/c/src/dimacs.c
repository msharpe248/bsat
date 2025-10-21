#include "dimacs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * DIMACS FORMAT PARSER
 * ============================================================================ */

bool dimacs_parse_file(const char *filename, Solver **solver_out) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "ERROR: Cannot open file '%s'\n", filename);
        return false;
    }

    char line[65536];  // Buffer for reading lines
    uint32_t num_vars = 0;
    uint32_t num_clauses = 0;
    Solver *solver = NULL;

    Literal clause_buffer[1024];  // Buffer for parsing clause
    uint32_t clause_size = 0;

    while (fgets(line, sizeof(line), file)) {
        // Skip whitespace
        char *p = line;
        while (isspace(*p)) p++;

        // Skip empty lines and comments
        if (*p == '\0' || *p == 'c') {
            continue;
        }

        // Parse header line
        if (*p == 'p') {
            if (sscanf(p, "p cnf %u %u", &num_vars, &num_clauses) != 2) {
                fprintf(stderr, "ERROR: Invalid header line: %s", line);
                fclose(file);
                return false;
            }

            // Create solver with parsed dimensions
            solver = solver_create(num_vars);
            continue;
        }

        // Parse clause line
        if (!solver) {
            fprintf(stderr, "ERROR: Clause before header\n");
            fclose(file);
            return false;
        }

        // Parse integers from line
        clause_size = 0;
        char *token = strtok(p, " \t\n");
        while (token) {
            int lit_int = atoi(token);

            if (lit_int == 0) {
                // End of clause - add to solver
                if (clause_size > 0) {
                    solver_add_clause(solver, clause_buffer, clause_size);
                }
                clause_size = 0;
            } else {
                // Convert DIMACS literal to internal representation
                uint32_t var = abs(lit_int);
                bool negated = (lit_int < 0);
                Literal lit = LIT_FROM_VAR(var, negated);

                clause_buffer[clause_size++] = lit;

                if (clause_size >= 1024) {
                    fprintf(stderr, "ERROR: Clause too large (max 1024 literals)\n");
                    solver_destroy(solver);
                    fclose(file);
                    return false;
                }
            }

            token = strtok(NULL, " \t\n");
        }
    }

    fclose(file);

    if (!solver) {
        fprintf(stderr, "ERROR: No header found in file\n");
        return false;
    }

    *solver_out = solver;
    return true;
}

void dimacs_write_solution(Solver *solver, bool sat) {
    if (sat) {
        printf("s SATISFIABLE\n");
        printf("v ");
        for (uint32_t v = 1; v <= solver->num_vars; v++) {
            int value = solver->assignments[v];
            if (value == 1) {
                printf("%u ", v);
            } else if (value == -1) {
                printf("-%u ", v);
            } else {
                // Unassigned - can be either, choose positive
                printf("%u ", v);
            }
        }
        printf("0\n");
    } else {
        printf("s UNSATISFIABLE\n");
    }
}
