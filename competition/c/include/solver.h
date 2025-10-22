/*********************************************************************
 * BSAT Competition Solver - Main Solver Interface
 *
 * Core CDCL solver with all modern optimizations.
 *********************************************************************/

#ifndef BSAT_SOLVER_H
#define BSAT_SOLVER_H

#include "types.h"
#include "arena.h"
#include "watch.h"
#include <time.h>

/*********************************************************************
 * Solver Options
 *********************************************************************/

typedef struct SolverOpts {
    // Core parameters
    uint32_t max_conflicts;      // Conflict limit (0 = unlimited)
    uint32_t max_decisions;      // Decision limit (0 = unlimited)
    double   max_time;           // Time limit in seconds (0 = unlimited)

    // VSIDS parameters
    double   var_decay;          // Variable activity decay (0.95)
    double   var_inc;            // Variable activity increment (1.0)
    double   clause_decay;       // Clause activity decay (0.999)

    // Restart parameters
    uint32_t restart_first;      // First restart interval (100)
    double   restart_inc;        // Restart interval increment (1.5)
    bool     glucose_restart;    // Use Glucose-style adaptive restarts
    uint32_t restart_postpone;   // Min trail growth to postpone restart (10%)
    double   glucose_fast_alpha; // Glucose fast MA decay factor (0.8)
    double   glucose_slow_alpha; // Glucose slow MA decay factor (0.9999)
    uint32_t glucose_min_conflicts; // Min conflicts before Glucose restarts (100)

    // Phase saving parameters
    bool     phase_saving;       // Enable phase saving (true)
    uint32_t phase_reset_period; // Reset saved phases periodically (10000)
    bool     random_phase;       // Use random phase selection (false)
    double   random_phase_prob;  // Probability of random phase (0.01)
    bool     adaptive_random;    // Enable when stuck (true)

    // Clause management
    uint32_t max_lbd;           // Max LBD for keeping learned clauses (30)
    uint32_t glue_lbd;          // LBD threshold for glue clauses (2)
    double   reduce_fraction;   // Fraction of learned clauses to keep (0.5)
    uint32_t reduce_interval;   // Conflicts between reductions (2000)

    // Preprocessing
    bool     bce;               // Enable blocked clause elimination (true)

    // Inprocessing
    bool     inprocess;         // Enable inprocessing (false)
    uint32_t inprocess_interval; // Conflicts between inprocessing (10000)
    bool     subsumption;       // Enable subsumption (true)
    bool     var_elim;          // Enable variable elimination (true)

    // Output options
    bool     verbose;           // Verbose output (false)
    bool     quiet;             // Suppress all output (false)
    bool     stats;             // Print statistics (true)
} SolverOpts;

// Get default options
SolverOpts default_opts(void);

/*********************************************************************
 * Variable Information
 *********************************************************************/

typedef struct VarInfo {
    lbool    value;          // Current assignment (UNDEF/TRUE/FALSE)
    Level    level;          // Decision level
    CRef     reason;         // Reason clause (INVALID_CLAUSE for decisions)
    uint32_t trail_pos;      // Position in trail

    // Phase saving
    bool     polarity;       // Saved polarity
    uint32_t last_polarity;  // Last conflict where polarity was saved

    // VSIDS activity
    double   activity;       // Variable activity score
    uint32_t heap_pos;       // Position in VSIDS heap
} VarInfo;

/*********************************************************************
 * Trail Entry
 *********************************************************************/

typedef struct Trail {
    Lit      lit;           // Assigned literal
    Level    level;         // Decision level
} Trail;

/*********************************************************************
 * Main Solver Structure
 *********************************************************************/

typedef struct Solver {
    // Problem size
    uint32_t num_vars;
    uint32_t num_clauses;
    uint32_t num_original;    // Number of original clauses

    // Core data structures
    Arena*        arena;       // Clause allocator
    WatchManager* watches;     // Watch lists
    VarInfo*      vars;        // Variable information

    // Trail (assignment stack)
    Trail*   trail;           // Assignment trail
    uint32_t trail_size;      // Current trail size
    uint32_t trail_lim;       // Next decision position
    uint32_t qhead;           // Propagation queue head
    Level*   trail_lims;      // Decision level limits
    Level    decision_level;  // Current decision level

    // Clause database
    CRef*    clauses;         // All clauses
    uint32_t num_learnts;     // Number of learned clauses
    CRef*    learnts;         // Learned clauses
    uint32_t learnts_size;    // Size of learned clause array

    // VSIDS heap
    struct {
        Var*     heap;        // Binary max-heap of variables
        uint32_t size;        // Current heap size
        double   var_inc;     // Activity increment
        double   var_decay;   // Activity decay factor
    } order;

    // Conflict analysis
    uint8_t* seen;            // Seen flags for conflict analysis
    Lit*     analyze_stack;   // Temporary stack for analysis
    uint32_t analyze_toclear; // Number of seen variables to clear

    // Statistics
    struct {
        uint64_t decisions;
        uint64_t propagations;
        uint64_t conflicts;
        uint64_t restarts;
        uint64_t reduces;
        uint64_t learned_clauses;
        uint64_t learned_literals;
        uint64_t deleted_clauses;
        uint64_t subsumed_clauses;   // Clauses removed by on-the-fly subsumption
        uint64_t minimized_literals; // Literals removed by clause minimization
        uint64_t blocked_clauses;    // Clauses removed by blocked clause elimination
        uint64_t max_lbd;
        uint64_t glue_clauses;
        double   start_time;
    } stats;

    // Restart state
    struct {
        uint32_t conflicts_since;     // Conflicts since last restart
        uint32_t threshold;           // Current restart threshold
        double   slow_ma;            // Slow moving average (Glucose)
        double   fast_ma;            // Fast moving average (Glucose)
        uint32_t stuck_conflicts;    // Conflicts without progress
    } restart;

    // Options
    SolverOpts opts;

    // Result
    lbool result;             // SAT/UNSAT/UNKNOWN
} Solver;

/*********************************************************************
 * Solver API
 *********************************************************************/

// Create a new solver
Solver* solver_new(void);

// Create solver with options
Solver* solver_new_with_opts(const SolverOpts* opts);

// Free solver and all resources
void solver_free(Solver* s);

// Add a variable (returns variable index)
Var solver_new_var(Solver* s);

// Add a clause
bool solver_add_clause(Solver* s, const Lit* lits, uint32_t size);

// Main solve function
lbool solver_solve(Solver* s);

// Solve with assumptions
lbool solver_solve_with_assumptions(Solver* s, const Lit* assumps, uint32_t n_assumps);

// Get variable value in model
lbool solver_model_value(const Solver* s, Var v);

// Get conflict clause (for UNSAT)
const Lit* solver_conflict(const Solver* s, uint32_t* size);

// Print statistics
void solver_print_stats(const Solver* s);

/*********************************************************************
 * Internal Functions (for testing/debugging)
 *********************************************************************/

// Unit propagation
CRef solver_propagate(Solver* s);

// Analyze conflict and learn clause
void solver_analyze(Solver* s, CRef conflict, Lit* learnt, uint32_t* learnt_size, Level* bt_level);

// Make a decision
bool solver_decide(Solver* s);

// Backtrack to level
void solver_backtrack(Solver* s, Level level);

// Reduce learned clause database
void solver_reduce_db(Solver* s);

// Check if should restart
bool solver_should_restart(Solver* s);

// Simplify clause database
bool solver_simplify(Solver* s);

#endif // BSAT_SOLVER_H