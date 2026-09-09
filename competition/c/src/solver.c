/*********************************************************************
 * BSAT Competition Solver - Core Solver Implementation
 *********************************************************************/

#include "../include/solver.h"
#include "../include/reduction_sort.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>


static bool solver_rebuild(Solver *s);
static bool solver_prepare_next(Solver *s, double start);

/*********************************************************************
 * Variable Array Growth Configuration
 *
 * These can be overridden at compile time for testing:
 *   cc -DVAR_INITIAL_CAPACITY=1 -DVAR_GROWTH_FACTOR=1 ...
 *********************************************************************/

#ifndef VAR_INITIAL_CAPACITY
#define VAR_INITIAL_CAPACITY 128
#endif

#ifndef VAR_GROWTH_FACTOR
#define VAR_GROWTH_FACTOR 2
#endif

/*********************************************************************
 * DRAT Proof Logging
 *
 * These functions log clause additions and deletions for proof verification.
 * Format: "<lit1> <lit2> ... 0" for add, "d <lit1> <lit2> ... 0" for delete
 *********************************************************************/

/* Write complete chunks through stdio so its normal buffering and final flush
   semantics remain intact. A short write invalidates the solve certificate. */
static FILE *proof_stream(const Solver *s) { return s->proof_file?s->proof_file:s->proof_journal; }
static bool proof_write(Solver *s, const void *buffer, uint32_t size) {
    size_t written=fwrite(buffer, 1, size, proof_stream(s));
    if (!s->proof_file && s->proof_journal) s->journal_bytes+=written;
    if (written == size) return true;
    s->error = true;
    return false;
}

static void proof_text_clause(Solver *s, const Lit *lits, uint32_t size, bool deletion) {
    char buffer[4096];
    uint32_t used = 0;
    if (deletion) { buffer[used++] = 'd'; buffer[used++] = ' '; }
    for (uint32_t i = 0; i < size; ++i) {
        /* uint32_t literals contain at most a ten-digit variable number,
           one minus sign and a separating space. */
        if (sizeof buffer - used < 12) {
            if (!proof_write(s, buffer, used)) return;
            used = 0;
        }
        uint32_t value = var(lits[i]);
        if (sign(lits[i]) && value) buffer[used++] = '-';
        char digits[10];uint32_t count = 0;
        do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value);
        while (count) buffer[used++] = digits[--count];
        buffer[used++] = ' ';
    }
    if (sizeof buffer - used < 2) {
        if (!proof_write(s, buffer, used)) return;
        used = 0;
    }
    buffer[used++] = '0';buffer[used++] = '\n';
    proof_write(s, buffer, used);
}

static void proof_binary_clause(Solver *s, const Lit *lits, uint32_t size, bool deletion) {
    unsigned char buffer[4096];
    uint32_t used=0;
    buffer[used++]=deletion ? 'd' : 'a';
    for (uint32_t i=0;i<size;++i) {
        /* A uint32_t takes at most five base-128 bytes. */
        if (sizeof buffer-used<5) {
            if (!proof_write(s,buffer,used)) return;
            used=0;
        }
        uint32_t x=lits[i];
        while (x>=128) { buffer[used++]=(unsigned char)((x&127)|128);x>>=7; }
        buffer[used++]=(unsigned char)x;
    }
    if (used==sizeof buffer) {
        if (!proof_write(s,buffer,used)) return;
        used=0;
    }
    buffer[used++]=0;
    proof_write(s,buffer,used);
}

static void proof_clause(Solver *s, const Lit *lits, uint32_t size, bool deletion);
static void proof_clause_account_impl(Solver *s, const Lit *lits, uint32_t size, bool deletion) {
    if (!proof_stream(s) || (deletion && !s->proof_file)) return;
    if (!s->proof_file && s->proof_journal) {
        if (s->error) return;
        /* Reserve a whole record before writing any of it. A refused addition
           invalidates the handle, so no dependent learning can be exported. */
        uint64_t bytes=2;
        for (uint32_t i=0;i<size;++i) {
            uint32_t x=s->opts.binary_proof?lits[i]:var(lits[i]);
            if (s->opts.binary_proof) {do {++bytes;x>>=7;} while(x);}
            else {bytes+=1+(sign(lits[i]) && x);do {++bytes;x/=10;} while(x);}
        }
        if (bytes>UINT64_MAX-s->journal_bytes ||
            (s->journal_limit && (s->journal_bytes>s->journal_limit || bytes>s->journal_limit-s->journal_bytes))) {
            s->error=true;return;
        }
    }
    if (s->opts.binary_proof) {
        proof_binary_clause(s,lits,size,deletion);
    } else {
        proof_text_clause(s, lits, size, deletion);
    }
    if (ferror(proof_stream(s))) s->error = true;
}
void proof_add_clause(Solver *s, const Lit *lits, uint32_t size) { proof_clause(s, lits, size, false); }
void proof_delete_clause(Solver *s, const Lit *lits, uint32_t size) { proof_clause(s, lits, size, true); }

static void check_cpu_deadline(Solver *s) {
    if (s->opts.max_time <= 0) return;
    s->stats.clock_checks++;
    s->clock_initialized = true;
    s->clock_polls = 0;
    s->clock_work = s->work;
    s->clock_minimize = s->stats.minimize_inspections;
    double now=solver_cpu_time();
    if (!isfinite(now)) s->error=true;
    else if (now - s->stats.start_time >= s->opts.max_time) s->interrupted = true;
}

static bool budget_exhausted(Solver *s, bool force_clock) {
    if (s->watches->failed) s->error = true;
    if (s->error || s->interrupted) return true;
    if (s->terminate && s->terminate(s->terminate_state)) { s->cancelled=true;s->interrupted=true;return true; }
    /* Avoid a system clock read at every cheap decision/preprocessing poll.
       Long inner loops already poll every 1024 inspections: either work counter
       reaching that interval must force a read, without a second throttle. */
    if (s->opts.max_time > 0 &&
        (force_clock || !s->clock_initialized || ++s->clock_polls >= 128 ||
         s->work - s->clock_work >= 1024 ||
         s->stats.minimize_inspections - s->clock_minimize >= 1024))
        check_cpu_deadline(s);
    return s->interrupted || (s->work_limit && s->work >= s->work_limit);
}

bool solver_budget_exhausted(Solver *s) {
    return budget_exhausted(s, false);
}

bool solver_budget_exhausted_now(Solver *s) {
    return budget_exhausted(s, true);
}

/*********************************************************************
 * Default Options
 *********************************************************************/

