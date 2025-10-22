/*********************************************************************
 * BSAT Competition Solver - Core Solver Implementation
 *********************************************************************/

#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <signal.h>

/*********************************************************************
 * Signal Handling for Progress Monitoring
 *********************************************************************/

// Global flag to request statistics dump (set by signal handler)
static volatile sig_atomic_t print_stats_requested = 0;

// Signal handler for SIGUSR1 - request statistics dump
static void sigusr1_handler(int signum) {
    (void)signum;  // Unused parameter
    print_stats_requested = 1;
}

// Install signal handler
static void install_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
}

// Print progress statistics (safe to call from main loop)
static void print_progress_stats(const Solver* s) {
    double elapsed = (double)(clock() - (clock_t)s->stats.start_time) / CLOCKS_PER_SEC;
    fprintf(stderr, "\n");
    fprintf(stderr, "c ========== Progress Update ==========\n");
    fprintf(stderr, "c Elapsed time     : %.3f s\n", elapsed);
    fprintf(stderr, "c Decisions        : %llu\n", (unsigned long long)s->stats.decisions);
    fprintf(stderr, "c Propagations     : %llu\n", (unsigned long long)s->stats.propagations);
    fprintf(stderr, "c Conflicts        : %llu\n", (unsigned long long)s->stats.conflicts);
    fprintf(stderr, "c Restarts         : %llu\n", (unsigned long long)s->stats.restarts);
    fprintf(stderr, "c Learned clauses  : %llu\n", (unsigned long long)s->stats.learned_clauses);
    fprintf(stderr, "c Decision level   : %u\n", s->decision_level);
    fprintf(stderr, "c Trail size       : %u\n", s->trail_size);
    if (elapsed > 0) {
        fprintf(stderr, "c Conflicts/sec    : %.0f\n", s->stats.conflicts / elapsed);
        fprintf(stderr, "c Decisions/sec    : %.0f\n", s->stats.decisions / elapsed);
    }
    fprintf(stderr, "c ======================================\n");
    fprintf(stderr, "\n");
    fflush(stderr);
}

/*********************************************************************
 * Default Options
 *********************************************************************/

SolverOpts default_opts(void) {
    SolverOpts opts = {
        .max_conflicts = 0,        // Unlimited
        .max_decisions = 0,        // Unlimited
        .max_time = 0.0,          // Unlimited

        .var_decay = 0.95,
        .var_inc = 1.0,
        .clause_decay = 0.999,

        .restart_first = 100,
        .restart_inc = 1.5,
        .glucose_restart = true,   // Hybrid Glucose/geometric restarts (best of both worlds)
        .restart_postpone = 10,
        .glucose_fast_alpha = 0.8,     // Fast MA decay factor (tracks recent ~5 conflicts)
        .glucose_slow_alpha = 0.9999,  // Slow MA decay factor (long-term average)
        .glucose_min_conflicts = 100,  // Minimum conflicts before enabling Glucose

        .phase_saving = true,
        .phase_reset_period = 10000,
        .random_phase = false,
        .random_phase_prob = 0.01,
        .adaptive_random = true,

        .max_lbd = 30,
        .glue_lbd = 2,
        .reduce_fraction = 0.5,
        .reduce_interval = 2000,

        .bce = true,              // Enable blocked clause elimination by default

        .inprocess = false,
        .inprocess_interval = 10000,
        .subsumption = true,
        .var_elim = true,

        .verbose = false,
        .quiet = false,
        .stats = true
    };
    return opts;
}

/*********************************************************************
 * VSIDS Heap Operations
 *********************************************************************/

static inline uint32_t heap_left(uint32_t i) { return 2 * i + 1; }
static inline uint32_t heap_right(uint32_t i) { return 2 * i + 2; }
static inline uint32_t heap_parent(uint32_t i) { return (i - 1) / 2; }

static void heap_percolate_up(Solver* s, uint32_t i) {
    Var v = s->order.heap[i];
    double act = s->vars[v].activity;

    while (i > 0) {
        uint32_t p = heap_parent(i);
        Var pv = s->order.heap[p];

        if (s->vars[pv].activity >= act) break;

        s->order.heap[i] = pv;
        s->vars[pv].heap_pos = i;
        i = p;
    }

    s->order.heap[i] = v;
    s->vars[v].heap_pos = i;
}

static void heap_percolate_down(Solver* s, uint32_t i) {
    Var v = s->order.heap[i];
    double act = s->vars[v].activity;

    while (heap_left(i) < s->order.size) {
        uint32_t child = heap_left(i);
        if (heap_right(i) < s->order.size &&
            s->vars[s->order.heap[heap_right(i)]].activity >
            s->vars[s->order.heap[child]].activity) {
            child = heap_right(i);
        }

        if (act >= s->vars[s->order.heap[child]].activity) break;

        s->order.heap[i] = s->order.heap[child];
        s->vars[s->order.heap[i]].heap_pos = i;
        i = child;
    }

    s->order.heap[i] = v;
    s->vars[v].heap_pos = i;
}

static void heap_insert(Solver* s, Var v) {
    if (s->vars[v].heap_pos != UINT32_MAX) return;  // Already in heap

    uint32_t i = s->order.size++;
    s->order.heap[i] = v;
    s->vars[v].heap_pos = i;
    heap_percolate_up(s, i);
}

static void heap_remove(Solver* s, Var v) {
    uint32_t pos = s->vars[v].heap_pos;
    if (pos == UINT32_MAX) return;  // Not in heap

    s->vars[v].heap_pos = UINT32_MAX;

    if (pos == s->order.size - 1) {
        s->order.size--;
        return;
    }

    Var last = s->order.heap[--s->order.size];
    s->order.heap[pos] = last;
    s->vars[last].heap_pos = pos;

    if (pos > 0 && s->vars[last].activity > s->vars[s->order.heap[heap_parent(pos)]].activity) {
        heap_percolate_up(s, pos);
    } else {
        heap_percolate_down(s, pos);
    }
}

static Var heap_extract_max(Solver* s) {
    if (s->order.size == 0) return INVALID_VAR;

    Var v = s->order.heap[0];
    heap_remove(s, v);
    return v;
}

static void bump_var_activity(Solver* s, Var v, double inc) {
    s->vars[v].activity += inc;
    if (s->vars[v].heap_pos != UINT32_MAX) {
        heap_percolate_up(s, s->vars[v].heap_pos);
    }

    // Rescale if needed
    if (s->vars[v].activity > 1e100) {
        for (Var i = 1; i <= s->num_vars; i++) {
            s->vars[i].activity *= 1e-100;
        }
        s->order.var_inc *= 1e-100;
    }
}

static void decay_var_inc(Solver* s) {
    s->order.var_inc /= s->order.var_decay;
}

/*********************************************************************
 * Solver Creation and Destruction
 *********************************************************************/

Solver* solver_new(void) {
    SolverOpts opts = default_opts();
    return solver_new_with_opts(&opts);
}

Solver* solver_new_with_opts(const SolverOpts* opts) {
    Solver* s = (Solver*)calloc(1, sizeof(Solver));
    if (!s) return NULL;

    // Copy options
    s->opts = *opts;

    // Initialize core structures
    s->arena = arena_init(0);
    if (!s->arena) goto error;

    s->watches = watch_init(0);  // Will grow as variables are added
    if (!s->watches) goto error;

    // Initialize order heap
    s->order.var_inc = opts->var_inc;
    s->order.var_decay = opts->var_decay;

    // Initialize restart state
    s->restart.threshold = opts->restart_first;

    // Set start time
    s->stats.start_time = (double)clock() / CLOCKS_PER_SEC;

    s->result = UNDEF;

    return s;

error:
    solver_free(s);
    return NULL;
}

void solver_free(Solver* s) {
    if (!s) return;

    arena_free(s->arena);
    watch_free(s->watches);

    free(s->vars);
    free(s->trail);
    free(s->trail_lims);
    free(s->clauses);
    free(s->learnts);
    free(s->order.heap);
    free(s->seen);
    free(s->analyze_stack);

    free(s);
}

/*********************************************************************
 * Variable Management
 *********************************************************************/

