/*********************************************************************
 * BSAT Competition Solver - Core Solver Implementation
 *********************************************************************/

#include "../include/solver.h"
#include "../include/inprocess.h"
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
        (force_clock || !s->clock_initialized || ++s->clock_polls >= 512 ||
         s->work - s->clock_work >= 65536 ||
         s->stats.minimize_inspections - s->clock_minimize >= 65536))
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
        .sbva = true,
        .sym = true,
        .chrono = true,
        .chrono_levels = 100,
        .protect_used = true,
        .dynamic_lbd = true,
        .max_conflicts = 0,        // Unlimited
        .max_decisions = 0,        // Unlimited
        .max_time = 0.0,          // Unlimited

        // Branching heuristic
        .vmtf = false,
        .lrb = false,             // Use VSIDS/VMTF alternating by default
        .var_decay = 0.95,
        .var_inc = 1.0,
        .clause_decay = 0.999,
        .lrb_step_min = 0.06,     // Minimum step for LRB (from MapleSAT)
        .lrb_step_max = 0.4,      // Maximum step for LRB (from MapleSAT)

        .restart_first = 100,
        .restart_inc = 1.5,
        .glucose_restart = true,   // LBD-based adaptive restarts
        .reuse_trail = true,       // Reuse high-priority restart prefixes
        .luby_restart = false,     // Disabled
        .luby_unit = 100,          // Base Luby interval
        .restart_postpone = 10,

        // Glucose EMA parameters (Biere-Fröhlich warmed EMAs)
        .glucose_use_ema = true,
        .glucose_fast_alpha = 32.0 / 33.0, // Fast MA (~33-conflict window)
        .glucose_slow_alpha = 0.99999,     // Slow MA (~100k-conflict window)
        .glucose_min_conflicts = 2,        // Fast focused restarts

        // Glucose sliding window parameters (for --glucose-restart-avg)
        .glucose_window_size = 50,     // Sliding-window length
        .glucose_k = 10.0 / 11.0,      // Restart when fast_ma > 1.1 * slow_ma

        .phase_saving = true,
        .phase_reset_period = 10000,
        .random_phase = false,         // Opt-in reproducible diversification
        .random_phase_prob = 0.01,     // 1% random decisions
        .adaptive_random = true,
        .rephase = true,               // Kissat-style cycling rephasing
        .rephase_interval = 1000,      // Base conflicts between rephases

        .max_lbd = 30,
        .glue_lbd = 2,
        .reduce_fraction = 0.5,
        .reduce_interval = 1000,
        .reduce_increment = 100,       // Smoothly growing reduction schedule
        .binary_minimize = true,       // Binary resolution minimization on short low-LBD clauses
        .iterative_minimize = false,
        .shrink = true,                // Block-level all-UIP shrinking
        .minimize_budget = 10000,      // Reason inspections per learned clause
        .minimize = true,              // 3-state binary-aware clause minimization

        .bce = false,             // DISABLED by default - can hurt SAT instance performance
        .probing = true,          // Enable failed literal probing

        // BVE options (opt-in)
        .elim = false,            // BVE disabled by default (opt-in with --elim)
        .elim_max_occ = 10,       // Max occurrences to consider for elimination
        .elim_grow = 0,           // No clause growth allowed by default

        // DRAT proof logging (opt-in)
        .proof_path = NULL,       // No proof by default
        .binary_proof = false,    // Text format by default

        .inprocess = true,
        .inprocess_interval = 10000,
        .subsumption = true,
        .var_elim = true,

        // Local search options (opt-in)
        .local_search = false,    // Disabled by default
        .ls_interval = 5000,      // Run local search every 5000 conflicts
        .ls_max_flips = 100000,   // Max flips per local search call
        .ls_save_phases = false,
        .ls_noise = 0.5,          // WalkSAT noise parameter

        .alternating = true,      // Focused (VMTF) / Stable (VSIDS) mode alternation
        .verbose = false,
        .debug = false,
        .quiet = false,
        .stats = true
    };

    return opts;
}

/*********************************************************************
 * VSIDS 4-ary Max-Heap Operations
 *********************************************************************/

static inline uint32_t heap_first_child(uint32_t i) { return (i << 2) + 1; }
static inline uint32_t heap_parent(uint32_t i) { return (i - 1) >> 2; }

static void heap_percolate_up(Solver* s, uint32_t i) {
    Var * restrict heap = s->order.heap;
    VarInfo * restrict vars = s->vars;
    Var v = heap[i];
    double act = vars[v].activity;

    while (i > 0) {
        uint32_t p = heap_parent(i);
        Var pv = heap[p];

        if (vars[pv].activity >= act) break;

        heap[i] = pv;
        vars[pv].heap_pos = i;
        i = p;
    }

    heap[i] = v;
    vars[v].heap_pos = i;
}

