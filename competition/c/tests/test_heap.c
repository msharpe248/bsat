#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void growth_and_order(void) {
    Solver *s = solver_new();assert(s);
    s->opts.random_phase = false;
    for (Var v = 1; v <= 8; ++v) {
        assert(solver_new_var(s) == v && s->vars[v].activity == 0);
    }
    // Initial heap has ascending IDs. Descending scores preserve its invariant.
    for (Var v = 1; v <= 8; ++v) s->vars[v].activity = 1000 - v;
    uint32_t initial = s->var_capacity;
    for (Var v = 9; v <= 4 * initial + 1; ++v) {
        assert(solver_new_var(s) == v && s->vars[v].activity == 0);
        for (Var old = 1; old <= 8; ++old) assert(s->vars[old].activity == 1000 - old);
    }
    assert(s->var_capacity > 4 * initial);
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        for (Var expected = 1; expected <= 8; ++expected) {
            assert(solver_decide(s));
            assert(var(s->trail[s->trail_size-1].lit) == expected);
        }
        solver_backtrack(s, 0);
    }
    solver_free(s);
}

static void rescale_and_rebuild(void) {
    Solver *s = solver_new();assert(s);
    s->opts.phase_saving = false;s->opts.random_phase = false;s->opts.probing = false;
    assert(dimacs_parse_string(s, "p cnf 4 2\n-1 2 0\n-1 -2 0\n") == DIMACS_OK);
    s->vars[1].activity = 1e100;s->vars[2].activity = 5e99;s->order.var_inc = 1e100;
    assert(solver_decide(s) && s->trail[0].lit == mkLit(1, false));
    CRef conflict = solver_propagate(s);assert(conflict == BINARY_CONFLICT);
    Lit learned[4];uint32_t n;Level backjump;
    solver_analyze(s, conflict, learned, &n, &backjump);
    assert(n == 1 && learned[0] == mkLit(1, true) && backjump == 0);
    assert(fabs(s->vars[1].activity - 2) < 1e-12);
    assert(fabs(s->vars[2].activity - 1.5) < 1e-12);
    assert(fabs(s->order.var_inc - 1) < 1e-12);
    solver_backtrack(s, 0);
    assert(solver_decide(s) && var(s->trail[0].lit) == 1);
    solver_backtrack(s, 0);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    // New variables after solving rebuild the original problem and its scores.
    assert(solver_new_var(s) == 5);
    for (Var v = 1; v <= 5; ++v) assert(s->vars[v].activity == 0);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    solver_free(s);
}

int main(void) {
    growth_and_order();rescale_and_rebuild();
    puts("PASS: activity growth, heap ordering, backtracking, rescaling and API rebuild");
    return 0;
}
