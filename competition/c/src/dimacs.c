/*********************************************************************
 * BSAT Competition Solver - DIMACS CNF Parser Implementation
 *********************************************************************/

#include "../include/dimacs.h"
#include "../include/arena.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Maximum line length for reading
#define MAX_LINE 1048576  // 1MB should be enough for any line

// Temporary buffer for clause literals
#define MAX_CLAUSE_SIZE 100000

/*********************************************************************
 * Helper Functions
 *********************************************************************/

// Skip whitespace
static const char* skip_whitespace(const char* p) {
    while (isspace(*p)) p++;
    return p;
}

// Skip rest of line
static const char* skip_line(const char* p) {
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
    return p;
}

// Parse integer
static const char* parse_int(const char* p, int* value) {
    p = skip_whitespace(p);

    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (!isdigit(*p)) {
        return NULL;  // Not a number
    }

    int val = 0;
    while (isdigit(*p)) {
        val = val * 10 + (*p - '0');
        p++;
    }

    *value = sign * val;
    return p;
}

/*********************************************************************
 * Main Parser
 *********************************************************************/

DimacsError dimacs_parse_stream(Solver* s, FILE* file) {
    if (!s || !file) {
        return DIMACS_ERROR_FILE;
    }

    char* line = (char*)malloc(MAX_LINE);
    if (!line) {
        return DIMACS_ERROR_MEMORY;
    }

    Lit* clause = (Lit*)malloc(MAX_CLAUSE_SIZE * sizeof(Lit));
    if (!clause) {
        free(line);
        return DIMACS_ERROR_MEMORY;
    }

    uint32_t expected_vars = 0;
    uint32_t expected_clauses = 0;
    uint32_t parsed_clauses = 0;
    bool header_found = false;

    DimacsError result = DIMACS_OK;

    // Read line by line
    while (fgets(line, MAX_LINE, file)) {
        #ifdef DEBUG
        if (getenv("DEBUG_CDCL")) {
            printf("[DIMACS] Read line: %s", line);
        }
        #endif
        const char* p = skip_whitespace(line);

        // Skip empty lines
        if (*p == '\0' || *p == '\n') {
            continue;
        }

        // Skip comments (except special ones)
        if (*p == 'c') {
            // Could parse special comments here (e.g., "c ind" for independent set)
            continue;
        }

        // Parse header
        if (*p == 'p') {
            if (header_found) {
                result = DIMACS_ERROR_FORMAT;  // Multiple headers
                goto cleanup;
            }

            p = skip_whitespace(p + 1);

            // Check for "cnf"
            if (strncmp(p, "cnf", 3) != 0) {
                result = DIMACS_ERROR_FORMAT;
                goto cleanup;
            }
            p += 3;

            // Parse number of variables
            int nvars;
            p = parse_int(p, &nvars);
            if (!p || nvars < 0 || nvars > MAX_VARS) {
                result = DIMACS_ERROR_FORMAT;
                goto cleanup;
            }
            expected_vars = (uint32_t)nvars;

            // Parse number of clauses
            int nclauses;
            p = parse_int(p, &nclauses);
            if (!p || nclauses < 0 || nclauses > MAX_CLAUSES) {
                result = DIMACS_ERROR_FORMAT;
                goto cleanup;
            }
            expected_clauses = (uint32_t)nclauses;

            header_found = true;

            // Ensure we have enough variables
            while (s->num_vars < expected_vars) {
                solver_new_var(s);
            }

            // Reserve arena capacity based on problem size
            size_t estimated_capacity = estimate_arena_size(expected_clauses, expected_vars);
            if (!arena_reserve(s->arena, estimated_capacity)) {
                result = DIMACS_ERROR_MEMORY;
                goto cleanup;
            }

            continue;
        }

        // Parse clause
        if (!header_found) {
            // Allow clauses before header for lenient parsing
            // Deduce problem size from clauses
        }

        uint32_t clause_size = 0;
        const char* clause_start = p;

        while (*p) {
            int lit;
            const char* next = parse_int(p, &lit);

            if (!next) {
                // Check if line continues
                if (strchr(p, '\n')) {
                    break;  // End of line, incomplete clause
                }
                result = DIMACS_ERROR_FORMAT;
                goto cleanup;
            }

            p = next;

            if (lit == 0) {
                // End of clause
                break;
            }

            // Ensure variable exists
            Var v = abs(lit);
            if (v > MAX_VARS) {
                result = DIMACS_ERROR_SIZE;
                goto cleanup;
            }

            while (s->num_vars < v) {
                solver_new_var(s);
            }

            // Add literal to clause
            if (clause_size >= MAX_CLAUSE_SIZE) {
                result = DIMACS_ERROR_SIZE;
                goto cleanup;
            }
            clause[clause_size++] = fromDimacs(lit);
        }

        // Add clause to solver
        if (clause_size > 0) {
            #ifdef DEBUG
            if (getenv("DEBUG_CDCL")) {
                printf("[DIMACS] Adding clause %u with %u literals\n", parsed_clauses + 1, clause_size);
            }
            #endif
            if (!solver_add_clause(s, clause, clause_size)) {
                // Solver detected UNSAT during clause addition (empty clause)
                // This is OK, continue parsing to validate format
            }
            parsed_clauses++;
        }
    }

    // Check if we got the expected number of clauses (warning only)
    if (header_found && parsed_clauses != expected_clauses) {
        // This is common in competition instances, just warn
        // fprintf(stderr, "Warning: expected %u clauses but parsed %u\n",
        //         expected_clauses, parsed_clauses);
    }

cleanup:
    free(clause);
    free(line);
    return result;
}