static void heap_percolate_down(Solver* s, uint32_t i) {
    Var * restrict heap = s->order.heap;
    VarInfo * restrict vars = s->vars;
    uint32_t size = s->order.size;
    Var v = heap[i];
    double act = vars[v].activity;

    for (;;) {
        uint32_t c = heap_first_child(i);
        if (c >= size) break;
        uint32_t best = c;
        double best_act = vars[heap[c]].activity;
        uint32_t end = c + 4;
        if (end > size) end = size;
        for (uint32_t k = c + 1; k < end; ++k) {
            double k_act = vars[heap[k]].activity;
            if (k_act > best_act) {
                best_act = k_act;
                best = k;
            }
        }
        if (act >= best_act) break;

        Var bv = heap[best];
        heap[i] = bv;
        vars[bv].heap_pos = i;
        i = best;
    }

    heap[i] = v;
    vars[v].heap_pos = i;
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

static inline bool track_vmtf(const Solver *s) {
    return s->opts.vmtf || (s->opts.alternating && !s->opts.lrb);
}

static inline bool use_vmtf_order(const Solver *s) {
    if (s->opts.vmtf) return true;
    if (s->opts.lrb) return false;
    return s->opts.alternating && !s->stable_mode;
}

static void bump_var_activity(Solver* s, Var v, double inc) {
    /* Maintain VMTF queue whenever queue mode or alternating mode is enabled. */
    if (track_vmtf(s)) {
        ASSERT(s->vmtf.pending < s->num_vars);
        s->analyze_stack[s->vmtf.pending++] = v;
        if (s->opts.vmtf) return; /* Pure VMTF mode ignores numeric scores. */
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

static void bump_learned_clause_activity(Solver *s, CRef cr) {
    if (!clause_learned(s->arena, cr)) return;
    ClauseHeader *h = CLAUSE_HEADER(s->arena, cr);
    h->activity += (float)s->clause_inc;
    if (h->activity > 1e20f) {
        for (uint32_t i = 0; i < s->num_learnts; ++i) {
            CRef c = s->learnts[i];
            if (c != INVALID_CLAUSE && !clause_deleted(s->arena, c)) {
                CLAUSE_HEADER(s->arena, c)->activity *= 1e-20f;
            }
        }
        s->clause_inc *= 1e-20;
    }
}

static void decay_var_inc(Solver* s) {
    s->clause_inc /= s->opts.clause_decay;
    if (s->opts.vmtf) return;
    // Dynamic VSIDS decay: start fast (0.80) for early focus, ramp smoothly to var_decay.
    double decay = s->order.var_decay;
    if (!s->opts.lrb && decay > 0.80 && s->stats.conflicts < 15000) {
        decay = 0.80 + (decay - 0.80) * ((double)s->stats.conflicts / 15000.0);
    }
    s->order.var_inc /= decay;
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

    // Initialize restart & mode state
    s->restart.threshold = opts->restart_first;
    s->restart.luby_index = 0;
    s->restart.reluctant_u = 1;
    s->restart.reluctant_v = 1;
    s->restart.reluctant_countdown = 1024;
    s->mode_limit = 1000;
    s->mode_ticks_span = 1000000;
    s->next_mode_ticks = 1000000;
    s->inprocess_ticks_span = 5000000;
    s->next_inprocess_ticks = 5000000;
    s->inprocess_rounds = 0;
    s->tier1_lbd = 2;
    s->tier2_lbd = 6;
    s->ignore_clause = INVALID_CLAUSE;
    s->next_rephase = opts->rephase_interval;
    s->clause_inc = 1.0;

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
    s->rephase.best_prefix_valid = false;
    s->rephase.target_phase = NULL;
    s->rephase.target_trail_size = 0;
    s->rephase.target_prefix_valid = false;
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

    gauss_free(s);
    arena_free(s->arena);
    watch_free(s->watches);

    free(s->conflict_clause);
    free(s->level_seen);
    free(s->input);
    free(s->lrb_last_conflict);
    free(s->vars);
    free(s->values);
    free(s->frozen);
    free(s->lit_values);
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
    free(s->rephase.target_phase); // Free Stable-mode target phase array

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

    uint8_t *new_frozen = realloc(s->frozen, alloc_size * sizeof *new_frozen);
    if (!new_frozen) return false;
    s->frozen = new_frozen;
    {
        uint32_t old_init = s->var_capacity ? s->var_capacity + 1u : 0u;
        memset(s->frozen + old_init, 0, (size_t)(alloc_size - old_init));
    }

    uint8_t *new_lit_values = realloc(s->lit_values, (size_t)alloc_size * 2u * sizeof *new_lit_values);
    if (!new_lit_values) return false;
    s->lit_values = new_lit_values;

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

    // Grow best and target phase arrays (for rephasing)
    if (s->opts.rephase) {
        uint8_t* new_best_phase = realloc(s->rephase.best_phase, alloc_size * sizeof *new_best_phase);
        if (!new_best_phase) return false;
        s->rephase.best_phase = new_best_phase;
        uint8_t* new_target_phase = realloc(s->rephase.target_phase, alloc_size * sizeof *new_target_phase);
        if (!new_target_phase) return false;
        s->rephase.target_phase = new_target_phase;
        // New variables have no known best/target phase.
        for (uint32_t i = s->var_capacity + 1; i <= new_capacity; i++) {
            s->rephase.best_phase[i] = UNDEF;
            s->rephase.target_phase[i] = UNDEF;
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

    // Keep ElimState arrays synchronized whenever auxiliary variables are added post-elim_init.
    if (s->elim && v >= s->elim->elim_capacity) {
        elim_init(s);
        if (s->error) { s->num_vars--; return INVALID_VAR; }
    }

    // Initialize new variable
    memset(&s->vars[v], 0, sizeof(VarInfo));
    s->values[v] = UNDEF;
    s->frozen[v] = 0;
    s->lit_values[v << 1] = UNDEF;
    s->lit_values[(v << 1) | 1u] = UNDEF;
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

void solver_freeze(Solver* s, Var v) {
    if (s && v && v <= s->num_vars && s->frozen) s->frozen[v] = 1;
}

void solver_melt(Solver* s, Var v) {
    if (s && v && v <= s->num_vars && s->frozen) s->frozen[v] = 0;
}

bool solver_is_frozen(const Solver* s, Var v) {
    return s && v && v <= s->num_vars && s->frozen && s->frozen[v] != 0;
}

/*********************************************************************
 * Trail Management
 *********************************************************************/

static inline void sync_lit_values(Solver *s) {
    if (!s->lit_values || SEARCH_DIAGNOSTICS(s)) { s->lit_values_live = false; return; }
    uint8_t * restrict lv = s->lit_values;
    const uint8_t * restrict vals = s->values;
    lv[0] = UNDEF; lv[1] = UNDEF;
    for (Var v = 1; v <= s->num_vars; ++v) {
        uint8_t val = vals[v];
        lv[v << 1] = val;
        lv[(v << 1) | 1u] = (val == UNDEF) ? UNDEF : (uint8_t)(3u - val);
    }
    s->lit_values_live = true;
}

static inline void push_trail(Solver* s, Lit lit) {
    Var v = var(lit);
    ASSERT(s->values[v] == UNDEF);

    s->vars[v].reason = INVALID_CLAUSE;
    s->binary_reasons[v] = LIT_UNDEF;
    s->values[v] = sign(lit) ? FALSE : TRUE;
    if (s->lit_values_live) {
        s->lit_values[lit] = TRUE;
        s->lit_values[lit ^ 1u] = FALSE;
    }
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
    if (pos < s->rephase.target_trail_size) s->rephase.target_prefix_valid = false;
    if (s->opts.chrono) {
        uint32_t kept=pos;
        for (uint32_t i=pos;i<s->trail_size;++i) {
            Lit lit=s->trail[i].lit;Var v=var(lit);
            if (s->vars[v].level<=level) {
                s->vars[v].trail_pos=kept;s->trail[kept++].lit=lit;
                ++s->stats.chrono_retained;
            } else {
                s->values[v]=UNDEF;
                if (s->lit_values_live) {
                    s->lit_values[v << 1] = UNDEF;
                    s->lit_values[(v << 1) | 1u] = UNDEF;
                }
                if (track_vmtf(s)) solver_vmtf_unassign(s,v);
                s->vars[v].level=INVALID_LEVEL;s->vars[v].reason=INVALID_CLAUSE;
                s->binary_reasons[v]=LIT_UNDEF;
                if (s->vars[v].heap_pos==UINT32_MAX) heap_insert(s,v);
            }
        }
        s->trail_size=kept;
        /* Revisit retained late implications: their watches may have changed
           while assignments from higher levels were still present. */
        if (s->qhead>pos) s->qhead=pos;
        if (s->gauss) gauss_backtrack(s, pos);
        s->decision_level=level;
        return;
    }
    for (uint32_t i = s->trail_size; i > pos;) {
        Var v = var(s->trail[--i].lit);
        s->values[v] = UNDEF;
        if (s->lit_values_live) {
            s->lit_values[v << 1] = UNDEF;
            s->lit_values[(v << 1) | 1u] = UNDEF;
        }
        if (track_vmtf(s)) solver_vmtf_unassign(s, v);
        s->vars[v].level = INVALID_LEVEL;
        s->vars[v].reason = INVALID_CLAUSE;
        s->binary_reasons[v] = LIT_UNDEF;
        if (s->vars[v].heap_pos == UINT32_MAX) heap_insert(s, v);
    }
    s->trail_size = pos;
    if (s->qhead > pos) s->qhead = pos;
    if (s->gauss) gauss_backtrack(s, pos);
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
    if (size) {
        memcpy(tmp, lits, size * sizeof *tmp);
        if (size <= 16) {
            for (uint32_t i = 1; i < size; ++i) {
                Lit key = tmp[i];
                uint32_t j = i;
                while (j > 0 && tmp[j - 1] > key) { tmp[j] = tmp[j - 1]; --j; }
                tmp[j] = key;
            }
        } else {
            qsort(tmp, size, sizeof *tmp, compare_lits);
        }
    }
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

static inline void watch_add_inline(WatchManager *wm, Lit lit, CRef cref, Lit blocker) {
    WatchList *wl = &wm->lists[toInt(lit)];
    if (wl->size >= wl->capacity) {
        if (!watchlist_ensure_capacity(wl)) { wm->failed = true; return; }
    }
    wl->watches[wl->size++] = (Watch){cref, blocker};
}

static bool clause_locked(Solver *s, CRef cr);
static CRef solver_propagate_account_impl(Solver* s) {
    uint8_t * restrict values = s->values;
    VarInfo * restrict vars = s->vars;
    Trail * restrict trail = s->trail;
    Lit * restrict binary_reasons = s->binary_reasons;
    WatchManager * restrict wm = s->watches;
    WatchList * restrict wlists = wm->lists;
    Arena * restrict arena = s->arena;
    uint32_t * restrict mem = arena->memory;

    const bool chrono = s->opts.chrono;
    const bool circular = s->opts.circular;
    const bool save_phase = s->opts.phase_saving;
    const bool has_work_limit = (s->work_limit != 0);
    const bool has_poll = (has_work_limit || s->opts.max_time > 0 || s->terminate != NULL);
    const bool diag = SEARCH_DIAGNOSTICS(s);
    const CRef ignore_cr = s->ignore_clause;
    const Level dec_level = s->decision_level;

    uint32_t qhead = s->qhead;
    uint32_t trail_size = s->trail_size;
    uint64_t ticks = 0;
    uint64_t props = 0;
    uint64_t visits = 0;
    uint64_t skipped = 0;
    uint32_t poll_counter = 0;
    CRef conflict = INVALID_CLAUSE;

    /* Hot literal-indexed BCP fast path (active only inside CDCL search loop). */
    if (s->lit_values_live && !diag && !has_work_limit) {
        uint8_t * restrict lval = s->lit_values;
        while (qhead < trail_size) {
            if (has_poll && (++poll_counter & 63) == 0) {
                s->work += ticks; ticks = 0;
                if (solver_budget_exhausted(s)) {
                    s->qhead = qhead; s->trail_size = trail_size;
                    s->stats.propagations += props; wm->visits += visits; wm->skipped += skipped;
                    return INVALID_CLAUSE;
                }
            }

            Lit p = trail[qhead++].lit;
            Lit neg_p = neg(p);
            Level p_level = chrono ? vars[var(p)].level : dec_level;

            WatchList *ws = &wlists[toInt(neg_p)];
            Watch *watches = ws->watches;
            uint32_t ws_size = ws->size;
            uint32_t i = 0, j = 0;

            props++;

            while (i < ws_size) {
                ticks++;
                visits++;
                Watch w = watches[i];

                if ((int32_t)w.cref < 0) {
                    Lit q = w.blocker;
                    uint8_t qv = lval[q];
                    if (qv == UNDEF) {
                        Var v = var(q);
                        values[v] = (uint8_t)(2u - sign(q));
                        lval[q] = TRUE;
                        lval[q ^ 1u] = FALSE;
                        vars[v].level = p_level;
                        vars[v].reason = watch_clause(w);
                        vars[v].trail_pos = trail_size;
                        binary_reasons[v] = neg_p;
                        trail[trail_size++].lit = q;
                        if (save_phase) vars[v].polarity = !sign(q);
                    } else if (qv == FALSE) {
                        CRef binary_reason = watch_clause(w);
                        s->binary_conflict_lits[0] = neg_p;
                        s->binary_conflict_lits[1] = q;
                        while (i < ws_size) watches[j++] = watches[i++];
                        ws->size = j;
                        conflict = (binary_reason != INVALID_CLAUSE) ? binary_reason : BINARY_CONFLICT;
                        goto done;
                    }
                    watches[j++] = w;
                    i++;
                    continue;
                }

                if (lval[w.blocker] == TRUE) {
                    watches[j++] = w;
                    i++;
                    skipped++;
                    continue;
                }

                CRef cref = w.cref;
                if (ignore_cr != INVALID_CLAUSE && cref == ignore_cr) {
                    watches[j++] = w;
                    i++;
                    continue;
                }
                if (i + 1 < ws_size && (int32_t)watches[i + 1].cref >= 0) {
                    __builtin_prefetch(&mem[watches[i + 1].cref], 0, 0);
                }
                ClauseHeader *h = (ClauseHeader *)&mem[cref];
                uint32_t size = h->size;
                Lit *lits = (Lit *)&mem[cref + sizeof(ClauseHeader) / sizeof(uint32_t)];

                if (lits[0] == neg_p) {
                    lits[0] = lits[1];
                    lits[1] = neg_p;
                }
                ASSERT(lits[1] == neg_p);

                Lit first = lits[0];
                uint8_t fval = lval[first];
                if (fval == TRUE) {
                    watches[j++] = (Watch){cref, first};
                    i++;
                    continue;
                }

                bool found = false;
                uint32_t begin = circular ? h->search : 2;
                if (begin < 2 || begin >= size) begin = 2;

                for (uint32_t k = begin; k < size; ++k) {
                    ticks++;
                    Lit lit = lits[k];
                    if (lval[lit] != FALSE) {
                        lits[1] = lit;
                        lits[k] = neg_p;
                        h->search = k;
                        watch_add_inline(wm, lit, cref, first);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    for (uint32_t k = 2; k < begin; ++k) {
                        ticks++;
                        Lit lit = lits[k];
                        if (lval[lit] != FALSE) {
                            lits[1] = lit;
                            lits[k] = neg_p;
                            h->search = k;
                            watch_add_inline(wm, lit, cref, first);
                            found = true;
                            break;
                        }
                    }
                }
                if (found) {
                    i++;
                    continue;
                }

                watches[j++] = w;
                i++;

                if (fval == UNDEF) {
                    Level reason_level = p_level;
                    if (chrono) {
                        for (uint32_t k = 2; k < size; ++k) {
                            Level kl = vars[var(lits[k])].level;
                            if (kl > reason_level) reason_level = kl;
                        }
                    }
                    Var fv = var(first);
                    values[fv] = (uint8_t)(2u - sign(first));
                    lval[first] = TRUE;
                    lval[first ^ 1u] = FALSE;
                    vars[fv].level = reason_level;
                    vars[fv].reason = cref;
                    vars[fv].trail_pos = trail_size;
                    binary_reasons[fv] = LIT_UNDEF;
                    trail[trail_size++].lit = first;
                    if (save_phase) vars[fv].polarity = !sign(first);
                } else {
                    while (i < ws_size) watches[j++] = watches[i++];
                    ws->size = j;
                    conflict = cref;
                    goto done;
                }
            }

            ws->size = j;
        }
        goto done;
    }

    while (qhead < trail_size) {
        if (has_poll) {
            if (has_work_limit && s->work + ticks >= s->work_limit) {
                s->work += ticks; ticks = 0;
                s->qhead = qhead; s->trail_size = trail_size;
                s->stats.propagations += props; wm->visits += visits; wm->skipped += skipped;
                if (solver_budget_exhausted(s)) return INVALID_CLAUSE;
                props = visits = skipped = 0;
            } else if ((++poll_counter & 63) == 0) {
                s->work += ticks; ticks = 0;
                if (solver_budget_exhausted(s)) {
                    s->qhead = qhead; s->trail_size = trail_size;
                    s->stats.propagations += props; wm->visits += visits; wm->skipped += skipped;
                    return INVALID_CLAUSE;
                }
            }
        }

        Lit p = trail[qhead++].lit;
        Lit neg_p = neg(p);
        Level p_level = chrono ? vars[var(p)].level : dec_level;

        WatchList *ws = &wlists[toInt(neg_p)];
        Watch *watches = ws->watches;
        uint32_t ws_size = ws->size;
        uint32_t i = 0, j = 0;

        props++;

        while (i < ws_size) {
            if (has_work_limit && s->work + ticks >= s->work_limit) {
                s->work += ticks; ticks = 0;
                if (solver_budget_exhausted(s)) {
                    while (i < ws_size) watches[j++] = watches[i++];
                    ws->size = j;
                    s->qhead = qhead - 1; s->trail_size = trail_size;
                    s->stats.propagations += props; wm->visits += visits; wm->skipped += skipped;
                    return INVALID_CLAUSE;
                }
            }
            ticks++;
            visits++;
            Watch w = watches[i];

            /* Fast path: binary clause (bit 31 set for both INVALID_CLAUSE and ARENA_BINARY_TAG). */
            if ((int32_t)w.cref < 0) {
                if (diag) s->accounting.binary_visits++;
                Lit q = w.blocker;
                Var v = var(q);
                uint8_t vval = values[v];
                uint8_t q_false = (uint8_t)(1u + sign(q));

                if (vval == UNDEF) {
                    CRef binary_reason = watch_clause(w);
#ifdef BSAT_SEARCH_DIAGNOSTICS
                    if (diag && binary_reason != INVALID_CLAUSE) {
                        ClauseHeader *h = (ClauseHeader *)&mem[binary_reason];
                        if (h->units < UINT32_MAX) ++h->units;
                        h->recent_use |= 1;
                        if (clause_learned(arena, binary_reason)) ++s->accounting.use_binary_units;
                    }
#endif
                    values[v] = (uint8_t)(2u - sign(q));
                    vars[v].level = p_level;
                    vars[v].reason = binary_reason;
                    vars[v].trail_pos = trail_size;
                    /* Store binary antecedent directly to avoid arena dereference on analysis/minimization! */
                    binary_reasons[v] = neg_p;
                    trail[trail_size++].lit = q;
                    if (save_phase) vars[v].polarity = !sign(q);
                } else if (vval == q_false) {
                    CRef binary_reason = watch_clause(w);
                    s->binary_conflict_lits[0] = neg_p;
                    s->binary_conflict_lits[1] = q;
                    while (i < ws_size) watches[j++] = watches[i++];
                    ws->size = j;
                    conflict = (binary_reason != INVALID_CLAUSE) ? binary_reason : BINARY_CONFLICT;
                    goto done;
                }
                watches[j++] = w;
                i++;
                continue;
            }

            if (diag) s->accounting.long_visits++;
            Lit blocker = w.blocker;
            if (values[var(blocker)] == (uint8_t)(2u - sign(blocker))) {
                if (diag) s->accounting.blocker_hits++;
                watches[j++] = w;
                i++;
                skipped++;
                continue;
            }

            CRef cref = w.cref;
            if (ignore_cr != INVALID_CLAUSE && cref == ignore_cr) {
                watches[j++] = w;
                i++;
                continue;
            }
            if (i + 1 < ws_size && (int32_t)watches[i + 1].cref >= 0) {
                __builtin_prefetch(&mem[watches[i + 1].cref], 0, 0);
            }
            ClauseHeader *h = (ClauseHeader *)&mem[cref];
            uint32_t size = h->size;
            Lit *lits = (Lit *)&mem[cref + sizeof(ClauseHeader) / sizeof(uint32_t)];

            if (lits[0] == neg_p) {
                lits[0] = lits[1];
                lits[1] = neg_p;
            }
            ASSERT(lits[1] == neg_p);

            Lit first = lits[0];
            Var fv = var(first);
            uint8_t fval = values[fv];
            if (fval == (uint8_t)(2u - sign(first))) {
                if (diag) s->accounting.first_hits++;
                watches[j++] = (Watch){cref, first};
                i++;
                continue;
            }

            /* Two-pass branchless circular scan for replacement watch. */
            bool found = false;
            uint32_t begin = circular ? h->search : 2;
            if (begin < 2 || begin >= size) begin = 2;

            if (!diag && !has_work_limit) {
                for (uint32_t k = begin; k < size; ++k) {
                    ticks++;
                    Lit lit = lits[k];
                    if (values[var(lit)] != (uint8_t)(1u + sign(lit))) {
                        lits[1] = lit;
                        lits[k] = neg_p;
                        h->search = k;
                        watch_add_inline(wm, lit, cref, first);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    for (uint32_t k = 2; k < begin; ++k) {
                        ticks++;
                        Lit lit = lits[k];
                        if (values[var(lit)] != (uint8_t)(1u + sign(lit))) {
                            lits[1] = lit;
                            lits[k] = neg_p;
                            h->search = k;
                            watch_add_inline(wm, lit, cref, first);
                            found = true;
                            break;
                        }
                    }
                }
            } else {
                for (uint32_t offset = 0; offset < size - 2; ++offset) {
                    uint32_t k = begin + offset;
                    if (k >= size) k = 2 + k - size;
                    ticks++;
                    if (has_work_limit && s->work + ticks >= s->work_limit) {
                        s->work += ticks; ticks = 0;
                        if (solver_budget_exhausted(s)) {
                            while (i < ws_size) watches[j++] = watches[i++];
                            ws->size = j;
                            s->qhead = qhead - 1; s->trail_size = trail_size;
                            s->stats.propagations += props; wm->visits += visits; wm->skipped += skipped;
                            return INVALID_CLAUSE;
                        }
                    }
                    if (diag) {
                        s->accounting.replacement_scans++;
#ifdef BSAT_SEARCH_DIAGNOSTICS
                        if (h->scans < UINT32_MAX) ++h->scans;
                        if (clause_learned(arena, cref)) {
                            ++s->accounting.learned_scans;
                            if (size <= 64) {
                                ++s->accounting.learned_scans_eligible_size;
                                if (!clause_locked(s, cref)) ++s->accounting.learned_scans_unlocked;
                            } else ++s->accounting.learned_scans_oversize;
                            if (h->strengthened) ++s->accounting.strengthened_scans;
                        } else ++s->accounting.original_scans;
                        uint64_t born = ((uint64_t)h->born_hi << 32) | h->born_lo;
                        uint64_t age = s->accounting.diagnostic_conflicts >= born ? s->accounting.diagnostic_conflicts - born : 0;
                        if (age < 100) ++s->accounting.scan_age_0_99;
                        else if (age < 1000) ++s->accounting.scan_age_100_999;
                        else ++s->accounting.scan_age_1000_plus;
                        if (h->units || h->analyses) ++s->accounting.scans_after_unit_or_analysis;
                        else ++s->accounting.scans_before_unit_or_analysis;
#endif
                        if (size == 3) s->accounting.scan_size_3++;
                        else if (size <= 8) s->accounting.scan_size_4_8++;
                        else s->accounting.scan_size_9_plus++;
                    }
                    Lit lit = lits[k];
                    if (values[var(lit)] != (uint8_t)(1u + sign(lit))) {
                        lits[1] = lit;
                        lits[k] = neg_p;
                        h->search = k;
                        watch_add_inline(wm, lit, cref, first);
                        if (diag) s->accounting.replacement_moves++;
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                i++;
                continue;
            }

            watches[j++] = w;
            i++;

            if (fval == UNDEF) {
                if (diag) {
                    s->accounting.long_units++;
#ifdef BSAT_SEARCH_DIAGNOSTICS
                    if (h->units < UINT32_MAX) ++h->units;
                    h->recent_use |= 1;
#endif
                }
                /* Compute chronological reason level ONLY on actual unit propagation! */
                Level reason_level = p_level;
                if (chrono) {
                    for (uint32_t k = 2; k < size; ++k) {
                        Level kl = vars[var(lits[k])].level;
                        if (kl > reason_level) reason_level = kl;
                    }
                }
                values[fv] = (uint8_t)(2u - sign(first));
                vars[fv].level = reason_level;
                vars[fv].reason = cref;
                vars[fv].trail_pos = trail_size;
                binary_reasons[fv] = LIT_UNDEF;
                trail[trail_size++].lit = first;
                if (save_phase) vars[fv].polarity = !sign(first);
            } else {
                if (diag) s->accounting.long_conflicts++;
                while (i < ws_size) watches[j++] = watches[i++];
                ws->size = j;
                conflict = cref;
                goto done;
            }
        }

        ws->size = j;
    }

done:
    s->qhead = qhead;
    s->trail_size = trail_size;
    s->work += ticks;
    s->stats.propagations += props;
    wm->visits += visits;
    wm->skipped += skipped;
    return conflict;
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
        if (level <= s->lbd_assumption_levels) continue;
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
#ifdef BSAT_SEARCH_DIAGNOSTICS
    /* Observe score staleness at valid conflict/reason assignments without
       changing stored scores. Counts are reuse events, not unique clauses. */
    if(SEARCH_DIAGNOSTICS(s) && clause_learned(s->arena,cr) && clause_lbd(s->arena,cr)>s->opts.glue_lbd) {
        uint32_t current=calc_lbd(s,CLAUSE_LITS(s->arena,cr),CLAUSE_SIZE(s->arena,cr));
        ++s->accounting.use_lbd_checks;
        if(current<clause_lbd(s->arena,cr)) ++s->accounting.use_lbd_lower;
        if(current<=s->opts.glue_lbd) ++s->accounting.use_lbd_to_glue;
    }
#endif
    if ((!s->opts.dynamic_lbd && !s->opts.protect_used) || !clause_learned(s->arena, cr)) return;
    ClauseHeader *h = CLAUSE_HEADER(s->arena, cr);
    if (s->opts.dynamic_lbd) {
        uint32_t old = h->lbd;
        if (old > s->opts.glue_lbd) {
            uint32_t current = calc_lbd(s, CLAUSE_LITS(s->arena, cr), h->size);
            if (current < old) {
                set_clause_lbd(s->arena, cr, current);
                s->stats.lbd_updates++;
            }
        }
    }
    if (s->opts.protect_used) {
        uint32_t clamped = h->lbd < 64 ? h->lbd : 64;
        s->tier_used[clamped]++;
        uint32_t t2 = s->tier2_lbd >= 6 ? s->tier2_lbd : 6;
        if (h->lbd <= 4) {
            h->used = 2;
            h->flags |= CLAUSE_FROZEN;
        } else if (h->lbd <= t2) {
            h->used = 1;
            h->flags |= CLAUSE_FROZEN;
        }
    }
}

static void solver_analyze_account_impl(Solver* s, CRef conflict, Lit* learnt, uint32_t* learnt_size, Level* bt_level) {
    s->vmtf.pending = 0;
    s->otfs_count = 0;
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
#ifdef BSAT_SEARCH_DIAGNOSTICS
        if(SEARCH_DIAGNOSTICS(s)) {
            ClauseHeader *h=CLAUSE_HEADER(s->arena,conflict);
            if(h->conflicts<UINT32_MAX) ++h->conflicts;
            h->recent_use|=4;
            if(clause_learned(s->arena,conflict)) ++s->accounting.use_learned_conflicts;
        }
#endif
        // Regular conflict from arena
        if (s->proof_file && s->gauss) gauss_ensure_clause_proved(s, conflict);
        improve_clause_lbd(s, conflict);
        bump_learned_clause_activity(s, conflict);
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
            if (!SEARCH_DIAGNOSTICS(s) && s->binary_reasons[v] != LIT_UNDEF) {
                /* Fast path: binary reason (both original and learned) without touching arena! */
                Lit q = s->binary_reasons[v];
                Var qv = var(q);
                if (!s->seen[qv] && s->vars[qv].level > 0) {
                    s->seen[qv] = 1;
                    bump_var_activity(s, qv, s->order.var_inc);
                    if (s->vars[qv].level >= s->decision_level) {
                        pathC++;
                    } else {
                        learnt[(*learnt_size)++] = q;
                        if (s->vars[qv].level > *bt_level) *bt_level = s->vars[qv].level;
                    }
                }
            } else if (reason != INVALID_CLAUSE) {
                // Expand reason clause
                if (s->proof_file && s->gauss) gauss_ensure_clause_proved(s, reason);
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
                    h->recent_use|=2;
                }
#endif
                bump_learned_clause_activity(s, reason);
                uint32_t expected_in_C = pathC + (*learnt_size - 1);
                uint32_t seen_in_R = 0;
                for (uint32_t i = 0; i < size; i++) {
                    Lit q = lits[i];
                    Var qv = var(q);
                    if (qv == v) continue;

                    if (s->seen[qv]) {
                        seen_in_R++;
                    } else if (s->vars[qv].level > 0) {
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
                /* On-the-fly self-subsumption (OTFS): if every other literal of
                 * the current resolvent is already in reason, then (reason \ {p})
                 * is strictly stronger and RUP. Strengthen learned reason after backtrack. */
                if (size >= 3 && seen_in_R == expected_in_C &&
                    clause_learned(s->arena, reason) && !gauss_is_temp_clause(s, reason) &&
                    s->otfs_count < 8) {
                    s->otfs_crefs[s->otfs_count] = reason;
                    s->otfs_pivots[s->otfs_count] = p;
                    s->otfs_count++;
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

    if (track_vmtf(s))
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

    // Focused mode uses VMTF queue; stable mode uses VSIDS heap.
    if (use_vmtf_order(s)) next = solver_vmtf_pick(s);
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
    if (s->opts.alternating && s->stable_mode && s->rephase.best_phase) {
        uint8_t tp = s->rephase.target_phase ? s->rephase.target_phase[next] : UNDEF;
        if (tp == UNDEF) tp = s->rephase.best_phase[next];
        if (tp != UNDEF) sign = (tp == FALSE);
    }
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
 * Save current assignment as best/target if we've assigned more variables than
 * the global best or current-epoch target high-water mark.
 */
void solver_maybe_save_best_phases(Solver* s) {
    if (!s->opts.rephase || !s->rephase.best_phase) return;

    // 1. Update global best phase high-water mark (reset on rephase_best)
    if (s->trail_size > s->rephase.best_trail_size) {
        uint32_t first = s->rephase.best_prefix_valid ? s->rephase.best_trail_size : 0;
        if (!s->rephase.best_prefix_valid) {
            memset(s->rephase.best_phase, 0, (s->num_vars+1)*sizeof *s->rephase.best_phase);
            s->stats.target_cleared += (uint64_t)s->num_vars + 1;
        }
        for (uint32_t i = first; i < s->trail_size; i++) {
            Lit lit = s->trail[i].lit;
            Var v = var(lit);
            s->rephase.best_phase[v] = sign(lit) ? FALSE : TRUE;
        }
        s->stats.target_copied += s->trail_size - first;
        s->rephase.best_trail_size = s->trail_size;
        s->rephase.best_prefix_valid = true;
    }

    // 2. Update active Stable-mode target phase (reset on EVERY rephase)
    if (s->rephase.target_phase && s->trail_size > s->rephase.target_trail_size) {
        uint32_t first = s->rephase.target_prefix_valid ? s->rephase.target_trail_size : 0;
        if (!s->rephase.target_prefix_valid) {
            memset(s->rephase.target_phase, 0, (s->num_vars+1)*sizeof *s->rephase.target_phase);
        }
        for (uint32_t i = first; i < s->trail_size; i++) {
            Lit lit = s->trail[i].lit;
            Var v = var(lit);
            s->rephase.target_phase[v] = sign(lit) ? FALSE : TRUE;
        }
        s->rephase.target_trail_size = s->trail_size;
        s->rephase.target_prefix_valid = true;
    }
}

/**
 * Perform Kissat-style cycling rephasing: B, W, I, B, W, O.
 * Copies the rephased saved polarities directly into Stable-mode target_phase[]
 * and resets target_trail_size so Stable mode immediately explores the new basin!
 */
static void solver_rephase(Solver* s) {
    if (!s->opts.rephase || !s->rephase.best_phase) return;

    unsigned phase_kind = s->rephase.rephase_count % 6;
    if (phase_kind == 0 || phase_kind == 3) {
        // B: Copy best phases to saved polarities, then reset best high-water mark
        for (Var v = 1; v <= s->num_vars; v++) {
            if (s->rephase.best_phase[v] != UNDEF)
                s->vars[v].polarity = s->rephase.best_phase[v] == TRUE;
        }
        s->rephase.best_trail_size = 0;
        s->rephase.best_prefix_valid = false;
    } else if (phase_kind == 1 || phase_kind == 4) {
        // W: Bounded ProbSAT walk (falls back to flipping saved polarities if skipped)
        bool walked = false;
        if (!s->proof_journal && s->num_clauses >= 64 && !solver_budget_exhausted_now(s)) {
            walked = solver_walk_rephase(s, 10000u, 15000000ULL);
        }
        if (!walked) {
            for (Var v = 1; v <= s->num_vars; v++) s->vars[v].polarity = !s->vars[v].polarity;
        }
    } else if (phase_kind == 2) {
        // I: Inverted (all positive) phase
        for (Var v = 1; v <= s->num_vars; v++) s->vars[v].polarity = true;
    } else {
        // O: Original (all negative) phase
        for (Var v = 1; v <= s->num_vars; v++) s->vars[v].polarity = false;
    }

    /* Crucially sync Stable-mode target_phase[] to the newly rephased saved polarities
     * and reset target_trail_size so Stable mode actually leaves stale basins! */
    if (s->rephase.target_phase) {
        for (Var v = 1; v <= s->num_vars; v++) {
            s->rephase.target_phase[v] = s->vars[v].polarity ? TRUE : FALSE;
        }
        s->rephase.target_trail_size = 0;
        s->rephase.target_prefix_valid = false;
    }

    s->rephase.conflicts_since = 0;
    s->rephase.rephase_count++;
    uint64_t interval = (uint64_t)s->opts.rephase_interval + 100ULL * s->rephase.rephase_count;
    s->next_rephase = s->stats.conflicts + interval;

    if (IS_VERBOSE(s)) {
        fprintf(stderr, "c [Rephase #%u kind=%u] trail=%u/%u\n",
                s->rephase.rephase_count, phase_kind,
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
    if (!s->opts.reuse_trail || (s->opts.alternating && s->stable_mode) || !s->decision_level) return 0;
    bool vmtf_active = use_vmtf_order(s);
    Var next = INVALID_VAR;
    if (vmtf_active) next = solver_vmtf_pick(s);
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
        if (vmtf_active) {
            if (s->vmtf.nodes[v].stamp <= s->vmtf.nodes[next].stamp) break;
        } else if (s->vars[v].activity <= s->vars[next].activity) break;
        ++retained;
    }
    return retained;
}

bool solver_should_restart(Solver* s) {
    if (s->opts.restart_first == UINT32_MAX) return false;
    if (s->opts.alternating && s->work >= s->next_mode_ticks) {
        s->stable_mode = !s->stable_mode;
        uint64_t next_span = s->mode_ticks_span + (s->mode_ticks_span >> 1);
        if (next_span > 100000000ULL) next_span = 100000000ULL;
        s->mode_ticks_span = next_span;
        s->next_mode_ticks = s->work + next_span;
        s->restart.conflicts_since = 0;
        s->restart.recent_lbds_count = s->restart.recent_lbds_head = 0;
        s->recent_lbd_sum = 0;
        return true;
    }
    bool restart = false;
    if (s->opts.alternating && s->stable_mode) {
        /* Reluctant doubling (Luby * 1024) for long stable-mode dives. */
        if (s->restart.reluctant_countdown <= 1) {
            uint64_t u = s->restart.reluctant_u;
            uint64_t v = s->restart.reluctant_v;
            if ((u & (~u + 1)) == v) { u++; v = 1; }
            else { v <<= 1; }
            s->restart.reluctant_u = u;
            s->restart.reluctant_v = v;
            s->restart.reluctant_countdown = v * 1024ULL;
            restart = true;
        } else {
            s->restart.reluctant_countdown--;
        }
    } else if (s->opts.luby_restart) {
        uint64_t threshold = (uint64_t)luby_sequence(s->restart.luby_index + 1) * s->opts.luby_unit;
        restart = s->restart.conflicts_since >= threshold;
        if (restart) s->restart.luby_index++;
    } else if (s->opts.glucose_restart) {
        if (s->restart.conflicts_since >= s->opts.glucose_min_conflicts) {
            /* Glucose/Kissat trail-blocking: postpone restart when trail is surging. */
            if (s->lbd_samples > 100 && s->restart.trail_ma > 0 &&
                (double)s->trail_size > 1.4 * s->restart.trail_ma) {
                s->restart.conflicts_since = 0;
                return false;
            }
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
    /* Biere-Fröhlich windowed warmup for fast/slow LBD and trail EMAs. */
    uint64_t n = ++s->lbd_samples;
    double inv_n = 1.0 / (double)n;
    double fast_beta = 1.0 - s->opts.glucose_fast_alpha;
    double slow_beta = 1.0 - s->opts.glucose_slow_alpha;
    double trail_beta = 1.0 / 5000.0;
    if (inv_n > fast_beta) fast_beta = inv_n;
    if (inv_n > slow_beta) slow_beta = inv_n;
    if (inv_n > trail_beta) trail_beta = inv_n;
    s->restart.fast_ma += fast_beta * ((double)lbd - s->restart.fast_ma);
    s->restart.slow_ma += slow_beta * ((double)lbd - s->restart.slow_ma);
    s->restart.trail_ma += trail_beta * ((double)s->trail_size - s->restart.trail_ma);

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
    if (!locked && size == 2 && s->values[var(lits[1])] != UNDEF &&
        s->vars[var(lits[1])].reason == cr) locked = true;
#ifdef DEBUG
    /* Propagation and explicit enqueueing place the implied literal first
       for long clauses; learned binaries may imply either literal without
       mutating arena memory. */
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
    if (!fresh) { s->error = true; return; }
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
    if (s->gauss) gauss_relocate_clauses(s, old);
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

/* Diagnostic visits count reduction decisions, not unique clauses. Metadata
   follows arena GC. Creation's mandatory assertion is deliberately not a reuse. */
static void account_reduction_use(Solver *s, CRef cr, bool deleted) {
#ifdef BSAT_SEARCH_DIAGNOSTICS
    if(!SEARCH_DIAGNOSTICS(s)) return;
    ClauseHeader *h=CLAUSE_HEADER(s->arena,cr);
    SolverAccounting *a=&s->accounting;
    bool unused=!h->units && !h->analyses && !h->conflicts;
    bool analyzed=(h->recent_use&6)!=0;
    ++a->use_reduction_visits;
    if(deleted) {
        ++a->use_deleted;
        a->use_deleted_scans+=h->scans;
        if(unused) {++a->use_deleted_never;a->use_deleted_never_scans+=h->scans;}
        if(h->recent_use) ++a->use_deleted_recent;
        if(analyzed) ++a->use_deleted_recent_analysis;
        if(analyzed && h->lbd>6) {
            ++a->use_deleted_recent_high_lbd;
            a->use_deleted_recent_high_lbd_scans+=h->scans;
        }
        uint64_t born=((uint64_t)h->born_hi<<32)|h->born_lo;
        if(a->diagnostic_conflicts>=born) a->use_deleted_age_sum+=a->diagnostic_conflicts-born;
    } else {
        ++a->use_kept;
        if(analyzed) ++a->use_kept_recent_analysis;
        if(unused) {++a->use_kept_unused;a->use_kept_unused_scans+=h->scans;}
    }
    h->recent_use=0;
#else
    (void)s;(void)cr;(void)deleted;
#endif
}

static void solver_sweep_deleted_watches(Solver *s) {
    uint32_t num_lits = 2 * (s->watches->num_vars + 1);
    for (uint32_t l = 0; l < num_lits; ++l) {
        WatchList *wl = &s->watches->lists[l];
        uint32_t out = 0;
        for (uint32_t i = 0; i < wl->size; ++i) {
            Watch w = wl->watches[i];
            if (!is_binary_watch(w) && clause_deleted(s->arena, watch_clause(w))) continue;
            wl->watches[out++] = w;
        }
        wl->size = out;
    }
}

static void recalculate_tiers(Solver *s) {
    if (!s->opts.protect_used || s->opts.glue_lbd != 2) return;
    if (s->num_vars < 300) return;
    uint64_t total = 0;
    for (unsigned i = 0; i <= 64; ++i) total += s->tier_used[i];
    if (total < 1000) return;
    uint64_t acc = 0;
    uint32_t t1 = 2, t2 = 6;
    uint64_t half = total / 2;
    uint64_t ninety = (total * 9) / 10;
    bool got1 = false;
    for (unsigned i = 0; i <= 64; ++i) {
        acc += s->tier_used[i];
        if (!got1 && acc >= half) {
            t1 = i < 2 ? 2 : (i > 3 ? 3 : i);
            got1 = true;
        }
        if (acc >= ninety) {
            t2 = i < 5 ? 5 : (i > 8 ? 8 : i);
            break;
        }
    }
    s->tier1_lbd = t1;
    s->tier2_lbd = t2;
}

static void solver_reduce_db_account_impl(Solver* s) {
    s->stats.reduces++;
    recalculate_tiers(s);
    uint32_t core_lbd = (s->opts.glue_lbd == 2 && s->opts.protect_used) ? s->tier1_lbd : s->opts.glue_lbd;
    uint32_t mid_lbd = (s->opts.glue_lbd == 2 && s->opts.protect_used) ? s->tier2_lbd : 6;
    ClauseScore *scores = malloc((s->num_learnts ? s->num_learnts : 1) * sizeof *scores);
    if (!scores) { s->error = true; return; }
    uint32_t n = 0;
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        ClauseHeader *h = CLAUSE_HEADER(s->arena, cr);
        if (h->size <= 2 || h->lbd <= core_lbd) {
            account_reduction_use(s, cr, false);
            continue;
        }
        bool locked = clause_locked(s, cr);
        if (s->opts.protect_used) {
            bool frozen = (h->flags & CLAUSE_FROZEN) != 0;
            h->flags &= ~CLAUSE_FROZEN;
            bool protected_use = (h->lbd <= mid_lbd && (h->used > 0 || frozen));
            if (h->used > 0) h->used--;
            if (locked || protected_use) {
                account_reduction_use(s, cr, false);
                continue;
            }
        } else if (locked) {
            account_reduction_use(s, cr, false);
            continue;
        }
        scores[n++] = (ClauseScore){cr, h->lbd, h->activity};
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
    bool deleted_any = false;
    for (uint32_t i = 0; i < n; ++i) {
        CRef cr = scores[i].cref;
        bool drop = (i >= keep || scores[i].lbd > s->opts.max_lbd);
        account_reduction_use(s, cr, drop);
        if (drop) {
            if (SEARCH_DIAGNOSTICS(s) && scores[i].activity == 0)
                s->accounting.deleted_without_analysis_use++;
            /* Lazy watch deletion: mark deleted in O(1), sweep watches once below. */
            proof_delete_clause(s, CLAUSE_LITS(s->arena, cr), CLAUSE_SIZE(s->arena, cr));
            arena_delete(s->arena, cr);
            s->stats.deleted_clauses++;
            deleted_any = true;
        } else {
            CLAUSE_HEADER(s->arena, cr)->activity *= (float)s->opts.clause_decay;
        }
    }
    free(scores);
    uint32_t out = 0;
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];
        if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr)) s->learnts[out++] = cr;
    }
    s->num_learnts = out;
    uint64_t gcs_before = s->garbage_collections;
    solver_collect_garbage(s);
    if (deleted_any && s->garbage_collections == gcs_before) {
        solver_sweep_deleted_watches(s);
    }
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

bool solver_import_clause(Solver *s, const Lit *lits, uint32_t size) {
    if (!s || s->decision_level != 0 || s->error || s->result == FALSE || size == 0) return false;
    Var max_v = s->factor_original_vars ? s->factor_original_vars : s->num_vars;
    Lit buf[16];
    if (size > 16) return true;
    uint32_t n = 0;
    for (uint32_t i = 0; i < size; ++i) {
        Lit l = lits[i];
        Var v = var(l);
        if (v == 0 || v > max_v || elim_is_eliminated(s, v)) return true;
        uint8_t val = s->values[v];
        if (val != UNDEF) {
            if (val == (uint8_t)(2u - sign(l))) return true;
            continue;
        }
        bool dup = false;
        for (uint32_t j = 0; j < n; ++j) {
            if (buf[j] == l) { dup = true; break; }
            if (buf[j] == neg(l)) return true;
        }
        if (!dup) buf[n++] = l;
    }
    if (n == 0) {
        s->base_unsat = true;
        s->result = FALSE;
        return false;
    }
    if (n == 1) {
        if (s->values[var(buf[0])] == UNDEF) push_trail(s, buf[0]);
        return true;
    }
    if (s->num_learnts == s->learnts_size) {
        uint32_t cap = s->learnts_size ? s->learnts_size * 2u : 64u;
        CRef *p = realloc(s->learnts, (size_t)cap * sizeof *p);
        if (!p) { s->error = true; return false; }
        s->learnts = p;
        s->learnts_size = cap;
    }
    CRef cr = arena_alloc(s->arena, buf, n, true);
    if (cr == INVALID_CLAUSE) { s->error = true; return false; }
    set_clause_lbd(s->arena, cr, MIN(n, 3u));
    CLAUSE_HEADER(s->arena, cr)->used = 2;
    s->learnts[s->num_learnts++] = cr;
    CRef watched = (n == 2) ? arena_binary_watch_ref(cr) : cr;
    watch_add(s->watches, buf[0], watched, buf[1]);
    watch_add(s->watches, buf[1], watched, buf[0]);
    if (s->watches->failed) { s->error = true; return false; }
    return true;
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
            if (s->values[var(pivot)] != UNDEF || solver_is_frozen(s, var(pivot))) continue;
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
    if (s->frozen && fresh->frozen) {
        for (Var v = 1; v <= original_vars; ++v) fresh->frozen[v] = s->frozen[v];
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
        !s->elim && !s->gauss && !s->opts.elim && !s->opts.bce && !s->opts.local_search &&
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
    s->restart.reluctant_u=1;s->restart.reluctant_v=1;s->restart.reluctant_countdown=1024;
    if (s->lrb_last_conflict) memset(s->lrb_last_conflict,0,((size_t)s->var_capacity+1)*sizeof *s->lrb_last_conflict);
    s->last_vivify=0;s->lbd_samples=0;s->recent_lbd_sum=0;
    s->rephase.conflicts_since=0;s->next_rephase=s->opts.rephase_interval;
    s->mode_limit=1000;s->mode_ticks_span=1000000;s->next_mode_ticks=s->work+1000000;s->stable_mode=false;
    s->inprocess_ticks_span=5000000;s->next_inprocess_ticks=s->work+5000000;s->inprocess_rounds=0;s->ignore_clause=INVALID_CLAUSE;
    memset(s->tier_used,0,sizeof s->tier_used);s->tier1_lbd=2;s->tier2_lbd=6;
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

static bool solver_try_lucky(Solver *s) {
    if (s->decision_level || s->num_vars == 0 || s->num_vars > 200000 ||
        s->error || s->interrupted || s->result == FALSE) return false;
    bool saved_chrono = s->opts.chrono;
    bool saved_phase = s->opts.phase_saving;
    s->opts.chrono = false;
    s->opts.phase_saving = false;
    bool sat = false;
    for (int phase = 0; phase < 2 && !sat && !s->error && !s->interrupted; ++phase) {
        bool ok = true;
        for (Var v = 1; v <= s->num_vars; ++v) {
            if (s->values[v] != UNDEF) continue;
            s->decision_level++;
            s->trail_lims[s->decision_level] = s->trail_size;
            push_trail(s, mkLit(v, phase == 0));
            if (solver_propagate(s) != INVALID_CLAUSE) {
                ok = false;
                break;
            }
            if (s->watches->failed) s->error = true;
            if (s->error || s->interrupted) { ok = false; break; }
        }
        if (s->watches->failed) s->error = true;
        if (ok && s->trail_size == s->num_vars && !s->error && !s->interrupted) {
            sat = true;
            break;
        }
        solver_backtrack(s, 0);
    }
    s->opts.chrono = saved_chrono;
    s->opts.phase_saving = saved_phase;
    return sat;
}

static void bump_reason_side_literals(Solver *s, const Lit *learnt, uint32_t n) {
    if (n <= 1 || n > 48 || s->num_vars < 300) return;
    double half_inc = 0.5 * s->order.var_inc;
    uint32_t touched = 0;
    for (uint32_t i = 0; i < n; ++i) {
        Var v = var(learnt[i]);
        if (!s->seen[v]) {
            s->seen[v] = 1;
            s->minimize_touched[touched++] = v;
        }
    }
    for (uint32_t i = 1; i < n; ++i) {
        Var v = var(learnt[i]);
        Lit b = s->binary_reasons[v];
        if (b != LIT_UNDEF) {
            Var u = var(b);
            if (!s->seen[u] && s->vars[u].level > 0) {
                s->seen[u] = 1;
                s->minimize_touched[touched++] = u;
                bump_var_activity(s, u, half_inc);
            }
            continue;
        }
        CRef r = s->vars[v].reason;
        if (r != INVALID_CLAUSE && !clause_deleted(s->arena, r)) {
            uint32_t rsz = CLAUSE_SIZE(s->arena, r);
            if (rsz <= 12) {
                const Lit *rlits = CLAUSE_LITS(s->arena, r);
                for (uint32_t j = 0; j < rsz; ++j) {
                    Var u = var(rlits[j]);
                    if (!s->seen[u] && s->vars[u].level > 0) {
                        s->seen[u] = 1;
                        s->minimize_touched[touched++] = u;
                        bump_var_activity(s, u, half_inc);
                    }
                }
            }
        }
    }
    for (uint32_t i = 0; i < touched; ++i) s->seen[s->minimize_touched[i]] = 0;
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
    /* Fast lucky-phase check before heavy preprocessing: solves Horn/monotone SAT in <1ms. */
    if (!n_assumps && !s->proof_journal && !s->opts.reuse_learnts && s->opts.inprocess &&
        s->opts.preprocess_budget && !s->opts.factor && !s->opts.local_search && !s->opts.max_conflicts &&
        s->num_clauses >= 64 && !solver_budget_exhausted(s)) {
        if (solver_try_lucky(s)) return TRUE;
        if (s->error || s->interrupted) return UNDEF;
    }
    /* Bounded congruence closure before inprocessing (100% RUP-verified).
     * Scale work budget with formula size so large hardware miters (up to 800k clauses)
     * have enough ticks to index gates and close equivalences before BVE. */
    if (!n_assumps && !s->proof_journal && !s->opts.reuse_learnts && s->opts.inprocess &&
        s->opts.preprocess_budget && !s->opts.congruence && !s->opts.factor && !s->opts.local_search &&
        s->num_vars >= 300 && s->num_clauses >= 64 && s->num_clauses <= 800000 && !solver_budget_exhausted(s)) {
        uint64_t cong_budget = (uint64_t)s->num_clauses * 160ULL;
        if (cong_budget < 5000000ULL) cong_budget = 5000000ULL;
        if (cong_budget > 80000000ULL) cong_budget = 80000000ULL;
        s->work_limit = s->work + cong_budget;
        double started = solver_account_begin(s);
        solver_congruence(s);
        solver_account_end(s, ACCOUNT_PREPROCESS, started);
        s->work_limit = 0;
        if (s->error || s->interrupted) return UNDEF;
        if (s->result == FALSE || solver_propagate(s) != INVALID_CLAUSE) {s->base_unsat=true;return FALSE;}
    }
    /* Exact syntactic symmetry breaking (BreakID-style; emits RAT/RUP DRAT proofs
     * in --proof mode when trail_size==0 && num_learnts==0, disabled in journals/reuse)
     * BEFORE AMO / SBVA / BVE so row-interchange & transposition symmetries are intact. */
    if (!n_assumps && !s->proof_journal && !s->opts.reuse_learnts &&
        s->opts.inprocess && s->opts.preprocess_budget && !s->opts.factor && !s->opts.local_search &&
        s->opts.sym && !solver_budget_exhausted(s)) {
        double started = solver_account_begin(s);
        bool sym_ok = solver_break_symmetries(s, 15000000ULL);
        solver_account_end(s, ACCOUNT_PREPROCESS, started);
        if (!sym_ok) {
            if (s->error || s->interrupted) return UNDEF;
            s->base_unsat = true;
            return FALSE;
        }
    }
    /* Explicit or adaptive bounded clause factoring (BVA) BEFORE inprocess/elim. */
    if (!n_assumps && s->opts.factor && s->opts.factor_budget && s->opts.factor_max_variables) {
        s->work_limit = s->work + MIN(s->opts.factor_budget, UINT64_MAX - s->work);
        double started = solver_account_begin(s);
        solver_factor(s);
        solver_account_end(s, ACCOUNT_PREPROCESS, started);
        s->work_limit = 0;
        if (s->error || s->interrupted) return UNDEF;
        if (s->num_vars + 2 > s->levels_capacity) {
            uint8_t *seen = calloc((size_t)s->num_vars + 2, 1);
            if (!seen) { s->error = true; return UNDEF; }
            free(s->level_seen); s->level_seen = seen; s->levels_capacity = s->num_vars + 2;
        }
    } else if (!n_assumps && !s->proof_journal && !s->opts.reuse_learnts &&
        s->opts.inprocess && s->opts.preprocess_budget && !s->opts.factor && !s->opts.local_search &&
        s->num_clauses >= 500 && s->num_clauses <= 250000 && !solver_budget_exhausted(s)) {
        /* 1. Extract large AMO binary cliques via proof-producing Sinz sequential counters. */
        if (!solver_amo_cliques(s)) return UNDEF;
        if (s->error || s->interrupted) return UNDEF;
        /* 2. Run proof-producing Structured Bounded Variable Addition (SBVA) on structured /
         *    binary-rich or dense formulas (emits RAT-valid DRAT proofs when s->proof_file is set). */
        if (s->opts.sbva) {
            uint32_t bin_cnt = 0;
            for (uint32_t i = 0; i < s->num_clauses; ++i) {
                CRef cr = s->clauses[i];
                if (cr != INVALID_CLAUSE && !clause_deleted(s->arena, cr) && CLAUSE_SIZE(s->arena, cr) == 2)
                    bin_cnt++;
            }
            if ((uint64_t)bin_cnt * 2ULL >= (uint64_t)s->num_clauses ||
                (s->num_vars <= 5000 && (uint64_t)s->num_clauses >= (uint64_t)s->num_vars * 8ULL)) {
                double started = solver_account_begin(s);
                uint32_t max_aux = MIN(2048u, MAX(64u, s->num_vars));
                if (!solver_sbva(s, 20000000ULL, max_aux, 2u)) {
                    solver_account_end(s, ACCOUNT_PREPROCESS, started);
                    return UNDEF;
                }
                solver_account_end(s, ACCOUNT_PREPROCESS, started);
                if (s->error || s->interrupted) return UNDEF;
            }
        }
    }
    if (!n_assumps && !s->proof_journal && !s->opts.reuse_learnts && s->opts.inprocess &&
        s->opts.preprocess_budget && !s->opts.local_search &&
        !solver_budget_exhausted(s)) {
        if (!solver_inprocess(s, true)) {
            if (s->error || s->interrupted) return UNDEF;
            s->base_unsat = true;
            return FALSE;
        }
        s->next_inprocess_ticks = s->work + s->inprocess_ticks_span;
    }
    if (s->error || s->interrupted) return UNDEF;
    if (s->result == FALSE) {s->base_unsat=true;return FALSE;}
    Lit *learnt = malloc((s->num_vars+1)*sizeof *learnt);
    if (!learnt) { s->error=true; return UNDEF; }
    lbool result=UNDEF;
    /* Initialize after preprocessing, which may replace the solver. Rebuilt
       databases start fresh; retained databases keep their maintenance schedule. */
    if (!s->reduce_span) {
        s->reduce_span = s->opts.reduce_interval;
        s->reduce_limit = s->reduce_span;
    }
    sync_lit_values(s);
    for (;;) {
        if (solver_budget_exhausted(s)) break;
        CRef conflict=solver_propagate(s);
        if (conflict == INVALID_CLAUSE && s->gauss && !s->error && !s->interrupted && s->result != FALSE) {
            while (conflict == INVALID_CLAUSE && gauss_has_work(s) &&
                   !s->error && !s->interrupted && s->result != FALSE) {
                conflict = gauss_propagate(s);
                if (conflict == INVALID_CLAUSE && s->qhead < s->trail_size &&
                    !s->error && !s->interrupted && s->result != FALSE) {
                    conflict = solver_propagate(s);
                }
            }
        }
        if (s->error || s->interrupted) break;
        if (s->result == FALSE) { s->base_unsat = true; result = FALSE; break; }
        if (conflict != INVALID_CLAUSE) {
            s->stats.conflicts++; s->restart.conflicts_since++;
            if(SEARCH_DIAGNOSTICS(s)) ++s->accounting.diagnostic_conflicts;
            if (s->opts.chrono && !solver_normalize_conflict(s,conflict)) {
                if (s->proof_file && s->gauss && conflict != BINARY_CONFLICT) {
                    gauss_ensure_clause_proved(s, conflict);
                }
                if (!solver_budget_exhausted(s)) {s->base_unsat=true;result=FALSE;}
                break;
            }
            if (!s->decision_level) {
                if (s->proof_file && s->gauss && conflict != BINARY_CONFLICT) {
                    gauss_ensure_clause_proved(s, conflict);
                }
                s->base_unsat=true;result=FALSE;break;
            }
            uint32_t n; Level backtrack;
            solver_analyze(s, conflict, learnt, &n, &backtrack);
            s->stats.minimized_literals += s->opts.shrink
                ? solver_shrink_clause(s, learnt, &n)
                : solver_minimize_clause(s, learnt, &n);
            uint32_t lbd=calc_lbd(s,learnt,n);
            uint32_t binary_removed = solver_minimize_binary(s,learnt,&n,lbd);
            s->stats.minimized_literals += binary_removed;
            if (binary_removed) lbd = calc_lbd(s,learnt,n);
            record_lbd(s,lbd);
            bump_reason_side_literals(s, learnt, n);
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
            /* Apply on-the-fly self-subsumption (OTFS) discovered during analysis.
             * All pivots were at the conflict level and are now unassigned. */
            for (uint32_t oi = 0; oi < s->otfs_count && !s->error; ++oi) {
                CRef ocr = s->otfs_crefs[oi];
                Lit pivot = s->otfs_pivots[oi];
                if (ocr == INVALID_CLAUSE || clause_deleted(s->arena, ocr) || clause_locked(s, ocr)) continue;
                uint32_t osize = CLAUSE_SIZE(s->arena, ocr);
                if (osize < 3) continue;
                uint32_t old_lbd = clause_lbd(s->arena, ocr);
                Lit small_buf[64];
                Lit *nbuf = osize <= 64 ? small_buf : malloc((size_t)osize * sizeof *nbuf);
                if (!nbuf) { s->error = true; break; }
                const Lit *olits = CLAUSE_LITS(s->arena, ocr);
                uint32_t nsz = 0;
                for (uint32_t k = 0; k < osize; ++k) {
                    if (olits[k] != pivot) nbuf[nsz++] = olits[k];
                }
                if (nsz == osize - 1 && nsz >= 2) {
                    /* Ensure two non-false (or highest-level) literals are in watch slots 0, 1. */
                    uint32_t wcount = 0;
                    for (uint32_t k = 0; k < nsz && wcount < 2; ++k) {
                        if (s->values[var(nbuf[k])] != (uint8_t)(1u + sign(nbuf[k]))) {
                            Lit tmp = nbuf[wcount]; nbuf[wcount++] = nbuf[k]; nbuf[k] = tmp;
                        }
                    }
                    if (wcount >= 2) {
                        proof_add_clause(s, nbuf, nsz);
                        if (!s->error) {
                            if (s->num_learnts == s->learnts_size) {
                                uint32_t cap = s->learnts_size ? s->learnts_size * 2 : 64;
                                CRef *p = realloc(s->learnts, cap * sizeof *p);
                                if (!p) { s->error = true; }
                                else { s->learnts = p; s->learnts_size = cap; }
                            }
                            if (!s->error && s->num_learnts < s->learnts_size) {
                                CRef ncr = arena_alloc(s->arena, nbuf, nsz, true);
                                if (ncr == INVALID_CLAUSE) {
                                    s->error = true;
                                } else {
                                    uint32_t nlbd = old_lbd > 1 ? old_lbd - 1 : 1;
                                    set_clause_lbd(s->arena, ncr, MIN(nlbd, nsz));
                                    CLAUSE_HEADER(s->arena, ncr)->used = 2;
                                    s->learnts[s->num_learnts++] = ncr;
                                    CRef wref = (nsz == 2) ? arena_binary_watch_ref(ncr) : ncr;
                                    watch_add_inline(s->watches, nbuf[0], wref, nbuf[1]);
                                    watch_add_inline(s->watches, nbuf[1], wref, nbuf[0]);
                                    solver_delete_clause(s, ocr);
                                    s->stats.subsumed_clauses++;
                                }
                            }
                        }
                    }
                }
                if (nbuf != small_buf) free(nbuf);
            }
            s->otfs_count = 0;
            if (s->error) break;
            proof_add_clause(s,learnt,n);
            if (s->share_export_callback &&
                (n == 1u || n == 2u || (n <= s->share_export_max_len && lbd <= s->share_export_max_lbd))) {
                s->share_export_callback(s->share_export_state, learnt, n, lbd);
            }
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
                CLAUSE_HEADER(s->arena,reason)->used = 0;
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
                if (s->import_callback) {
                    s->lit_values_live = false;
                    s->import_callback(s, s->import_state);
                    sync_lit_values(s);
                    if (s->error || s->interrupted) break;
                    if (s->result == FALSE) { s->base_unsat = true; result = FALSE; break; }
                    if (s->qhead < s->trail_size) continue;
                }
                if (!n_assumps && !s->proof_journal && !s->opts.reuse_learnts && s->opts.inprocess &&
                    !s->opts.factor && !s->opts.local_search &&
                    (s->work >= s->next_inprocess_ticks ||
                     s->stats.conflicts >= s->last_vivify + s->opts.inprocess_interval)) {
                    s->last_vivify = s->stats.conflicts;
                    s->lit_values_live = false;
                    if (!solver_inprocess(s, false)) {
                        if (s->error || s->interrupted) break;
                        s->base_unsat = true; result = FALSE; break;
                    }
                    sync_lit_values(s);
                    uint64_t next_span = s->inprocess_ticks_span + (s->inprocess_ticks_span >> 1);
                    if (next_span > 50000000ULL) next_span = 50000000ULL;
                    s->inprocess_ticks_span = next_span;
                    s->next_inprocess_ticks = s->work + next_span;
                    if (s->qhead < s->trail_size) continue;
                } else {
                    s->lit_values_live = false;
                    if (!solver_simplify(s)) {s->base_unsat=true;result=FALSE;break;}
                    sync_lit_values(s);
                    if (s->qhead<s->trail_size) continue;
                }
            }
            if (s->opts.rephase && s->stats.conflicts >= s->next_rephase) {
                if (!n_assumps && s->decision_level) {
                    solver_backtrack(s, 0);
                }
                s->lit_values_live = false;
                solver_rephase(s);
                sync_lit_values(s);
                if (s->qhead < s->trail_size) continue;
            }
            if (!n_assumps && !s->elim && s->opts.local_search &&
                s->stats.conflicts >= s->local_search.conflicts_since+s->opts.ls_interval) {
                // The walk owns a separate assignment; failed walks must not
                // discard the CDCL trail or turn into uncounted root restarts.
                s->lit_values_live = false;
                bool found=solver_try_local_search(s);
                sync_lit_values(s);
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
    s->lit_values_live = false;
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
    s->lbd_assumption_levels=s->opts.assumption_lbd ? n_assumps : 0;
    lbool result=solve_internal(s,assumps,n_assumps);
    s->lbd_assumption_levels=0;
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
