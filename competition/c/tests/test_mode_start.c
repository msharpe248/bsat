#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

static void schedule(bool queue, bool average) {
    SolverOpts o = default_opts();
    o.alternating = true;o.vmtf = queue;o.glucose_use_ema = !average;
    Solver *s = solver_new_with_opts(&o);assert(s);
    assert(!s->stable_mode && s->next_mode_ticks == 1000000);
    assert(!solver_should_restart(s));
    for (unsigned step = 0; step < 6; ++step) {
        bool before = s->stable_mode;
        uint64_t boundary = s->next_mode_ticks;
        s->work = boundary - 1;s->restart.conflicts_since = 0;
        assert(!solver_should_restart(s) && s->stable_mode == before);
        s->work = boundary;s->restart.conflicts_since = 17;
        assert(solver_should_restart(s) && s->stable_mode != before);
        assert(s->next_mode_ticks > boundary && !s->restart.conflicts_since);
        assert(!solver_should_restart(s));
    }
    solver_free(s);
}

static void phases(bool queue) {
    SolverOpts o = default_opts();o.alternating = true;o.vmtf = queue;
    Solver *s = solver_new_with_opts(&o);assert(s);
    assert(solver_new_var(s) == 1);s->rephase.best_phase[1] = TRUE;
    assert(!solver_should_restart(s));
    assert(solver_decide(s) && s->values[1] == FALSE);
    solver_backtrack(s, 0);s->work = s->next_mode_ticks;
    assert(solver_should_restart(s));
    assert(solver_decide(s) && s->values[1] == TRUE);
    solver_free(s);
}

static void disabled(void) {
    SolverOpts o = default_opts();o.alternating = false;
    Solver *s = solver_new_with_opts(&o);assert(s);
    s->work = s->next_mode_ticks;
    assert(!solver_should_restart(s) && !s->stable_mode);
    solver_free(s);
    o.alternating = true;o.restart_first = UINT32_MAX;
    s = solver_new_with_opts(&o);assert(s);
    s->work = s->next_mode_ticks;
    assert(!solver_should_restart(s) && !s->stable_mode);
    solver_free(s);
}

int main(void) {
    for (unsigned q = 0; q < 2; ++q) {
        for (unsigned a = 0; a < 2; ++a) schedule(q, a);
        phases(q);
    }
    disabled();
    puts("PASS: 32 alternating boundaries, focused/target phases and disabled policies");
}
