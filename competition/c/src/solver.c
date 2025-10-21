/*********************************************************************
 * BSAT Competition Solver - Core Solver Implementation
 *********************************************************************/

#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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
        .glucose_restart = true,
        .restart_postpone = 10,

        .phase_saving = true,
        .phase_reset_period = 10000,
        .random_phase = false,
        .random_phase_prob = 0.01,
        .adaptive_random = true,

        .max_lbd = 30,
        .glue_lbd = 2,
        .reduce_fraction = 0.5,
        .reduce_interval = 2000,

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
    // Use simple geometric restarts for now
    // TODO: Implement Glucose-style adaptive restarts based on LBD moving averages
    if (s->restart.conflicts_since >= s->restart.threshold) {
        s->restart.conflicts_since = 0;
        s->restart.threshold = (uint32_t)(s->restart.threshold * s->opts.restart_inc);
        return true;
    }
    return false;
}

/*********************************************************************
 * Clause Database Reduction
 *********************************************************************/

void solver_reduce_db(Solver* s) {
    // TODO: Implement clause reduction based on LBD
    // For now, just track that we called it
    s->stats.reduces++;
}

/*********************************************************************
 * Simplification
 *********************************************************************/

bool solver_simplify(Solver* s) {
    // Can only simplify at level 0
    if (s->decision_level > 0) return true;

    // TODO: Remove satisfied clauses
    // TODO: Strengthen clauses with unit literals

    return true;
}

/*********************************************************************
 * Main Solve Function
 *********************************************************************/

lbool solver_solve(Solver* s) {
    return solver_solve_with_assumptions(s, NULL, 0);
}

lbool solver_solve_with_assumptions(Solver* s, const Lit* assumps, uint32_t n_assumps) {
    // Check if already solved
    if (s->result != UNDEF) {
        return s->result;
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

            // Progress output every 1000 conflicts
            if (s->stats.conflicts % 1000 == 0 && getenv("DEBUG_CDCL")) {
                printf("[PROGRESS] Conflicts: %u, Decisions: %u, Level: %u\n",
                       s->stats.conflicts, s->stats.decisions, s->decision_level);
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

            // Backtrack
            solver_backtrack(s, backtrack_level);

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