SolverOpts default_opts(void) {
    SolverOpts opts = {
        .preprocess_budget = 1000000,
        .subsume_budget = 128,
        .circular = true,
        .seed = 1,
        .equiv = false,
        .equiv_budget = 1000000,
        .congruence = false,
        .congruence_budget = 100000000,
        .factor_budget = 100000000,
        .factor_max_variables = 1024,
        .factor_min_gain = 1,
        .chrono = false,
        .chrono_levels = 100,
        .protect_used = false,
        .dynamic_lbd = false,
        .max_conflicts = 0,        // Unlimited
        .max_decisions = 0,        // Unlimited
        .max_time = 0.0,          // Unlimited

        // Branching heuristic
        .vmtf = false,
        .lrb = false,             // Use VSIDS by default
        .var_decay = 0.95,
        .var_inc = 1.0,
        .clause_decay = 0.999,
        .lrb_step_min = 0.06,     // Minimum step for LRB (from MapleSAT)
        .lrb_step_max = 0.4,      // Maximum step for LRB (from MapleSAT)

        .restart_first = 100,
        .restart_inc = 1.5,
        .glucose_restart = true,   // LBD-based adaptive restarts
        .luby_restart = false,     // Disabled
        .luby_unit = 100,          // Base Luby interval
        .restart_postpone = 10,

        // Glucose EMA parameters (for --glucose-restart-ema)
        .glucose_use_ema = true,       // EMA alternative; both modes use the corrected threshold
        .glucose_fast_alpha = 0.8,     // Fast MA decay factor (tracks recent ~5 conflicts)
        .glucose_slow_alpha = 0.9999,  // Slow MA decay factor (long-term average)
        .glucose_min_conflicts = 100,  // Minimum conflicts since the last restart

        // Glucose sliding window parameters (for --glucose-restart-avg)
        .glucose_window_size = 50,     // Sliding-window length
        .glucose_k = 0.8,              // Restart when recent_average * K > global_average

        .phase_saving = true,
        .phase_reset_period = 10000,
        .random_phase = false,         // Opt-in reproducible diversification
        .random_phase_prob = 0.01,     // 1% random decisions
        .adaptive_random = true,
        .rephase = true,               // Kissat-style target phase rephasing
        .rephase_interval = 1000,      // Rephase every 1000 conflicts

        .max_lbd = 30,
        .glue_lbd = 2,
        .reduce_fraction = 0.5,
        .reduce_interval = 2000,
        .reduce_increment = 0,
        .iterative_minimize = false, // Opt-in pending representative speed gains
        .minimize_budget = 10000, // Reason inspections per learned clause, either mode
        .minimize = true,         // MiniSat-style clause minimization

        .bce = false,             // DISABLED by default - can hurt SAT instance performance
        .probing = true,          // Enable failed literal probing

        // BVE options (opt-in)
        .elim = false,            // BVE disabled by default (opt-in with --elim)
        .elim_max_occ = 10,       // Max occurrences to consider for elimination
        .elim_grow = 0,           // No clause growth allowed by default

        // DRAT proof logging (opt-in)
        .proof_path = NULL,       // No proof by default
        .binary_proof = false,    // Text format by default

        .inprocess = false,
        .inprocess_interval = 10000,
        .subsumption = true,
        .var_elim = true,

        // Local search options (opt-in)
        .local_search = false,    // Disabled by default
        .ls_interval = 5000,      // Run local search every 5000 conflicts
        .ls_max_flips = 100000,   // Max flips per local search call
        .ls_save_phases = false,
        .ls_noise = 0.5,          // WalkSAT noise parameter

        .verbose = false,
        .debug = false,
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
    /* Queue decisions do not consume numeric activity scores. */
    if (s->opts.vmtf) {
        ASSERT(s->vmtf.pending < s->num_vars);
        s->analyze_stack[s->vmtf.pending++] = v;
        return;
    }
    if (s->opts.lrb) {
        // Hybrid VSIDS+LRB: Use VSIDS-style additive bumps with LRB-inspired
        // weighting based on recency of conflict participation
        uint64_t current = s->stats.conflicts;
        uint64_t last = s->lrb_last_conflict[v];
        uint64_t age = (current > last) ? (current - last) : 1;

        // Reward decreases with age: recent participation gets more weight
        // Use inverse sqrt decay (similar to CHB but additive like VSIDS)
        double multiplier = 1.0 / (1.0 + 0.1 * sqrt((double)age));

        // Add weighted increment (like VSIDS but with recency bonus)
        s->vars[v].activity += s->order.var_inc * multiplier;
        s->lrb_last_conflict[v] = current;

        // Rescale if needed (same as VSIDS)
        if (s->vars[v].activity > 1e100) {
            for (Var i = 1; i <= s->num_vars; i++) {
                s->vars[i].activity *= 1e-100;
            }
            s->order.var_inc *= 1e-100;
        }
    } else {
        // Standard VSIDS: add increment to activity
        s->vars[v].activity += inc;

        // Rescale if needed
        if (s->vars[v].activity > 1e100) {
            for (Var i = 1; i <= s->num_vars; i++) {
                s->vars[i].activity *= 1e-100;
            }
            s->order.var_inc *= 1e-100;
        }
    }

    // Update heap position
    if (s->vars[v].heap_pos != UINT32_MAX) {
        heap_percolate_up(s, s->vars[v].heap_pos);
    }
}

static void decay_var_inc(Solver* s) {
    if (s->opts.vmtf) return;
    // Apply decay for both VSIDS and hybrid LRB
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
    if (!opts || !isfinite(opts->var_decay) || opts->var_decay <= 0 || opts->var_decay >= 1 ||
        !isfinite(opts->var_inc) || opts->var_inc <= 0 ||
        !isfinite(opts->clause_decay) || opts->clause_decay <= 0 || opts->clause_decay > 1 ||
        !isfinite(opts->restart_inc) || opts->restart_inc < 1 ||
        !opts->restart_first || !opts->luby_unit || !opts->reduce_interval ||
        !opts->glucose_window_size || !opts->inprocess_interval || !opts->rephase_interval ||
        !opts->ls_interval || !isfinite(opts->max_time) || opts->max_time < 0 ||
        !isfinite(opts->reduce_fraction) || opts->reduce_fraction < 0 || opts->reduce_fraction > 1 ||
        !isfinite(opts->glucose_k) || opts->glucose_k <= 0 || opts->glucose_k > 1 ||
        !isfinite(opts->glucose_fast_alpha) || opts->glucose_fast_alpha < 0 || opts->glucose_fast_alpha >= 1 ||
        !isfinite(opts->glucose_slow_alpha) || opts->glucose_slow_alpha < 0 || opts->glucose_slow_alpha >= 1 ||
        !isfinite(opts->random_phase_prob) || opts->random_phase_prob < 0 || opts->random_phase_prob > 1 ||
        !isfinite(opts->ls_noise) || opts->ls_noise < 0 || opts->ls_noise > 1) return NULL;
    Solver* s = (Solver*)calloc(1, sizeof(Solver));
    if (!s) return NULL;

    // Copy options
    s->opts = *opts;

    // Initialize core structures
    s->arena = arena_init(0);
    if (!s->arena) goto error;
    s->arena->verbose=opts->verbose;

    s->watches = watch_init(0);  // Will grow as variables are added
    if (!s->watches) goto error;

    // Initialize order heap
    s->order.var_inc = opts->var_inc;
    s->order.var_decay = opts->var_decay;

    // Initialize variable capacity (will allocate on first variable add)
    s->var_capacity = 0;

    // Initialize restart state
    s->restart.threshold = opts->restart_first;
    s->restart.luby_index = 0;
    s->mode_limit = 1000; // Give alternating search an initial focused interval.

    // Initialize Glucose sliding window (if using avg mode or as default)
    // Always allocate if glucose_restart is enabled (may switch modes at runtime)
    if (opts->glucose_restart) {
        s->restart.recent_lbds = (uint32_t*)calloc(opts->glucose_window_size, sizeof(uint32_t));
        if (!s->restart.recent_lbds) goto error;
        s->restart.recent_lbds_count = 0;
        s->restart.recent_lbds_head = 0;
        s->restart.lbd_sum = 0;
        s->restart.lbd_count = 0;
    } else {
        s->restart.recent_lbds = NULL;
    }

    // Set start time
    s->stats.start_time = solver_cpu_time();
    if(!isfinite(s->stats.start_time)) goto error;

    // Initialize BVE state (will be allocated on demand)
    s->elim = NULL;

    // Initialize DRAT proof file
    s->proof_file = NULL;
    if (opts->proof_path) {
        s->proof_file = fopen(opts->proof_path, opts->binary_proof ? "wb" : "w");
        if (!s->proof_file) goto error;
    }

    // Initialize rephasing state
    s->rephase.best_phase = NULL;  // Allocated on demand in grow_var_arrays
    s->rephase.best_trail_size = 0;
    s->rephase.conflicts_since = 0;
    s->rephase.rephase_count = 0;

    // Initialize local search state
    s->local_search.state = NULL;  // Allocated on demand when first needed
    s->local_search.conflicts_since = 0;
    s->local_search.calls = 0;
    s->local_search.successes = 0;

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

    free(s->conflict_clause);
    free(s->level_seen);
    free(s->input);
    free(s->lrb_last_conflict);
    free(s->vars);
    free(s->values);
    free(s->trail);
    free(s->trail_lims);
    free(s->clauses);
    free(s->learnts);
    free(s->order.heap);
    free(s->vmtf.nodes);
    free(s->seen);
    free(s->analyze_stack);
    free(s->minimize_touched);
    free(s->binary_reasons);
    free(s->restart.recent_lbds);  // Free Glucose sliding window buffer
    free(s->rephase.best_phase);   // Free rephasing best phase array

    // Free local search state
    if (s->local_search.state) {
        local_search_free(s->local_search.state);
    }

    // Free BVE state
    elim_free(s);

    // Close DRAT proof file
    if (s->proof_file) {
        fclose(s->proof_file);
        s->proof_file = NULL;
    }

    free(s);
}

/*********************************************************************
 * Variable Management
 *********************************************************************/

/**
 * Grow variable-related arrays to new capacity
 * Returns true on success, false on allocation failure
 */
static bool grow_var_arrays(Solver* s, uint32_t new_capacity) {
    // Allocate +1 for 1-indexed variables
    uint32_t alloc_size = new_capacity + 1;

    // Grow variable info array
    VarInfo* new_vars = (VarInfo*)realloc(s->vars, alloc_size * sizeof(VarInfo));
    if (!new_vars) return false;
    s->vars = new_vars;

    if (s->opts.lrb) {
        uint64_t *timestamps = realloc(s->lrb_last_conflict, alloc_size * sizeof *timestamps);
        if (!timestamps) return false;
        s->lrb_last_conflict = timestamps;
    }

    // Dense propagation values: no duplicate assignment state to synchronize.
    uint8_t *new_values = realloc(s->values, alloc_size * sizeof *new_values);
    if (!new_values) return false;
    s->values = new_values;

    // Grow trail
    Trail* new_trail = (Trail*)realloc(s->trail, alloc_size * sizeof(Trail));
    if (!new_trail) return false;
    s->trail = new_trail;

    // Grow trail limits
    Level* new_lims = (Level*)realloc(s->trail_lims, alloc_size * sizeof(Level));
    if (!new_lims) return false;
    s->trail_lims = new_lims;
    s->trail_limits_capacity = alloc_size;

    // Grow heap
    Var* new_heap = (Var*)realloc(s->order.heap, alloc_size * sizeof(Var));
    if (!new_heap) return false;
    s->order.heap = new_heap;

    // Grow seen array
    uint8_t* new_seen = (uint8_t*)realloc(s->seen, alloc_size * sizeof(uint8_t));
    if (!new_seen) return false;
    s->seen = new_seen;
    memset(s->seen + s->var_capacity, 0, alloc_size - s->var_capacity);

    // Grow analyze stack
    Lit* new_stack = (Lit*)realloc(s->analyze_stack, alloc_size * sizeof(Lit));
    if (!new_stack) return false;
    s->analyze_stack = new_stack;
    Var *new_touched = realloc(s->minimize_touched, alloc_size * sizeof *new_touched);
    if (!new_touched) return false;
    s->minimize_touched = new_touched;

    // Grow binary reasons array
    Lit* new_binary_reasons = (Lit*)realloc(s->binary_reasons, alloc_size * sizeof(Lit));
    if (!new_binary_reasons) return false;
    s->binary_reasons = new_binary_reasons;

    // Grow best phase array (for rephasing)
    if (s->opts.rephase) {
        uint8_t* new_best_phase = realloc(s->rephase.best_phase, alloc_size * sizeof *new_best_phase);
        if (!new_best_phase) return false;
        s->rephase.best_phase = new_best_phase;
        // New variables have no known target phase.
        for (uint32_t i = s->var_capacity + 1; i <= new_capacity; i++) {
            s->rephase.best_phase[i] = UNDEF;
        }
    }

    // Resize watch manager in-place (preserves existing watches)
    if (!watch_resize(s->watches, new_capacity)) return false;

    return true;
}

Var solver_new_var(Solver* s) {
    if (!s || s->error || s->watches->failed) return INVALID_VAR;
    if (s->has_solved && !solver_prepare_next(s, -1)) return INVALID_VAR;
    if (s->num_vars >= MAX_VARS) return INVALID_VAR;
    Var v = ++s->num_vars;

    // Grow arrays if needed (geometric growth strategy)
    if (s->num_vars > s->var_capacity) {
        // Calculate new capacity
        uint32_t new_capacity;
        if (s->var_capacity == 0) {
            // First allocation
            new_capacity = VAR_INITIAL_CAPACITY;
        } else {
            // Geometric growth
            new_capacity = s->var_capacity * VAR_GROWTH_FACTOR;
        }

        // Ensure new capacity is sufficient
        if (new_capacity < s->num_vars) {
            new_capacity = s->num_vars;
        }

        // Grow all variable-related arrays
        if (!grow_var_arrays(s, new_capacity)) {
            s->num_vars--;
            s->error = true;
            return INVALID_VAR;
        }

        s->var_capacity = new_capacity;
    }

    // Initialize new variable
    memset(&s->vars[v], 0, sizeof(VarInfo));
    s->values[v] = UNDEF;
    if (s->lrb_last_conflict) s->lrb_last_conflict[v] = 0;
    s->vars[v].level = INVALID_LEVEL;
    s->vars[v].reason = INVALID_CLAUSE;
    s->vars[v].heap_pos = UINT32_MAX;
    s->vars[v].polarity = false;  // Default phase

    // Initialize seen flag
    s->seen[v] = 0;

    // Initialize binary reason (LIT_UNDEF means no binary propagation)
    s->binary_reasons[v] = LIT_UNDEF;

    // Add to decision heap
    heap_insert(s, v);

    return v;
}

/*********************************************************************
 * Trail Management
 *********************************************************************/

static inline void push_trail(Solver* s, Lit lit) {
    Var v = var(lit);
    ASSERT(s->values[v] == UNDEF);

    s->vars[v].reason = INVALID_CLAUSE;
    s->binary_reasons[v] = LIT_UNDEF;
    s->values[v] = sign(lit) ? FALSE : TRUE;
    s->vars[v].level = s->decision_level;
    s->vars[v].trail_pos = s->trail_size;

    s->trail[s->trail_size].lit = lit;
    s->trail_size++;

    // Save phase
    if (s->opts.phase_saving) {
        s->vars[v].polarity = !sign(lit);
    }
}

void solver_backtrack(Solver* s, Level level) {
    if (level >= s->decision_level) return;
    uint32_t pos = s->trail_lims[level + 1];
    if (pos < s->rephase.best_trail_size) s->rephase.best_prefix_valid = false;
    if (s->opts.chrono) {
        uint32_t kept=pos;
        for (uint32_t i=pos;i<s->trail_size;++i) {
            Lit lit=s->trail[i].lit;Var v=var(lit);
            if (s->vars[v].level<=level) {
                s->vars[v].trail_pos=kept;s->trail[kept++].lit=lit;
                ++s->stats.chrono_retained;
            } else {
                s->values[v]=UNDEF;
                if (s->opts.vmtf) solver_vmtf_unassign(s,v);
                s->vars[v].level=INVALID_LEVEL;s->vars[v].reason=INVALID_CLAUSE;
                s->binary_reasons[v]=LIT_UNDEF;
                if (s->vars[v].heap_pos==UINT32_MAX) heap_insert(s,v);
            }
        }
        s->trail_size=kept;
        /* Revisit retained late implications: their watches may have changed
           while assignments from higher levels were still present. */
        if (s->qhead>pos) s->qhead=pos;
        s->decision_level=level;
        return;
    }
    for (uint32_t i = s->trail_size; i > pos;) {
        Var v = var(s->trail[--i].lit);
        s->values[v] = UNDEF;
        if (s->opts.vmtf) solver_vmtf_unassign(s, v);
        s->vars[v].level = INVALID_LEVEL;
        s->vars[v].reason = INVALID_CLAUSE;
        s->binary_reasons[v] = LIT_UNDEF;
        if (s->vars[v].heap_pos == UINT32_MAX) heap_insert(s, v);
    }
    s->trail_size = pos;
    if (s->qhead > pos) s->qhead = pos;
    s->decision_level = level;
}

/*********************************************************************
 * Clause Addition
 *********************************************************************/

static int compare_lits(const void *a, const void *b) {
    Lit x = *(const Lit*)a, y = *(const Lit*)b;
    return (x > y) - (x < y);
}

bool solver_add_clause(Solver* s, const Lit* lits, uint32_t size) {
    if (!s) return false;
    if (size && !lits) { s->error=true; s->result=UNDEF; return false; }
    if (s->error || s->watches->failed) return false;
    if (s->has_solved && !solver_prepare_next(s, -1)) return false;
    ASSERT(s->decision_level == 0);
    for (uint32_t i = 0; i < size; ++i)
        if (!var(lits[i]) || var(lits[i]) > s->num_vars) { s->error = true; return false; }
    if (!s->internal_add) {
        size_t needed = s->input_size + (size_t)size + 1;
        if (needed > s->input_capacity) {
            size_t cap = MAX(needed, s->input_capacity * 2 + 64);
            Lit *p = realloc(s->input, cap * sizeof *p);
            if (!p) { s->error = true; return false; }
            s->input = p; s->input_capacity = cap;
        }
        if (size) memcpy(s->input + s->input_size, lits, size * sizeof *lits);
        s->input_size += size; s->input[s->input_size++] = 0;
        s->input_clauses++;
    }
    if (s->result == FALSE) return false;
    Lit small[16];
    Lit *tmp = size <= 16 ? small : malloc(size * sizeof *tmp);
    if (size && !tmp) { s->error = true; return false; }
    if (size) { memcpy(tmp, lits, size * sizeof *tmp); qsort(tmp, size, sizeof *tmp, compare_lits); }
    uint32_t n = 0;
    for (uint32_t i = 0; i < size; ++i) {
        Lit lit = tmp[i];
        if (n && lit == tmp[n-1]) continue;
        if (n && lit == neg(tmp[n-1])) { if (tmp != small) free(tmp); return true; }
        tmp[n++] = lit;
    }
    /* Keep all normalized input clauses, including units and binaries, in
       the arena. Binary propagation still uses compact implicit watches. */
    CRef cr = arena_alloc(s->arena, tmp, n, false);
    if (cr == INVALID_CLAUSE) { if (tmp != small) free(tmp); s->error = true; return false; }
    if (s->num_clauses == s->clauses_capacity) {
        uint32_t cap = s->clauses_capacity ? s->clauses_capacity * 2 : 64;
        CRef *p = realloc(s->clauses, cap * sizeof *p);
        if (!p) { if (tmp != small) free(tmp); s->error = true; return false; }
        s->clauses = p; s->clauses_capacity = cap;
    }
    s->clauses[s->num_clauses++] = cr;
    s->num_original = s->num_clauses;
    if (!n) { s->base_unsat=true;s->result = FALSE; if (tmp != small) free(tmp); return false; }
    Lit *cl = CLAUSE_LITS(s->arena, cr);
    uint32_t alive = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (lxor(s->values[var(cl[i])], sign(cl[i])) != FALSE) {
            Lit t = cl[alive]; cl[alive++] = cl[i]; cl[i] = t;
        }
    }
    if (n == 2) {
        watch_add(s->watches, cl[0], INVALID_CLAUSE, cl[1]);
        watch_add(s->watches, cl[1], INVALID_CLAUSE, cl[0]);
    } else if (n > 2) {
        watch_add(s->watches, cl[0], cr, cl[1]);
        watch_add(s->watches, cl[1], cr, cl[0]);
    }
    if (!alive) {s->base_unsat=true;s->result = FALSE;}
    else if (alive == 1 && s->values[var(cl[0])] == UNDEF) {
        push_trail(s, cl[0]);
        s->vars[var(cl[0])].reason = cr;
    }
    if (tmp != small) free(tmp);
    return s->result != FALSE;
}

