/*********************************************************************
 * BSAT Competition Solver - DIMACS CNF Parser
 *
 * Parser for DIMACS CNF format used in SAT competitions.
 *********************************************************************/

#ifndef BSAT_DIMACS_H
#define BSAT_DIMACS_H

#include "solver.h"
#include <stdio.h>

/*********************************************************************
 * DIMACS Parser Result
 *********************************************************************/

typedef enum {
    DIMACS_OK = 0,           // Success
    DIMACS_ERROR_FILE,       // Cannot open/read file
    DIMACS_ERROR_FORMAT,     // Invalid format
    DIMACS_ERROR_MEMORY,     // Out of memory
    DIMACS_ERROR_SIZE        // Problem too large
} DimacsError;

/*********************************************************************
 * Parser API
 *********************************************************************/

// Parse DIMACS file and add clauses to solver
// Returns DIMACS_OK on success, error code otherwise
DimacsError dimacs_parse_file(Solver* s, const char* filename);

// Parse DIMACS from FILE stream
DimacsError dimacs_parse_stream(Solver* s, FILE* file);

// Parse DIMACS from string buffer
DimacsError dimacs_parse_string(Solver* s, const char* str);

// Parse DIMACS CNF with optional CryptoMiniSat-style XOR lines ("x l1 l2 ... 0").
// If out_header_vars != NULL, stores the original variable count from "p cnf".
DimacsError dimacs_parse_stream_xor(Solver* s, FILE* file, uint32_t* out_header_vars);
DimacsError dimacs_parse_file_xor(Solver* s, const char* filename, uint32_t* out_header_vars);
DimacsError dimacs_parse_string_xor(Solver* s, const char* str, uint32_t* out_header_vars);

// Get error description
const char* dimacs_error_string(DimacsError err);

/*********************************************************************
 * Output Functions
 *********************************************************************/

// Write solution in DIMACS format
void dimacs_write_solution(const Solver* s, FILE* out);

// Write UNSAT proof/core (if available)
bool dimacs_write_proof(const Solver* s, FILE* out);

// Write CNF in DIMACS format (for debugging)
void dimacs_write_cnf(const Solver* s, FILE* out);

#endif // BSAT_DIMACS_H