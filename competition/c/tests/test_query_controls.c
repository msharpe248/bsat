#include "bsat.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int
stop(void *p)
{
    return *(int *)p;
}

static void
search_policy_sessions(void)
{
    for (uint32_t flags = 8; flags < 64; ++flags) {
        bsat *s = bsat_create(1, flags);

        if (flags != 11 && flags != 15 && flags != 19 && flags != 23 && flags != 27 &&
            flags != 31) {
            assert(!s);
            continue;
        }
        assert(s);
        int a[] = {1, 2}, b[] = {-1, 2}, no = -2;

        assert(bsat_add_clause(s, a, 2) && bsat_add_clause(s, b, 2));
        assert(bsat_solve(s, &no, 1) == BSAT_UNSAT && bsat_failed(s, no));
        assert(bsat_solve(s, NULL, 0) == BSAT_SAT && bsat_value(s, 2) > 0);
        assert(bsat_checkpoint(s));
        int cancel = 1;

        bsat_set_terminate(s, &cancel, stop);
        assert(bsat_solve(s, NULL, 0) == BSAT_UNKNOWN);
        cancel = 0;
        assert(bsat_solve(s, NULL, 0) == BSAT_SAT && bsat_value(s, 2) > 0);
        assert(bsat_add_clause(s, &no, 1));
        assert(bsat_solve(s, NULL, 0) == BSAT_UNSAT && !bsat_error(s));
        bsat_destroy(s);
    }
}

int
main(void)
{
    search_policy_sessions();
    bsat *s = bsat_create(1, BSAT_REUSE_LEARNTS);

    assert(s);
    bsat_stats_v1 a, b;

    assert(!bsat_get_stats(s, &a, sizeof a));
    for (int i = 0; i < 7; ++i) {
        int row[6];

        for (int j = 0; j < 6; ++j)
            row[j] = i * 6 + j + 1;
        assert(bsat_add_clause(s, row, 6));
        for (int k = 0; k < i; ++k)
            for (int j = 0; j < 6; ++j) {
                int c[] = {-(i * 6 + j + 1), -(k * 6 + j + 1)};

                assert(bsat_add_clause(s, c, 2));
            }
    }
    assert(bsat_set_query_limits(s, 0, 1, 0));
    assert(bsat_solve(s, NULL, 0) == BSAT_UNKNOWN);
    assert(bsat_get_stats(s, &a, sizeof a));
    assert(a.version == 1 && a.result == 0 && a.conflicts >= 1 && a.cpu_seconds >= 0 &&
           a.owned_capacity_bytes);
    memset(&b, 0x5a, sizeof b);
    bsat_stats_v1 unchanged = b;

    assert(!bsat_get_stats(s, &b, sizeof b - 1));
    assert(!memcmp(&b, &unchanged, sizeof b));
    assert(bsat_set_query_limits(s, 0, 0, 0));
    assert(bsat_solve(s, NULL, 0) == BSAT_UNSAT);
    assert(bsat_get_stats(s, &a, sizeof a));
    assert(a.result == 20 && a.conflicts > 1);
    assert(bsat_get_stats(s, &b, sizeof b));
    assert(!memcmp(&a, &b, sizeof a));
    bsat_destroy(s);
    s = bsat_create(1, BSAT_REUSE_LEARNTS);
    int c[] = {1, 2};

    assert(bsat_add_clause(s, c, 2));
    assert(bsat_solve(s, NULL, 0) == 10);
    int v = bsat_value(s, 1);

    assert(bsat_set_query_limits(s, 0, 100, 100));
    assert(bsat_value(s, 1) == v);
    assert(bsat_solve(s, NULL, 0) == 10);
    assert(bsat_get_stats(s, &a, sizeof a));
    assert(a.reused_preparations > 0);
    int cancel = 1;

    bsat_set_terminate(s, &cancel, stop);
    assert(bsat_solve(s, NULL, 0) == 0);
    cancel = 0;
    assert(bsat_solve(s, NULL, 0) == 10);
    assert(!bsat_set_query_limits(s, NAN, 0, 0));
    assert(bsat_error(s));
    bsat_destroy(s);
    puts("PASS: mutable query limits and stable per-query statistics");
}
