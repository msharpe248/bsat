#include "solver.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    unsigned calls, stop;
} Poll;

static int
cancel(void *p)
{
    Poll *q = p;

    return ++q->calls >= q->stop;
}

typedef struct {
    bool used, cancel;
} OneShot;

static int
delayed(void *state)
{
    OneShot *p = state;

    if (p->used) return 0;
    p->used = true;
    double begin = solver_cpu_time();

    while (solver_cpu_time() - begin < .01) {
    }
    return p->cancel;
}

static void
portfolio_cancel(void)
{
    SolverOpts o = default_opts();

    o.probing = false;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    assert(solver_new_var(s));
    OneShot p = {false, true};

    solver_set_terminate(s, &p, delayed);
    assert(solver_solve_portfolio(s, .001) == UNDEF);
    assert(s->cancelled && s->portfolio_attempts == 1 && !s->error);
    solver_set_terminate(s, NULL, NULL);
    assert(solver_solve(s) == TRUE);
    solver_free(s);
    o.max_time = .001;
    s = solver_new_with_opts(&o);
    assert(s);
    assert(!solver_add_clause(s, NULL, 0));
    p = (OneShot){false, false};
    solver_set_terminate(s, &p, delayed);
    assert(solver_solve(s) == UNDEF && s->interrupted && !s->cancelled);
    solver_free(s);
}

int
main(void)
{
    portfolio_cancel();
    for (unsigned profile = 0; profile < 4; ++profile) {
        SolverOpts o = default_opts();

        o.reuse_learnts = profile & 1;
        o.equiv = profile & 2;
        o.probing = false;
        Solver *s = solver_new_with_opts(&o);

        assert(s);
        for (unsigned v = 0; v < 300; ++v)
            assert(solver_new_var(s));
        for (unsigned v = 2; v <= 300; ++v) {
            Lit a[] = {mkLit(1, true), mkLit(v, false)}, b[] = {mkLit(1, false), mkLit(v, true)};

            assert(solver_add_clause(s, a, 2));
            assert(solver_add_clause(s, b, 2));
        }
        Poll p = {0, 3};

        solver_set_terminate(s, &p, cancel);
        assert(solver_solve(s) == UNDEF && s->interrupted && !s->error);
        p.calls = 0;
        p.stop = 10000000;
        assert(solver_solve(s) == TRUE && solver_check_model(s));
        assert(s->terminate == cancel && s->terminate_state == &p && p.calls > 0);
        p.calls = 0;
        p.stop = 1;
        assert(solver_solve(s) == UNDEF && !s->error);
        solver_set_terminate(s, NULL, NULL);
        assert(solver_solve(s) == TRUE && solver_check_model(s));
        solver_free(s);
    }
    Solver *s = solver_new();

    assert(s);
    assert(!solver_add_clause(s, NULL, 0));
    Poll p = {0, 1};

    solver_set_terminate(s, &p, cancel);
    assert(solver_solve(s) == UNDEF && !s->error);
    solver_set_terminate(s, NULL, NULL);
    assert(solver_solve(s) == FALSE);
    solver_free(s);
    puts("PASS: cancellation through search, rebuild, equivalence replacement and cached UNSAT");
}
