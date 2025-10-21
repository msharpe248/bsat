#ifndef SOLVER_H
#define SOLVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * LITERAL REPRESENTATION
 * ============================================================================
 * Literals are represented as packed integers:
 *   literal = (variable << 1) | negated
 *
 * Examples:
 *   Variable 1 positive:  (1 << 1) | 0 = 2
 *   Variable 1 negative:  (1 << 1) | 1 = 3
 *   Variable 5 positive:  (5 << 1) | 0 = 10
 *   Variable 5 negative:  (5 << 1) | 1 = 11
 *
 * This packing allows:
 *   - Extract variable: lit >> 1
 *   - Check negation: lit & 1
 *   - Negate literal: lit ^ 1
 *   - Literal from var: (var << 1) or (var << 1) | 1
 */
typedef uint32_t Literal;

#define LIT_VAR(lit)        ((lit) >> 1)
#define LIT_IS_NEG(lit)     ((lit) & 1)
#define LIT_NEGATE(lit)     ((lit) ^ 1)
#define LIT_FROM_VAR(v, n)  (((v) << 1) | (n))
#define LIT_UNDEF           0

/* ============================================================================
 * CLAUSE STRUCTURE
 * ============================================================================
 * Clauses are stored as:
 *   [header | lit0 | lit1 | ... | litN-1]
 *
 * Header contains:
 *   - size: Number of literals
 *   - lbd: Literal Block Distance (clause quality metric)
 *   - learned: Is this a learned clause?
 *   - activity: For clause deletion heuristics
 */
typedef struct {
    uint32_t size;          // Number of literals
    uint32_t lbd;           // Literal Block Distance
    bool learned;           // Is this a learned clause?
    float activity;         // Clause activity (for deletion)
    Literal literals[];     // Flexible array member for literals
} Clause;

/* ============================================================================
 * ASSIGNMENT
 * ============================================================================
 * Represents a variable assignment on the trail
 */
typedef struct {
    uint32_t variable;      // Variable number
    bool value;             // Assignment (true/false)
    uint32_t level;         // Decision level
    Clause *reason;         // Antecedent clause (NULL for decisions)
} Assignment;

/* ============================================================================
 * WATCH LIST
 * ============================================================================
 * For two-watched literals optimization
 * Each literal has a list of clauses watching it
 */
typedef struct {
    Clause **clauses;       // Array of clause pointers
    uint32_t size;          // Number of clauses in list
    uint32_t capacity;      // Allocated capacity
} WatchList;

/* ============================================================================
 * VSIDS HEAP
 * ============================================================================
 * Binary heap for variable selection using VSIDS heuristic
 */
typedef struct {
    uint32_t *heap;         // Heap array (variable numbers)
    uint32_t *positions;    // Position of each variable in heap
    double *activity;       // Activity score for each variable
    uint32_t size;          // Current heap size
    uint32_t capacity;      // Allocated capacity
} VSIDSHeap;

/* ============================================================================
 * SOLVER STATE
 * ============================================================================
 * Main solver structure containing all state
 */
typedef struct {
    // Problem instance
    uint32_t num_vars;              // Number of variables
    uint32_t num_clauses;           // Number of original clauses
    Clause **clauses;               // Array of all clauses (original + learned)
    uint32_t clauses_capacity;      // Allocated clause array capacity

    // Two-watched literals
    WatchList *watch_lists;         // Watch lists (2 * num_vars + 2 entries)

    // Trail (assignment stack)
    Assignment *trail;              // Assignment trail
    uint32_t trail_size;            // Current trail size
    uint32_t trail_capacity;        // Allocated trail capacity
    uint32_t propagated_index;      // Index of last propagated assignment

    // Decision levels
    uint32_t *level_starts;         // Trail index where each level starts
    uint32_t current_level;         // Current decision level

    // Variable state
    int8_t *assignments;            // Current assignment (0=unassigned, 1=true, -1=false)
    bool *phase_cache;              // Phase saving (remembered polarities)

    // VSIDS heuristic
    VSIDSHeap *vsids;               // VSIDS heap for variable selection
    double var_inc;                 // Variable activity increment
    double var_decay;               // Variable activity decay (0.95)

    // Clause learning
    double clause_inc;              // Clause activity increment
    double clause_decay;            // Clause activity decay (0.999)

    // Restart policy
    uint32_t conflicts;             // Total conflicts
    uint32_t conflicts_since_restart; // Conflicts since last restart
    uint32_t restart_threshold;     // Restart after this many conflicts

    // Statistics
    uint64_t decisions;             // Number of decisions made
    uint64_t propagations;          // Number of propagations
    uint64_t learned_clauses;       // Number of learned clauses
    uint64_t restarts;              // Number of restarts
} Solver;

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

// solver.c
Solver* solver_create(uint32_t num_vars);
void solver_destroy(Solver *solver);
void solver_add_clause(Solver *solver, Literal *lits, uint32_t size);
bool solver_solve(Solver *solver);
void solver_print_solution(Solver *solver);
void solver_print_stats(Solver *solver);

// propagate.c
Clause* solver_propagate(Solver *solver);

// analyze.c
void solver_analyze_conflict(Solver *solver, Clause *conflict, uint32_t *backtrack_level, Clause **learned);

// vsids.c
VSIDSHeap* vsids_create(uint32_t num_vars);
void vsids_destroy(VSIDSHeap *heap);
uint32_t vsids_select(VSIDSHeap *heap, int8_t *assignments);
void vsids_bump_var(VSIDSHeap *heap, uint32_t var, double increment);
void vsids_decay(VSIDSHeap *heap, double decay);

// clause.c
Clause* clause_create(Literal *lits, uint32_t size, bool learned);
void clause_destroy(Clause *clause);
uint32_t clause_lbd(Clause *clause, Assignment *trail);

// memory.c
void* xmalloc(size_t size);
void* xrealloc(void *ptr, size_t size);

#endif // SOLVER_H
