/*********************************************************************
 * BSAT Competition Solver - Multi-Core Clause-Sharing Portfolio
 *
 * Spawns N diversified, completely isolated Solver workers via POSIX
 * pthreads. Logically-equivalent workers (var_elim=false, bce=false,
 * sbva=false) export short learned clauses (size <= 4) into a mutex-
 * guarded ring buffer; all workers import batches at decision level 0.
 * An atomic cancellation flag stops all sibling workers immediately
 * once any worker proves SAT or UNSAT.
 *********************************************************************/

#include "../include/portfolio.h"
#include "../include/solver.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define PORTFOLIO_MAX_THREADS    64u
#define PORTFOLIO_RING_CAP       4096u /* Must be power of 2 */
#define PORTFOLIO_MAX_SHARE_LEN  4u
#define PORTFOLIO_IMPORT_BATCH   128u

typedef struct {
    uint32_t producer;
    uint8_t  size;
    Lit      lits[PORTFOLIO_MAX_SHARE_LEN];
} SharedClause;

typedef struct {
    const Lit *input;
    size_t     input_size;
    Var        orig_vars;
    SolverOpts base_opts;
    double     wall_start;
    double     wall_limit;

    atomic_int done;
    pthread_mutex_t result_mu;
    int        winner_id;
    lbool      winner_result;
    uint8_t   *winner_model;     /* orig_vars + 1 bytes */
    uint64_t   total_conflicts;
    uint64_t   total_decisions;
    uint64_t   total_propagations;
    uint64_t   shared_exported;
    uint64_t   shared_imported;

    pthread_mutex_t ring_mu;
    SharedClause ring[PORTFOLIO_RING_CAP];
    uint64_t   ring_seq;         /* Monotonically increasing write cursor */
} PortfolioCtx;

typedef struct {
    PortfolioCtx *ctx;
    uint32_t      id;
    uint64_t      read_seq;
    bool          can_export;
    Solver       *solver;
} WorkerState;