/*********************************************************************
 * Model Access
 *********************************************************************/

lbool solver_model_value(const Solver* s, Var v) {
    if (!s || !v || v > s->num_vars) return UNDEF;
    return s->values[v];
}

/*********************************************************************
 * Statistics
 *********************************************************************/

void solver_print_stats(const Solver* s) {
    double cpu_time = solver_cpu_time() - s->stats.start_time;

    printf("c\n");
    printf("c ========== Statistics ==========\n");
    printf("c CPU time          : %.3f s\n", cpu_time);
    if(s->opts.factor) {
        printf("c Factoring work: %llu\n",(unsigned long long)s->stats.factor_work);
        printf("c Factoring candidates: %llu\n",(unsigned long long)s->stats.factor_candidates);
        printf("c Factoring pruned: %llu\n",(unsigned long long)s->stats.factor_pruned);
        printf("c Factoring variables: %u\n",s->stats.factor_variables);
        printf("c Factoring added: %llu\n",(unsigned long long)s->stats.factor_added);
        printf("c Factoring deleted: %llu\n",(unsigned long long)s->stats.factor_deleted);
    }
    if (s->opts.chrono) {
        printf("c Chronological backtracks: %llu\n", (unsigned long long)s->stats.chronological);
        printf("c Chronological retained assignments: %llu\n", (unsigned long long)s->stats.chrono_retained);
        printf("c Chronological repaired watches: %llu\n", (unsigned long long)s->stats.chrono_rewatched);
        printf("c Lower-level conflicts: %llu\n", (unsigned long long)s->stats.chrono_lower_conflicts);
    }
    if (s->opts.congruence) {
        printf("c Congruence work: %llu\n", (unsigned long long)s->stats.congruence_work);
        printf("c Congruence gates: %llu\n", (unsigned long long)s->stats.congruence_gates);
        printf("c Congruence binary seeds: %llu\n", (unsigned long long)s->stats.congruence_seeds);
        printf("c Congruence strengthened: %llu\n", (unsigned long long)s->stats.congruence_strengthened);
        printf("c Congruence root units: %llu\n", (unsigned long long)s->stats.congruence_units);
        printf("c Congruence merges: %llu\n", (unsigned long long)s->stats.congruence_merges);
        printf("c Congruence clauses: %llu\n", (unsigned long long)s->stats.congruence_clauses);
    }
    if (s->portfolio_attempts) {
        printf("c Portfolio attempts: %u\n", s->portfolio_attempts);
        printf("c Portfolio first conflicts: %llu\n", (unsigned long long)s->portfolio_first_conflicts);
        printf("c Portfolio first decisions: %llu\n", (unsigned long long)s->portfolio_first_decisions);
        printf("c Portfolio total conflicts: %llu\n", (unsigned long long)(s->portfolio_first_conflicts+s->stats.conflicts));
        printf("c Portfolio total decisions: %llu\n", (unsigned long long)(s->portfolio_first_decisions+s->stats.decisions));
    }
    printf("c Decisions         : %llu\n", (unsigned long long)s->stats.decisions);
    printf("c Propagations      : %llu\n", (unsigned long long)s->stats.propagations);
    printf("c Conflicts         : %llu\n", (unsigned long long)s->stats.conflicts);
    printf("c Restarts          : %llu\n", (unsigned long long)s->stats.restarts);
    printf("c Reused levels     : %llu\n", (unsigned long long)s->stats.reused_levels);
    printf("c Local search calls: %u\n", s->local_search.calls);
    printf("c Local search wins : %u\n", s->local_search.successes);
    printf("c Local search flips: %llu\n",
           (unsigned long long)(s->local_search.state ? s->local_search.state->flips : 0));
    printf("c Learned clauses   : %llu\n", (unsigned long long)s->stats.learned_clauses);
    printf("c Learned literals  : %llu\n", (unsigned long long)s->stats.learned_literals);
    printf("c Probing calls     : %llu\n", (unsigned long long)s->stats.probing_calls);
    printf("c Probing work      : %llu\n", (unsigned long long)s->stats.probing_work);
    printf("c Deleted clauses   : %llu\n", (unsigned long long)s->stats.deleted_clauses);
    printf("c Binary minimize checks: %llu\n", (unsigned long long)s->stats.binary_minimize_checks);
    printf("c Binary minimize removed: %llu\n", (unsigned long long)s->stats.binary_minimize_removed);
    printf("c Minimize inspections: %llu\n", (unsigned long long)s->stats.minimize_inspections);
    printf("c Minimize binary steps: %llu\n", (unsigned long long)s->stats.minimize_binary_steps);
    printf("c Minimize cache hits : %llu\n", (unsigned long long)s->stats.minimize_cache_hits);
    printf("c Minimize budget hits: %llu\n", (unsigned long long)s->stats.minimize_budget_hits);
    printf("c Equivalence work  : %llu\n", (unsigned long long)s->stats.equiv_work);
    printf("c Substituted vars  : %llu\n", (unsigned long long)s->stats.equiv_variables);
    printf("c Rewritten clauses : %llu\n", (unsigned long long)s->stats.equiv_clauses);
    printf("c SCC contradictions: %llu\n", (unsigned long long)s->stats.equiv_conflicts);
    printf("c Derived binaries  : %llu\n", (unsigned long long)s->stats.equiv_binaries);
    printf("c LBD improvements  : %llu\n", (unsigned long long)s->stats.lbd_updates);
    printf("c Deadline clock reads: %llu\n", (unsigned long long)s->stats.clock_checks);
    printf("c Target literals copied: %llu\n", (unsigned long long)s->stats.target_copied);
    printf("c Target values cleared: %llu\n", (unsigned long long)s->stats.target_cleared);
    printf("c Blocked clauses   : %llu\n", (unsigned long long)s->stats.blocked_clauses);
    printf("c Eliminated variables: %llu\n", (unsigned long long)(s->elim ? s->elim->vars_eliminated : 0));
    printf("c Elimination resolvents: %llu\n", (unsigned long long)(s->elim ? s->elim->resolvents_added : 0));
    printf("c Elimination removed: %llu\n", (unsigned long long)(s->elim ? s->elim->clauses_removed : 0));
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

    printf("c Literal inspections : %llu\n", (unsigned long long)s->work);
    printf("c Garbage collections : %llu\n", (unsigned long long)s->garbage_collections);
    printf("c Reductions          : %llu\n", (unsigned long long)s->stats.reduces);
    printf("c Mode                : %s\n", s->stable_mode ? "stable" : "focused");
    // Memory statistics
    ArenaStats astats = arena_stats(s->arena);
    printf("c Memory used       : %.2f MB\n", astats.used_bytes / (1024.0 * 1024.0));
    printf("c Memory allocated  : %.2f MB\n", astats.total_bytes / (1024.0 * 1024.0));
    printf("c Memory peak       : %.2f MB\n", s->arena->peak_size * sizeof(uint32_t) / (1024.0 * 1024.0));
    printf("c Memory wasted     : %.2f MB\n", astats.wasted_bytes / (1024.0 * 1024.0));
    printf("c Arena growths     : %u\n", s->arena->num_growths);

    printf("c\n");
}

/*********************************************************************
 * Unit Propagation (Two-Watched Literals)
 *********************************************************************/

