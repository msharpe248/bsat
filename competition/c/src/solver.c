#include "solver.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Debug flag (set via environment variable DEBUG_CDCL=1)
static int debug_mode = -1;  // -1 = not initialized

int is_debug() {
    if (debug_mode == -1) {
        debug_mode = getenv("DEBUG_CDCL") != NULL;
    }
    return debug_mode;
}

// Forward declarations
static bool solver_verify_solution(Solver *solver);
static void solver_add_clause_internal(Solver *solver, Literal *lits, uint32_t size, bool learned);

/* ============================================================================
 * SOLVER INITIALIZATION AND CLEANUP
 * ============================================================================ */

Solver* solver_create(uint32_t num_vars) {
    Solver *solver = (Solver*)xmalloc(sizeof(Solver));

    solver->num_vars = num_vars;
    solver->num_clauses = 0;

    // Allocate clause array
    solver->clauses_capacity = 1024;
    solver->clauses = (Clause**)xmalloc(solver->clauses_capacity * sizeof(Clause*));

    // Allocate watch lists (2 per variable: positive and negative literal)
    // Watch list indices: LIT_FROM_VAR(v, 0) and LIT_FROM_VAR(v, 1)
    uint32_t num_lits = (num_vars + 1) * 2;
    solver->watch_lists = (WatchList*)xmalloc(num_lits * sizeof(WatchList));
    for (uint32_t i = 0; i < num_lits; i++) {
        solver->watch_lists[i].clauses = NULL;
        solver->watch_lists[i].size = 0;
        solver->watch_lists[i].capacity = 0;
    }

    // Allocate trail
    solver->trail_capacity = num_vars + 1;
    solver->trail = (Assignment*)xmalloc(solver->trail_capacity * sizeof(Assignment));
    solver->trail_size = 0;
    solver->propagated_index = 0;

    // Allocate level tracking
    solver->level_starts = (uint32_t*)xmalloc((num_vars + 1) * sizeof(uint32_t));
    solver->current_level = 0;

    // Allocate variable state
    solver->assignments = (int8_t*)xmalloc((num_vars + 1) * sizeof(int8_t));
    solver->phase_cache = (bool*)xmalloc((num_vars + 1) * sizeof(bool));
    memset(solver->assignments, 0, (num_vars + 1) * sizeof(int8_t));
    // Initialize phase_cache to false
    // Note: memset to 0 works for false
    memset(solver->phase_cache, 0, (num_vars + 1) * sizeof(bool));

    // Create VSIDS heap
    solver->vsids = vsids_create(num_vars);

    // Initialize parameters
    solver->var_inc = 1.0;
    solver->var_decay = 0.95;
    solver->clause_inc = 1.0;
    solver->clause_decay = 0.999;

    // Initialize restart policy
    solver->conflicts = 0;
    solver->conflicts_since_restart = 0;
    solver->restart_threshold = 100;  // Luby sequence starts at 100

    // Initialize statistics
    solver->decisions = 0;
    solver->propagations = 0;
    solver->learned_clauses = 0;
    solver->restarts = 0;

    return solver;
}

void solver_destroy(Solver *solver) {
    // Free all clauses
    for (uint32_t i = 0; i < solver->num_clauses; i++) {
        if (solver->clauses[i]) {
            clause_destroy(solver->clauses[i]);
        }
    }
    free(solver->clauses);

    // Free watch lists
    uint32_t num_lits = (solver->num_vars + 1) * 2;
    for (uint32_t i = 0; i < num_lits; i++) {
        free(solver->watch_lists[i].clauses);
    }
    free(solver->watch_lists);

    // Free trail and level tracking
    free(solver->trail);
    free(solver->level_starts);

    // Free variable state
    free(solver->assignments);
    free(solver->phase_cache);

    // Free VSIDS
    vsids_destroy(solver->vsids);

    free(solver);
}

/* ============================================================================
 * WATCH LIST MANAGEMENT
 * ============================================================================ */

static void watch_list_add(WatchList *wl, Clause *clause) {
    if (wl->size >= wl->capacity) {
        wl->capacity = (wl->capacity == 0) ? 4 : wl->capacity * 2;
        wl->clauses = (Clause**)xrealloc(wl->clauses, wl->capacity * sizeof(Clause*));
    }
    wl->clauses[wl->size++] = clause;
}

/* ============================================================================
 * CLAUSE ADDITION
 * ============================================================================ */

void solver_add_clause(Solver *solver, Literal *lits, uint32_t size) {
    solver_add_clause_internal(solver, lits, size, false);
}