static double wall_time_sec(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static int worker_terminate(void *state) {
    WorkerState *w = (WorkerState *)state;
    PortfolioCtx *ctx = w->ctx;
    if (atomic_load_explicit(&ctx->done, memory_order_acquire)) return 1;
    if (ctx->wall_limit > 0.0 && (wall_time_sec() - ctx->wall_start) >= ctx->wall_limit) {
        atomic_store_explicit(&ctx->done, 1, memory_order_release);
        return 1;
    }
    return 0;
}

static void worker_export_clause(void *state, int *dimacs_lits) {
    WorkerState *w = (WorkerState *)state;
    if (!w->can_export) return;
    PortfolioCtx *ctx = w->ctx;
    if (atomic_load_explicit(&ctx->done, memory_order_relaxed)) return;

    Lit lits[PORTFOLIO_MAX_SHARE_LEN];
    uint32_t n = 0;
    while (dimacs_lits[n] != 0) {
        if (n >= PORTFOLIO_MAX_SHARE_LEN) return;
        int d = dimacs_lits[n];
        Var v = (Var)(d < 0 ? -d : d);
        if (v == 0 || v > ctx->orig_vars) return;
        lits[n] = mkLit(v, d < 0);
        n++;
    }
    if (n == 0) return;

    pthread_mutex_lock(&ctx->ring_mu);
    uint64_t slot = ctx->ring_seq & (PORTFOLIO_RING_CAP - 1u);
    ctx->ring[slot].producer = w->id;
    ctx->ring[slot].size = (uint8_t)n;
    memcpy(ctx->ring[slot].lits, lits, (size_t)n * sizeof(Lit));
    ctx->ring_seq++;
    ctx->shared_exported++;
    pthread_mutex_unlock(&ctx->ring_mu);
}

static void worker_import_clauses(Solver *s, void *state) {
    WorkerState *w = (WorkerState *)state;
    PortfolioCtx *ctx = w->ctx;
    if (atomic_load_explicit(&ctx->done, memory_order_relaxed)) return;

    SharedClause batch[PORTFOLIO_IMPORT_BATCH];
    uint32_t count = 0;

    pthread_mutex_lock(&ctx->ring_mu);
    uint64_t cur = ctx->ring_seq;
    if (cur > w->read_seq) {
        if (cur - w->read_seq > PORTFOLIO_RING_CAP) {
            w->read_seq = cur - PORTFOLIO_RING_CAP;
        }
        while (w->read_seq < cur && count < PORTFOLIO_IMPORT_BATCH) {
            SharedClause sc = ctx->ring[w->read_seq & (PORTFOLIO_RING_CAP - 1u)];
            w->read_seq++;
            if (sc.producer != w->id) {
                batch[count++] = sc;
            }
        }
        ctx->shared_imported += count;
    }
    pthread_mutex_unlock(&ctx->ring_mu);

    for (uint32_t i = 0; i < count && !s->error && s->result != FALSE; ++i) {
        if (!solver_import_clause(s, batch[i].lits, batch[i].size)) break;
    }
}

static SolverOpts make_worker_opts(const SolverOpts *base, uint32_t id, bool *can_export) {
    SolverOpts o = *base;
    o.proof_path = NULL;
    o.quiet = true;
    o.stats = false;
    /* Wall-clock deadline is enforced centrally by worker_terminate; keep per-thread
     * CPU limit disabled so thread CPU != wall time doesn't prematurely cut off. */
    o.max_time = 0.0;
    o.seed = base->seed + id * 1009u + 17u;

    if (id == 0) {
        /* Worker 0: Full BSAT defaults (BVE, SBVA, AMO, Gauss, alternating).
         * Does not export (BVE/SBVA can restrict equisatisfiable models), does import! */
        *can_export = false;
        return o;
    }

    /* Workers 1..N-1: 100% logically-equivalent solvers (no BVE/BCE/SBVA) so every
     * learned clause is globally entailed by the original formula and safe to share! */
    o.var_elim = false;
    o.elim = false;
    o.bce = false;
    o.sbva = false;
    o.sym = false;
    o.factor = false;
    *can_export = true;

    switch (id % 6u) {
        case 1:
            /* Focused-first VMTF with aggressive Glucose EMA restarts */
            o.vmtf = true;
            o.alternating = false;
            o.glucose_k = 0.85;
            o.reduce_interval = 1500;
            break;
        case 2:
            /* Stable 4-ary VSIDS + Luby restarts + non-chrono */
            o.vmtf = false;
            o.alternating = false;
            o.luby_restart = true;
            o.luby_unit = 128;
            o.chrono = false;
            o.var_decay = 0.95;
            break;
        case 3:
            /* LRB/CHB branching + mild random phase diversification */
            o.lrb = true;
            o.alternating = false;
            o.random_phase = true;
            o.random_phase_prob = 0.02;
            o.rephase_interval = 500;
            break;
        case 4:
            /* Alternating VMTF/VSIDS with tighter chrono + slower decay */
            o.alternating = true;
            o.chrono = true;
            o.chrono_levels = 25;
            o.var_decay = 0.98;
            o.glucose_k = 0.80;
            break;
        case 5:
            /* High-retention VSIDS + fast rephasing + WalkSAT hybrid */
            o.alternating = true;
            o.reduce_interval = 2500;
            o.rephase_interval = 400;
            o.glucose_k = 0.95;
            break;
        default:
            /* Diversified alternating configuration for id >= 6 */
            o.alternating = (id & 1u) == 0;
            o.vmtf = (id & 2u) != 0;
            o.chrono = (id & 4u) == 0;
            o.var_decay = 0.90 + 0.015 * (double)(id % 5u);
            o.reduce_interval = 800u + 300u * (id % 5u);
            break;
    }
    return o;
}

static void *portfolio_worker_main(void *arg) {
    WorkerState *w = (WorkerState *)arg;
    PortfolioCtx *ctx = w->ctx;

    SolverOpts o = make_worker_opts(&ctx->base_opts, w->id, &w->can_export);
    Solver *s = solver_new_with_opts(&o);
    if (!s) return NULL;
    w->solver = s;

    /* Load permanent formula from master's immutable input buffer. */
    for (Var v = 0; v < ctx->orig_vars; ++v) {
        if (atomic_load_explicit(&ctx->done, memory_order_relaxed)) goto finish;
        if (!solver_new_var(s)) goto finish;
    }
    size_t start = 0;
    for (size_t i = 0; i < ctx->input_size; ++i) {
        if ((i & 4095u) == 0 && atomic_load_explicit(&ctx->done, memory_order_relaxed)) goto finish;
        if (ctx->input[i] == 0) {
            solver_add_clause(s, ctx->input + start, (uint32_t)(i - start));
            start = i + 1;
            if (s->error) goto finish;
        }
    }

    solver_set_terminate(s, w, worker_terminate);
    if (w->can_export) {
        s->learn_callback = worker_export_clause;
        s->learn_state = w;
        s->learn_max_length = PORTFOLIO_MAX_SHARE_LEN;
    }
    s->import_callback = worker_import_clauses;
    s->import_state = w;

    lbool r = solver_solve(s);

    pthread_mutex_lock(&ctx->result_mu);
    ctx->total_conflicts += s->stats.conflicts;
    ctx->total_decisions += s->stats.decisions;
    ctx->total_propagations += s->stats.propagations;
    if ((r == TRUE || r == FALSE) && !s->error && ctx->winner_id < 0) {
        ctx->winner_id = (int)w->id;
        ctx->winner_result = r;
        if (r == TRUE && ctx->winner_model) {
            for (Var v = 1; v <= ctx->orig_vars; ++v) {
                ctx->winner_model[v] = (uint8_t)solver_model_value(s, v);
            }
        }
        atomic_store_explicit(&ctx->done, 1, memory_order_release);
    }
    pthread_mutex_unlock(&ctx->result_mu);

finish:
    solver_free(s);
    w->solver = NULL;
    return NULL;
}

lbool solver_solve_parallel(Solver *master, uint32_t num_threads) {
    if (!master) return UNDEF;
    if (num_threads <= 1u) return solver_solve(master);
    if (master->proof_file || master->proof_journal) {
        master->error = true;
        return UNDEF;
    }
    if (master->error || master->watches->failed) return UNDEF;
    if (master->result == FALSE) {
        master->base_unsat = true;
        return FALSE;
    }
    if (num_threads > PORTFOLIO_MAX_THREADS) num_threads = PORTFOLIO_MAX_THREADS;

    PortfolioCtx *ctx = calloc(1, sizeof *ctx);
    WorkerState *workers = calloc(num_threads, sizeof *workers);
    pthread_t *threads = calloc(num_threads, sizeof *threads);
    uint8_t *model_buf = calloc((size_t)master->num_vars + 1u, 1);
    if (!ctx || !workers || !threads || !model_buf) {
        free(ctx); free(workers); free(threads); free(model_buf);
        master->error = true;
        return UNDEF;
    }

    ctx->input = master->input;
    ctx->input_size = master->input_size;
    ctx->orig_vars = master->factor_original_vars ? master->factor_original_vars : master->num_vars;
    ctx->base_opts = master->opts;
    ctx->wall_start = wall_time_sec();
    ctx->wall_limit = master->opts.max_time;
    atomic_init(&ctx->done, 0);
    ctx->winner_id = -1;
    ctx->winner_result = UNDEF;
    ctx->winner_model = model_buf;
    pthread_mutex_init(&ctx->result_mu, NULL);
    pthread_mutex_init(&ctx->ring_mu, NULL);

    uint32_t launched = 0;
    for (uint32_t i = 0; i < num_threads; ++i) {
        workers[i].ctx = ctx;
        workers[i].id = i;
        workers[i].read_seq = 0;
        if (pthread_create(&threads[i], NULL, portfolio_worker_main, &workers[i]) != 0) {
            atomic_store_explicit(&ctx->done, 1, memory_order_release);
            master->error = true;
            break;
        }
        launched++;
    }
    for (uint32_t i = 0; i < launched; ++i) {
        pthread_join(threads[i], NULL);
    }

    lbool result = ctx->winner_result;
    if (result == TRUE) {
        for (Var v = 1; v <= ctx->orig_vars; ++v) {
            master->values[v] = model_buf[v];
        }
        if (!solver_check_model(master)) {
            master->error = true;
            result = UNDEF;
        }
    } else if (result == FALSE) {
        master->base_unsat = true;
    }

    master->stats.conflicts = ctx->total_conflicts;
    master->stats.decisions = ctx->total_decisions;
    master->stats.propagations = ctx->total_propagations;
    master->has_solved = true;
    master->result = master->error ? UNDEF : result;

    if (!master->opts.quiet && master->opts.stats) {
        printf("c Parallel portfolio: %u threads (winner=worker %d, exported=%llu, imported=%llu, wall=%.3f s)\n",
               launched, ctx->winner_id,
               (unsigned long long)ctx->shared_exported,
               (unsigned long long)ctx->shared_imported,
               wall_time_sec() - ctx->wall_start);
    }

    pthread_mutex_destroy(&ctx->result_mu);
    pthread_mutex_destroy(&ctx->ring_mu);
    free(model_buf);
    free(threads);
    free(workers);
    free(ctx);
    return master->result;
}
