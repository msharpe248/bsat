#include "bsat.h"
#include "solver.h"
#include <assert.h>
#include <stdio.h>

static int
stopped(void *p)
{
    return *(int *)p;
}

int
main(void)
{
    for (unsigned limit = 1; limit <= 2; ++limit) {
        bsat *s = bsat_create(1, BSAT_CERTIFICATES);

        assert(s);
        assert(bsat_set_journal_limit(s, limit));
        assert(bsat_add_clause(s, NULL, 0));
        assert(bsat_solve(s, NULL, 0) == (limit == 1 ? 0 : 20));
        uint64_t bytes = 99;

        assert(bsat_get_journal_bytes(s, &bytes));
        assert(bytes == (limit == 1 ? 0 : 2));
        assert(bsat_error(s) == (limit == 1));
        if (limit == 1) {
            assert(!bsat_checkpoint(s));
            assert(!bsat_export_query(s, "/unused/a", "/unused/b"));
        } else {
            assert(!bsat_set_journal_limit(s, 1));
            assert(!bsat_error(s));
        }
        bsat_destroy(s);
    }
    bsat *s = bsat_create(1, BSAT_CERTIFICATES | BSAT_REUSE_LEARNTS);

    assert(s);
    int c[] = {1, 2}, d[] = {-1, 2}, a = -2;

    assert(bsat_add_clause(s, c, 2));
    assert(bsat_add_clause(s, d, 2));
    assert(bsat_set_journal_limit(s, 512));
    uint64_t peak = 0;

    for (unsigned i = 0; i < 4096; ++i) {
        assert(bsat_solve(s, &a, 1) == 20);
        assert(bsat_solve(s, NULL, 0) == 10);
        uint64_t bytes;

        assert(bsat_get_journal_bytes(s, &bytes));
        assert(bytes <= 512);
        peak = MAX(peak, bytes);
        if (i % 64 == 63) {
            assert(bsat_checkpoint(s));
            assert(bsat_get_journal_bytes(s, &bytes) && !bytes);
            assert(!bsat_value(s, 2));
            bsat_stats_v1 stats;

            assert(!bsat_get_stats(s, &stats, sizeof stats));
        }
    }
    assert(peak > 0);
    int stop = 1;

    bsat_set_terminate(s, &stop, stopped);
    assert(!bsat_checkpoint(s) && !bsat_error(s));
    stop = 0;
    assert(bsat_checkpoint(s));
    assert(bsat_solve(s, NULL, 0) == 10);
    bsat_destroy(s);
    /* A multi-buffer record is rejected atomically before the first write. */
    SolverOpts o = default_opts();

    o.binary_proof = true;
    Solver *core = solver_new_with_opts(&o);

    assert(core);
    FILE *journal = tmpfile();

    assert(journal);
    core->proof_journal = journal;
    core->journal_limit = 4096;
    Lit large[3000];

    for (unsigned i = 0; i < 3000; ++i)
        large[i] = mkLit(i + 1, false);
    proof_add_clause(core, large, 3000);
    assert(core->error && !core->journal_bytes && ftell(journal) == 0);
    solver_free(core);
    assert(!fclose(journal));
    printf("PASS: 8192 bounded queries, checkpoints/cancellation and atomic record quotas (peak "
           "%llu bytes)\n",
           (unsigned long long)peak);
}
