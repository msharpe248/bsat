#ifndef DIMACS_H
#define DIMACS_H

#include "solver.h"

/* ============================================================================
 * DIMACS FORMAT PARSER
 * ============================================================================
 * Parses CNF files in DIMACS format:
 *
 * c This is a comment
 * p cnf 5 3
 * 1 -2 3 0
 * -1 2 0
 * 4 5 0
 *
 * Header: p cnf <num_vars> <num_clauses>
 * Clauses: space-separated integers, terminated by 0
 *   Positive integer N -> variable N
 *   Negative integer -N -> negation of variable N
 */

// Parse DIMACS CNF file and populate solver
// Returns: true on success, false on parse error
bool dimacs_parse_file(const char *filename, Solver **solver_out);

// Write solution in DIMACS format
// SAT: s SATISFIABLE
//      v <assignment>
// UNSAT: s UNSATISFIABLE
void dimacs_write_solution(Solver *solver, bool sat);

#endif // DIMACS_H
