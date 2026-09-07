#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

static void clean(Solver *s) {
    for (Var v = 1; v <= s->num_vars; ++v) assert(!s->seen[v]);
}
static unsigned clause(unsigned mask, bool side, Lit *out) {
    unsigned n = 0;
    for (Var v = 2; v <= 5; ++v, mask /= 3)
        if (mask % 3) out[n++] = mkLit(v, mask % 3 == 2);
    out[n++] = mkLit(1, side); // Pivot at the end, rather than assuming a prefix.
    return n;
}
static void pairs(void) {
    Solver *s = solver_new();assert(s);
    for (unsigned i = 0; i < 5; ++i) assert(solver_new_var(s));
    for (unsigned a = 0; a < 81; ++a) for (unsigned b = 0; b < 81; ++b) {
        Lit x[5], y[5];unsigned nx = clause(a, false, x), ny = clause(b, true, y);
        bool expected = elim_is_tautology(x, nx, y, ny, 1);
        assert(elim_bounded_tautology(s, x, nx, y, ny, 1) == expected);clean(s);
        // All small cutoffs must preserve scratch, and may never assert a
        // tautology unless the independent quadratic predicate agrees.
        for (unsigned budget = 0; budget <= 16; ++budget) {
            s->work = 1;s->work_limit = 1 + budget;
            bool actual = elim_bounded_tautology(s, x, nx, y, ny, 1);
            assert(!actual || expected);clean(s);
            if (s->work < s->work_limit) assert(actual == expected);
        }
        s->work_limit = 0;
    }
    solver_free(s);
}
static void integration(void) {
    const unsigned n = 1000;
    SolverOpts o = default_opts();o.probing = false;o.bce = true;o.preprocess_budget = 10000;
    Solver *s = solver_new_with_opts(&o);assert(s);
    Lit *lits = malloc(n * sizeof *lits);assert(lits);
    for (unsigned i = 0; i < n; ++i) { assert(solver_new_var(s));lits[i] = mkLit(i + 1, false); }
    assert(solver_add_clause(s, lits, n));lits[0] = neg(lits[0]);
    assert(solver_add_clause(s, lits, n));free(lits);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(s->stats.blocked_clauses == 2);clean(s);solver_free(s);
}
int main(void) {
    pairs();integration();
    puts("PASS: 6561 canonical pairs, 111537 budget cutoffs, scratch cleanup and bounded BCE model reconstruction");
}
