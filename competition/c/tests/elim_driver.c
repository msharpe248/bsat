/* Isolated single-pivot elimination benchmark; model output is independently
   checked by benchmark.py. Internal CPU excludes parsing/occurrence building. */
#include "../include/solver.h"
#include "../include/dimacs.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main(int argc, char **argv)
{
    if (argc != 3) return 2;
    char *end;
    unsigned long runs = strtoul(argv[2], &end, 10);

    if (!*argv[2] || *end || !runs || runs > 10000) return 2;
    double cpu = 0;
    uint64_t work = 0, eliminated = 0;
    Solver *s = NULL;

    for (unsigned long i = 0; i < runs; ++i) {
        solver_free(s);
        s = solver_new();
        if (!s) return 2;
        if (dimacs_parse_file(s, argv[1]) != DIMACS_OK || !s->num_vars) {
            solver_free(s);
            return 2;
        }
        elim_build_occs(s);
        if (s->error || !s->elim->occs_complete) {
            solver_free(s);
            return 2;
        }
        uint64_t before = s->work;
        double start = (double)clock() / CLOCKS_PER_SEC;
        bool ok = elim_eliminate_var(s, 1);

        cpu += (double)clock() / CLOCKS_PER_SEC - start;
        work += s->work - before;
        eliminated += ok;
        if (!ok || s->error) {
            solver_free(s);
            return 2;
        }
    }
    uint64_t words = 0;

    for (uint32_t i = 0; i < s->elim->stack_size; ++i)
        words += s->elim->stack[i].clause_size;
    printf("c Reconstruction words: %llu\n", (unsigned long long)words);
    elim_extend_model(s);
    bool valid = solver_check_model(s);

    printf("c Elimination CPU: %.9f\nc Elimination work: %llu\nc Eliminated: %llu\n", cpu,
           (unsigned long long)work, (unsigned long long)eliminated);
    if (valid) {
        puts("s SATISFIABLE");
        printf("v");
        for (Var v = 1; v <= s->num_vars; ++v)
            printf(" %d", s->values[v] == TRUE ? (int)v : -(int)v);
        puts(" 0");
    } else
        puts("s UNKNOWN");
    solver_free(s);
    return valid ? 10 : 0;
}
