#include "../include/solver.h"
#include "../include/local_search.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

int
main(void)
{
    Solver *s = solver_new();

    assert(s);
    assert(solver_new_var(s));
    assert(solver_new_var(s));
    for (unsigned mask = 0; mask < 4; ++mask) {
        Lit clause[] = {mkLit(1, mask & 1), mkLit(2, (mask >> 1) & 1)};

        assert(solver_add_clause(s, clause, 2));
    }
    LocalSearchState *ls = local_search_init(s);

    assert(ls);
    unsigned limits[] = {0, 1, 255, 256, 257, 1000, 10000, 100000};

    for (unsigned i = 0; i < sizeof limits / sizeof *limits; ++i) {
        ls->flips = 0;
        assert(!local_search_run(s, ls, limits[i], 0.5));
        assert(ls->flips == limits[i]);
    }
    assert(s->stats.clock_checks == 0);
    /* An already-throttled caller must not reuse a stale cached CPU reading. */
    s->opts.max_time = 1;
    s->stats.start_time = solver_cpu_time();
    assert(!solver_budget_exhausted(s));
    assert(s->stats.clock_checks == 1);
    s->stats.start_time -= 2;
    ls->flips = 0;
    assert(!local_search_run(s, ls, 100000, 0.5));
    assert(s->interrupted && ls->flips == 0);
    assert(s->stats.clock_checks == 2);
    /* Without a deadline, cancellation still stops before the first flip. */
    s->opts.max_time = 0;
    assert(!local_search_run(s, ls, 100000, 0.5));
    assert(ls->flips == 0 && s->stats.clock_checks == 2);
    s->interrupted = false;
    assert(!local_search_run(s, ls, 1, 0.5));
    assert(ls->flips == 1);
    s->opts.max_time = 3600;
    s->stats.start_time = solver_cpu_time();
    uint64_t checks = s->stats.clock_checks;

    ls->flips = 0;
    assert(!local_search_run(s, ls, 100000, 0.5));
    assert(ls->flips == 100000);
    assert(s->stats.clock_checks - checks == (100000 + 255) / 256);
    local_search_free(ls);
    solver_free(s);
    puts("PASS: exact walk budgets, fresh deadline checkpoints and cancellation");
    return 0;
}