Var solver_new_var(Solver* s) {
    if (s->num_vars >= MAX_VARS) {
        return INVALID_VAR;
    }

    Var v = ++s->num_vars;

    // Grow arrays if needed
    if (s->num_vars > s->order.size) {
        // Reallocate variable info
        VarInfo* new_vars = (VarInfo*)realloc(s->vars, (s->num_vars + 1) * sizeof(VarInfo));
        if (!new_vars) return INVALID_VAR;
        s->vars = new_vars;

        // Initialize new variable
        memset(&s->vars[v], 0, sizeof(VarInfo));
        s->vars[v].value = UNDEF;
        s->vars[v].level = INVALID_LEVEL;
        s->vars[v].reason = INVALID_CLAUSE;
        s->vars[v].heap_pos = UINT32_MAX;
        s->vars[v].polarity = false;  // Default phase

        // Grow trail
        Trail* new_trail = (Trail*)realloc(s->trail, (s->num_vars + 1) * sizeof(Trail));
        if (!new_trail) return INVALID_VAR;
        s->trail = new_trail;

        // Grow trail limits
        Level* new_lims = (Level*)realloc(s->trail_lims, (s->num_vars + 1) * sizeof(Level));
        if (!new_lims) return INVALID_VAR;
        s->trail_lims = new_lims;

        // Grow heap
        Var* new_heap = (Var*)realloc(s->order.heap, (s->num_vars + 1) * sizeof(Var));
        if (!new_heap) return INVALID_VAR;
        s->order.heap = new_heap;

        // Grow seen array
        uint8_t* new_seen = (uint8_t*)realloc(s->seen, (s->num_vars + 1) * sizeof(uint8_t));
        if (!new_seen) return INVALID_VAR;
        s->seen = new_seen;
        s->seen[v] = 0;

        // Grow analyze stack
        Lit* new_stack = (Lit*)realloc(s->analyze_stack, (s->num_vars + 1) * sizeof(Lit));
        if (!new_stack) return INVALID_VAR;
        s->analyze_stack = new_stack;

        // Grow watch manager
        watch_free(s->watches);
        s->watches = watch_init(s->num_vars);
        if (!s->watches) return INVALID_VAR;
    }

    // Add to decision heap
    heap_insert(s, v);

    return v;
}

/*********************************************************************
 * Trail Management
 *********************************************************************/

static inline void push_trail(Solver* s, Lit lit) {
    Var v = var(lit);
    ASSERT(s->vars[v].value == UNDEF);

    s->vars[v].value = sign(lit) ? FALSE : TRUE;
    s->vars[v].level = s->decision_level;
    s->vars[v].trail_pos = s->trail_size;

    s->trail[s->trail_size].lit = lit;
    s->trail[s->trail_size].level = s->decision_level;
    s->trail_size++;

    // Save phase
    if (s->opts.phase_saving) {
        s->vars[v].polarity = !sign(lit);
    }
}

void solver_backtrack(Solver* s, Level level) {
    if (level >= s->decision_level) return;

    // Find trail position for target level
    uint32_t trail_pos = (level == 0) ? 0 : s->trail_lims[level];

    // Undo assignments
    for (uint32_t i = trail_pos; i < s->trail_size; i++) {
        Var v = var(s->trail[i].lit);
        s->vars[v].value = UNDEF;
        s->vars[v].level = INVALID_LEVEL;
        s->vars[v].reason = INVALID_CLAUSE;

        // Re-insert into decision heap
        if (s->vars[v].heap_pos == UINT32_MAX) {
            heap_insert(s, v);
        }
    }

    s->trail_size = trail_pos;
    s->qhead = trail_pos;
    s->decision_level = level;
}

// Chronological backtracking: backtrack one level at a time
// instead of jumping directly to target level
// Returns the level we actually backtracked to
static Level solver_backtrack_chronological(Solver* s, const Lit* learnt, uint32_t learnt_size, Level target_level) {
    // Always use chronological backtracking if enabled
    // For each level from current down to target, check if clause is unit

    Level current = s->decision_level;

    // Backtrack one level at a time
    while (current > target_level) {
        Level next_level = current - 1;

        // Backtrack to next level
        solver_backtrack(s, next_level);

        // Count unassigned literals in learned clause at this level
        uint32_t unassigned = 0;
        Lit propagate_lit = 0;

        for (uint32_t i = 0; i < learnt_size; i++) {
            Var v = var(learnt[i]);
            if (s->vars[v].value == UNDEF) {
                unassigned++;
                propagate_lit = learnt[i];
            } else if (s->vars[v].value == (sign(learnt[i]) ? FALSE : TRUE)) {
                // Literal is true - clause is satisfied, no need to propagate
                unassigned = 0;
                break;
            }
        }

        // If clause is unit (exactly one unassigned literal), stop here
        if (unassigned == 1) {
            return next_level;
        }

        // If clause is satisfied or all false, continue
        current = next_level;
    }

    // Reached target level
    return target_level;
}

/*********************************************************************
 * Clause Addition
 *********************************************************************/

bool solver_add_clause(Solver* s, const Lit* lits, uint32_t size) {
    if (size == 0) {
        s->result = FALSE;  // Empty clause = UNSAT
        return false;
    }

    // TODO: Simplify clause (remove duplicates, check tautology)

    // Unit clause - immediately assign
    if (size == 1) {
        Var v = var(lits[0]);
        if (s->vars[v].value == UNDEF) {
            push_trail(s, lits[0]);
        } else if (s->vars[v].value == (sign(lits[0]) ? TRUE : FALSE)) {
            s->result = FALSE;  // Conflicting unit clause
            return false;
        }
        return true;
    }

    // For binary clauses, don't allocate in arena - handle specially
    CRef cref = INVALID_CLAUSE;

    if (size > 2) {
        // Allocate non-binary clauses in arena
        cref = arena_alloc(s->arena, lits, size, false);
        if (cref == INVALID_CLAUSE) {
            return false;  // Out of memory
        }

        // Add to clause list
        if (s->num_clauses >= s->num_original) {
            uint32_t new_cap = s->num_original ? s->num_original * 2 : 1024;
            CRef* new_clauses = (CRef*)realloc(s->clauses, new_cap * sizeof(CRef));
            if (!new_clauses) {
                arena_delete(s->arena, cref);
                return false;
            }
            s->clauses = new_clauses;
            s->num_original = new_cap;
        }
        s->clauses[s->num_clauses] = cref;
    }

    // Count all clauses, including binary ones
    s->num_clauses++;

    // Add watches - need to find two non-false literals if possible
    if (size == 2) {
        // Binary clause - check if it's already unit or conflicting
        Var v0 = var(lits[0]);
        Var v1 = var(lits[1]);
        lbool val0 = s->vars[v0].value;
        lbool val1 = s->vars[v1].value;

        // Check for immediate conflict or unit
        if (val0 == (sign(lits[0]) ? TRUE : FALSE) && val1 == (sign(lits[1]) ? TRUE : FALSE)) {
            // Both literals are false - conflict
            s->result = FALSE;
            return false;
        } else if (val0 == (sign(lits[0]) ? TRUE : FALSE) && val1 == UNDEF) {
            // First literal false, second unassigned - unit propagate
            push_trail(s, lits[1]);
        } else if (val1 == (sign(lits[1]) ? TRUE : FALSE) && val0 == UNDEF) {
            // Second literal false, first unassigned - unit propagate
            push_trail(s, lits[0]);
        }

        // Always add watches for binary clauses
        watch_add(s->watches, lits[0], INVALID_CLAUSE, lits[1]);
        watch_add(s->watches, lits[1], INVALID_CLAUSE, lits[0]);
    } else {
        // For larger clauses, find two literals that are not false to watch
        // First, get the clause literals from arena (they may be reordered)
        Lit* clause_lits = CLAUSE_LITS(s->arena, cref);

        // Find first non-false literal
        uint32_t watch1 = 0;
        for (uint32_t i = 0; i < size; i++) {
            Var v = var(clause_lits[i]);
            if (s->vars[v].value != (sign(clause_lits[i]) ? TRUE : FALSE)) {
                // This literal is not false
                if (i != 0) {
                    // Swap it to position 0
                    Lit tmp = clause_lits[0];
                    clause_lits[0] = clause_lits[i];
                    clause_lits[i] = tmp;
                }
                watch1 = 0;
                break;
            }
        }

        // Find second non-false literal
        uint32_t watch2 = 1;
        for (uint32_t i = 1; i < size; i++) {
            Var v = var(clause_lits[i]);
            if (s->vars[v].value != (sign(clause_lits[i]) ? TRUE : FALSE)) {
                // This literal is not false
                if (i != 1) {
                    // Swap it to position 1
                    Lit tmp = clause_lits[1];
                    clause_lits[1] = clause_lits[i];
                    clause_lits[i] = tmp;
                }
                watch2 = 1;
                break;
            }
        }

        // Add watches for the two chosen literals
        watch_add(s->watches, clause_lits[watch1], cref, clause_lits[watch2]);
        watch_add(s->watches, clause_lits[watch2], cref, clause_lits[watch1]);
    }

    return true;
}

