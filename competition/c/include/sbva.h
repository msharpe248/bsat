/*********************************************************************
 * BSAT Competition Solver - Structured Bounded Variable Addition (SBVA)
 *
 * Implements Haberlandt-Green-Heule Structured Bounded Variable Addition:
 * arbitrary-length clause factoring with a 3-hop structural adjacency
 * tie-breaking heuristic and iterative auxiliary variable chaining.
 *********************************************************************/

#ifndef BSAT_SBVA_H
#define BSAT_SBVA_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

struct Solver;

/* Run bounded Structured Bounded Variable Addition at decision level 0.
 * Introduces auxiliary variables (> s->factor_original_vars) to compress
 * structured clause rectangles while preserving equisatisfiability and
 * original-variable models. Returns false only on OOM error. */
bool solver_sbva(struct Solver *s, uint64_t tick_budget, uint32_t max_aux_vars, uint32_t min_gain);

#endif /* BSAT_SBVA_H */