static bool clause_locked(Solver *s, CRef cr);
static CRef solver_propagate_account_impl(Solver* s) {
    while (s->qhead < s->trail_size) {
        if ((s->work & 1023) == 0 && solver_budget_exhausted(s)) return INVALID_CLAUSE;
        Lit p = s->trail[s->qhead++].lit;

#ifdef DEBUG
        if (IS_DEBUG(s)) {
            printf("[PROPAGATE] qhead=%u trail_size=%u Processing literal %d (var=%u, value=%d)\n",
                   s->qhead - 1, s->trail_size, toDimacs(p), var(p), s->values[var(p)]);
        }
#endif

        // Get watches for ~p (literals that could become unit)
        WatchList* ws = watch_list(s->watches, neg(p));
        Watch* watches = ws->watches;
        uint32_t i = 0, j = 0;

        s->stats.propagations++;

#ifdef DEBUG
        if (IS_DEBUG(s)) {
            printf("[PROPAGATE] Checking %u watches for literal %d\n",
                   ws->size, toDimacs(neg(p)));
        }
#endif

        while (i < ws->size) {
            if ((s->work & 1023)==0 && solver_budget_exhausted(s)) {
                while (i<ws->size) watches[j++]=watches[i++];
                ws->size=j; s->qhead--; return INVALID_CLAUSE;
            }
            s->work++;
            s->watches->visits++; // Count examined watches, including blocker hits.
            Watch w = watches[i];

            // Binary clause special case
            if (is_binary_watch(w) || is_arena_binary_watch(w)) {
                if (SEARCH_DIAGNOSTICS(s)) s->accounting.binary_visits++;
                CRef binary_reason = watch_clause(w);
                Lit q = w.blocker;
                Var v = var(q);

#ifdef DEBUG
                if (IS_DEBUG(s)) {
                    printf("[PROPAGATE] Binary clause: literal %d, other lit %d, var %u value=%d\n",
                           toDimacs(neg(p)), toDimacs(q), v, s->values[v]);
                }
#endif

                if (s->values[v] == UNDEF) {
                    // Unit propagation via binary clause
                    s->values[v] = sign(q) ? FALSE : TRUE;
                    s->vars[v].level = s->opts.chrono ? s->vars[var(p)].level : s->decision_level;
                    s->vars[v].reason = binary_reason;
                    if (binary_reason != INVALID_CLAUSE) {
                        Lit *lits = CLAUSE_LITS(s->arena, binary_reason);
                        lits[0] = q;lits[1] = neg(p);
                    }
                    s->vars[v].trail_pos = s->trail_size;

                    // Store the other literal for conflict analysis
                    // Binary clause is (neg(p) | q), so neg(p) is the "reason" for q
                    s->binary_reasons[v] = binary_reason == INVALID_CLAUSE ? neg(p) : LIT_UNDEF;

                    s->trail[s->trail_size].lit = q;
                    s->trail_size++;

#ifdef DEBUG
                    if (IS_DEBUG(s)) {
                        printf("[PROPAGATE] Binary unit: propagated %d (var %u = %s)\n",
                               toDimacs(q), v, sign(q) ? "false" : "true");
                    }
#endif

                    if (s->opts.phase_saving) {
                        s->vars[v].polarity = !sign(q);
                    }
                } else if (s->values[v] == (sign(q) ? TRUE : FALSE)) {
                    // Conflict in binary clause: (neg(p) | q) with both literals false
#ifdef DEBUG
                    if (IS_DEBUG(s)) {
                        printf("[PROPAGATE] Binary conflict! %d and %d are both false\n",
                               toDimacs(neg(p)), toDimacs(q));
                    }
#endif
                    // Store the conflicting literals for conflict analysis
                    s->binary_conflict_lits[0] = neg(p);  // The watched literal (false)
                    s->binary_conflict_lits[1] = q;       // The other literal (false)

                    // Put watches back
                    while (i < ws->size) {
                        watches[j++] = watches[i++];
                    }
                    ws->size = j;
                    if (binary_reason != INVALID_CLAUSE) {
                        Lit *lits = CLAUSE_LITS(s->arena, binary_reason);
                        lits[0] = q;lits[1] = neg(p);
                        return binary_reason;
                    }
                    return BINARY_CONFLICT;  // Signal implicit binary conflict
                }

                watches[j++] = w;
                i++;
                continue;
            }

            if (SEARCH_DIAGNOSTICS(s)) s->accounting.long_visits++;
            // Non-binary clause
            CRef cref = w.cref;
            Lit blocker = w.blocker;

            // Check blocker first
            Var bv = var(blocker);
            if (s->values[bv] == (sign(blocker) ? FALSE : TRUE)) {
                if (SEARCH_DIAGNOSTICS(s)) s->accounting.blocker_hits++;
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
            if (s->values[fv] == (sign(first) ? FALSE : TRUE)) {
                if (SEARCH_DIAGNOSTICS(s)) s->accounting.first_hits++;
                watches[j++] = (Watch){cref, first};
                i++;
                continue;
            }

            // Look for another literal to watch
            bool found = false;
            Level reason_level=s->opts.chrono ? s->vars[var(p)].level : s->decision_level;
            uint32_t begin = s->opts.circular ? CLAUSE_HEADER(s->arena, cref)->search : 2;
            if (begin < 2 || begin >= size) begin = 2;
            for (uint32_t offset = 0; offset < size - 2; offset++) {
                uint32_t k = begin + offset;
                if (k >= size) k = 2 + k - size;
                s->work++;
                if ((s->work & 1023)==0 && solver_budget_exhausted(s)) {
                    while (i<ws->size) watches[j++]=watches[i++];
                    ws->size=j; s->qhead--; return INVALID_CLAUSE;
                }
                if (SEARCH_DIAGNOSTICS(s)) {
                    s->accounting.replacement_scans++;
#ifdef BSAT_SEARCH_DIAGNOSTICS
                    ClauseHeader *h=CLAUSE_HEADER(s->arena,cref);
                    if(h->scans<UINT32_MAX) ++h->scans;
                    if(clause_learned(s->arena,cref)) {
                        ++s->accounting.learned_scans;
                        if(size<=64) {
                            ++s->accounting.learned_scans_eligible_size;
                            if(!clause_locked(s,cref)) ++s->accounting.learned_scans_unlocked;
                        } else ++s->accounting.learned_scans_oversize;
                        if(h->strengthened) ++s->accounting.strengthened_scans;
                    }
                    else ++s->accounting.original_scans;
                    uint64_t born=((uint64_t)h->born_hi<<32)|h->born_lo;
                    uint64_t age=s->accounting.diagnostic_conflicts>=born ? s->accounting.diagnostic_conflicts-born : 0;
                    if(age<100) ++s->accounting.scan_age_0_99;
                    else if(age<1000) ++s->accounting.scan_age_100_999;
                    else ++s->accounting.scan_age_1000_plus;
                    if(h->units || h->analyses) ++s->accounting.scans_after_unit_or_analysis;
                    else ++s->accounting.scans_before_unit_or_analysis;
#endif
                    if (size == 3) s->accounting.scan_size_3++;
                    else if (size <= 8) s->accounting.scan_size_4_8++;
                    else s->accounting.scan_size_9_plus++;
                }
                Lit lit = lits[k];
                Var v = var(lit);

                if (s->values[v] != (sign(lit) ? TRUE : FALSE)) {
                    // Found a non-false literal
                    lits[1] = lit;
                    lits[k] = neg(p);

                    CLAUSE_HEADER(s->arena, cref)->search = k;
                    // Add new watch
                    watch_add(s->watches, lit, cref, first);
                    if (SEARCH_DIAGNOSTICS(s)) s->accounting.replacement_moves++;
                    found = true;
                    break;
                }
                if (s->opts.chrono) reason_level=MAX(reason_level,s->vars[v].level);
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
            if (s->values[fv] == UNDEF) {
                if (SEARCH_DIAGNOSTICS(s)) {
                    s->accounting.long_units++;
#ifdef BSAT_SEARCH_DIAGNOSTICS
                    ClauseHeader *h=CLAUSE_HEADER(s->arena,cref);
                    if(h->units<UINT32_MAX) ++h->units;
#endif
                }
                // Unit clause - propagate
                s->values[fv] = sign(first) ? FALSE : TRUE;
                s->vars[fv].level = reason_level;
                s->vars[fv].reason = cref;
                s->vars[fv].trail_pos = s->trail_size;

                s->trail[s->trail_size].lit = first;
                s->trail_size++;

                if (s->opts.phase_saving) {
                    s->vars[fv].polarity = !sign(first);
                }
            } else {
                if (SEARCH_DIAGNOSTICS(s)) s->accounting.long_conflicts++;
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
    // Separate level marks also cover dummy assumption levels.
    uint32_t lbd = 0;

    // Track which levels we've seen
    for (uint32_t i = 0; i < size; i++) {
        Level level = s->vars[var(lits[i])].level;
#ifdef BSAT_ASSUMPTION_LBD
        if (level <= s->lbd_assumption_levels) continue;
#else
        if (level == 0) continue;  // Level 0 doesn't count for LBD
#endif
        if (level < s->levels_capacity && !s->level_seen[level]) {
            s->level_seen[level] = 1;
            lbd++;
        }
    }

    // Clear the seen flags for levels we marked
    for (uint32_t i = 0; i < size; i++) {
        Level level = s->vars[var(lits[i])].level;
        if (level != 0 && level < s->levels_capacity) {
            s->level_seen[level] = 0;
        }
    }

    return lbd;
}

static void improve_clause_lbd(Solver *s, CRef cr) {
    if ((!s->opts.dynamic_lbd && !s->opts.protect_used) || !clause_learned(s->arena, cr)) return;
    if (s->opts.dynamic_lbd) {
        uint32_t old = clause_lbd(s->arena, cr);
        if (old > s->opts.glue_lbd) {
            uint32_t current = calc_lbd(s, CLAUSE_LITS(s->arena, cr), CLAUSE_SIZE(s->arena, cr));
            if (current < old) {
                set_clause_lbd(s->arena, cr, current);
                s->stats.lbd_updates++;
            }
        }
    }
    if (s->opts.protect_used && clause_lbd(s->arena, cr) <= 6)
        CLAUSE_HEADER(s->arena, cr)->flags |= CLAUSE_FROZEN;
}

static void solver_analyze_account_impl(Solver* s, CRef conflict, Lit* learnt, uint32_t* learnt_size, Level* bt_level) {
    s->vmtf.pending = 0;
    uint32_t index = s->trail_size - 1;
    uint32_t pathC = 0;
    Lit p = LIT_UNDEF;

    *learnt_size = 0;
    learnt[(*learnt_size)++] = LIT_UNDEF;  // Leave room for asserting literal
    *bt_level = 0;

    // Process conflict clause
    if (conflict == BINARY_CONFLICT) {
        // Binary conflict - use stored conflict literals
        // Both literals in the binary clause are false
        for (int i = 0; i < 2; i++) {
            Lit q = s->binary_conflict_lits[i];
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
    } else if (conflict != INVALID_CLAUSE) {
        // Regular conflict from arena
        improve_clause_lbd(s, conflict);
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
        while (!s->seen[var(s->trail[index].lit)] ||
               (s->opts.chrono && s->vars[var(s->trail[index].lit)].level!=s->decision_level)) {
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
                improve_clause_lbd(s, reason);
                uint32_t size = CLAUSE_SIZE(s->arena, reason);
                Lit* lits = CLAUSE_LITS(s->arena, reason);

                if (SEARCH_DIAGNOSTICS(s) && clause_learned(s->arena, reason)) {
                    s->accounting.learned_reason_uses++;
                    s->accounting.learned_reason_lbd_sum += clause_lbd(s->arena, reason);
                }
#ifdef BSAT_SEARCH_DIAGNOSTICS
                if(SEARCH_DIAGNOSTICS(s)) {
                    ClauseHeader *h=CLAUSE_HEADER(s->arena,reason);
                    if(h->analyses<UINT32_MAX) ++h->analyses;
                }
#endif
                bump_clause_activity(s->arena, reason, 1.0f);
                for (uint32_t i = 0; i < size; i++) {
                    Lit q = lits[i];
                    Var qv = var(q);
                    if (qv == v) continue;

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
            } else if (s->binary_reasons[v] != LIT_UNDEF) {
                // Binary propagation - expand binary clause reason
                // The binary clause is (binary_reasons[v] | p)
                Lit q = s->binary_reasons[v];
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
            // else: decision variable, no reason to expand
        }

        if (index > 0) index--;
    }

    if (s->opts.vmtf)
        solver_vmtf_bump_batch(s, s->analyze_stack, s->vmtf.pending);
    s->vmtf.pending = 0;

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

    // Queue mode retains the score heap for bookkeeping and API reuse.
    if (s->opts.vmtf) next = solver_vmtf_pick(s);
    else while (s->order.size > 0) {
        next = heap_extract_max(s);
        // Skip assigned variables
        if (s->values[next] != UNDEF) {
            next = INVALID_VAR;
            continue;
        }
        // Skip eliminated variables (from BVE preprocessing)
        if (s->elim && s->elim->eliminated[next]) {
            next = INVALID_VAR;
            continue;
        }
        break;
    }

    if (next == INVALID_VAR) {
        return false;  // All variables assigned
    }

    bool sign = s->opts.phase_saving ? !s->vars[next].polarity : false;
    if (s->opts.alternating && s->stable_mode && s->rephase.best_phase && s->rephase.best_phase[next] != UNDEF)
        sign = s->rephase.best_phase[next] == FALSE;
    if (s->opts.random_phase && (bsat_random(&s->random_state) / 4294967296.0) < s->opts.random_phase_prob)
        sign = (bsat_random(&s->random_state) & 1) != 0;

    // Make decision
    s->decision_level++;
    s->trail_lims[s->decision_level] = s->trail_size;

    Lit dec = mkLit(next, sign);
    push_trail(s, dec);

    s->stats.decisions++;

    return true;
}

/*********************************************************************
 * Target Phases / Rephasing (Kissat-style)
 *********************************************************************/

/**
 * Save current assignment as best if we've assigned more variables than ever before.
 * This tracks the assignment that got closest to a solution.
 */
void solver_maybe_save_best_phases(Solver* s) {
    if (!s->opts.rephase || !s->rephase.best_phase) return;

    // Only save if we've assigned more variables than the previous best
    if (s->trail_size > s->rephase.best_trail_size) {
        uint32_t first = s->rephase.best_prefix_valid ? s->rephase.best_trail_size : 0;
        if (!s->rephase.best_prefix_valid) {
            memset(s->rephase.best_phase, 0, (s->num_vars+1)*sizeof *s->rephase.best_phase);
            s->stats.target_cleared += (uint64_t)s->num_vars + 1;
        }
        // Save the partial target assignment
        for (uint32_t i = first; i < s->trail_size; i++) {
            Lit lit = s->trail[i].lit;
            Var v = var(lit);
            // Save polarity: true=positive, false=negative
            s->rephase.best_phase[v] = sign(lit) ? FALSE : TRUE;
        }
        s->stats.target_copied += s->trail_size - first;
        s->rephase.best_trail_size = s->trail_size;
        s->rephase.best_prefix_valid = true;
        // Copying a target can be expensive without advancing either work
        // counter. Do not defer its deadline check across further decisions.
        check_cpu_deadline(s);
    }
}

/**
 * Perform rephasing: reset saved phases to best phases seen so far.
 * This helps break out of local search areas.
 */
static void solver_rephase(Solver* s) {
    if (!s->opts.rephase || !s->rephase.best_phase) return;

    // Copy best phases to saved polarities
    for (Var v = 1; v <= s->num_vars; v++) {
        if (s->rephase.best_phase[v] != UNDEF)
            s->vars[v].polarity = s->rephase.best_phase[v] == TRUE;
    }

    s->rephase.conflicts_since = 0;
    s->rephase.rephase_count++;

    if (IS_VERBOSE(s)) {
        fprintf(stderr, "c [Rephase #%u] Reset phases to best (trail=%u/%u)\n",
                s->rephase.rephase_count,
                s->rephase.best_trail_size,
                s->num_vars);
    }
}

/*********************************************************************
 * Local Search Hybridization
 *********************************************************************/

/**
 * Try to solve with local search.
 * Returns true if a satisfying assignment was found.
 */
static bool solver_try_local_search(Solver* s) {
    if (!s->opts.local_search) return false;

    // Initialize local search state on first use
    if (!s->local_search.state) {
        s->local_search.state = local_search_init(s);
        if (!s->local_search.state) {
            // Allocation failed, disable local search
            s->opts.local_search = false;
            return false;
        }
    }

    s->local_search.calls++;

    if (IS_VERBOSE(s)) {
        fprintf(stderr, "c [Local Search #%u] Starting with %u max flips, noise=%.2f\n",
                s->local_search.calls, s->opts.ls_max_flips, s->opts.ls_noise);
    }

    // Run local search
    bool found = local_search_run(s, s->local_search.state,
                                  s->opts.ls_max_flips, s->opts.ls_noise);

    if (found) {
        s->local_search.successes++;
        // Copy solution back to solver
        local_search_copy_solution(s, s->local_search.state);

        if (IS_VERBOSE(s)) {
            fprintf(stderr, "c [Local Search] Found satisfying assignment!\n");
        }
    } else if (IS_VERBOSE(s)) {
        fprintf(stderr, "c [Local Search] No solution found (%u unsat remaining)\n",
                s->local_search.state->num_unsat);
    }

    s->local_search.conflicts_since = 0;

    return found;
}

/*********************************************************************
 * Luby Restart Sequence
 *********************************************************************/

// Compute the i-th value in the Luby sequence
// Luby sequence: 1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, ...
// This provides a good balance of short and long restarts
static uint32_t luby_sequence(uint32_t index) {
    uint64_t i=index ? index : 1;
    for (;;) {
        uint64_t boundary=1;
        while (boundary<i) boundary=2*boundary+1;
        if (boundary==i) return (uint32_t)((boundary+1)/2);
        i-=boundary/2;
    }
}

/*********************************************************************
 * Restart Decision
 *********************************************************************/

/* Called only after propagation reaches a consistent fixpoint. Keep a prefix
   whose decisions outrank the next available variable. This is a heuristic,
   not a promise to reproduce all implications of a fresh root restart. */
Level solver_restart_level(Solver *s) {
    if (!s->opts.reuse_trail || s->opts.alternating || !s->decision_level) return 0;
    Var next = INVALID_VAR;
    if (s->opts.vmtf) next = solver_vmtf_pick(s);
    else {
        uint32_t scanned = 0;
        while (s->order.size) {
            if (!(scanned++ & 127) && solver_budget_exhausted(s)) return 0;
            next = s->order.heap[0];
            if (s->values[next] == UNDEF && !(s->elim && s->elim->eliminated[next])) break;
            heap_extract_max(s);
            next = INVALID_VAR;
        }
    }
    if (s->error || s->interrupted) return 0;
    if (next == INVALID_VAR) return s->decision_level;
    Level retained = 0;
    while (retained < s->decision_level) {
        if (!(retained & 127) && solver_budget_exhausted(s)) return 0;
        uint32_t pos = s->trail_lims[retained + 1];
        ASSERT(pos < s->trail_size);
        Var v = var(s->trail[pos].lit);
        if (s->opts.vmtf) {
            if (s->vmtf.nodes[v].stamp <= s->vmtf.nodes[next].stamp) break;
        } else if (s->vars[v].activity <= s->vars[next].activity) break;
        ++retained;
    }
    return retained;
}

bool solver_should_restart(Solver* s) {
    if (s->opts.restart_first == UINT32_MAX) return false;
    if (s->opts.alternating && s->stats.conflicts >= s->mode_limit) {
        s->stable_mode = !s->stable_mode;
        s->mode_limit = s->mode_limit ? s->mode_limit * 2 : 1000;
        s->restart.conflicts_since = 0;
        return true;
    }
    bool restart = false;
    if (s->opts.luby_restart || (s->opts.alternating && s->stable_mode)) {
        uint64_t threshold = (uint64_t)luby_sequence(s->restart.luby_index + 1) * s->opts.luby_unit;
        if (s->opts.alternating && s->stable_mode) threshold *= 10;
        restart = s->restart.conflicts_since >= threshold;
        if (restart) s->restart.luby_index++;
    } else if (s->opts.glucose_restart) {
        if (s->restart.conflicts_since >= s->opts.glucose_min_conflicts) {
            if (s->opts.glucose_use_ema)
                restart = s->lbd_samples && s->restart.fast_ma * s->opts.glucose_k > s->restart.slow_ma;
            else if (s->restart.recent_lbds_count == s->opts.glucose_window_size)
                restart = ((double)s->recent_lbd_sum / s->restart.recent_lbds_count) * s->opts.glucose_k >
                          (double)s->restart.lbd_sum / s->restart.lbd_count;
        }
    } else {
        restart = s->restart.conflicts_since >= s->restart.threshold;
        if (restart) {
            double next = s->restart.threshold * s->opts.restart_inc;
            s->restart.threshold = next >= UINT32_MAX ? UINT32_MAX : (uint32_t)next;
        }
    }
    if (restart) {
        s->restart.conflicts_since = 0;
        s->restart.recent_lbds_count = s->restart.recent_lbds_head = 0;
        s->recent_lbd_sum = 0;
    }
    return restart;
}

static void record_lbd(Solver *s, uint32_t lbd) {
    if (!s->lbd_samples++) s->restart.fast_ma = s->restart.slow_ma = lbd;
    else {
        s->restart.fast_ma = s->opts.glucose_fast_alpha * s->restart.fast_ma + (1-s->opts.glucose_fast_alpha)*lbd;
        s->restart.slow_ma = s->opts.glucose_slow_alpha * s->restart.slow_ma + (1-s->opts.glucose_slow_alpha)*lbd;
    }
    s->restart.lbd_sum += lbd; s->restart.lbd_count++;
    if (!s->restart.recent_lbds) return;
    uint32_t at = s->restart.recent_lbds_head;
    if (s->restart.recent_lbds_count == s->opts.glucose_window_size)
        s->recent_lbd_sum -= s->restart.recent_lbds[at];
    else s->restart.recent_lbds_count++;
    s->restart.recent_lbds[at] = lbd; s->recent_lbd_sum += lbd;
    s->restart.recent_lbds_head = (at + 1) % s->opts.glucose_window_size;
}

/*********************************************************************
 * Clause Database Reduction
 *********************************************************************/

static bool clause_locked(Solver *s, CRef cr) {
    uint32_t size = CLAUSE_SIZE(s->arena, cr);
    Lit *lits = CLAUSE_LITS(s->arena, cr);
    bool locked = size && s->values[var(lits[0])] != UNDEF &&
                  s->vars[var(lits[0])].reason == cr;
#ifdef DEBUG
    /* Propagation and explicit enqueueing place the implied literal first.
       A locked clause cannot move it: it is true until the reason is cleared. */
    bool scanned = false;
    for (uint32_t i = 0; i < size; ++i) {
        Var v = var(lits[i]);
        if (s->values[v] != UNDEF && s->vars[v].reason == cr) scanned = true;
    }
    ASSERT(locked == scanned);
#endif
    return locked;
}

void solver_delete_clause(Solver *s, CRef cr) {
    if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) return;
    proof_delete_clause(s, CLAUSE_LITS(s->arena, cr), CLAUSE_SIZE(s->arena, cr));
    watch_remove_clause(s->watches, s->arena, cr);
    arena_delete(s->arena, cr);
}

static void solver_collect_garbage_account_impl(Solver *s) {
    if (!s->arena->wasted || s->arena->wasted * 4 < s->arena->size) return;
    if (solver_budget_exhausted_now(s)) return;
    Arena *old = s->arena;
    Arena *fresh = arena_init(MAX((size_t)1024, old->size - old->wasted + 1));
    if (!fresh) return;
    fresh->verbose=s->opts.verbose;
    for (size_t at = 1; at < old->size;) {
        if (solver_budget_exhausted(s)) { arena_free(fresh); return; }
        uint32_t words = sizeof(ClauseHeader)/sizeof(uint32_t) + CLAUSE_SIZE(old, at);
        ++s->work; // Visiting a dead header also consumes bounded work.
        if (solver_budget_exhausted(s)) { arena_free(fresh); return; }
        if (!clause_deleted(old, at)) {
            // The fresh arena reserves the full live payload before copying.
            if (words > fresh->capacity - fresh->size) { arena_free(fresh); return; }
            for (uint32_t copied = 0; copied < words;) {
                uint32_t chunk = MIN(words-copied, 1024u-(uint32_t)(s->work & 1023));
                if (s->work_limit && chunk > s->work_limit-s->work)
                    chunk = (uint32_t)(s->work_limit-s->work);
                memcpy(fresh->memory+fresh->size+copied, old->memory+at+copied,
                       chunk*sizeof(uint32_t));
                copied += chunk;s->work += chunk;
                if (solver_budget_exhausted(s)) { arena_free(fresh); return; }
            }
            fresh->size += words;
        }
        at += words;
    }
    if (solver_budget_exhausted_now(s)) { arena_free(fresh); return; }
    // All potentially failing copies are complete. Reuse old search cursors as
    // forwarding references; fresh headers retain the original cursor values.
    CRef next = 1;
    for (size_t at = 1; at < old->size;) {
        ClauseHeader *header = CLAUSE_HEADER(old, at);
        uint32_t words = sizeof(ClauseHeader)/sizeof(uint32_t) + header->size;
        header->search = clause_deleted(old, at) ? INVALID_CLAUSE : next;
        if (!clause_deleted(old, at)) next += words;
        at += words;
    }
    ASSERT(next == fresh->size);
    for (uint32_t l = 0; l < 2*(s->watches->num_vars+1); ++l) {
        WatchList *wl = &s->watches->lists[l];
        uint32_t out = 0;
        for (uint32_t i = 0; i < wl->size; ++i) {
            Watch w = wl->watches[i];
            if (!is_binary_watch(w)) {
                bool binary = is_arena_binary_watch(w);
                CRef cr = CLAUSE_HEADER(old, watch_clause(w))->search;
                if (cr == INVALID_CLAUSE) continue;
                w.cref = binary ? arena_binary_watch_ref(cr) : cr;
            }
            wl->watches[out++] = w;
        }
        wl->size = out;
    }
    for (Var v = 1; v <= s->num_vars; ++v)
        if (s->vars[v].reason != INVALID_CLAUSE) {
            s->vars[v].reason = CLAUSE_HEADER(old, s->vars[v].reason)->search;
            ASSERT(s->vars[v].reason != INVALID_CLAUSE || s->values[v] == UNDEF);
        }
    uint32_t out = 0;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr != INVALID_CLAUSE && CLAUSE_HEADER(old, cr)->search != INVALID_CLAUSE) s->clauses[out++] = CLAUSE_HEADER(old, cr)->search;
    }
    s->num_original = s->num_clauses = out;
    out = 0;
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr != INVALID_CLAUSE && CLAUSE_HEADER(old, cr)->search != INVALID_CLAUSE) s->learnts[out++] = CLAUSE_HEADER(old, cr)->search;
    }
    s->num_learnts = out;
    fresh->peak_size = MAX(old->peak_size, fresh->peak_size);
    fresh->num_growths += old->num_growths;
    s->arena = fresh;
    if (s->elim) {
        elim_clear_occs(s);
        s->elim->resolvent_crefs_size = 0;
        elim_build_occs(s);
    }
    arena_free(old); s->garbage_collections++;
}

bool solver_should_reduce(Solver* s) {
    uint64_t conflicts=s->reduce_conflict_offset+MIN(s->stats.conflicts,UINT64_MAX-s->reduce_conflict_offset);
    if (!s->opts.reduce_increment)
        return (s->reduce_conflict_offset%s->opts.reduce_interval+
                s->stats.conflicts%s->opts.reduce_interval)%s->opts.reduce_interval == 0;
    if (conflicts < s->reduce_limit || s->reduce_limit == UINT64_MAX)
        return false;
    s->reduce_span += MIN((uint64_t)s->opts.reduce_increment, UINT64_MAX-s->reduce_span);
    s->reduce_limit = conflicts + MIN(s->reduce_span, UINT64_MAX-conflicts);
    return true;
}

static void solver_reduce_db_account_impl(Solver* s) {
    s->stats.reduces++;
    ClauseScore *scores = malloc((s->num_learnts ? s->num_learnts : 1) * sizeof *scores);
    if (!scores) return;
    uint32_t n = 0;
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        if (s->opts.protect_used) {
            bool used = (CLAUSE_HEADER(s->arena, cr)->flags & CLAUSE_FROZEN) != 0;
            CLAUSE_HEADER(s->arena, cr)->flags &= ~CLAUSE_FROZEN;
            if (used && clause_lbd(s->arena, cr) <= 6) continue;
        }
        if (CLAUSE_SIZE(s->arena, cr) <= 2 || clause_lbd(s->arena, cr) <= s->opts.glue_lbd || clause_locked(s, cr)) continue;
        scores[n++] = (ClauseScore){cr, clause_lbd(s->arena, cr), clause_activity(s->arena, cr)};
    }
    if (SEARCH_DIAGNOSTICS(s)) s->accounting.reduced_candidates += n;
#ifdef BSAT_REDUCTION_TRACE
    if (s->stats.conflicts<=4001) for(uint32_t i=0;i<n;++i)
        fprintf(stderr,"c reduction-before %llu %u %u %u %a\n",
                (unsigned long long)s->stats.conflicts,i,scores[i].cref,
                scores[i].lbd,(double)scores[i].activity);
#endif
    bsat_sort_clause_scores(scores,n);
#ifdef BSAT_REDUCTION_TRACE
    if (s->stats.conflicts<=4001) for(uint32_t i=0;i<n;++i)
        fprintf(stderr,"c reduction-after %llu %u %u %u %a\n",
                (unsigned long long)s->stats.conflicts,i,scores[i].cref,
                scores[i].lbd,(double)scores[i].activity);
#endif
    uint32_t keep = (uint32_t)(n * s->opts.reduce_fraction);
    for (uint32_t i = 0; i < n; ++i) {
        if (i >= keep || scores[i].lbd > s->opts.max_lbd) {
            if (SEARCH_DIAGNOSTICS(s) && scores[i].activity == 0)
                s->accounting.deleted_without_analysis_use++;
            solver_delete_clause(s, scores[i].cref); s->stats.deleted_clauses++;
        } else CLAUSE_HEADER(s->arena, scores[i].cref)->activity *= s->opts.clause_decay;
    }
    free(scores);
    uint32_t out = 0;
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr)) s->learnts[out++] = cr;
    }
    s->num_learnts = out;
    solver_collect_garbage(s);
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

    uint32_t checks = MIN(num_to_check, s->opts.subsume_budget);
    for (uint32_t k = 0; k < checks; k++) {
        uint32_t i = s->subsume_cursor++ % num_to_check;
        CRef cref = s->learnts[i];
        if (cref == INVALID_CLAUSE) continue;

        // Skip if already deleted
        if (clause_deleted(s->arena, cref)) continue;

        // Get the clause size and literals using macros
        uint32_t other_size = CLAUSE_SIZE(s->arena, cref);
        if (other_size > 64) continue;
        const Lit* other_lits = CLAUSE_LITS(s->arena, cref);

        // Check if learned clause subsumes this clause
        if (clause_subsumes(learnt, learnt_size, other_lits, other_size)) {
            if (!clause_locked(s, cref)) {
                // Log deletion to DRAT proof file BEFORE deleting
                solver_delete_clause(s, cref);
                subsumed++;
            }
        }
    }

    if (subsumed > 0) {
        s->stats.subsumed_clauses += subsumed;
    }
}