static void solver_add_clause_internal(Solver *solver, Literal *lits, uint32_t size, bool learned) {
    // Create clause
    Clause *clause = clause_create(lits, size, learned);

    // Add to clause array
    if (solver->num_clauses >= solver->clauses_capacity) {
        solver->clauses_capacity *= 2;
        solver->clauses = (Clause**)xrealloc(solver->clauses,
                                             solver->clauses_capacity * sizeof(Clause*));
    }
    solver->clauses[solver->num_clauses++] = clause;

    // Setup watched literals
    if (size >= 2) {
        // Watch first two literals
        Literal w0 = clause->literals[0];
        Literal w1 = clause->literals[1];

        watch_list_add(&solver->watch_lists[w0], clause);
        watch_list_add(&solver->watch_lists[w1], clause);
    } else if (size == 1) {
        // Unit clause - watch the single literal
        Literal w0 = clause->literals[0];
        watch_list_add(&solver->watch_lists[w0], clause);
    }
    // Empty clause means UNSAT (handled during solving)
}

/* ============================================================================
 * ASSIGNMENT AND BACKTRACKING
 * ============================================================================ */

static void solver_assign(Solver *solver, uint32_t var, bool value, Clause *reason) {
    solver->assignments[var] = value ? 1 : -1;

    Assignment assign;
    assign.variable = var;
    assign.value = value;
    assign.level = solver->current_level;
    assign.reason = reason;

    solver->trail[solver->trail_size++] = assign;

    // Update phase cache
    solver->phase_cache[var] = value;
}

static void solver_backtrack(Solver *solver, uint32_t level) {
    if (level >= solver->current_level) {
        return;
    }

    // Find trail index for target level
    uint32_t target_size = (level == 0) ? 0 : solver->level_starts[level];

    // Unassign variables
    for (uint32_t i = target_size; i < solver->trail_size; i++) {
        uint32_t var = solver->trail[i].variable;
        solver->assignments[var] = 0;
    }

    solver->trail_size = target_size;
    solver->propagated_index = target_size;
    solver->current_level = level;
}

/* ============================================================================
 * MAIN SOLVE LOOP
 * ============================================================================ */

bool solver_solve(Solver *solver) {
    if (is_debug()) {
        fprintf(stderr, "solver_solve() started\n");
    }

    // Initialize level_starts[0] at the beginning
    solver->level_starts[0] = 0;

    // Propagate initial unit clauses
    for (uint32_t i = 0; i < solver->num_clauses; i++) {
        Clause *clause = solver->clauses[i];
        if (clause->size == 1) {
            Literal lit = clause->literals[0];
            uint32_t var = LIT_VAR(lit);
            bool value = !LIT_IS_NEG(lit);

            if (solver->assignments[var] != 0) {
                // Already assigned - check for conflict
                bool assigned_value = (solver->assignments[var] == 1);
                if (assigned_value != value) {
                    // Conflict at level 0
                    return false;
                }
            } else {
                // Assign the unit literal at level 0
                solver_assign(solver, var, value, clause);
                if (is_debug()) {
                    fprintf(stderr, "Initial unit clause: x%u = %s\n", var, value ? "T" : "F");
                }
            }
        }
    }

    uint32_t iteration = 0;

    while (true) {
        iteration++;
        if (is_debug() && iteration % 10 == 0) {
            fprintf(stderr, "Iteration %u: trail=%u, propagated=%u, level=%u\n",
                   iteration, solver->trail_size, solver->propagated_index, solver->current_level);
        }

        // Propagate assignments
        Clause *conflict = solver_propagate(solver);

        if (conflict) {
            solver->conflicts++;

            if (is_debug()) {
                fprintf(stderr, "\nCONFLICT at level %u, conflicts=%u\n", solver->current_level, solver->conflicts);
            }

            // Analyze conflict
            if (solver->current_level == 0) {
                // Conflict at decision level 0 = UNSAT
                return false;
            }

            uint32_t backtrack_level;
            Clause *learned = NULL;
            solver_analyze_conflict(solver, conflict, &backtrack_level, &learned);

            if (is_debug() && learned) {
                fprintf(stderr, "Learned clause size=%u, backtrack_level=%u\n", learned->size, backtrack_level);
            }

            // Backtrack
            solver_backtrack(solver, backtrack_level);

            // Add learned clause
            if (learned) {
                uint32_t clause_idx = solver->num_clauses;
                solver_add_clause_internal(solver, learned->literals, learned->size, true);
                solver->learned_clauses++;

                // Assign the asserting literal (first literal in learned clause)
                if (learned->size >= 1) {
                    uint32_t var = LIT_VAR(learned->literals[0]);
                    bool value = !LIT_IS_NEG(learned->literals[0]);
                    Clause *added_clause = solver->clauses[clause_idx];

                    if (solver->assignments[var] == 0) {
                        solver_assign(solver, var, value, added_clause);
                        if (is_debug()) {
                            fprintf(stderr, "Assigned asserting literal: x%u = %s\n", var, value ? "T" : "F");
                        }
                    } else if (is_debug()) {
                        fprintf(stderr, "WARNING: Asserting literal x%u already assigned to %d\n",
                               var, solver->assignments[var]);
                    }
                }

                clause_destroy(learned);
            } else {
                // Conflict analysis failed - use simple chronological backtracking
                if (is_debug()) {
                    fprintf(stderr, "Conflict analysis failed, using chronological backtracking\n");
                }
                // Backtrack was already done to backtrack_level which may be wrong
                // Just backtrack one more level to be safe
                if (solver->current_level > 0) {
                    solver_backtrack(solver, solver->current_level - 1);
                }
            }

        } else {
            // No conflict - make decision or finish

            if (solver->trail_size == solver->num_vars) {
                // All variables assigned - verify solution before claiming SAT
                if (is_debug()) {
                    fprintf(stderr, "All %u variables assigned, verifying solution...\n", solver->num_vars);
                }
                if (!solver_verify_solution(solver)) {
                    fprintf(stderr, "INTERNAL ERROR: All variables assigned but solution is invalid!\n");
                    fprintf(stderr, "This indicates a bug in propagation or conflict analysis.\n");
                    return false;  // Claim UNSAT to avoid reporting invalid solution
                }
                if (is_debug()) {
                    fprintf(stderr, "Solution verified OK!\n");
                }
                return true;
            }

            // Select next variable using VSIDS
            uint32_t var = vsids_select(solver->vsids, solver->assignments);
            if (var == 0) {
                // No unassigned variables - but this should have been caught above!
                fprintf(stderr, "WARNING: VSIDS returned 0 but trail_size=%u, num_vars=%u\n",
                       solver->trail_size, solver->num_vars);

                if (is_debug()) {
                    fprintf(stderr, "Assignments:\n");
                    for (uint32_t v = 1; v <= solver->num_vars; v++) {
                        fprintf(stderr, "  x%u = %d\n", v, solver->assignments[v]);
                    }
                }

                // Verify solution anyway
                if (!solver_verify_solution(solver)) {
                    fprintf(stderr, "INTERNAL ERROR: VSIDS says all assigned but solution is invalid!\n");
                    return false;
                }

                return true;
            }

            // Make decision using phase cache
            bool value = solver->phase_cache[var];

            solver->current_level++;
            solver->level_starts[solver->current_level] = solver->trail_size;

            solver_assign(solver, var, value, NULL);
            solver->decisions++;
        }
    }
}