/*********************************************************************
 * Model Access
 *********************************************************************/

lbool solver_model_value(const Solver* s, Var v) {
    if (v > s->num_vars) return UNDEF;
    return s->vars[v].value;
}

/*********************************************************************
 * Statistics
 *********************************************************************/

void solver_print_stats(const Solver* s) {
    double cpu_time = (double)clock() / CLOCKS_PER_SEC - s->stats.start_time;

    printf("c\n");
    printf("c ========== Statistics ==========\n");
    printf("c CPU time          : %.3f s\n", cpu_time);
    printf("c Decisions         : %llu\n", (unsigned long long)s->stats.decisions);
    printf("c Propagations      : %llu\n", (unsigned long long)s->stats.propagations);
    printf("c Conflicts         : %llu\n", (unsigned long long)s->stats.conflicts);
    printf("c Restarts          : %llu\n", (unsigned long long)s->stats.restarts);
    printf("c Learned clauses   : %llu\n", (unsigned long long)s->stats.learned_clauses);
    printf("c Learned literals  : %llu\n", (unsigned long long)s->stats.learned_literals);
    printf("c Deleted clauses   : %llu\n", (unsigned long long)s->stats.deleted_clauses);
    printf("c Blocked clauses   : %llu\n", (unsigned long long)s->stats.blocked_clauses);
    printf("c Subsumed clauses  : %llu\n", (unsigned long long)s->stats.subsumed_clauses);
    printf("c Minimized literals: %llu\n", (unsigned long long)s->stats.minimized_literals);
    printf("c Glue clauses      : %llu\n", (unsigned long long)s->stats.glue_clauses);
    printf("c Max LBD           : %llu\n", (unsigned long long)s->stats.max_lbd);

    if (s->stats.decisions > 0) {
        printf("c Decisions/sec     : %.0f\n", s->stats.decisions / cpu_time);
    }
    if (s->stats.propagations > 0) {
        printf("c Propagations/sec  : %.0f\n", s->stats.propagations / cpu_time);
    }
    if (s->stats.conflicts > 0) {
        printf("c Conflicts/sec     : %.0f\n", s->stats.conflicts / cpu_time);
    }

    // Memory statistics
    ArenaStats astats = arena_stats(s->arena);
    printf("c Memory used       : %.2f MB\n", astats.used_bytes / (1024.0 * 1024.0));
    printf("c Memory allocated  : %.2f MB\n", astats.total_bytes / (1024.0 * 1024.0));

    printf("c\n");
}

/*********************************************************************
 * Unit Propagation (Two-Watched Literals)
 *********************************************************************/

CRef solver_propagate(Solver* s) {
    #ifdef DEBUG
    static uint64_t prop_count = 0;
    #endif

    while (s->qhead < s->trail_size) {
        #ifdef DEBUG
        if (getenv("DEBUG_CDCL")) {
            prop_count++;
            if (prop_count > 1000) {
                printf("[ERROR] Infinite propagation loop detected! qhead=%u trail_size=%u\n",
                       s->qhead, s->trail_size);
                exit(1);
            }
        }
        #endif

        Lit p = s->trail[s->qhead++].lit;

#ifdef DEBUG
        if (getenv("DEBUG_CDCL")) {
            printf("[PROPAGATE] qhead=%u trail_size=%u Processing literal %d (var=%u, value=%d)\n",
                   s->qhead - 1, s->trail_size, toDimacs(p), var(p), s->vars[var(p)].value);
        }
#endif

        // Get watches for ~p (literals that could become unit)
        WatchList* ws = watch_list(s->watches, neg(p));
        Watch* watches = ws->watches;
        uint32_t i = 0, j = 0;

        s->stats.propagations++;
        s->watches->visits++;

#ifdef DEBUG
        if (getenv("DEBUG_CDCL")) {
            printf("[PROPAGATE] Checking %u watches for literal %d\n",
                   ws->size, toDimacs(neg(p)));
        }
#endif

        while (i < ws->size) {
            Watch w = watches[i];

            // Binary clause special case
            if (is_binary_watch(w)) {
                Lit q = w.blocker;
                Var v = var(q);

#ifdef DEBUG
                if (getenv("DEBUG_CDCL")) {
                    printf("[PROPAGATE] Binary clause: literal %d, other lit %d, var %u value=%d\n",
                           toDimacs(neg(p)), toDimacs(q), v, s->vars[v].value);
                }
#endif

                if (s->vars[v].value == UNDEF) {
                    // Unit propagation
                    s->vars[v].value = sign(q) ? FALSE : TRUE;
                    s->vars[v].level = s->decision_level;
                    s->vars[v].reason = INVALID_CLAUSE;  // Binary clause
                    s->vars[v].trail_pos = s->trail_size;

                    s->trail[s->trail_size].lit = q;
                    s->trail[s->trail_size].level = s->decision_level;
                    s->trail_size++;

#ifdef DEBUG
                    if (getenv("DEBUG_CDCL")) {
                        printf("[PROPAGATE] Binary unit: propagated %d (var %u = %s)\n",
                               toDimacs(q), v, sign(q) ? "false" : "true");
                    }
#endif

                    if (s->opts.phase_saving) {
                        s->vars[v].polarity = !sign(q);
                    }
                } else if (s->vars[v].value == (sign(q) ? TRUE : FALSE)) {
                    // Conflict in binary clause
#ifdef DEBUG
                    if (getenv("DEBUG_CDCL")) {
                        printf("[PROPAGATE] Binary conflict! %d and %d are both false\n",
                               toDimacs(neg(p)), toDimacs(neg(q)));
                    }
#endif
                    // Put watches back
                    while (i < ws->size) {
                        watches[j++] = watches[i++];
                    }
                    ws->size = j;
                    return BINARY_CONFLICT;  // Signal binary conflict
                }

                watches[j++] = w;
                i++;
                continue;
            }

            // Non-binary clause
            CRef cref = w.cref;
            Lit blocker = w.blocker;

            // Check blocker first
            Var bv = var(blocker);
            if (s->vars[bv].value == (sign(blocker) ? FALSE : TRUE)) {
                // Blocker is satisfied - keep watching
                watches[j++] = w;
                i++;
                s->watches->skipped++;
                continue;
            }

            // Need to examine clause
            uint32_t size = CLAUSE_SIZE(s->arena, cref);
            Lit* lits = CLAUSE_LITS(s->arena, cref);

            // Ensure watched literals are in first two positions
            if (lits[0] == neg(p)) {
                lits[0] = lits[1];
                lits[1] = neg(p);
            }
            ASSERT(lits[1] == neg(p));

            // Look for new watch
            Lit first = lits[0];
            Var fv = var(first);

            // If first literal is true, clause is satisfied
            if (s->vars[fv].value == (sign(first) ? FALSE : TRUE)) {
                watches[j++] = (Watch){cref, first};
                i++;
                continue;
            }

            // Look for another literal to watch
            bool found = false;
            for (uint32_t k = 2; k < size; k++) {
                Lit lit = lits[k];
                Var v = var(lit);

                if (s->vars[v].value != (sign(lit) ? TRUE : FALSE)) {
                    // Found a non-false literal
                    lits[1] = lit;
                    lits[k] = neg(p);

                    // Add new watch
                    watch_add(s->watches, lit, cref, first);
                    found = true;
                    break;
                }
            }

            if (found) {
                // Don't keep old watch
                i++;
                continue;
            }

            // Clause is unit or conflicting
            watches[j++] = w;
            i++;

            // Check if unit or conflict
            if (s->vars[fv].value == UNDEF) {
                // Unit clause - propagate
                s->vars[fv].value = sign(first) ? FALSE : TRUE;
                s->vars[fv].level = s->decision_level;
                s->vars[fv].reason = cref;
                s->vars[fv].trail_pos = s->trail_size;

                s->trail[s->trail_size].lit = first;
                s->trail[s->trail_size].level = s->decision_level;
                s->trail_size++;

                if (s->opts.phase_saving) {
                    s->vars[fv].polarity = !sign(first);
                }
            } else {
                // Conflict!
                // Put remaining watches back
                while (i < ws->size) {
                    watches[j++] = watches[i++];
                }
                ws->size = j;
                // NOTE: Don't modify qhead here - leave it for backtracking to handle
                return cref;
            }
        }

        ws->size = j;
    }

    return INVALID_CLAUSE;  // No conflict
}

