#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

int
main(void)
{
    const double valid[] = {DBL_MIN, 0.5, 0.999, 1};

    for (unsigned i = 0; i < sizeof valid / sizeof *valid; ++i) {
        SolverOpts o = default_opts();

        o.clause_decay = valid[i];
        Solver *s = solver_new_with_opts(&o);

        assert(s);
        assert(dimacs_parse_string(s, "p cnf 2 2\n1 2 0\n-1 2 0\n") == DIMACS_OK);
        assert(solver_solve(s) == TRUE && solver_check_model(s));
        assert(solver_solve(s) == TRUE && solver_check_model(s));
        solver_free(s);
    }
    const double invalid[] = {0, -DBL_MIN, -1, 1.01, DBL_MAX, INFINITY, -INFINITY, NAN};

    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        SolverOpts o = default_opts();

        o.clause_decay = invalid[i];
        assert(!solver_new_with_opts(&o));
    }
    puts("PASS: finite clause-decay bounds, valid solver construction and repeated solves");
    return 0;
}
