/* Repeated identical, satisfiable API query sequences. The first model fixes
   each later assumption; benchmark.py independently checks the final model. */
#include "solver.h"
#include "dimacs.h"
#include <stdio.h>

int
main(int argc, char **argv)
{
    if (argc != 4) return 2;
    char *end;
    unsigned long count = strtoul(argv[3], &end, 10);

    if (!*argv[3] || *end || !count || count > 10000) return 2;
    if (strcmp(argv[2], "0") && strcmp(argv[2], "1")) return 2;
    SolverOpts o = default_opts();

    o.reuse_learnts = argv[2][0] == '1';
    Solver *s = solver_new_with_opts(&o);

    if (!s) return 2;
    if (dimacs_parse_file(s, argv[1]) != DIMACS_OK) return 2;
    double begin = (double)clock() / CLOCKS_PER_SEC;

    if (solver_solve(s) != TRUE) return 2;
    double first = (double)clock() / CLOCKS_PER_SEC - begin;
    uint8_t *model = malloc((size_t)s->num_vars + 1);

    if (!model) return 2;
    printf("c Initial model: ");
    for (Var v = 1; v <= s->num_vars; ++v) {
        model[v] = solver_model_value(s, v);
        putchar(model[v] == TRUE ? '1' : '0');
    }
    putchar('\n');
    fflush(stdout); /* Preserve query guard even on timeout. */
    uint64_t conflicts = 0;

    begin = (double)clock() / CLOCKS_PER_SEC;
    for (unsigned long i = 0; i < count; ++i) {
        Var v = s->num_vars ? 1 + i % s->num_vars : 0;
        Lit a = v ? mkLit(v, model[v] != TRUE) : 0;
        lbool result = solver_solve_with_assumptions(s, &a, v && (i & 1));

        if (result != TRUE || s->error) return 2;
        conflicts += s->stats.conflicts;
    }
    double repeated = (double)clock() / CLOCKS_PER_SEC - begin;

    printf("c Initial solve CPU: %.9f\nc Repeated API CPU: %.9f\nc Repeated conflicts: %llu\nc "
           "Reused preparations: %llu\nc API repetitions: %lu\n",
           first, repeated, (unsigned long long)conflicts, (unsigned long long)s->reused_solves,
           count);
    puts("s SATISFIABLE");
    printf("v");
    for (Var v = 1; v <= s->num_vars; ++v)
        printf(" %d", solver_model_value(s, v) == TRUE ? (int)v : -(int)v);
    puts(" 0");
    free(model);
    solver_free(s);
    return 10;
}