/*********************************************************************
 * Conflict Analysis (First UIP)
 *********************************************************************/

static uint32_t calc_lbd(Solver* s, const Lit* lits, uint32_t size) {
    static Level levels[256];
    uint32_t lbd = 0;

    for (uint32_t i = 0; i < size; i++) {
        Level level = s->vars[var(lits[i])].level;
        if (level == 0) continue;

        // Check if we've seen this level
        bool seen = false;
        for (uint32_t j = 0; j < lbd; j++) {
            if (levels[j] == level) {
                seen = true;
                break;
            }
        }

        if (!seen && lbd < 256) {
            levels[lbd++] = level;
        }
    }

    return lbd;
}

void solver_analyze(Solver* s, CRef conflict, Lit* learnt, uint32_t* learnt_size, Level* bt_level) {
    uint32_t index = s->trail_size - 1;
    uint32_t pathC = 0;
    Lit p = LIT_UNDEF;

    *learnt_size = 0;
    learnt[(*learnt_size)++] = LIT_UNDEF;  // Leave room for asserting literal
    *bt_level = 0;

    // Process conflict clause
    if (conflict == BINARY_CONFLICT) {
        // Binary conflict - reconstruct from trail
        // The conflict is between the last propagated literal and its negation
        p = s->trail[index].lit;
        pathC = 1;
        s->seen[var(p)] = 1;
        bump_var_activity(s, var(p), s->order.var_inc);
    } else if (conflict != INVALID_CLAUSE) {
        // Regular conflict from arena
        uint32_t size = CLAUSE_SIZE(s->arena, conflict);
        Lit* lits = CLAUSE_LITS(s->arena, conflict);

        for (uint32_t i = 0; i < size; i++) {
            Lit q = lits[i];
            Var v = var(q);

            if (!s->seen[v] && s->vars[v].level > 0) {
                s->seen[v] = 1;
                bump_var_activity(s, v, s->order.var_inc);

                if (s->vars[v].level >= s->decision_level) {
                    pathC++;
                } else {
                    learnt[(*learnt_size)++] = q;
                    if (s->vars[v].level > *bt_level) {
                        *bt_level = s->vars[v].level;
                    }
                }
            }
        }
    } else {
        // Should not happen
        ASSERT(false);
    }

    // Traverse implication graph backwards
    while (pathC > 0) {
        ASSERT(index < s->trail_size);

        // Pick next literal from trail
        while (!s->seen[var(s->trail[index].lit)]) {
            ASSERT(index > 0);
            index--;
        }

        p = s->trail[index].lit;
        Var v = var(p);
        CRef reason = s->vars[v].reason;

        s->seen[v] = 0;
        pathC--;

        if (pathC > 0) {
            // Not the asserting literal yet
            if (reason != INVALID_CLAUSE) {
                // Expand reason clause
                uint32_t size = CLAUSE_SIZE(s->arena, reason);
                Lit* lits = CLAUSE_LITS(s->arena, reason);

                for (uint32_t i = 1; i < size; i++) {  // Skip first (it's p)
                    Lit q = lits[i];
                    Var qv = var(q);

                    if (!s->seen[qv] && s->vars[qv].level > 0) {
                        s->seen[qv] = 1;
                        bump_var_activity(s, qv, s->order.var_inc);

                        if (s->vars[qv].level >= s->decision_level) {
                            pathC++;
                        } else {
                            learnt[(*learnt_size)++] = q;
                            if (s->vars[qv].level > *bt_level) {
                                *bt_level = s->vars[qv].level;
                            }
                        }
                    }
                }
            }
        }

        if (index > 0) index--;
    }

    // First literal is the asserting literal
    learnt[0] = neg(p);

    // Clear seen flags
    for (uint32_t i = 0; i < *learnt_size; i++) {
        s->seen[var(learnt[i])] = 0;
    }
}

/*********************************************************************
 * Decision Making
 *********************************************************************/

bool solver_decide(Solver* s) {
    Var next = INVALID_VAR;

    // Pick unassigned variable with highest activity
    while (s->order.size > 0) {
        next = heap_extract_max(s);
        if (s->vars[next].value == UNDEF) break;
        next = INVALID_VAR;
    }

    if (next == INVALID_VAR) {
        return false;  // All variables assigned
    }

    // Choose polarity
    bool sign = false;

    if (s->opts.phase_saving && s->vars[next].polarity) {
        sign = !s->vars[next].polarity;
    } else if (s->opts.random_phase) {
        // Random phase with probability
        if ((rand() / (double)RAND_MAX) < s->opts.random_phase_prob) {
            sign = rand() & 1;
        } else {
            sign = s->vars[next].polarity;
        }
    } else {
        sign = s->vars[next].polarity;
    }

    // Make decision
    s->decision_level++;
    s->trail_lims[s->decision_level] = s->trail_size;

    Lit dec = mkLit(next, sign);
    s->vars[next].value = sign ? FALSE : TRUE;
    s->vars[next].level = s->decision_level;
    s->vars[next].reason = INVALID_CLAUSE;
    s->vars[next].trail_pos = s->trail_size;

    s->trail[s->trail_size].lit = dec;
    s->trail[s->trail_size].level = s->decision_level;
    s->trail_size++;

    s->stats.decisions++;

    return true;
}

/*********************************************************************
 * Restart Decision
 *********************************************************************/

bool solver_should_restart(Solver* s) {
    if (s->opts.glucose_restart) {
        // Glucose-style adaptive restarts based on LBD moving averages
        // Restart when fast_ma > slow_ma (recent LBD worse than long-term average)
        //
        // This means: recent conflicts are producing worse clauses (higher LBD)
        // than the long-term average, indicating we're in a bad search region.
        //
        // HYBRID STRATEGY: Fall back to geometric if Glucose hasn't triggered
        // This handles cases where LBD is too stable (always same value)

        bool should_restart_glucose = false;

        // Check Glucose condition after enough conflicts
        if (s->stats.conflicts > s->opts.glucose_min_conflicts &&
            s->restart.fast_ma > s->restart.slow_ma) {
            // Restart postponing (Glucose 2.1+): Don't restart if trail is growing
            // This prevents restarting during productive search
            if (s->opts.restart_postpone > 0) {
                // Check if trail has grown significantly since last restart
                uint32_t trail_growth_threshold = s->opts.restart_postpone;  // e.g., 10% growth
                // For simplicity, just check if we have enough trail entries
                if (s->trail_size < trail_growth_threshold) {
                    #ifdef DEBUG
                    if (getenv("DEBUG_CDCL")) {
                        printf("[RESTART] Postponed: trail too small (%u < %u)\n",
                               s->trail_size, trail_growth_threshold);
                    }
                    #endif
                    return false;  // Postpone restart
                }
            }

            should_restart_glucose = true;
        }

        // Hybrid fallback: If Glucose hasn't triggered in too long, use geometric
        // This prevents getting stuck when LBD is too stable
        bool should_restart_geometric = false;
        if (s->restart.conflicts_since >= s->restart.threshold) {
            should_restart_geometric = true;
            s->restart.conflicts_since = 0;
            s->restart.threshold = (uint32_t)(s->restart.threshold * s->opts.restart_inc);
        }

        // Restart if either strategy says so
        return should_restart_glucose || should_restart_geometric;
    } else {
        // Simple geometric restarts (original strategy)
        if (s->restart.conflicts_since >= s->restart.threshold) {
            s->restart.conflicts_since = 0;
            s->restart.threshold = (uint32_t)(s->restart.threshold * s->opts.restart_inc);
            return true;
        }
        return false;
    }
}

