#include "solver.h"
#include <assert.h>
#include <stdio.h>

#ifdef BSAT_SEARCH_DIAGNOSTICS
static Solver *
formula(bool accounting, bool prefix)
{
    SolverOpts o = default_opts();

    o.accounting = accounting;
    o.probing = false;
    o.reuse_learnts = true;
    o.restart_assumptions = prefix;
    o.assumption_lbd = prefix;
    o.luby_unit = 1;
    o.luby_restart = true;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (unsigned i = 0; i < 31; ++i)
        assert(solver_new_var(s));
    /* The selector enables PHP(6,5); its opposite permits a checked SAT query. */
    for (unsigned p = 0; p < 6; ++p) {
        Lit clause[6] = {mkLit(31, true)};

        for (unsigned h = 0; h < 5; ++h)
            clause[h + 1] = mkLit(1 + p * 5 + h, false);
        assert(solver_add_clause(s, clause, 6));
        for (unsigned q = 0; q < p; ++q)
            for (unsigned h = 0; h < 5; ++h) {
                Lit pair[] = {mkLit(31, true), mkLit(1 + p * 5 + h, true),
                              mkLit(1 + q * 5 + h, true)};

                assert(solver_add_clause(s, pair, 3));
            }
    }
    return s;
}

static void
restarts(bool prefix)
{
    Solver *plain = formula(false, prefix);
    Solver *observed = formula(true, prefix);
    uint64_t previous_events = 0;

    for (unsigned query = 0; query < 3; ++query) {
        Lit assumption = mkLit(31, query == 1);
        lbool expected = query == 1 ? TRUE : FALSE;

        assert(solver_solve_with_assumptions(plain, &assumption, 1) == expected);
        assert(solver_solve_with_assumptions(observed, &assumption, 1) == expected);
        assert(!plain->error && !observed->error);
        assert(plain->stats.conflicts == observed->stats.conflicts);
        assert(plain->stats.decisions == observed->stats.decisions);
        assert(plain->stats.propagations == observed->stats.propagations);
        assert(plain->work == observed->work);
        assert(observed->accounting.restart_events - previous_events == observed->stats.restarts);
        previous_events = observed->accounting.restart_events;
        assert(!plain->accounting.restart_events && !plain->accounting.minimize_lbd_seconds);
        if (!query) {
            assert(previous_events > 0);
            assert(observed->accounting.restart_trail_before >
                   observed->accounting.restart_trail_kept);
            assert(observed->accounting.restart_levels_before >
                   observed->accounting.restart_levels_kept);
            assert((observed->accounting.restart_levels_kept > 0) == prefix);
            assert(observed->accounting.minimize_lbd_seconds >= 0);
        }
        if (expected == TRUE) {
            assert(solver_check_model(plain) && solver_check_model(observed));
            assert(solver_model_value(observed, 31) == FALSE);
        }
    }
    solver_free(plain);
    solver_free(observed);
}
#endif

int
main(void)
{
#ifdef BSAT_SEARCH_DIAGNOSTICS
    restarts(false);
    restarts(true);
    puts("PASS: restart accounting preserves search, assumption replacement and retained queries");
#else
    puts("SKIP: compile BSAT_SEARCH_DIAGNOSTICS for search scaling counters");
#endif
    return 0;
}
