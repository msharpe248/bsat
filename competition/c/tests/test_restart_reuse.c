#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>

static void
prefixes(bool queue)
{
    SolverOpts o = default_opts();

    o.reuse_trail = true;
    o.vmtf = queue;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (unsigned i = 0; i < 4; ++i)
        assert(solver_new_var(s));
    assert(solver_restart_level(s) == 0);
    Var decisions[3];

    for (unsigned i = 0; i < 3; ++i) {
        assert(solver_decide(s));
        decisions[i] = var(s->trail[s->trail_lims[i + 1]].lit);
    }
    Var next = 0;

    for (Var v = 1; v <= 4; ++v)
        if (s->values[v] == UNDEF) next = v;
    assert(next);
    if (queue) {
        /* Move the available variable above decisions 2/3, below decision 1. */
        solver_vmtf_bump(s, next);
        solver_vmtf_bump(s, decisions[0]);
    } else {
        s->vars[next].activity = 5;
        s->vars[decisions[0]].activity = 10;
        s->vars[decisions[1]].activity = 5; /* Ties are not retained. */
        s->vars[decisions[2]].activity = 9;
    }
    assert(solver_restart_level(s) == 1);
    assert(s->decision_level == 3 && s->values[next] == UNDEF);
    s->opts.reuse_trail = false;
    assert(solver_restart_level(s) == 0);
    s->opts.reuse_trail = true;
    s->opts.alternating = true;
    assert(solver_restart_level(s) == 0);
    s->opts.alternating = false;
    solver_backtrack(s, 1);
    assert(s->trail_size == 1 && s->values[decisions[0]] != UNDEF);
    while (solver_decide(s)) {
    }
    assert(solver_restart_level(s) == s->decision_level);
    solver_backtrack(s, 0);
    unsigned assigned = 0;

    while (solver_decide(s))
        ++assigned;
    assert(assigned == 4); /* Peeking must not lose a variable from the order. */
    solver_free(s);
}

static void
implications(void)
{
    SolverOpts o = default_opts();

    o.reuse_trail = true;
    o.probing = false;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    assert(dimacs_parse_string(s, "p cnf 5 2\n1 2 0\n1 -2 3 0\n") == DIMACS_OK);
    assert(solver_decide(s) && var(s->trail[0].lit) == 1);
    assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(s->trail_size == 3 && s->qhead == 3);
    CRef reason = s->vars[3].reason;

    assert(reason != INVALID_CLAUSE);
    Lit binary_reason = s->binary_reasons[2];

    assert(binary_reason != LIT_UNDEF);
    assert(solver_decide(s));
    Var dropped = var(s->trail[3].lit), next = dropped == 4 ? 5 : 4;

    s->vars[1].activity = 10;
    s->vars[dropped].activity = 0;
    s->vars[next].activity = 5;
    assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(solver_restart_level(s) == 1);
    solver_backtrack(s, 1);
    assert(s->trail_size == 3 && s->qhead == 3 && s->vars[3].reason == reason);
    assert(s->binary_reasons[2] == binary_reason);
    assert(s->values[2] == TRUE && s->values[3] == TRUE && s->values[dropped] == UNDEF);
    while (solver_decide(s))
        assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(solver_check_model(s));
    s->interrupted = true;
    assert(solver_restart_level(s) == 0);
    solver_free(s);
}

static void
assumptions(void)
{
    SolverOpts o = default_opts();

    o.reuse_trail = true;
    o.probing = false;
    o.luby_restart = true;
    o.luby_unit = 1;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    assert(dimacs_parse_string(s, "p cnf 3 3\n1 2 0\n-1 2 0\n-2 3 0\n") == DIMACS_OK);
    Lit repeated[300];

    for (unsigned i = 0; i < 300; ++i)
        repeated[i] = mkLit(3, false);
    assert(solver_solve_with_assumptions(s, repeated, 300) == TRUE);
    assert(solver_check_model(s) && !s->stats.reused_levels);
    Lit negative = mkLit(3, true);

    assert(solver_solve_with_assumptions(s, &negative, 1) == FALSE);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    solver_free(s);
}

/* Activated pigeonhole modules force real restarts below many dummy assumption
   levels. Conditional UNSAT must leave the permanent formula satisfiable. */
static void
assumption_prefixes(void)
{
    for (unsigned profile = 0; profile < 8; ++profile) {
        SolverOpts o = default_opts();

        o.probing = false;
        o.reuse_learnts = true;
        o.restart_assumptions = true;
        o.reuse_trail = profile & 4;
        o.assumption_lbd = true;
        o.luby_restart = true;
        o.luby_unit = 1;
        o.chrono = profile & 1;
        o.chrono_levels = 0;
        o.vmtf = profile & 2;
        Solver *s = solver_new_with_opts(&o);

        assert(s);
        const unsigned holes = 4, pigeons = 5;
        const Var guard = 21;

        while (s->num_vars < guard)
            assert(solver_new_var(s));
        for (unsigned p = 0; p < pigeons; ++p) {
            Lit c[5];

            c[0] = mkLit(guard, true);
            for (unsigned h = 0; h < holes; ++h)
                c[h + 1] = mkLit(1 + p * holes + h, false);
            assert(solver_add_clause(s, c, 5));
            for (unsigned q = 0; q < p; ++q)
                for (unsigned h = 0; h < holes; ++h) {
                    Lit d[] = {mkLit(guard, true), mkLit(1 + p * holes + h, true),
                               mkLit(1 + q * holes + h, true)};

                    assert(solver_add_clause(s, d, 3));
                }
        }
        Lit repeated[300];

        for (unsigned i = 0; i < 300; ++i)
            repeated[i] = mkLit(guard, false);
        assert(solver_solve_with_assumptions(s, repeated, 300) == FALSE);
        assert(!s->base_unsat && s->stats.restarts && s->stats.reused_levels);
        assert(!s->lbd_assumption_levels);
        uint32_t count = 0;
        const Lit *core = solver_conflict(s, &count);

        assert(core && count == 300);
        for (unsigned i = 0; i < count; ++i)
            assert(core[i] == neg(repeated[i]));
        Lit negative = mkLit(guard, true);

        assert(solver_solve_with_assumptions(s, &negative, 1) == TRUE);
        assert(solver_model_value(s, guard) == FALSE && solver_check_model(s));
        assert(solver_solve(s) == TRUE && solver_check_model(s));
        assert(!s->lbd_assumption_levels);
        /* Adding the previously temporary guard makes the contradiction permanent. */
        solver_add_clause(s, repeated, 1); /* A known root contradiction returns false. */
        assert(solver_solve(s) == FALSE && !s->error);
        solver_free(s);
    }
}

int
main(void)
{
    prefixes(false);
    prefixes(true);
    implications();
    assumptions();
    assumption_prefixes();
    puts("PASS: heap and queue restart prefix boundaries, fallbacks and order recovery");
    return 0;
}
