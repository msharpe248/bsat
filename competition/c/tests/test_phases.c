#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

static void
default_policy(void)
{
    Solver *s = solver_new();

    assert(s && !s->opts.random_phase);
    assert(solver_new_var(s) == 1);
    s->random_state = 2;
    assert(solver_decide(s) && s->values[1] == FALSE);
    assert(s->random_state == 2); // Default decisions do not draw random phases.
    solver_backtrack(s, 0);
    s->opts.random_phase = true;
    s->opts.random_phase_prob = 1;
    assert(solver_decide(s) && s->values[1] == TRUE);
    assert(s->random_state != 2);
    solver_free(s);
}

static void
random_decisions(bool queue)
{
    SolverOpts o = default_opts();

    o.vmtf = queue;
    o.random_phase = true;
    o.random_phase_prob = 1;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    assert(solver_new_var(s) == 1 && !s->vars[1].polarity);
    /* Seed 2 draws a positive phase, overriding the initial negative phase. */
    s->random_state = 2;
    assert(solver_decide(s) && s->values[1] == TRUE);
    assert(s->vars[1].polarity);
    solver_backtrack(s, 0);
    s->opts.random_phase = false;
    assert(solver_decide(s) && s->values[1] == TRUE);
    solver_backtrack(s, 0);
    s->opts.random_phase = true;
    s->random_state = 1;
    assert(solver_decide(s) && s->values[1] == FALSE);
    assert(!s->vars[1].polarity);
    solver_backtrack(s, 0);
    s->opts.random_phase = false;
    assert(solver_decide(s) && s->values[1] == FALSE);
    assert(s->stats.decisions == 4);
    solver_free(s);
}

static void
target_decisions(bool queue, bool saving)
{
    SolverOpts o = default_opts();

    o.vmtf = queue;
    o.alternating = true;
    o.random_phase = false;
    o.phase_saving = saving;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    assert(solver_new_var(s) == 1 && s->rephase.best_phase);
    s->rephase.best_phase[1] = TRUE;
    s->stable_mode = true;
    assert(solver_decide(s) && s->values[1] == TRUE);
    assert(s->vars[1].polarity == saving);
    assert(s->vars[1].reason == INVALID_CLAUSE && s->binary_reasons[1] == LIT_UNDEF);
    assert(s->trail_size == 1 && s->vars[1].trail_pos == 0 && s->vars[1].level == 1);
    solver_backtrack(s, 0);
    s->stable_mode = false;
    assert(solver_decide(s) && s->values[1] == TRUE);
    assert(s->vars[1].polarity == saving);
    solver_free(s);
}

int
main(void)
{
    default_policy();
    for (unsigned queue = 0; queue < 2; ++queue) {
        random_decisions(queue);
        target_decisions(queue, true);
        target_decisions(queue, false);
    }
    puts("PASS: save decision phases across random overrides, target phases and backtracking");
    return 0;
}
