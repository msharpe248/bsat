/*********************************************************************
 * BSAT Competition Solver - Local Search Hybridization
 *
 * WalkSAT-style local search for SAT instances.
 *********************************************************************/

#ifndef BSAT_LOCAL_SEARCH_H
#define BSAT_LOCAL_SEARCH_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct Solver Solver;

/*********************************************************************
 * Local Search State
 *********************************************************************/

typedef struct LocalSearchState {
    // Current assignment (1-indexed)
    bool*    assignment;     // assignment[v] = true/false for variable v
    uint32_t num_vars;

    // Clause satisfaction tracking
    uint32_t* num_true_lits;  // num_true_lits[c] = count of true literals in clause c
    uint32_t  num_unsat;      // Number of unsatisfied clauses

    // Break counts (for focused random walk)
    int32_t* break_count;    // break_count[v] = net unsatisfied clauses if we flip v

    // Clause data (copied from solver for fast access)
    Lit**    clause_lits;    // clause_lits[c] = array of literals in clause c
    uint32_t* clause_sizes;   // clause_sizes[c] = size of clause c
    uint32_t num_clauses;

    // Occurrence lists (for break count updates)
    uint32_t** pos_occs;     // pos_occs[v] = clauses where v appears positively
    uint32_t*  pos_occ_count;
    uint32_t** neg_occs;     // neg_occs[v] = clauses where v appears negatively
    uint32_t*  neg_occ_count;

    // Statistics
    uint64_t flips;
    uint64_t restarts;
} LocalSearchState;

/*********************************************************************
 * Local Search API
 *********************************************************************/

/**
 * Initialize local search state from solver.
 * Returns NULL on allocation failure.
 */
LocalSearchState* local_search_init(Solver* s);

/**
 * Free local search state.
 */
void local_search_free(LocalSearchState* ls);

/**
 * Run WalkSAT from current solver state.
 * Returns true if a satisfying assignment was found.
 *
 * @param s       Solver (used to read current phases and write back solution)
 * @param ls      Local search state
 * @param max_flips  Maximum number of variable flips
 * @param noise   Probability of random walk (0.0 to 1.0)
 */
bool local_search_run(Solver* s, LocalSearchState* ls, uint32_t max_flips, double noise);

/**
 * Copy local search solution back to solver.
 * Should only be called after local_search_run returns true.
 */
void local_search_copy_solution(Solver* s, LocalSearchState* ls);

#endif // BSAT_LOCAL_SEARCH_H
