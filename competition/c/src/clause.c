#include "solver.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CLAUSE MANAGEMENT
 * ============================================================================ */

Clause* clause_create(Literal *lits, uint32_t size, bool learned) {
    // Allocate clause with flexible array member
    Clause *clause = (Clause*)xmalloc(sizeof(Clause) + size * sizeof(Literal));

    clause->size = size;
    clause->lbd = 0;  // Will be computed later for learned clauses
    clause->learned = learned;
    clause->activity = 0.0f;

    // Copy literals
    memcpy(clause->literals, lits, size * sizeof(Literal));

    return clause;
}

void clause_destroy(Clause *clause) {
    free(clause);
}

/* ============================================================================
 * LBD COMPUTATION
 * ============================================================================
 * Literal Block Distance: count number of distinct decision levels in clause
 * Lower LBD = better quality learned clause
 */
uint32_t clause_lbd(Clause *clause, Assignment *trail, uint32_t trail_size) {
    // Use a simple array to track which levels we've seen
    // Maximum decision level is bounded by trail size
    static bool seen[10000];  // Assume max 10000 decision levels
    uint32_t distinct_levels = 0;

    // Clear seen array
    memset(seen, 0, sizeof(seen));

    // Count distinct decision levels
    for (uint32_t i = 0; i < clause->size; i++) {
        Literal lit = clause->literals[i];
        uint32_t var = LIT_VAR(lit);

        // Find this variable's assignment on the trail
        // (This is inefficient but correct for now - can optimize later)
        for (uint32_t j = 0; j < trail_size; j++) {  // FIX: Use actual trail_size
            if (trail[j].variable == var) {
                uint32_t level = trail[j].level;
                if (!seen[level]) {
                    seen[level] = true;
                    distinct_levels++;
                }
                break;
            }
        }
    }

    return distinct_levels;
}
