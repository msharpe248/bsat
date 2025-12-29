/*********************************************************************
 * BSAT Competition Solver - Bounded Variable Elimination (BVE)
 *
 * Implements SatELite-style variable elimination preprocessing.
 * Reference: Eén & Biere, "Effective Preprocessing in SAT through
 *            Variable and Clause Elimination" (SAT 2005)
 *
 * Theory:
 * Variable elimination resolves away a variable by computing all
 * resolvents. For variable x:
 *   - Let P = clauses containing x (positive occurrences)
 *   - Let N = clauses containing ¬x (negative occurrences)
 *   - Resolvents R = all non-tautological resolutions of P × N
 *   - Elimination is beneficial if |R| <= |P| + |N|
 *
 * After solving, eliminated variables are reconstructed by processing
 * the elimination stack in reverse order.
 *********************************************************************/

#ifndef BSAT_ELIM_H
#define BSAT_ELIM_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

// Forward declaration
struct Solver;

/*********************************************************************
 * Occurrence List
 *
 * Tracks which clauses contain a given literal.
 * Used for efficient variable elimination cost calculation.
 *********************************************************************/

typedef struct OccList {
    CRef*    clauses;    // Array of clause references
    uint32_t size;       // Number of clauses in list
    uint32_t capacity;   // Allocated capacity
} OccList;

/*********************************************************************
 * Elimination Stack Entry
 *
 * Records information needed to reconstruct eliminated variables.
 * After solving, we process the stack in reverse order to assign
 * values to eliminated variables.
 *********************************************************************/

typedef struct ElimEntry {
    Var      var;         // The eliminated variable
    Lit*     clause;      // Copy of one clause containing +var (for reconstruction)
    uint32_t clause_size; // Size of the saved clause
} ElimEntry;

/*********************************************************************
 * Elimination State
 *
 * All state needed for variable elimination, kept separate from
 * main solver for clarity.
 *********************************************************************/

typedef struct ElimState {
    // Occurrence lists: occs[lit] = clauses containing lit
    OccList* occs;
    uint32_t occs_capacity;  // Capacity (2 * num_vars)

    // Elimination stack for solution reconstruction
    ElimEntry* stack;
    uint32_t   stack_size;
    uint32_t   stack_capacity;

    // Per-variable elimination status
    bool*    eliminated;     // eliminated[v] = true if v was eliminated
    uint32_t elim_capacity;  // Capacity of eliminated array

    // Statistics
    uint64_t vars_eliminated;
    uint64_t clauses_removed;
    uint64_t resolvents_added;

    // Track resolvent CRefs for debugging/dumping
    CRef*    resolvent_crefs;
    uint32_t resolvent_crefs_size;
    uint32_t resolvent_crefs_capacity;
} ElimState;

/*********************************************************************
 * Initialization and Cleanup
 *********************************************************************/

// Initialize elimination state (call after solver has variables)
void elim_init(struct Solver* s);

// Free all elimination structures
void elim_free(struct Solver* s);

/*********************************************************************
 * Occurrence List Management
 *********************************************************************/

// Build occurrence lists from current clause database
void elim_build_occs(struct Solver* s);

// Add clause to occurrence lists for all its literals
void elim_add_occ(struct Solver* s, Lit lit, CRef cref);

// Remove clause from occurrence lists for all its literals
void elim_remove_occ(struct Solver* s, Lit lit, CRef cref);

// Clear all occurrence lists (but keep allocated memory)
void elim_clear_occs(struct Solver* s);

/*********************************************************************
 * Variable Elimination
 *********************************************************************/

// Calculate the cost of eliminating a variable
// Returns number of resolvents that would be created (filtering tautologies)
// Returns -1 if elimination is not worthwhile (too many resolvents)
int elim_cost(struct Solver* s, Var v);

// Check if a resolvent would be a tautology (contains both x and ¬x)
bool elim_is_tautology(const Lit* c1, uint32_t s1,
                       const Lit* c2, uint32_t s2,
                       Var pivot);

// Eliminate a single variable
// Returns true on success, false if elimination not possible
bool elim_eliminate_var(struct Solver* s, Var v);

// Main BVE preprocessing loop
// Eliminates variables with positive cost-benefit ratio
// Returns number of variables eliminated
uint32_t elim_preprocess(struct Solver* s);

/*********************************************************************
 * Solution Reconstruction
 *********************************************************************/

// After solving, extend the model to include eliminated variables
// Must be called after solver finds SAT, before returning model
void elim_extend_model(struct Solver* s);

/*********************************************************************
 * Utility Functions
 *********************************************************************/

// Check if a variable has been eliminated
static inline bool elim_is_eliminated(const struct Solver* s, Var v);

// Get occurrence list for a literal
static inline OccList* elim_get_occs(struct Solver* s, Lit lit);

// Print elimination statistics
void elim_print_stats(const struct Solver* s);

#endif // BSAT_ELIM_H
