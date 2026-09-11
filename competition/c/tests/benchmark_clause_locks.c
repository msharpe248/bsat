/* Reduction throughput with equal-ranked, unlocked clauses. Setup is untimed. */
#include "../include/solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main(int argc, char **argv)
{
    unsigned length = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 128;

    enum { COUNT = 10000, REPEATS = 100 };

    if (length < 3 || length > 4096) return 1;
    SolverOpts o = default_opts();

    o.reduce_fraction = 1;
    o.glue_lbd = 0;
    o.max_lbd = UINT32_MAX;
    Solver *s = solver_new_with_opts(&o);

    if (!s) return 1;
    Lit *lits = malloc(length * sizeof *lits);

    if (!lits) return 1;
    for (Var v = 1; v <= length; ++v) {
        if (!solver_new_var(s)) return 1;
        lits[v - 1] = mkLit(v, false);
    }
    s->learnts = malloc(COUNT * sizeof *s->learnts);
    if (!s->learnts) return 1;
    s->learnts_size = COUNT;
    for (unsigned i = 0; i < COUNT; ++i) {
        CRef cr = arena_alloc(s->arena, lits, length, true);

        if (cr == INVALID_CLAUSE) return 1;
        set_clause_lbd(s->arena, cr, 3);
        s->learnts[s->num_learnts++] = cr;
    }
    double start = (double)clock() / CLOCKS_PER_SEC;

    for (unsigned i = 0; i < REPEATS; ++i)
        solver_reduce_db(s);
    double elapsed = (double)clock() / CLOCKS_PER_SEC - start;

    if (s->num_learnts != COUNT || s->stats.deleted_clauses || s->garbage_collections) return 1;
    printf("{\"length\":%u,\"clauses\":%u,\"reductions\":%u,\"cpu_seconds\":%.6f}\n", length, COUNT,
           REPEATS, elapsed);
    free(lits);
    solver_free(s);
    return 0;
}
