/* Fixed-work comparison of general and tagged arena binary propagation.
   This isolates dispatch cost; it is not a competition solver benchmark. */
#include "../include/solver.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int
main(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "general") && strcmp(argv[1], "tagged"))) {
        fprintf(stderr, "usage: %s general|tagged\n", argv[0]);
        return 2;
    }
    bool tagged = !strcmp(argv[1], "tagged");
    const unsigned variables = 50000, repeats = 1000;
    Solver *s = solver_new();

    if (!s) return 1;
    for (unsigned i = 0; i < variables; ++i)
        if (!solver_new_var(s)) return 1;
    for (Var v = 1; v < variables; ++v) {
        Lit lits[] = {mkLit(v, true), mkLit(v + 1, false)};
        CRef cr = arena_alloc(s->arena, lits, 2, true);

        if (cr == INVALID_CLAUSE) return 1;
        CRef watched = tagged ? arena_binary_watch_ref(cr) : cr;

        watch_add(s->watches, lits[0], watched, lits[1]);
        watch_add(s->watches, lits[1], watched, lits[0]);
    }
    if (s->watches->failed) return 1;
    double start = (double)clock() / CLOCKS_PER_SEC;

    for (unsigned i = 0; i < repeats; ++i) {
        s->decision_level = 1;
        s->trail_lims[1] = 0;
        s->values[1] = TRUE;
        s->vars[1].level = 1;
        s->vars[1].trail_pos = 0;
        s->trail[0] = (Trail){mkLit(1, false)};
        s->trail_size = 1;
        if (solver_propagate(s) != INVALID_CLAUSE || s->error || s->trail_size != variables ||
            s->values[variables] != TRUE)
            return 1;
        solver_backtrack(s, 0);
    }
    double elapsed = (double)clock() / CLOCKS_PER_SEC - start;

    printf("{\"mode\":\"%s\",\"variables\":%u,\"repeats\":%u,"
           "\"cpu_seconds\":%.6f,\"propagations\":%llu,\"work\":%llu}\n",
           argv[1], variables, repeats, elapsed, (unsigned long long)s->stats.propagations,
           (unsigned long long)s->work);
    solver_free(s);
    return 0;
}