/*********************************************************************
 * Vivification (Clause Strengthening)
 *********************************************************************/

/* RUP vivification: test removals sequentially against the current formula.
   Replacing a clause allocates a new arena record; shrinking in place would
   invalidate arena traversal and garbage collection. */
static bool rup_candidate(Solver *s, const Lit *lits, uint32_t size) {
    bool conflict = false;
    s->decision_level = 1;
    s->trail_lims[1] = s->trail_size;
    for (uint32_t i = 0; i < size; ++i) {
        lbool val = lxor(s->values[var(lits[i])], sign(lits[i]));
        if (val == TRUE) { conflict = true; break; }
        if (val == UNDEF) push_trail(s, neg(lits[i]));
    }
    if (!conflict) conflict = solver_propagate(s) != INVALID_CLAUSE;
    solver_backtrack(s, 0);
    return conflict && !s->interrupted && !s->error;
}

static void propagate_rup_root(Solver *s) {
    if (s->result == FALSE || s->error || s->interrupted) return;
    /* Root units must be closed at level zero before any temporary RUP trail.
       This propagation is mandatory even after the optional work allowance,
       while the CPU deadline remains active. */
    uint64_t limit=s->work_limit;s->work_limit=0;
    if (solver_propagate(s) != INVALID_CLAUSE) s->result=FALSE;
    s->work_limit=limit;
}

