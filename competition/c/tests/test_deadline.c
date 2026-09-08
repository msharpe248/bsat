#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

static void scheduling(void) {
    Solver *s = solver_new();assert(s);
    for (unsigned i = 0; i < 256; ++i) assert(!solver_budget_exhausted(s));
    assert(s->stats.clock_checks == 0); // No clock reads without a CPU limit.
    s->opts.max_time = 1000000;
    s->stats.start_time = solver_cpu_time();
    assert(!solver_budget_exhausted(s) && s->stats.clock_checks == 1);
    for (unsigned i = 0; i < 127; ++i) assert(!solver_budget_exhausted(s));
    assert(s->stats.clock_checks == 1);
    assert(!solver_budget_exhausted(s) && s->stats.clock_checks == 2);
    s->work = 1023;assert(!solver_budget_exhausted(s));
    assert(s->stats.clock_checks == 2);
    s->work = 1024;assert(!solver_budget_exhausted(s));
    assert(s->stats.clock_checks == 3);
    s->stats.minimize_inspections = 1024;
    assert(!solver_budget_exhausted(s) && s->stats.clock_checks == 4);
    // These conditions are immediate even with a fresh cached clock reading.
    s->work_limit = s->work;assert(solver_budget_exhausted(s));s->work_limit = 0;
    assert(s->stats.clock_checks == 4 && !s->interrupted);
    s->interrupted = true;assert(solver_budget_exhausted(s));s->interrupted = false;
    s->error = true;assert(solver_budget_exhausted(s));s->error = false;
    s->watches->failed = true;assert(solver_budget_exhausted(s) && s->error);
    assert(s->stats.clock_checks == 4);
    solver_free(s);
}

static void expiration(void) {
    for (unsigned mode = 0; mode < 3; ++mode) {
        Solver *s = solver_new();assert(s);
        s->opts.max_time = 1;s->stats.start_time = solver_cpu_time();
        assert(!solver_budget_exhausted(s));
        s->stats.start_time -= 2; // Expired since the cached reading.
        if (mode == 0) {
            for (unsigned i = 0; i < 127; ++i) assert(!solver_budget_exhausted(s));
        } else if (mode == 1) s->work += 1024;
        else s->stats.minimize_inspections += 1024;
        assert(solver_budget_exhausted(s) && s->interrupted);
        assert(s->stats.clock_checks == 2);
        solver_free(s);
    }
}

static void empty_watch_search(bool rephase) {
    SolverOpts o = default_opts();o.probing = false;o.rephase = rephase;o.max_time = 0.001;
    Solver *s = solver_new_with_opts(&o);assert(s);
    for (unsigned i = 0; i < 100000; ++i) assert(solver_new_var(s));
    // No watch inspections advance work. Poll-count scheduling must still stop.
    assert(solver_solve(s) == UNDEF && s->interrupted);
    // Startup may consume the limit before the first decision (e.g. under ASan).
    // Early interruption is valid; completing every decision would miss polling.
    assert(s->stats.decisions < s->num_vars);
    assert(s->work == 0 && s->stats.clock_checks > 1);
    double elapsed = solver_cpu_time() - s->stats.start_time;
    assert(elapsed < 1); // Broad guard against lost deadline polling.
    s->opts.max_time = 0;s->opts.rephase = false;
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(!s->interrupted && s->stats.clock_checks == 0);
    solver_free(s);
}

static void terminal_check(void) {
    SolverOpts o = default_opts();o.max_time = 1000;
    Solver *s = solver_new_with_opts(&o);assert(s);
    assert(!solver_add_clause(s, NULL, 0));
    // An input contradiction returns before the search loop or propagation.
    assert(solver_solve(s) == FALSE);
    assert(s->stats.clock_checks == 1);
    solver_free(s);
}

int main(void) {
    scheduling();expiration();empty_watch_search(false);empty_watch_search(true);terminal_check();
    puts("PASS: deadline poll bounds, work/minimization triggers, immediate limits and API reuse");
    return 0;
}
