/*********************************************************************
 * BSAT Competition Solver - Exact Syntactic Symmetry Breaking
 *
 * Detects exact formula automorphisms (variable transpositions and
 * BreakID-style matrix row-interchange symmetry groups) verified against
 * an exact clause hash set, and emits equisatisfiable lex-leader
 * symmetry-breaking clauses over disjoint variable supports.
 *********************************************************************/

#ifndef BSAT_SYMMETRY_H
#define BSAT_SYMMETRY_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

struct Solver;

/* Run bounded exact syntactic symmetry detection and lex-leader symmetry
 * breaking at decision level 0 (non-proof, non-assumption, non-reuse mode).
 * Returns false only on UNSAT or OOM error. */
bool solver_break_symmetries(struct Solver *s, uint64_t tick_budget);

#endif /* BSAT_SYMMETRY_H */