bool solver_add_rup_clause(Solver *s, const Lit *lits, uint32_t size) {
    if (!s || (size && !lits) || s->decision_level || s->has_solved || s->elim ||
        s->result == FALSE || solver_budget_exhausted(s)) return false;
    for (uint32_t i = 0; i < size; ++i)
        if (!var(lits[i]) || var(lits[i]) > s->num_vars) return false;
    propagate_rup_root(s);
    if (s->result == FALSE || solver_budget_exhausted(s)) return false;
    /* Certificate checks are not search probes. Preserve the caller's saved
       phases while exploring their temporary assumptions and implications. */
    bool save_phases=s->opts.phase_saving;s->opts.phase_saving=false;
    bool implied=rup_candidate(s,lits,size);
    s->opts.phase_saving=save_phases;
    if (!implied || solver_budget_exhausted(s)) return false;
    proof_add_clause(s,lits,size);
    if (s->error) return false;
    bool internal = s->internal_add;s->internal_add = true;
    solver_add_clause(s,lits,size);s->internal_add = internal;
    propagate_rup_root(s);
    if (s->watches->failed) s->error = true;
    return !s->error && !s->interrupted;
}

static bool solver_simplify_account_impl(Solver *s) {
    if (s->decision_level || !s->opts.inprocess || !s->opts.preprocess_budget ||
        s->stats.conflicts < s->last_vivify + s->opts.inprocess_interval) return true;
    s->last_vivify = s->stats.conflicts;
    s->work_limit = s->work + MIN(s->opts.preprocess_budget, UINT64_MAX-s->work);
    uint32_t count = MIN(s->num_learnts, 100u);
    for (uint32_t k = 0; k < count && !solver_budget_exhausted(s); ++k) {
        uint32_t at = s->vivify_cursor++ % s->num_learnts;
        CRef cr = s->learnts[at];
        if (clause_deleted(s->arena, cr) || clause_locked(s, cr)) continue;
        uint32_t n = CLAUSE_SIZE(s->arena, cr);
        if (n <= 2 || n > 64) continue;
        Lit *candidate = malloc(n * sizeof *candidate);
        if (!candidate) continue;
        memcpy(candidate, CLAUSE_LITS(s->arena, cr), n * sizeof *candidate);
        uint32_t old_n = n;
#ifdef BSAT_SEARCH_DIAGNOSTICS
        uint64_t trial_start_work=s->work;
        if(SEARCH_DIAGNOSTICS(s)) ++s->accounting.vivify_attempts;
#endif
        for (uint32_t i = 0; i < n && !solver_budget_exhausted(s);) {
            Lit removed = candidate[i];
            memmove(candidate+i, candidate+i+1, (n-i-1)*sizeof *candidate);
            if (rup_candidate(s, candidate, n-1)) {
                --n; proof_add_clause(s, candidate, n);
            } else {
                memmove(candidate+i+1, candidate+i, (n-i-1)*sizeof *candidate);
                candidate[i++] = removed;
            }
        }
        if (n < old_n && !s->error) {
            uint32_t lbd = clause_lbd(s->arena, cr);
            CRef fresh = arena_alloc(s->arena, candidate, n, true);
            if (fresh == INVALID_CLAUSE) s->error = true;
            else {
#ifdef BSAT_SEARCH_DIAGNOSTICS
                CLAUSE_HEADER(s->arena,fresh)->born_lo=(uint32_t)s->accounting.diagnostic_conflicts;
                CLAUSE_HEADER(s->arena,fresh)->born_hi=(uint32_t)(s->accounting.diagnostic_conflicts>>32);
                CLAUSE_HEADER(s->arena,fresh)->strengthened=1;
                if(SEARCH_DIAGNOSTICS(s)) {
                    ++s->accounting.vivify_replaced;
                    s->accounting.vivify_removed_literals+=old_n-n;
                }
#endif
                solver_delete_clause(s, cr); s->learnts[at] = fresh;
                set_clause_lbd(s->arena, fresh, MIN(lbd,n));
                if (!n) s->result = FALSE;
                else if (n == 1) {
                    lbool val = lxor(s->values[var(candidate[0])], sign(candidate[0]));
                    if (val == FALSE) s->result = FALSE;
                    else if (val == UNDEF) { push_trail(s, candidate[0]); s->vars[var(candidate[0])].reason = fresh; }
                } else {
                    CRef watched = n == 2 ? arena_binary_watch_ref(fresh) : fresh;
                    watch_add(s->watches, candidate[0], watched, candidate[1]);
                    watch_add(s->watches, candidate[1], watched, candidate[0]);
                    /* Existing root assignments must be replayed for new watches. */
                    s->qhead = 0;
                }
                s->stats.minimized_literals += old_n-n;
            }
        }
#ifdef BSAT_SEARCH_DIAGNOSTICS
        if(SEARCH_DIAGNOSTICS(s)) s->accounting.vivify_work+=s->work-trial_start_work;
#endif
        free(candidate);
        if (s->result == FALSE || s->error) break;
        /* Complete root propagation before the next temporary RUP check. */
        s->work_limit = 0;
        if (solver_propagate(s) != INVALID_CLAUSE) { s->result = FALSE; break; }
        s->work_limit = s->work + MIN(s->opts.preprocess_budget, UINT64_MAX-s->work) / (count ? count : 1);
    }
    s->work_limit = 0;
    return s->result != FALSE;
}

static uint32_t solver_eliminate_blocked_clauses(Solver *s);
static uint32_t solver_eliminate_blocked_clauses_account_impl(Solver *s) {
    elim_build_occs(s);
    uint32_t eliminated = 0;
    if (!s->elim || !s->elim->occs_complete) return 0;
    for (uint32_t i = 0; i < s->num_clauses && !solver_budget_exhausted(s); ++i) {
        CRef cr = s->clauses[i];
        if (clause_deleted(s->arena, cr) || clause_locked(s, cr)) continue;
        uint32_t size = CLAUSE_SIZE(s->arena, cr);
        Lit *lits = CLAUSE_LITS(s->arena, cr);
        if (size < 2) continue;
        for (uint32_t j = 0; j < size && !solver_budget_exhausted(s); ++j) {
            Lit pivot = lits[j];
            if (s->values[var(pivot)] != UNDEF) continue;
            OccList *o = elim_get_occs(s, neg(pivot));
            bool blocked = true;
            for (uint32_t k = 0; k < o->size; ++k) {
                CRef other = o->clauses[k];
                if (clause_deleted(s->arena, other)) continue;
                uint32_t n = CLAUSE_SIZE(s->arena, other);
                if (!elim_bounded_tautology(s,lits,size,CLAUSE_LITS(s->arena,other),n,var(pivot))) {
                    blocked = false; break;
                }
            }
            if (blocked) {
                Lit *saved = malloc((size+1)*sizeof *saved);
                if (!saved) { s->error = true; return eliminated; }
                memcpy(saved,lits,size*sizeof *saved); saved[size]=0;
                bool ok = elim_save(s,var(pivot),saved,size+1); free(saved);
                if (!ok) return eliminated;
                solver_delete_clause(s,cr); eliminated++; break;
            }
        }
    }
    return eliminated;
}

static int failed_literal_probing(Solver *s);
static int failed_literal_probing_account_impl(Solver *s) {
    int found = 0;
    for (Var v = 1; v <= s->num_vars && !solver_budget_exhausted(s); ++v) {
        if (s->values[v] != UNDEF || elim_is_eliminated(s, v)) continue;
        for (unsigned polarity = 0; polarity < 2; ++polarity) {
            if (solver_budget_exhausted(s)) break;
            Lit implied = mkLit(v, polarity != 0);
            if (rup_candidate(s, &implied, 1)) {
                proof_add_clause(s, &implied, 1);
                push_trail(s, implied); found++;
                /* Root propagation is mandatory even after the probing budget. */
                uint64_t budget = s->work_limit; s->work_limit = 0;
                CRef conflict = solver_propagate(s); s->work_limit = budget;
                if (conflict != INVALID_CLAUSE) return -1;
                break;
            }
        }
    }
    return found;
}

/*********************************************************************
 * Main Solve Function
 *********************************************************************/

static bool solver_rebuild_timed(Solver *s, double start_time, double max_time) {
    if (s->error || s->watches->failed) return false;
    /* Restore input after destructive preprocessing or an assumption solve.
       This deliberately sacrifices learned-clause reuse for a simple, safe API. */
    SolverOpts opts = s->opts;
    const char *path = opts.proof_path;
    opts.proof_path = NULL;
    Solver *fresh = solver_new_with_opts(&opts);
    if (!fresh) { s->error = true; return false; }
    fresh->proof_journal=s->proof_journal;
    fresh->journal_bytes=s->journal_bytes;fresh->journal_limit=s->journal_limit;
    fresh->terminate=s->terminate;fresh->terminate_state=s->terminate_state;
    fresh->learn_callback=s->learn_callback;fresh->learn_state=s->learn_state;fresh->learn_max_length=s->learn_max_length;
    fresh->opts.proof_path = path;
    fresh->stats.start_time = start_time;
    fresh->opts.max_time = max_time;
    Var original_vars=s->factor_original_vars?s->factor_original_vars:s->num_vars;
    while (fresh->num_vars < original_vars) {
        if (solver_budget_exhausted(fresh)) goto incomplete;
        if (!solver_new_var(fresh)) { solver_free(fresh); s->error=true; return false; }
    }
    fresh->internal_add=true; /* Borrow original input until commit, never duplicate it. */
    size_t start = 0;
    for (size_t i = 0; i < s->input_size; ++i) {
        if (!(i & 1023) && solver_budget_exhausted_now(fresh)) goto incomplete;
        if (!s->input[i]) {
            solver_add_clause(fresh, s->input+start, (uint32_t)(i-start)); start=i+1;
        }
    }
    if (fresh->error) { solver_free(fresh); s->error=true; return false; }
    if (solver_budget_exhausted_now(fresh)) goto incomplete;
    fresh->opts.max_time = opts.max_time;
    if (s->proof_file) {
        int failed = fclose(s->proof_file); s->proof_file=NULL;
        if (failed) { solver_free(fresh); s->error=true; return false; }
    }
    if (path) {
        fresh->proof_file=fopen(path, opts.binary_proof ? "wb" : "w");
        if (!fresh->proof_file) { solver_free(fresh); s->error=true; return false; }
    }
    fresh->internal_add=false;
    fresh->input=s->input;fresh->input_size=s->input_size;
    fresh->input_capacity=s->input_capacity;fresh->input_clauses=s->input_clauses;
    s->input=NULL;s->input_size=s->input_capacity=0;s->input_clauses=0;
    if (s->opts.accounting) {
        uint64_t overlap=solver_memory(s).total+solver_memory(fresh).total;
        fresh->accounting.rebuild_overlap_peak=MAX(overlap,s->accounting.rebuild_overlap_peak);
        fresh->accounting.congruence_temporary_peak=MAX(fresh->accounting.congruence_temporary_peak,s->accounting.congruence_temporary_peak);
    }
    for (unsigned phase=0;phase<ACCOUNT_PHASES;++phase) {
        fresh->accounting.seconds[phase] += s->accounting.seconds[phase];
        fresh->accounting.calls[phase] += s->accounting.calls[phase];
    }
    fresh->reused_solves=s->reused_solves;
    Solver old = *s; *s = *fresh; *fresh = old; solver_free(fresh);
    return true;
incomplete:
    s->error |= fresh->error;
    s->interrupted |= fresh->interrupted;
    s->cancelled |= fresh->cancelled;
    solver_free(fresh);
    return false;
}