DimacsError dimacs_parse_file(Solver* s, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return DIMACS_ERROR_FILE;
    }

    DimacsError result = dimacs_parse_stream(s, file);
    fclose(file);
    return result;
}

DimacsError dimacs_parse_string(Solver* s, const char* str) {
    if (!s || !str) {
        return DIMACS_ERROR_FILE;
    }

    // Create a memory stream
    FILE* stream = fmemopen((void*)str, strlen(str), "r");
    if (!stream) {
        return DIMACS_ERROR_MEMORY;
    }

    DimacsError result = dimacs_parse_stream(s, stream);
    fclose(stream);
    return result;
}

/*********************************************************************
 * Error Messages
 *********************************************************************/

const char* dimacs_error_string(DimacsError err) {
    switch (err) {
        case DIMACS_OK:
            return "Success";
        case DIMACS_ERROR_FILE:
            return "Cannot open or read file";
        case DIMACS_ERROR_FORMAT:
            return "Invalid DIMACS format";
        case DIMACS_ERROR_MEMORY:
            return "Out of memory";
        case DIMACS_ERROR_SIZE:
            return "Problem too large";
        default:
            return "Unknown error";
    }
}

/*********************************************************************
 * Output Functions
 *********************************************************************/

void dimacs_write_solution(const Solver* s, FILE* out) {
    if (s->result == TRUE) {
        fprintf(out, "s SATISFIABLE\n");
        fprintf(out, "v ");

        for (Var v = 1; v <= s->num_vars; v++) {
            lbool val = solver_model_value(s, v);
            if (val == TRUE) {
                fprintf(out, "%u ", v);
            } else if (val == FALSE) {
                fprintf(out, "-%u ", v);
            }
            // Skip UNDEF variables

            // Line wrapping
            if (v % 20 == 0) {
                fprintf(out, "\nv ");
            }
        }
        fprintf(out, "0\n");
    } else if (s->result == FALSE) {
        fprintf(out, "s UNSATISFIABLE\n");
    } else {
        fprintf(out, "s UNKNOWN\n");
    }
}

void dimacs_write_proof(const Solver* s, FILE* out) {
    // TODO: Implement proof output (DRAT format)
    (void)s;
    (void)out;
}

void dimacs_write_cnf(const Solver* s, FILE* out) {
    fprintf(out, "p cnf %u %u\n", s->num_vars, s->num_original);

    // Write only original clauses
    for (uint32_t i = 0; i < s->num_clauses; i++) {
        CRef cref = s->clauses[i];
        if (!clause_learned(s->arena, cref)) {
            uint32_t size = CLAUSE_SIZE(s->arena, cref);
            Lit* lits = CLAUSE_LITS(s->arena, cref);

            for (uint32_t j = 0; j < size; j++) {
                fprintf(out, "%d ", toDimacs(lits[j]));
            }
            fprintf(out, "0\n");
        }
    }
}