#include "solver.h"
#include <assert.h>
#include <stdio.h>

#ifdef BSAT_SEARCH_DIAGNOSTICS
/* Signed implication chains distinguish covered binary boundaries from the
   recursive minimizer's deliberately unchanged production behavior. */
static void
binary_boundary_observations(void)
{
    for (unsigned signs = 0; signs < 16; ++signs) {
        SolverOpts o = default_opts();

        o.accounting = true;
        o.probing = false;
        Solver *s = solver_new_with_opts(&o);
        Lit p[4];

        assert(s);
        for (unsigned i = 0; i < 4; ++i) {
            assert(solver_new_var(s));
            p[i] = mkLit(i + 1, (signs >> i) & 1);
        }
        Lit edge1[] = {neg(p[0]), p[1]};
        Lit edge2[] = {neg(p[1]), p[2]};
        Lit source[] = {p[3], neg(p[0]), neg(p[1]), neg(p[2])};

        assert(solver_add_clause(s, edge1, 2));
        assert(solver_add_clause(s, edge2, 2));
        assert(solver_add_clause(s, source, 4));
        s->decision_level = 1;
        s->trail_lims[1] = s->trail_size;
        s->values[1] = sign(p[0]) ? FALSE : TRUE;
        s->vars[1].level = 1;
        s->vars[1].reason = INVALID_CLAUSE;
        s->vars[1].trail_pos = s->trail_size;
        s->trail[s->trail_size++].lit = p[0];
        assert(solver_propagate(s) == INVALID_CLAUSE);
        unsigned n = 4;

        assert(solver_minimize_clause(s, source, &n) == 0 && n == 4);
        assert(s->accounting.boundary_literals == 3);
        assert(s->accounting.boundary_binary == 2);
        assert(s->accounting.boundary_binary_covered == 2);
        for (Var v = 1; v <= 4; ++v)
            assert(!s->seen[v]);
        solver_free(s);
    }
}

static void
replay_causes(void)
{
    SolverOpts o = default_opts();

    o.accounting = true;
    o.probing = false;
    o.phase_saving = false;
    for (unsigned chrono = 0; chrono < 2; ++chrono) {
        o.chrono = chrono != 0;
        Solver *s = solver_new_with_opts(&o);

        assert(s && solver_new_var(s));
        for (unsigned cause = REMOVAL_RESTART; cause <= REMOVAL_BACKJUMP; ++cause) {
            assert(solver_decide(s));
            assert(solver_propagate(s) == INVALID_CLAUSE);
            s->accounting.removal_cause = cause;
            solver_backtrack(s, 0);
            assert(s->accounting.removed_processed[cause] == 1);
            assert(solver_decide(s));
            assert(solver_propagate(s) == INVALID_CLAUSE);
            assert(s->accounting.replay_same[cause] == 1);
            assert(s->accounting.replay_opposite[cause] == 0);
            solver_backtrack(s, 0);
            s->opts.phase_saving = true;
            s->vars[1].polarity = false;
            assert(solver_decide(s));
            assert(solver_propagate(s) == INVALID_CLAUSE);
            assert(s->accounting.replay_opposite[cause] == 1);
            /* Start a new observation interval for the other cause. */
            s->accounting.removal_cause = REMOVAL_OTHER;
            solver_backtrack(s, 0);
            s->vars[1].removed_value = 0;
            s->opts.phase_saving = false;
        }
        assert(s->accounting.propagation_source[PROP_DECISION] == s->stats.propagations);
        solver_free(s);
    }
}

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
    binary_boundary_observations();
    replay_causes();
    restarts(false);
    restarts(true);
    puts("PASS: restart accounting preserves search, assumption replacement and retained queries");
#else
    puts("SKIP: compile BSAT_SEARCH_DIAGNOSTICS for search scaling counters");
#endif
    return 0;
}
