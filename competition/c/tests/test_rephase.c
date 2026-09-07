#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const bool initial[] = {false, false, true, true, false};
    unsigned cases = 0;
    for (unsigned queue = 0; queue < 2; ++queue)
    for (unsigned stable = 0; stable < 2; ++stable)
    for (unsigned enabled = 0; enabled < 2; ++enabled)
    for (unsigned trigger = 0; trigger < 2; ++trigger) {
        SolverOpts o = default_opts();o.probing = false;o.vmtf = queue;
        o.alternating = stable;o.rephase = enabled;o.rephase_interval = 1;o.max_decisions = 1;
        Solver *s = solver_new_with_opts(&o);assert(s);
        for (Var v = 1; v <= 4; ++v) {
            assert(solver_new_var(s) == v);s->vars[v].polarity = initial[v];
        }
        if (enabled) {
            s->rephase.best_phase[1] = TRUE;s->rephase.best_phase[2] = FALSE;
            s->rephase.best_trail_size = 2;s->rephase.best_prefix_valid = false;
        } else assert(!s->rephase.best_phase);
        s->stable_mode = stable;s->stats.conflicts = trigger;
        assert(solver_solve(s) == UNDEF && !s->error && s->trail_size == 1);
        assert(s->rephase.rephase_count == (enabled && trigger));
        Lit lit = s->trail[0].lit;Var chosen = var(lit);
        for (Var v = 1; v <= 4; ++v) {
            // Only known target entries replace saved polarities. Stable-mode
            // decisions can also select a known target without a rephase event.
            bool use_target = enabled && v <= 2 && (trigger || (stable && v == chosen));
            bool expected = use_target ? v == 1 : initial[v];
            assert(s->vars[v].polarity == expected);
            if (v == chosen) assert(s->values[v] == (expected ? TRUE : FALSE));
            else assert(s->values[v] == UNDEF);
        }
        assert(s->vars[chosen].reason == INVALID_CLAUSE);
        solver_free(s);++cases;
    }
    assert(cases == 16);
    puts("PASS: 16 partial-target rephasing, disabled, boundary and stable/heap/queue cases");
}
