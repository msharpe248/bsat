/*********************************************************************
 * BSAT Competition Solver - Modern Inprocessing Engine
 *
 * Tick-budgeted inprocessing pipeline (Kissat / CaDiCaL style):
 *  1. Root clause cleanup (remove satisfied clauses, strip level-0 false lits)
 *  2. Binary transitive reduction + failed-literal discovery
 *  3. In-place Tarjan SCC equivalence substitution (original + learned clauses)
 *  4. Forward subsumption & self-subsuming resolution (SSR) with 64-bit signatures
 *  5. Gate-aware bounded variable elimination (BVE: Equiv, AND, ITE, XOR gates)
 *  6. O(n) prefix-sharing clause vivification (irredundant + Tier-1/Tier-2)
 *  7. Root-prioritized failed-literal probing with hyper-binary resolution (HBR)
 *********************************************************************/

#ifndef BSAT_INPROCESS_H
#define BSAT_INPROCESS_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

struct Solver;

/* Run a full tick-budgeted preprocessing or inprocessing round at level 0.
 * Returns false if root UNSAT is derived (sets s->base_unsat / s->result = FALSE). */
bool solver_inprocess(struct Solver *s, bool preprocessing);

/* Rebuild all watch lists cleanly from active clauses in s->clauses and s->learnts. */
void solver_rebuild_watches(struct Solver *s);

#endif // BSAT_INPROCESS_H