static bool solver_rebuild(Solver *s) { return solver_rebuild_timed(s, 0, 0); }
bool solver_reset_learning(Solver *s) {
    if (!s || s->error || s->watches->failed) return false;
    return solver_rebuild_timed(s,solver_cpu_time(),s->opts.max_time);
}

/* Fast reuse is deliberately conservative. Reconstruction state, destructive
   preprocessing modes, local search's model trail, proofs and interrupted
   propagation retain the rebuild path. CDCL learning treats assumptions as
   decisions, so learned clauses remain consequences of the permanent formula
   even after conditional UNSAT. Congruence's entailed additions are compatible;
   actual equivalence substitution records reconstruction state and falls back. */
static bool solver_prepare_next(Solver *s, double start) {
    if (s->error || s->watches->failed) return false;
    bool reuse=s->opts.reuse_learnts && !s->factor_original_vars && !s->proof_file && !s->opts.proof_path &&
        !s->elim && !s->opts.elim && !s->opts.bce && !s->opts.local_search && !s->opts.inprocess &&
        !s->interrupted;
    if (!reuse) return start<0 ? solver_rebuild(s) : solver_rebuild_timed(s,start,s->opts.max_time);
    bool unsat=s->base_unsat || (s->result==FALSE && !s->last_assumptions);
    /* Database maintenance follows the retained database across short queries,
       while public work counters and query budgets still restart at zero. */
    s->reduce_conflict_offset+=MIN(s->stats.conflicts,UINT64_MAX-s->reduce_conflict_offset);
    memset(&s->stats,0,sizeof s->stats);
    s->stats.start_time=start<0 ? solver_cpu_time() : start;
    s->work_limit=0;s->clock_initialized=false;s->clock_polls=0;
    solver_backtrack(s,0);
    s->qhead=0; /* Replay root assignments against retained/new watches. */
    free(s->conflict_clause);s->conflict_clause=NULL;s->conflict_size=0;
    uint32_t *recent=s->restart.recent_lbds;
    memset(&s->restart,0,sizeof s->restart);
    s->restart.recent_lbds=recent;s->restart.threshold=s->opts.restart_first;
    if (s->lrb_last_conflict) memset(s->lrb_last_conflict,0,((size_t)s->var_capacity+1)*sizeof *s->lrb_last_conflict);
    s->last_vivify=0;s->lbd_samples=0;s->recent_lbd_sum=0;
    s->rephase.conflicts_since=0;s->mode_limit=1000;s->stable_mode=false;
    s->has_solved=false;s->last_assumptions=0;s->result=unsat?FALSE:UNDEF;
    ++s->reused_solves;
    return true;
}


bool solver_check_model(const Solver *s) {
    bool satisfied = false;
    for (size_t i=0;i<s->input_size;++i) {
        Lit l=s->input[i];
        if (!l) { if (!satisfied) return false; satisfied=false; }
        else if (lxor(s->values[var(l)], sign(l)) == TRUE) satisfied=true;
    }
    return true;
}

bool solver_normalize_conflict(Solver *s, CRef conflict) {
    Lit *lits=conflict==BINARY_CONFLICT ? s->binary_conflict_lits : CLAUSE_LITS(s->arena,conflict);
    uint32_t size=conflict==BINARY_CONFLICT ? 2 : CLAUSE_SIZE(s->arena,conflict);
    Level highest=0,second_level=0;uint32_t first=0,second=1;
    for (uint32_t i=0;i<size;++i) {
        Level level=s->vars[var(lits[i])].level;
        if (!i || level>highest) {
            second=first;second_level=highest;first=i;highest=level;
        } else if (i==1 || level>second_level) { second=i;second_level=level; }
        ++s->work;
        if ((s->work & 1023)==0 && solver_budget_exhausted(s)) return false;
    }
    if (solver_budget_exhausted(s)) return false;
    /* With out-of-order levels, a low-level trigger can conflict with higher
       non-watched literals. Keep the two highest levels watched before any
       backtracking so later assignments cannot hide a unit or conflict. */
    if (size>2 && highest && (first>1 || second>1)) {
        watch_remove_clause(s->watches,s->arena,conflict);
        Lit tmp=lits[0];lits[0]=lits[first];lits[first]=tmp;
        if (!second) second=first;
        tmp=lits[1];lits[1]=lits[second];lits[second]=tmp;
        watch_add(s->watches,lits[0],conflict,lits[1]);
        watch_add(s->watches,lits[1],conflict,lits[0]);
        if (s->watches->failed) { s->error=true;return false; }
        ++s->stats.chrono_rewatched;
    }
    if (highest<s->decision_level) {
        ++s->stats.chrono_lower_conflicts;
        solver_backtrack(s,highest);
    }
    return highest>0;
}

static lbool solve_internal(Solver *s, const Lit *assumps, uint32_t n_assumps);
static lbool solve_internal_account_impl(Solver *s, const Lit *assumps, uint32_t n_assumps) {
    if (s->result == FALSE) {s->base_unsat=true;return FALSE;}
    if (solver_propagate(s) != INVALID_CLAUSE) {s->base_unsat=true;return FALSE;}
    if (s->error || s->interrupted) return UNDEF;
    s->work_limit = s->work + MIN(s->opts.preprocess_budget, UINT64_MAX-s->work);
    if (s->opts.preprocess_budget && s->opts.probing &&
        (!s->opts.probe_on_change || !s->probed_input || s->probed_input_size!=s->input_size)) {
        s->probed_input=true;s->probed_input_size=s->input_size;
        if (failed_literal_probing(s) < 0) { s->work_limit=0;s->base_unsat=true;return FALSE; }
    }
    if (!n_assumps && s->opts.congruence && s->opts.congruence_budget) {
        uint64_t remaining = s->work_limit > s->work ? s->work_limit-s->work : 0;
        s->work_limit = s->work + MIN(s->opts.congruence_budget, UINT64_MAX-s->work);
        double started=solver_account_begin(s);
        solver_congruence(s);
        solver_account_end(s,ACCOUNT_PREPROCESS,started);
        s->work_limit = 0;
        if (s->error || s->interrupted) return UNDEF;
        if (s->result == FALSE || solver_propagate(s) != INVALID_CLAUSE) {s->base_unsat=true;return FALSE;}
        s->work_limit = s->work + MIN(remaining, UINT64_MAX-s->work);
    }
    if (!n_assumps && s->opts.equiv && s->opts.equiv_budget) {
        uint64_t remaining = s->work_limit > s->work ? s->work_limit-s->work : 0;
        s->work_limit = s->work + MIN(s->opts.equiv_budget, UINT64_MAX-s->work);
        double started=solver_account_begin(s);
        solver_substitute_equivalences(s);
        solver_account_end(s,ACCOUNT_PREPROCESS,started);
        s->work_limit = 0;
        if (s->error || s->interrupted) return UNDEF;
        if (s->result == FALSE || solver_propagate(s) != INVALID_CLAUSE) {s->base_unsat=true;return FALSE;}
        s->work_limit = s->work + MIN(remaining, UINT64_MAX-s->work);
    }
    if (s->opts.preprocess_budget && !n_assumps && s->opts.bce && !solver_budget_exhausted(s))
        s->stats.blocked_clauses = solver_eliminate_blocked_clauses(s);
    if (s->opts.preprocess_budget && !n_assumps && s->opts.elim && !solver_budget_exhausted(s)) {
        double started=solver_account_begin(s);
        elim_preprocess(s);
        solver_account_end(s,ACCOUNT_PREPROCESS,started);
    }
    s->work_limit = 0;
    if (s->error || s->interrupted) return UNDEF;
    if (s->result == FALSE) {s->base_unsat=true;return FALSE;}
    if(!n_assumps && s->opts.factor && s->opts.factor_budget && s->opts.factor_max_variables) {
        s->work_limit=s->work+MIN(s->opts.factor_budget,UINT64_MAX-s->work);
        double started=solver_account_begin(s);
        solver_factor(s);
        solver_account_end(s,ACCOUNT_PREPROCESS,started);
        s->work_limit=0;
        if(s->error || s->interrupted)return UNDEF;
        if(s->num_vars+2>s->levels_capacity) {
            uint8_t *seen=calloc((size_t)s->num_vars+2,1);
            if(!seen){s->error=true;return UNDEF;}
            free(s->level_seen);s->level_seen=seen;s->levels_capacity=s->num_vars+2;
        }
    }
    Lit *learnt = malloc((s->num_vars+1)*sizeof *learnt);
    if (!learnt) { s->error=true; return UNDEF; }
    lbool result=UNDEF;
    /* Initialize after preprocessing, which may replace the solver. Rebuilt
       databases start fresh; retained databases keep their maintenance schedule. */
    if (!s->reduce_span) {
        s->reduce_span = s->opts.reduce_interval;
        s->reduce_limit = s->reduce_span;
    }
    for (;;) {
        if (solver_budget_exhausted(s)) break;
        CRef conflict=solver_propagate(s);
        if (s->error || s->interrupted) break;
        if (conflict != INVALID_CLAUSE) {
            s->stats.conflicts++; s->restart.conflicts_since++;
            if(SEARCH_DIAGNOSTICS(s)) ++s->accounting.diagnostic_conflicts;
            if (s->opts.chrono && !solver_normalize_conflict(s,conflict)) {
                if (!solver_budget_exhausted(s)) {s->base_unsat=true;result=FALSE;}
                break;
            }
            if (!s->decision_level) {s->base_unsat=true;result=FALSE;break;}
            uint32_t n; Level backtrack;
            solver_analyze(s, conflict, learnt, &n, &backtrack);
            s->stats.minimized_literals += solver_minimize_clause(s,learnt,&n);
            uint32_t lbd=calc_lbd(s,learnt,n);
            uint32_t binary_removed = solver_minimize_binary(s,learnt,&n,lbd);
            s->stats.minimized_literals += binary_removed;
            if (binary_removed) lbd = calc_lbd(s,learnt,n);
            record_lbd(s,lbd);
            /* Put the highest remaining decision level in watch position 1. */
            backtrack=0;
            for (uint32_t i=1;i<n;++i) if (s->vars[var(learnt[i])].level > backtrack) {
                backtrack=s->vars[var(learnt[i])].level;
                Lit tmp=learnt[1];learnt[1]=learnt[i];learnt[i]=tmp;
            }
            Level assertion_level=backtrack;
            if (s->opts.chrono && s->decision_level-1-backtrack>s->opts.chrono_levels) {
                backtrack=s->decision_level-1;
                ++s->stats.chronological;
            }
            /* The assertion keeps its logical level, including zero for units. */
            solver_backtrack(s,backtrack);
            proof_add_clause(s,learnt,n);
            if (s->learn_callback && n<=s->learn_max_length) {
                int *exported=malloc(((size_t)n+1)*sizeof *exported);
                if (!exported) {s->error=true;break;}
                for(uint32_t i=0;i<n;++i)exported[i]=toDimacs(learnt[i]);
                exported[n]=0;s->learn_callback(s->learn_state,exported);free(exported);
            }
            CRef reason=INVALID_CLAUSE;
            if (n>1) {
                if (s->num_learnts == s->learnts_size) {
                    uint32_t cap=s->learnts_size ? s->learnts_size*2 : 64;
                    CRef *p=realloc(s->learnts,cap*sizeof *p);
                    if (!p) { s->error=true;break; }
                    s->learnts=p;s->learnts_size=cap;
                }
                reason=arena_alloc(s->arena,learnt,n,true);
                if (reason==INVALID_CLAUSE) { s->error=true;break; }
#ifdef BSAT_SEARCH_DIAGNOSTICS
                CLAUSE_HEADER(s->arena,reason)->born_lo=(uint32_t)s->accounting.diagnostic_conflicts;
                CLAUSE_HEADER(s->arena,reason)->born_hi=(uint32_t)(s->accounting.diagnostic_conflicts>>32);
#endif
                set_clause_lbd(s->arena,reason,lbd);
                s->learnts[s->num_learnts++]=reason;
                if (s->opts.subsumption) solver_on_the_fly_subsumption(s,learnt,n);
                CRef watched = n == 2 ? arena_binary_watch_ref(reason) : reason;
                watch_add(s->watches,learnt[0],watched,learnt[1]);
                watch_add(s->watches,learnt[1],watched,learnt[0]);
            }
            ASSERT(n && s->values[var(learnt[0])]==UNDEF);
            push_trail(s,learnt[0]);s->vars[var(learnt[0])].reason=reason;
            if (s->opts.chrono) s->vars[var(learnt[0])].level=assertion_level;
            s->stats.learned_clauses++;s->stats.learned_literals+=n;
            s->stats.max_lbd=MAX(s->stats.max_lbd,lbd);
            if (lbd<=s->opts.glue_lbd) s->stats.glue_clauses++;
            decay_var_inc(s);
            if (solver_should_reduce(s)) solver_reduce_db(s);
        } else {
            if (solver_should_restart(s)) {
                Level level = n_assumps ? 0 : solver_restart_level(s);
                /* Keep only established assumptions, never ordinary decisions.
                   Conflict backjumps remain unrestricted; new queries remove the
                   prefix during preparation. Certified facade policy is opt-in. */
                if (n_assumps && s->opts.restart_assumptions)
                    level = MIN(s->decision_level, n_assumps);
                solver_backtrack(s,level);
                s->stats.restarts++;s->stats.reused_levels += level;
                if (s->qhead<s->trail_size) continue;
            }
            if (!s->decision_level) {
                if (!solver_simplify(s)) {s->base_unsat=true;result=FALSE;break;}
                if (s->qhead<s->trail_size) continue;
            }
            if (s->opts.rephase && s->stats.conflicts >= s->rephase.conflicts_since+s->opts.rephase_interval) {
                solver_rephase(s);s->rephase.conflicts_since=s->stats.conflicts;
            }
            if (!n_assumps && !s->elim && s->opts.local_search &&
                s->stats.conflicts >= s->local_search.conflicts_since+s->opts.ls_interval) {
                // The walk owns a separate assignment; failed walks must not
                // discard the CDCL trail or turn into uncounted root restarts.
                bool found=solver_try_local_search(s);
                s->local_search.conflicts_since=s->stats.conflicts;
                if (found) { result=TRUE;break; }
            }
            bool assumption=false;
            while (s->decision_level<n_assumps) {
                Lit a=assumps[s->decision_level];
                lbool value=lxor(s->values[var(a)],sign(a));
                if (value==FALSE) { result=FALSE;goto done; }
                s->decision_level++;s->trail_lims[s->decision_level]=s->trail_size;
                if (value==UNDEF) { push_trail(s,a);assumption=true;break; }
            }
            if (!assumption && !solver_decide(s)) {
                /* An interrupted queue scan is not an exhausted decision set. */
                if (!s->error && !s->interrupted) result=TRUE;
                break;
            }
            solver_maybe_save_best_phases(s);
        }
        if ((s->opts.max_conflicts && s->stats.conflicts>=s->opts.max_conflicts) ||
            (s->opts.max_decisions && s->stats.decisions>=s->opts.max_decisions)) break;
    }
done:
    free(learnt);
    return result;
}