/*********************************************************************
 * Clause Database Reduction
 *********************************************************************/

// Helper structure for sorting clauses
typedef struct {
    CRef cref;
    uint32_t lbd;
    float activity;
} ClauseScore;

// Comparison function for qsort - keep clauses with:
// 1. Lower LBD (better quality)
// 2. Higher activity (more recently used)
static int compare_clauses(const void* a, const void* b) {
    const ClauseScore* ca = (const ClauseScore*)a;
    const ClauseScore* cb = (const ClauseScore*)b;

    // First, compare by LBD (lower is better)
    if (ca->lbd != cb->lbd) {
        return ca->lbd - cb->lbd;  // Ascending order (keep low LBD)
    }

    // If LBD is equal, compare by activity (higher is better)
    if (ca->activity > cb->activity) return -1;  // Descending order
    if (ca->activity < cb->activity) return 1;
    return 0;
}

void solver_reduce_db(Solver* s) {
    s->stats.reduces++;

    // Count learned clauses
    uint32_t num_learned = 0;
    for (uint32_t i = 0; i < s->num_clauses; i++) {
        CRef cref = s->clauses[i];
        if (cref == INVALID_CLAUSE) continue;
        if (clause_deleted(s->arena, cref)) continue;
        if (clause_learned(s->arena, cref)) {
            num_learned++;
        }
    }

    // If not too many learned clauses, skip reduction
    uint32_t max_learned = s->num_clauses / 2 + 1000;  // Allow some learned clauses
    if (num_learned < max_learned) {
        return;
    }

    #ifdef DEBUG
    if (getenv("DEBUG_CDCL")) {
        printf("[REDUCE] Reducing clause database: %u learned clauses, max %u\n",
               num_learned, max_learned);
    }
    #endif

    // Collect all learned clauses with their scores
    ClauseScore* scores = (ClauseScore*)malloc(num_learned * sizeof(ClauseScore));
    if (!scores) return;  // Out of memory, skip reduction

    uint32_t j = 0;
    for (uint32_t i = 0; i < s->num_clauses; i++) {
        CRef cref = s->clauses[i];
        if (cref == INVALID_CLAUSE) continue;
        if (clause_deleted(s->arena, cref)) continue;
        if (!clause_learned(s->arena, cref)) continue;

        scores[j].cref = cref;
        scores[j].lbd = clause_lbd(s->arena, cref);
        scores[j].activity = clause_activity(s->arena, cref);
        j++;
    }

    // Sort by quality (low LBD, high activity)
    qsort(scores, num_learned, sizeof(ClauseScore), compare_clauses);

    // Keep the best half, delete the rest
    // But ALWAYS keep glue clauses (LBD <= glue_lbd threshold, typically 2)
    uint32_t to_keep = num_learned / 2;
    uint32_t deleted = 0;

    for (uint32_t i = to_keep; i < num_learned; i++) {
        // Check if this is a glue clause - never delete these
        if (scores[i].lbd <= s->opts.glue_lbd) {
            continue;  // Keep glue clauses even if beyond the limit
        }

        // Delete this clause
        arena_delete(s->arena, scores[i].cref);
        deleted++;
    }

    free(scores);

    s->stats.deleted_clauses += deleted;

    #ifdef DEBUG
    if (getenv("DEBUG_CDCL")) {
        printf("[REDUCE] Deleted %u clauses, kept %u\n", deleted, num_learned - deleted);
    }
    #endif

    // Optionally trigger garbage collection if many deletions
    // For now, let arena GC happen naturally when space is needed
}

/*********************************************************************
 * On-the-Fly Subsumption
 *********************************************************************/

