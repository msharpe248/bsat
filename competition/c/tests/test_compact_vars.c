#include "../include/solver.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void
growth(bool lrb)
{
    SolverOpts o = default_opts();

    o.lrb = lrb;
    Solver *s = solver_new_with_opts(&o);

    assert(s && !s->lrb_last_conflict);
    for (Var v = 1; v <= 20000; ++v) {
        assert(solver_new_var(s) == v);
        if (lrb) {
            assert(!s->lrb_last_conflict[v]);
            s->lrb_last_conflict[v] = (UINT64_C(1) << 40) + v;
        } else
            assert(!s->lrb_last_conflict);
    }
    if (lrb)
        for (Var v = 1; v <= 20000; ++v)
            assert(s->lrb_last_conflict[v] == (UINT64_C(1) << 40) + v);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(solver_new_var(s) == 20001); // Rebuild must own and initialize its new storage.
    if (lrb)
        for (Var v = 1; v <= 20001; ++v)
            assert(!s->lrb_last_conflict[v]);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    solver_free(s);
}

static void
scoring(bool lrb, bool rescale, uint64_t current)
{
    SolverOpts o = default_opts();

    o.lrb = lrb;
    o.var_inc = rescale ? 1e101 : 1;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (Var v = 1; v <= 3; ++v)
        assert(solver_new_var(s) == v);
    s->decision_level = 1;
    s->stats.conflicts = current;
    double expected[4] = {0, 1, 2, 3}, inc = o.var_inc;
    uint64_t last[] = {0, 0, current, current + 1};

    for (Var v = 1; v <= 3; ++v) {
        s->vars[v].activity = expected[v];
        s->vars[v].level = 1;
        s->values[v] = TRUE;
        s->vars[v].trail_pos = v - 1;
        s->trail[s->trail_size++] = (Trail){mkLit(v, false)};
        if (lrb) s->lrb_last_conflict[v] = last[v];
        if (v > 1) s->binary_reasons[v] = mkLit(1, true);
    }
    // Conflict (-2 | -3), with 1 implying both 2 and 3.
    s->binary_conflict_lits[0] = mkLit(2, true);
    s->binary_conflict_lits[1] = mkLit(3, true);
    Var order[] = {2, 3, 1};

    for (unsigned j = 0; j < 3; ++j) {
        Var v = order[j];
        uint64_t age = current > last[v] ? current - last[v] : 1;

        expected[v] += lrb ? inc / (1 + 0.1 * sqrt((double)age)) : inc;
        if (expected[v] > 1e100) {
            for (Var q = 1; q <= 3; ++q)
                expected[q] *= 1e-100;
            inc *= 1e-100;
        }
    }
    Lit learnt[4];
    uint32_t n;
    Level bt;

    solver_analyze(s, BINARY_CONFLICT, learnt, &n, &bt);
    assert(n == 1 && learnt[0] == mkLit(1, true) && bt == 0);
    for (Var v = 1; v <= 3; ++v) {
        assert(fabs(s->vars[v].activity - expected[v]) <= 1e-14 * fmax(1, expected[v]));
        assert(!s->seen[v]);
        if (lrb) assert(s->lrb_last_conflict[v] == current);
    }
    assert(s->order.var_inc == inc);
    solver_free(s);
}

int
main(void)
{
    assert(sizeof(VarInfo) <= 32);
    growth(false);
    growth(true);
    for (unsigned lrb = 0; lrb < 2; ++lrb)
        for (unsigned scale = 0; scale < 2; ++scale) {
            scoring(lrb, scale, 10);
            scoring(lrb, scale, (UINT64_C(1) << 40) + 7);
        }
    puts("PASS: compact variable growth/rebuild and eight VSIDS/LRB scoring/rescaling cases");
}
