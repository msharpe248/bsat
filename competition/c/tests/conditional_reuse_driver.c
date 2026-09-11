/* Repeat a structurally UNSAT guarded pigeonhole query and check every base
   model against original input. CPU timing includes construction and all calls. */
#include "solver.h"
#include <assert.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    assert(argc == 2);
    SolverOpts o = default_opts();

    o.probing = false;
    o.reuse_learnts = atoi(argv[1]) != 0;
    double start = solver_cpu_time();
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (unsigned v = 0; v < 73; ++v)
        assert(solver_new_var(s));
    for (unsigned p = 0; p < 9; ++p) {
        Lit c[9];

        c[0] = mkLit(1, true);
        for (unsigned h = 0; h < 8; ++h)
            c[h + 1] = mkLit(2 + p * 8 + h, false);
        assert(solver_add_clause(s, c, 9));
    }
    for (unsigned h = 0; h < 8; ++h)
        for (unsigned p = 0; p < 9; ++p)
            for (unsigned q = p + 1; q < 9; ++q) {
                Lit c[] = {mkLit(1, true), mkLit(2 + p * 8 + h, true), mkLit(2 + q * 8 + h, true)};

                assert(solver_add_clause(s, c, 3));
            }
    uint64_t conflicts = 0;
    Lit active = mkLit(1, false);

    for (unsigned i = 0; i < 10; ++i) {
        assert(solver_solve_with_assumptions(s, &active, 1) == FALSE && !s->error);
        conflicts += s->stats.conflicts;
        assert(solver_solve(s) == TRUE && !s->error);
        conflicts += s->stats.conflicts;
        bool satisfied = false;

        for (size_t j = 0; j < s->input_size; ++j) {
            Lit l = s->input[j];

            if (!l) {
                assert(satisfied);
                satisfied = false;
            } else
                satisfied |= (solver_model_value(s, var(l)) == TRUE) != sign(l);
        }
    }
    printf("queries=20 conflicts=%llu reused=%llu cpu=%.9f\n", (unsigned long long)conflicts,
           (unsigned long long)s->reused_solves, solver_cpu_time() - start);
    solver_free(s);
}