static lbool solver_solve_at(Solver *s, const Lit *assumps, uint32_t n_assumps, double start_time) {
    if (!s) return UNDEF;
    if (n_assumps && !assumps) { s->error=true; s->result=UNDEF; return UNDEF; }
    if (s->error || s->watches->failed) { s->error=true; s->result=UNDEF; return UNDEF; }
    if (start_time<0) start_time=solver_cpu_time();
    if (!isfinite(start_time)) {s->error=true;s->result=UNDEF;return UNDEF;}
    if (s->has_solved && !solver_prepare_next(s, start_time)) return UNDEF;
    for (uint32_t i=0;i<n_assumps;++i)
        if (!var(assumps[i]) || var(assumps[i])>s->num_vars) {
            s->error=true; s->result=UNDEF; return UNDEF;
        }
    /* Proofs under assumptions need an augmented input or a conditional proof
       interface. Do not emit an unconditional UNSAT certificate for them. */
    if (n_assumps && s->proof_file) return UNDEF;
    if ((uint64_t)n_assumps+s->num_vars>s->var_capacity) {
        Level *p=realloc(s->trail_lims,((size_t)n_assumps+s->num_vars+2)*sizeof *p);
        if (!p) { s->error=true;return UNDEF; } s->trail_lims=p;
        s->trail_limits_capacity=(size_t)n_assumps+s->num_vars+2;
    }
    uint32_t levels=n_assumps+s->num_vars+2;
    if (levels>s->levels_capacity) {
        uint8_t *p=calloc(levels,1);
        if (!p) { s->error=true;return UNDEF; }
        free(s->level_seen);s->level_seen=p;s->levels_capacity=levels;
    }
    s->stats.start_time=start_time < 0 ? solver_cpu_time() : start_time;
    s->work_limit=0;s->interrupted=false;s->cancelled=false;
    s->clock_initialized=false;s->clock_polls=0;
    s->random_state=s->opts.seed;
#ifdef BSAT_ASSUMPTION_LBD
    s->lbd_assumption_levels=s->opts.restart_assumptions ? n_assumps : 0;
#endif
    lbool result=solve_internal(s,assumps,n_assumps);
#ifdef BSAT_ASSUMPTION_LBD
    s->lbd_assumption_levels=0;
#endif
    s->has_solved=true;s->last_assumptions=n_assumps;
    if (result==TRUE) {
        double started=solver_account_begin(s);
        elim_extend_model(s);
        solver_account_end(s,ACCOUNT_RECONSTRUCT,started);
        started=solver_account_begin(s);
        if (!solver_check_model(s)) s->error=true;
        solver_account_end(s,ACCOUNT_MODEL,started);
        for (uint32_t i=0;i<n_assumps;++i)
            if (lxor(s->values[var(assumps[i])],sign(assumps[i]))!=TRUE) s->error=true;
    }
    if (result==FALSE && n_assumps) {
        s->conflict_clause=malloc(n_assumps*sizeof *s->conflict_clause);
        if (!s->conflict_clause) s->error=true;
        else {
            s->conflict_size=n_assumps;
            for (uint32_t i=0;i<n_assumps;++i) s->conflict_clause[i]=neg(assumps[i]);
        }
    }
    if (result==FALSE && !s->error && !s->interrupted) {
        /* Only a globally entailed blocking clause belongs in the shared
           ledger. Query exports add their empty clause in the assumption context. */
        if(s->proof_journal && !s->proof_file && n_assumps)
            proof_add_clause(s,s->conflict_clause,s->conflict_size);
        else proof_add_clause(s,NULL,0);
    }
    double flush_started=solver_account_begin(s);
    if (proof_stream(s) && (fflush(proof_stream(s)) || ferror(proof_stream(s)))) s->error=true;
    solver_account_end(s,ACCOUNT_PROOF,flush_started);
    if (s->watches->failed) s->error=true;
    /* Cached polls must not permit a completed result after the CPU deadline,
       including time spent reconstructing/checking models or flushing proofs. */
    if (s->terminate && s->terminate(s->terminate_state)) {s->cancelled=true;s->interrupted=true;}
    check_cpu_deadline(s);
    if (s->error || s->interrupted) result=UNDEF;
    s->result=result;
    return result;
}
lbool solver_solve_with_assumptions(Solver *s, const Lit *assumps, uint32_t n_assumps) {
    return solver_solve_at(s, assumps, n_assumps, -1);
}
lbool solver_solve(Solver *s) { return solver_solve_with_assumptions(s,NULL,0); }

lbool solver_solve_portfolio(Solver *s, double focused_seconds) {
    if (!s || !isfinite(focused_seconds) || focused_seconds <= 0 || s->error ||
        (s->proof_file && !s->opts.proof_path)) return UNDEF;
    SolverOpts saved = s->opts;
    double start = solver_cpu_time();
    lbool result = UNDEF;
    unsigned attempts = 0;
    uint64_t first_conflicts = 0, first_decisions = 0;
    /* Repeated calls also charge rebuilding to the total deadline. */
    if (s->has_solved && !solver_rebuild_timed(s, start, saved.max_time)) goto done;
    attempts = 1;
    s->opts.alternating = false;
    s->opts.max_time = saved.max_time > 0 ? fmin(saved.max_time, focused_seconds) : focused_seconds;
    result = solver_solve_at(s, NULL, 0, start);
    if (result != UNDEF || s->error || s->cancelled || !s->interrupted) goto done;
    /* Only expiration of the private first slice authorizes a second attempt.
       Global conflict/decision limits must never be refreshed by rebuilding. */
    double elapsed = solver_cpu_time() - start;
    if (elapsed < focused_seconds || (saved.max_time > 0 && elapsed >= saved.max_time) ||
        (saved.max_conflicts && s->stats.conflicts >= saved.max_conflicts) ||
        (saved.max_decisions && s->stats.decisions >= saved.max_decisions)) goto done;
    uint64_t conflicts = s->stats.conflicts, decisions = s->stats.decisions;
    s->opts = saved;
    if (!solver_rebuild_timed(s, start, saved.max_time)) goto done;
    attempts = 2;
    first_conflicts = conflicts;
    first_decisions = decisions;
    s->opts.alternating = true;
    if (saved.max_conflicts) s->opts.max_conflicts -= (uint32_t)conflicts;
    if (saved.max_decisions) s->opts.max_decisions -= (uint32_t)decisions;
    result = solver_solve_at(s, NULL, 0, start);
done:
    s->portfolio_attempts = attempts;
    s->portfolio_first_conflicts = first_conflicts;
    s->portfolio_first_decisions = first_decisions;
    s->opts = saved;
    s->stats.start_time = start;
    s->result = result;
    return result;
}

const Lit *solver_conflict(const Solver *s, uint32_t *size) {
    if (size) *size=s && s->result==FALSE ? s->conflict_size : 0;
    return s && s->result==FALSE ? s->conflict_clause : NULL;
}

CRef solver_propagate(Solver* s) {
    double started=solver_account_begin(s);
    CRef result = solver_propagate_account_impl(s);
    solver_account_end(s, ACCOUNT_PROPAGATE, started);
    return result;
}

void solver_analyze(Solver* s, CRef conflict, Lit* learnt, uint32_t* learnt_size, Level* bt_level) {
    double started=solver_account_begin(s);
    solver_analyze_account_impl(s, conflict, learnt, learnt_size, bt_level);
    solver_account_end(s, ACCOUNT_ANALYZE, started);
}

void solver_reduce_db(Solver* s) {
    double started=solver_account_begin(s);
    solver_reduce_db_account_impl(s);
    solver_account_end(s, ACCOUNT_REDUCE, started);
}

void solver_collect_garbage(Solver *s) {
    double started=solver_account_begin(s);
    solver_collect_garbage_account_impl(s);
    solver_account_end(s, ACCOUNT_GC, started);
}

bool solver_simplify(Solver *s) {
    double started=solver_account_begin(s);
    bool result = solver_simplify_account_impl(s);
    solver_account_end(s, ACCOUNT_SIMPLIFY, started);
    return result;
}

static void proof_clause(Solver *s, const Lit *lits, uint32_t size, bool deletion) {
    double started=solver_account_begin(s);
    proof_clause_account_impl(s, lits, size, deletion);
    solver_account_end(s, ACCOUNT_PROOF, started);
}

static int failed_literal_probing(Solver *s) {
    double started=solver_account_begin(s);
    uint64_t work=s->work;
    ++s->stats.probing_calls;
    int result = failed_literal_probing_account_impl(s);
    s->stats.probing_work+=s->work-work;
    solver_account_end(s, ACCOUNT_PREPROCESS, started);
    return result;
}

static uint32_t solver_eliminate_blocked_clauses(Solver *s) {
    double started=solver_account_begin(s);
    uint32_t result = solver_eliminate_blocked_clauses_account_impl(s);
    solver_account_end(s, ACCOUNT_PREPROCESS, started);
    return result;
}

static lbool solve_internal(Solver *s, const Lit *assumps, uint32_t n_assumps) {
    double started=solver_account_begin(s);
    lbool result = solve_internal_account_impl(s, assumps, n_assumps);
    solver_account_end(s, ACCOUNT_SEARCH, started);
    return result;
}

void solver_set_terminate(Solver *s, void *state, int (*terminate)(void *)) {
    if(s) {s->terminate=terminate;s->terminate_state=state;}
}

double solver_cpu_time(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts)) return NAN;
    return (double)ts.tv_sec+1e-9*(double)ts.tv_nsec;
}