/* ============================================================================
 * SOLUTION VERIFICATION
 * ============================================================================ */

static bool solver_verify_solution(Solver *solver) {
    // Check that all clauses are satisfied
    for (uint32_t i = 0; i < solver->num_clauses; i++) {
        Clause *clause = solver->clauses[i];
        bool clause_sat = false;

        for (uint32_t j = 0; j < clause->size; j++) {
            Literal lit = clause->literals[j];
            uint32_t var = LIT_VAR(lit);
            bool is_neg = LIT_IS_NEG(lit);

            int assignment = solver->assignments[var];

            // Literal is TRUE if:
            // - Positive literal (is_neg=0) and variable is TRUE (assignment=1)
            // - Negative literal (is_neg=1) and variable is FALSE (assignment=-1)
            bool lit_value = (is_neg == 0 && assignment == 1) ||
                            (is_neg == 1 && assignment == -1);

            if (lit_value) {
                clause_sat = true;
                break;
            }
        }

        if (!clause_sat) {
            if (is_debug()) {
                fprintf(stderr, "ERROR: Clause %u is NOT satisfied:\n  ", i);
                for (uint32_t j = 0; j < clause->size; j++) {
                    Literal lit = clause->literals[j];
                    uint32_t var = LIT_VAR(lit);
                    bool is_neg = LIT_IS_NEG(lit);
                    fprintf(stderr, "%sx%u ", is_neg ? "¬" : "", var);
                }
                fprintf(stderr, "\n");
            }
            return false;
        }
    }
    return true;
}

/* ============================================================================
 * OUTPUT
 * ============================================================================ */

void solver_print_solution(Solver *solver) {
    printf("v ");
    for (uint32_t v = 1; v <= solver->num_vars; v++) {
        int value = solver->assignments[v];
        if (value == 1) {
            printf("%u ", v);
        } else if (value == -1) {
            printf("-%u ", v);
        } else {
            printf("%u ", v);  // Unassigned - arbitrary
        }
    }
    printf("0\n");
}

void solver_print_stats(Solver *solver) {
    fprintf(stderr, "c Decisions:       %llu\n", solver->decisions);
    fprintf(stderr, "c Conflicts:       %u\n", solver->conflicts);
    fprintf(stderr, "c Propagations:    %llu\n", solver->propagations);
    fprintf(stderr, "c Learned clauses: %llu\n", solver->learned_clauses);
    fprintf(stderr, "c Restarts:        %llu\n", solver->restarts);
}