// Check if clause A subsumes clause B
// A subsumes B if all literals in A are in B
// Returns true if A subsumes B
static bool clause_subsumes(const Lit* a, uint32_t a_size, const Lit* b, uint32_t b_size) {
    // Quick check: A cannot subsume B if A is larger
    if (a_size > b_size) {
        return false;
    }

    // Check if every literal in A appears in B
    for (uint32_t i = 0; i < a_size; i++) {
        bool found = false;
        for (uint32_t j = 0; j < b_size; j++) {
            if (a[i] == b[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;  // Literal a[i] not in B
        }
    }

    return true;  // All literals in A are in B
}

// Perform on-the-fly backward subsumption
// Check if the newly learned clause subsumes any existing learned clauses
// If so, delete the subsumed clauses
static void solver_on_the_fly_subsumption(Solver* s, const Lit* learnt, uint32_t learnt_size) {
    // Only do subsumption for small learned clauses (size <= 5)
    // Large clauses are unlikely to subsume others and checking is expensive
    if (learnt_size > 5) {
        return;
    }

    uint32_t subsumed = 0;

    // Check all learned clauses in the database (except the one we just added)
    // The newly added clause is at s->learnts[s->num_learnts - 1]
    uint32_t num_to_check = s->num_learnts > 0 ? s->num_learnts - 1 : 0;

    for (uint32_t i = 0; i < num_to_check; i++) {
        CRef cref = s->learnts[i];
        if (cref == INVALID_CLAUSE) continue;

        // Skip if already deleted
        if (clause_deleted(s->arena, cref)) continue;

        // Get the clause size and literals using macros
        uint32_t other_size = CLAUSE_SIZE(s->arena, cref);
        const Lit* other_lits = CLAUSE_LITS(s->arena, cref);

        // Check if learned clause subsumes this clause
        if (clause_subsumes(learnt, learnt_size, other_lits, other_size)) {
            // Subsumes! Delete the subsumed clause
            arena_delete(s->arena, cref);
            subsumed++;
        }
    }

    if (subsumed > 0) {
        s->stats.subsumed_clauses += subsumed;
    }
}

/*********************************************************************
 * Clause Minimization (Recursive Literal Removal)
 *********************************************************************/

// Recursive clause minimization: check if a literal is redundant
// by recursively checking its reason clause and the reasons of those literals
//
// A literal is redundant if ALL literals in its reason clause are either:
// 1. Already in the learned clause (seen = 1), or
// 2. At decision level 0 (always true), or
// 3. Recursively redundant (can be proven by checking their reasons)
//
// Uses seen array as state:
//   seen[v] = 0: not seen
//   seen[v] = 1: in learned clause (not redundant)
//   seen[v] = 2: being explored (prevents infinite recursion)
//   seen[v] = 3: proven redundant
static bool literal_redundant_recursive(Solver* s, Lit p, int depth) {
    // Recursion depth limit to prevent stack overflow
    if (depth > 100) {
        return false;
    }

    Var v = var(p);

    // If this variable is a decision, it's not redundant
    if (s->vars[v].reason == INVALID_CLAUSE) {
        return false;
    }

    // Check the reason clause
    CRef reason = s->vars[v].reason;

    // Binary clauses: handle specially
    if (reason == BINARY_CONFLICT) {
        // For binary reasons, we'd need to track the other literal
        // For simplicity and speed, skip binary reasons
        return false;
    }

    // Mark this variable as being explored to detect cycles
    uint8_t old_seen = s->seen[v];
    if (old_seen == 2) {
        // Cycle detected - not redundant
        return false;
    }
    s->seen[v] = 2;  // Mark as being explored

    uint32_t size = CLAUSE_SIZE(s->arena, reason);
    Lit* lits = CLAUSE_LITS(s->arena, reason);

    // Check if ALL literals in the reason are either:
    // 1. In the learned clause (seen = 1), or
    // 2. At decision level 0, or
    // 3. Recursively redundant
    for (uint32_t i = 0; i < size; i++) {
        Lit q = lits[i];
        Var qv = var(q);

        // Skip the literal itself
        if (qv == v) continue;

        // If at level 0, it's fine (always true)
        if (s->vars[qv].level == 0) continue;

        // If already in learned clause, it's fine
        if (s->seen[qv] == 1) {
            continue;
        }

        // If already proven redundant, it's fine
        if (s->seen[qv] == 3) {
            continue;
        }

        // Try to recursively prove this literal is redundant
        if (!literal_redundant_recursive(s, q, depth + 1)) {
            // Failed to prove redundancy - restore and return false
            s->seen[v] = old_seen;
            return false;
        }
    }

    // All literals in reason are redundant or in learned clause
    // Mark as proven redundant
    s->seen[v] = 3;
    return true;
}

// Minimize learned clause by removing redundant literals
// Uses recursive analysis of reason clauses
static void solver_minimize_clause(Solver* s, Lit* learnt, uint32_t* learnt_size) {
    uint32_t original_size = *learnt_size;

    if (original_size <= 2) {
        // Don't minimize unit clauses or binary clauses
        return;
    }

    // Mark all literals in the learned clause with seen = 1
    for (uint32_t i = 0; i < *learnt_size; i++) {
        s->seen[var(learnt[i])] = 1;
    }

    // Abstract level for pruning (not used in simple version)
    uint32_t abstract_level = 0;

    // Try to remove each literal (except the asserting literal at position 0)
    uint32_t new_size = 1;  // Keep the asserting literal
    for (uint32_t i = 1; i < *learnt_size; i++) {
        Lit p = learnt[i];
        Var v = var(p);

        // Check if this literal is redundant (recursive deep check)
        if (literal_redundant_recursive(s, p, 0)) {
            // Redundant! It's already marked as seen = 3
            // Don't include in minimized clause
        } else {
            // Not redundant, keep it
            learnt[new_size++] = p;
        }
    }

    *learnt_size = new_size;

    // Clear seen array for all variables that were marked
    // (both those in the original clause and those marked during recursion)
    for (Var v = 1; v <= s->num_vars; v++) {
        if (s->seen[v]) {
            s->seen[v] = 0;
        }
    }
}

/*********************************************************************
 * Vivification (Clause Strengthening)
 *********************************************************************/

// Try to strengthen a clause by removing redundant literals
// Returns true if clause was strengthened (or removed), false if unchanged
static bool vivify_clause(Solver* s, CRef cref) {
    // Can only vivify at decision level 0
    if (s->decision_level > 0) return false;

    // Skip unit and binary clauses
    uint32_t size = CLAUSE_SIZE(s->arena, cref);
    if (size <= 2) return false;

    Lit* lits = CLAUSE_LITS(s->arena, cref);

    // Save current trail size for backtracking
    uint32_t trail_before = s->trail_size;

    // Try to remove each literal
    bool strengthened = false;
    uint32_t new_size = 0;
    Lit new_lits[size];

    for (uint32_t i = 0; i < size; i++) {
        // Assume all OTHER literals are false
        uint32_t assumptions = 0;
        for (uint32_t j = 0; j < size; j++) {
            if (i == j) continue;

            Lit lit = lits[j];
            Var v = var(lit);

            // If already assigned, skip
            if (s->vars[v].value != UNDEF) {
                if (s->vars[v].value == (sign(lit) ? FALSE : TRUE)) {
                    // Literal is true - clause is satisfied, done
                    // Backtrack assumptions
                    while (s->trail_size > trail_before) {
                        s->trail_size--;
                        Lit trail_lit = s->trail[s->trail_size].lit;
                        s->vars[var(trail_lit)].value = UNDEF;
                    }
                    return false;
                }
                continue;
            }

            // Assign the negation (assume this literal is false)
            s->vars[v].value = sign(lit) ? TRUE : FALSE;
            s->vars[v].level = 0;
            s->vars[v].reason = INVALID_CLAUSE;
            s->vars[v].trail_pos = s->trail_size;

            s->trail[s->trail_size].lit = neg(lit);
            s->trail[s->trail_size].level = 0;
            s->trail_size++;
            assumptions++;
        }

        // Now propagate with all other literals false
        CRef conflict = solver_propagate(s);

        if (conflict != INVALID_CLAUSE) {
            // Conflict! This means lits[i] is implied by the other literals
            // So lits[i] is redundant - don't include it
            strengthened = true;
        } else {
            // Check if lits[i] was propagated to false
            Var v = var(lits[i]);
            if (s->vars[v].value == (sign(lits[i]) ? TRUE : FALSE)) {
                // Literal propagated to false - it's redundant!
                strengthened = true;
            } else {
                // Literal is not redundant, keep it
                new_lits[new_size++] = lits[i];
            }
        }

        // Backtrack all assumptions
        while (s->trail_size > trail_before) {
            s->trail_size--;
            Lit trail_lit = s->trail[s->trail_size].lit;
            s->vars[var(trail_lit)].value = UNDEF;
        }
        s->qhead = trail_before;
    }

    // If we strengthened the clause, update it
    if (strengthened && new_size > 0) {
        // Update clause in place
        for (uint32_t i = 0; i < new_size; i++) {
            lits[i] = new_lits[i];
        }

        // Update size
        CLAUSE_HEADER(s->arena, cref)->size = new_size;

        // If became unit, propagate it
        if (new_size == 1) {
            Lit unit = lits[0];
            Var v = var(unit);
            if (s->vars[v].value == UNDEF) {
                s->vars[v].value = sign(unit) ? FALSE : TRUE;
                s->vars[v].level = 0;
                s->vars[v].reason = INVALID_CLAUSE;
                s->vars[v].trail_pos = s->trail_size;

                s->trail[s->trail_size].lit = unit;
                s->trail[s->trail_size].level = 0;
                s->trail_size++;
            }
        }

        return true;
    } else if (strengthened && new_size == 0) {
        // Clause became empty - UNSAT!
        s->result = FALSE;
        return true;
    }

    return false;
}

/*********************************************************************
 * Blocked Clause Elimination (BCE)
 *********************************************************************/

// Check if resolving clauses c1 and c2 on variable v results in a tautology
// A resolvent is a tautology if it contains both a literal and its negation
static bool resolvent_is_tautology(Solver* s, CRef c1, CRef c2, Var v) {
    // Safety check: validate clause references before accessing
    if (c1 == INVALID_CLAUSE || c1 >= s->arena->size ||
        c2 == INVALID_CLAUSE || c2 >= s->arena->size) {
        return false;
    }

    // Safety check: ensure clauses are not deleted
    if (clause_deleted(s->arena, c1) || clause_deleted(s->arena, c2)) {
        return false;
    }

    // Get clause sizes and validate they're reasonable
    uint32_t size1 = CLAUSE_SIZE(s->arena, c1);
    uint32_t size2 = CLAUSE_SIZE(s->arena, c2);

    // Safety check: clause sizes must be reasonable (not corrupted)
    // Maximum clause size should not exceed num_vars
    if (size1 == 0 || size1 > s->num_vars || size2 == 0 || size2 > s->num_vars) {
        return false;
    }

    // Clear seen array
    for (Var i = 1; i <= s->num_vars; i++) {
        s->seen[i] = 0;
    }

    // Add all literals from c1 except v and ¬v
    Lit* lits1 = CLAUSE_LITS(s->arena, c1);
    for (uint32_t i = 0; i < size1; i++) {
        if (var(lits1[i]) != v) {
            Var lit_var = var(lits1[i]);

            // Safety check: validate literal variable is in bounds
            if (lit_var < 1 || lit_var > s->num_vars) {
                // Invalid literal - clause is corrupted, can't check tautology
                for (Var j = 1; j <= s->num_vars; j++) {
                    s->seen[j] = 0;
                }
                return false;
            }

            bool is_negated = sign(lits1[i]);

            // Check if we've seen the opposite polarity
            if (s->seen[lit_var] == (is_negated ? 1 : 2)) {
                // Tautology! We have both x and ¬x
                // Clear seen array before returning
                for (Var j = 1; j <= s->num_vars; j++) {
                    s->seen[j] = 0;
                }
                return true;
            }
            // Mark this polarity as seen (1 = positive, 2 = negative)
            s->seen[lit_var] = is_negated ? 2 : 1;
        }
    }

    // Add all literals from c2 except v and ¬v
    Lit* lits2 = CLAUSE_LITS(s->arena, c2);
    for (uint32_t i = 0; i < size2; i++) {
        if (var(lits2[i]) != v) {
            Var lit_var = var(lits2[i]);

            // Safety check: validate literal variable is in bounds
            if (lit_var < 1 || lit_var > s->num_vars) {
                // Invalid literal - clause is corrupted, can't check tautology
                for (Var j = 1; j <= s->num_vars; j++) {
                    s->seen[j] = 0;
                }
                return false;
            }

            bool is_negated = sign(lits2[i]);

            // Check if we've seen the opposite polarity
            if (s->seen[lit_var] == (is_negated ? 1 : 2)) {
                // Tautology! We have both x and ¬x
                // Clear seen array before returning
                for (Var j = 1; j <= s->num_vars; j++) {
                    s->seen[j] = 0;
                }
                return true;
            }
            // Mark this polarity as seen (1 = positive, 2 = negative)
            s->seen[lit_var] = is_negated ? 2 : 1;
        }
    }

    // Clear seen array
    for (Var i = 1; i <= s->num_vars; i++) {
        s->seen[i] = 0;
    }

    return false;
}

// Check if clause c is blocked on literal lit
// A clause is blocked on a literal L if for every clause D containing ¬L,
// the resolvent of C and D on var(L) is a tautology
static bool clause_is_blocked(Solver* s, CRef cref, Lit blocking_lit) {
    Var v = var(blocking_lit);
    Lit negated = neg(blocking_lit);

    // Get all clauses containing ¬blocking_lit by checking watch lists
    WatchList* wl = watch_list(s->watches, negated);

    // Safety check: watch list might not be initialized
    if (!wl || !wl->watches) {
        return false;
    }

    for (uint32_t i = 0; i < wl->size; i++) {
        CRef other_cref = wl->watches[i].cref;

        // Safety check: validate clause reference
        if (other_cref == INVALID_CLAUSE || other_cref >= s->arena->size) {
            continue;
        }

        // Skip deleted clauses
        if (clause_deleted(s->arena, other_cref)) {
            continue;
        }

        // Skip the same clause
        if (other_cref == cref) {
            continue;
        }

        // Additional validation: check clause size is reasonable
        uint32_t other_size = CLAUSE_SIZE(s->arena, other_cref);
        if (other_size == 0 || other_size > s->num_vars) {
            continue;  // Skip corrupted clauses
        }

        // Check if resolvent is a tautology
        if (!resolvent_is_tautology(s, cref, other_cref, v)) {
            // Found a resolvent that is NOT a tautology
            // This literal is not blocking
            return false;
        }
    }

    // All resolvents are tautologies (or no clauses with ¬L exist)
    // This clause is blocked on blocking_lit
    return true;
}

// Blocked Clause Elimination preprocessing
// Removes clauses that are blocked on some literal
// Returns number of clauses eliminated
static uint32_t solver_eliminate_blocked_clauses(Solver* s) {
    if (!s->opts.bce) {
        return 0;
    }

    uint32_t eliminated = 0;

    // Only eliminate from original clauses (not learned clauses)
    for (uint32_t i = 0; i < s->num_original; i++) {
        // Check if progress stats requested via signal (BCE can be slow)
        if (print_stats_requested) {
            print_stats_requested = 0;
            fprintf(stderr, "c [BCE] Processing clause %u / %u (%.1f%% complete)\n",
                    i, s->num_original, (100.0 * i) / s->num_original);
            fflush(stderr);
        }

        CRef cref = s->clauses[i];

        // Skip deleted clauses - validate bounds before accessing
        if (cref == INVALID_CLAUSE || cref >= s->arena->size) {
            continue;
        }

        if (clause_deleted(s->arena, cref)) {
            continue;
        }

        // Skip learned clauses (shouldn't happen in original clauses, but be safe)
        if (clause_learned(s->arena, cref)) {
            continue;
        }

        uint32_t size = CLAUSE_SIZE(s->arena, cref);
        Lit* lits = CLAUSE_LITS(s->arena, cref);

        // Try each literal as a blocking literal
        bool blocked = false;
        for (uint32_t j = 0; j < size && !blocked; j++) {
            Lit lit = lits[j];

            if (clause_is_blocked(s, cref, lit)) {
                // This clause is blocked on lit - eliminate it!
                // Note: We just mark it as INVALID instead of calling arena_delete()
                // to avoid corrupting watch lists that still point to this clause.
                // The clause memory remains allocated but won't be used in search.
                s->clauses[i] = INVALID_CLAUSE;
                eliminated++;
                blocked = true;

                if (s->opts.verbose) {
                    printf("c [BCE] Eliminated clause blocked on literal %d%d\n",
                           sign(lit) ? -((int)var(lit)) : (int)var(lit), sign(lit));
                }
            }
        }
    }

    if (eliminated > 0 && s->opts.verbose) {
        printf("c [BCE] Eliminated %u blocked clauses\n", eliminated);
    }

    return eliminated;
}

/*********************************************************************
 * Simplification
 *********************************************************************/

bool solver_simplify(Solver* s) {
    // Can only simplify at level 0
    if (s->decision_level > 0) return true;

    // Vivification is DISABLED by default (too expensive)
    // Enable with: s->opts.inprocess = true (future command-line option)
    //
    // Vivify learned clauses periodically
    // static uint32_t last_vivify = 0;
    // if (s->opts.inprocess && s->stats.conflicts > last_vivify + 5000) {
    //     last_vivify = s->stats.conflicts;
    //
    //     uint32_t vivified = 0;
    //     for (uint32_t i = 0; i < s->num_learnts && i < 100; i++) {
    //         CRef cref = s->learnts[i];
    //         if (cref == INVALID_CLAUSE) continue;
    //         if (clause_deleted(s->arena, cref)) continue;
    //
    //         if (vivify_clause(s, cref)) {
    //             vivified++;
    //         }
    //     }
    //
    //     if (vivified > 0 && s->opts.verbose) {
    //         printf("c Vivified %u clauses\n", vivified);
    //     }
    // }

    return true;
}

/*********************************************************************
 * Main Solve Function
 *********************************************************************/

lbool solver_solve(Solver* s) {
    return solver_solve_with_assumptions(s, NULL, 0);
}

lbool solver_solve_with_assumptions(Solver* s, const Lit* assumps, uint32_t n_assumps) {
    // Install signal handlers for progress monitoring
    install_signal_handlers();

    // Check if already solved
    if (s->result != UNDEF) {
        return s->result;
    }

    // Preprocessing: Blocked Clause Elimination
    if (s->opts.bce) {
        uint32_t blocked = solver_eliminate_blocked_clauses(s);
        s->stats.blocked_clauses = blocked;
    }

    // Add assumptions
    for (uint32_t i = 0; i < n_assumps; i++) {
        Lit a = assumps[i];
        Var v = var(a);

        if (s->vars[v].value == UNDEF) {
            s->decision_level++;
            s->trail_lims[s->decision_level] = s->trail_size;

            s->vars[v].value = sign(a) ? FALSE : TRUE;
            s->vars[v].level = s->decision_level;
            s->vars[v].reason = INVALID_CLAUSE;
            s->vars[v].trail_pos = s->trail_size;

            s->trail[s->trail_size].lit = a;
            s->trail[s->trail_size].level = s->decision_level;
            s->trail_size++;
        } else if (s->vars[v].value == (sign(a) ? TRUE : FALSE)) {
            // Conflicting assumption
            s->result = FALSE;
            return FALSE;
        }
    }

    // Initial unit propagation at level 0
    #ifdef DEBUG
    if (getenv("DEBUG_CDCL")) {
        printf("[SOLVE] Starting initial propagation...\n");
    }
    #endif
    CRef initial_conflict = solver_propagate(s);
    if (initial_conflict != INVALID_CLAUSE) {
        // Conflict at level 0 = UNSAT
        #ifdef DEBUG
        if (getenv("DEBUG_CDCL")) {
            printf("[SOLVE] Initial conflict at level 0 - UNSAT\n");
        }
        #endif
        s->result = FALSE;
        return FALSE;
    }
    #ifdef DEBUG
    if (getenv("DEBUG_CDCL")) {
        printf("[SOLVE] Initial propagation complete, entering main loop\n");
    }
    #endif

    // Main solve loop
    // Allocate learnt clause buffer - size based on actual number of variables
    Lit* learnt_clause = (Lit*)malloc((s->num_vars + 1) * sizeof(Lit));
    if (!learnt_clause) {
        s->result = UNDEF;
        return UNDEF;
    }

    #ifdef DEBUG
    uint64_t loop_count = 0;
    #endif

    for (;;) {
        // Check if progress stats requested via signal
        if (print_stats_requested) {
            print_stats_requested = 0;  // Clear flag
            print_progress_stats(s);
        }

        #ifdef DEBUG
        loop_count++;
        if (loop_count % 10000 == 0 && getenv("DEBUG_CDCL")) {
            printf("[LOOP] Iteration: %llu, Trail: %u, Level: %u\n",
                   loop_count, s->trail_size, s->decision_level);
        }
        #endif

        CRef conflict = solver_propagate(s);

        if (conflict != INVALID_CLAUSE) {
            // Conflict!
            s->stats.conflicts++;
            s->restart.conflicts_since++;

            // Adaptive random phase: detect stuck states
            // If many conflicts at low decision levels, we're likely stuck in a local minimum
            // Increase random phase probability to escape
            if (s->opts.random_phase && s->decision_level < 10) {
                s->restart.stuck_conflicts++;
                // After 100 consecutive low-level conflicts, boost randomness
                if (s->restart.stuck_conflicts > 100) {
                    // Temporarily increase random phase probability
                    // This helps escape local minima
                    if (s->opts.random_phase_prob < 0.5) {
                        // Gradually increase random phase when stuck
                        // (will be reset on restart)
                        s->opts.random_phase_prob = 0.2;  // Boost to 20%
                    }
                }
            } else {
                s->restart.stuck_conflicts = 0;  // Reset if we reach higher levels
            }

            // Progress output every 1000 conflicts
            if (s->stats.conflicts % 1000 == 0 && getenv("DEBUG_CDCL")) {
                printf("[PROGRESS] Conflicts: %u, Decisions: %u, Level: %u, Random: %.2f\n",
                       s->stats.conflicts, s->stats.decisions, s->decision_level,
                       s->opts.random_phase_prob);
            }

            if (s->decision_level == 0) {
                // Conflict at level 0 = UNSAT
                s->result = FALSE;
                free(learnt_clause);
                return FALSE;
            }

            // Learn clause
            uint32_t learnt_size;
            Level backtrack_level;
            solver_analyze(s, conflict, learnt_clause, &learnt_size, &backtrack_level);

            // Minimize the learned clause to remove redundant literals
            uint32_t original_size = learnt_size;
            solver_minimize_clause(s, learnt_clause, &learnt_size);
            s->stats.minimized_literals += (original_size - learnt_size);

            // Backtrack (chronological or non-chronological)
            // Chronological backtracking: step down one level at a time
            // Non-chronological: jump directly to target level
            // Research shows chronological is often better - enabled by default
            Level actual_backtrack_level = solver_backtrack_chronological(s, learnt_clause, learnt_size, backtrack_level);

            // Update backtrack_level to reflect where we actually ended up
            backtrack_level = actual_backtrack_level;

            // Add learned clause
            if (learnt_size == 1) {
                // Unit clause
                Lit unit = learnt_clause[0];
                Var v = var(unit);
                ASSERT(s->vars[v].value == UNDEF);

                s->vars[v].value = sign(unit) ? FALSE : TRUE;
                s->vars[v].level = 0;
                s->vars[v].reason = INVALID_CLAUSE;
                s->vars[v].trail_pos = s->trail_size;

                s->trail[s->trail_size].lit = unit;
                s->trail[s->trail_size].level = 0;
                s->trail_size++;

                // BUG FIX: Update Glucose moving averages for unit clauses too!
                // Unit clauses have LBD = 1 (best possible quality)
                if (s->stats.conflicts > 0) {
                    double alpha_fast = s->opts.glucose_fast_alpha;
                    double alpha_slow = s->opts.glucose_slow_alpha;
                    uint32_t lbd = 1;  // Unit clauses have LBD = 1
                    s->restart.fast_ma = alpha_fast * s->restart.fast_ma + (1.0 - alpha_fast) * lbd;
                    s->restart.slow_ma = alpha_slow * s->restart.slow_ma + (1.0 - alpha_slow) * lbd;
                } else {
                    s->restart.fast_ma = 1;
                    s->restart.slow_ma = 1;
                }
            } else {
                // Add learned clause
                CRef learnt_ref = arena_alloc(s->arena, learnt_clause, learnt_size, true);

                if (learnt_ref != INVALID_CLAUSE) {
                    // Update LBD
                    uint32_t lbd = calc_lbd(s, learnt_clause, learnt_size);
                    set_clause_lbd(s->arena, learnt_ref, lbd);

                    if (lbd > s->stats.max_lbd) {
                        s->stats.max_lbd = lbd;
                    }
                    if (lbd <= s->opts.glue_lbd) {
                        s->stats.glue_clauses++;
                    }

                    // Update LBD moving averages for Glucose adaptive restarts
                    // Uses configurable decay factors from opts
                    if (s->stats.conflicts > 0) {
                        double alpha_fast = s->opts.glucose_fast_alpha;
                        double alpha_slow = s->opts.glucose_slow_alpha;
                        s->restart.fast_ma = alpha_fast * s->restart.fast_ma + (1.0 - alpha_fast) * lbd;
                        s->restart.slow_ma = alpha_slow * s->restart.slow_ma + (1.0 - alpha_slow) * lbd;
                    } else {
                        // Initialize moving averages with first LBD
                        s->restart.fast_ma = lbd;
                        s->restart.slow_ma = lbd;
                    }

                    // Add to learned clauses
                    if (s->num_learnts >= s->learnts_size) {
                        uint32_t new_size = s->learnts_size ? s->learnts_size * 2 : 1024;
                        CRef* new_learnts = (CRef*)realloc(s->learnts, new_size * sizeof(CRef));
                        if (new_learnts) {
                            s->learnts = new_learnts;
                            s->learnts_size = new_size;
                        }
                    }
                    if (s->num_learnts < s->learnts_size) {
                        s->learnts[s->num_learnts++] = learnt_ref;
                    }

                    // On-the-fly backward subsumption
                    // Check if this learned clause subsumes any existing clauses
                    solver_on_the_fly_subsumption(s, learnt_clause, learnt_size);

                    // Add watches
                    watch_add(s->watches, learnt_clause[0], learnt_ref, learnt_clause[1]);
                    watch_add(s->watches, learnt_clause[1], learnt_ref, learnt_clause[0]);

                    // Unit propagate the asserting literal
                    Lit unit = learnt_clause[0];
                    Var v = var(unit);
                    ASSERT(s->vars[v].value == UNDEF);

                    s->vars[v].value = sign(unit) ? FALSE : TRUE;
                    s->vars[v].level = backtrack_level;
                    s->vars[v].reason = learnt_ref;
                    s->vars[v].trail_pos = s->trail_size;

                    s->trail[s->trail_size].lit = unit;
                    s->trail[s->trail_size].level = backtrack_level;
                    s->trail_size++;

                    s->stats.learned_clauses++;
                    s->stats.learned_literals += learnt_size;
                }
            }

            // Decay activities
            decay_var_inc(s);

            // Check for restart
            if (solver_should_restart(s)) {
                solver_backtrack(s, n_assumps);  // Keep assumptions
                s->stats.restarts++;
            }

            // Reduce clause database periodically
            if (s->stats.conflicts % s->opts.reduce_interval == 0) {
                solver_reduce_db(s);
            }

            // Simplify/vivify clauses periodically
            solver_simplify(s);

        } else {
            // No conflict
            if (!solver_decide(s)) {
                // No more variables to decide = SAT
                s->result = TRUE;
                free(learnt_clause);
                return TRUE;
            }
        }

        // Check resource limits
        if (s->opts.max_conflicts && s->stats.conflicts >= s->opts.max_conflicts) {
            s->result = UNDEF;
            free(learnt_clause);
            return UNDEF;
        }
        if (s->opts.max_decisions && s->stats.decisions >= s->opts.max_decisions) {
            s->result = UNDEF;
            free(learnt_clause);
            return UNDEF;
        }
        if (s->opts.max_time > 0) {
            double elapsed = (double)clock() / CLOCKS_PER_SEC - s->stats.start_time;
            if (elapsed >= s->opts.max_time) {
                s->result = UNDEF;
                free(learnt_clause);
                return UNDEF;
            }
        }
    }
}