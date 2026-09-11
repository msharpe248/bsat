/* Isolate vivification on redundant input-clause copies. Generated inputs have
 * an all-positive model, which the benchmark independently checks. */
#include "../include/solver.h"
#include "../include/dimacs.h"
#include <stdio.h>
#include <time.h>

int
main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) return 2;
    bool unchanged = argc == 4;

    if (unchanged && strcmp(argv[3], "unchanged")) return 2;
    char *end;
    unsigned long runs = strtoul(argv[2], &end, 10);

    if (!*argv[2] || *end || !runs || runs > 10000) return 2;
    double cpu = 0;
    uint64_t work = 0, removed = 0, remaining = 0;
    Solver *s = NULL;

    for (unsigned long round = 0; round < runs; ++round) {
        solver_free(s);
        s = solver_new();
        if (!s) return 2;
        if (dimacs_parse_file(s, argv[1]) != DIMACS_OK) return 2;
        s->opts.inprocess = true;
        s->opts.inprocess_interval = 1;
        s->opts.preprocess_budget = 1000000000;
        s->learnts = malloc(100 * sizeof *s->learnts);
        if (!s->learnts) return 2;
        s->learnts_size = 100;
        Lit originals[100][64];
        unsigned lengths[100];

        for (unsigned i = 0; i < s->num_clauses && s->num_learnts < 100; ++i) {
            CRef cr = s->clauses[i];
            unsigned n = CLAUSE_SIZE(s->arena, cr);

            if (n < 4 || n > 64) continue;
            Lit lits[64];

            memcpy(lits, CLAUSE_LITS(s->arena, cr), n * sizeof *lits);
            CRef copy = arena_alloc(s->arena, lits, n, true);

            if (copy == INVALID_CLAUSE) return 2;
            if (unchanged) {
                lengths[s->num_learnts] = n;
                memcpy(originals[s->num_learnts], lits, n * sizeof *lits);
            }
            s->learnts[s->num_learnts++] = copy;
            watch_add(s->watches, lits[0], copy, lits[1]);
            watch_add(s->watches, lits[1], copy, lits[0]);
        }
        if (s->num_learnts != 100 || solver_propagate(s) != INVALID_CLAUSE) return 2;
        s->stats.conflicts = 1;
        uint64_t before = s->work;
        double start = (double)clock() / CLOCKS_PER_SEC;

        if (!solver_simplify(s) || s->error || s->interrupted) return 2;
        cpu += (double)clock() / CLOCKS_PER_SEC - start;
        work += s->work - before;
        removed += s->stats.minimized_literals;
        for (unsigned i = 0; i < s->num_learnts; ++i) {
            CRef cr = s->learnts[i];
            unsigned n = CLAUSE_SIZE(s->arena, cr);
            bool first = false, second = false;

            for (unsigned j = 0; j < n; ++j) {
                Lit lit = CLAUSE_LITS(s->arena, cr)[j];

                first |= lit == mkLit(1, false);
                second |= lit == mkLit(2, false);
            }
            /* The original formula entails (1 or 2), so retaining both is an
               independent entailment check for this generated family. Report
               residual size rather than assuming equal strengthening power. */
            if (unchanged) {
                if (n != lengths[i]) return 2;
                for (unsigned j = 0; j < n; ++j) {
                    Lit lit = CLAUSE_LITS(s->arena, cr)[j];
                    bool found = false;

                    for (unsigned k = 0; k < n; ++k)
                        found |= lit == originals[i][k];
                    if (!found) return 2;
                    for (unsigned k = 0; k < j; ++k)
                        if (lit == CLAUSE_LITS(s->arena, cr)[k]) return 2;
                }
            } else if (!first || !second)
                return 2;
            remaining += n;
        }
    }
    for (Var v = 1; v <= s->num_vars; ++v)
        s->values[v] = TRUE;
    if (!solver_check_model(s)) return 2;
    printf("c Vivification CPU: %.9f\nc Vivification work: %llu\nc Removed literals: %llu\nc "
           "Remaining literals: %llu\n",
           cpu, (unsigned long long)work, (unsigned long long)removed,
           (unsigned long long)remaining);
    puts("s SATISFIABLE");
    printf("v");
    for (Var v = 1; v <= s->num_vars; ++v)
        printf(" %u", v);
    puts(" 0");
    solver_free(s);
    return 10;
}
