#include "bsat.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    int lits[3];
} clause;

static unsigned state;

static unsigned
random_word(void)
{
    state = 1664525u * state + 1013904223u;
    return state;
}

static bool
satisfied(unsigned bits, const int *lits, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) {
        int l = lits[i];
        unsigned v = (unsigned)(l < 0 ? -l : l);

        if (!!(bits & (1u << (v - 1))) == (l > 0)) return true;
    }
    return false;
}

static bool
oracle(const clause *cs, unsigned count, const int *a, unsigned n)
{
    for (unsigned bits = 0; bits < 64; ++bits) {
        bool okay = true;

        for (unsigned i = 0; i < count; ++i)
            okay &= satisfied(bits, cs[i].lits, 3);
        for (unsigned i = 0; i < n; ++i)
            okay &= satisfied(bits, a + i, 1);
        if (okay) return true;
    }
    return false;
}

static int
cancelled(void *state)
{
    return *(int *)state;
}

int
main(void)
{
    assert(!bsat_create(1, BSAT_CERTIFIED_PROBING));
    assert(!bsat_create(1, BSAT_CERTIFIED_PROBING | BSAT_REUSE_LEARNTS));
    unsigned queries = 0;
    const unsigned modes[] = {2, 3, 6, 7};

    for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned trial = 0; trial < 64; ++trial) {
            state = trial + 17;
            bsat *s = bsat_create(1, modes[mode]);

            assert(s);
            int tautology[] = {6, -6};

            assert(bsat_add_clause(s, tautology, 2));
            clause cs[32];
            unsigned count = 0;
            int stop = 0;

            bsat_set_terminate(s, &stop, cancelled);
            for (unsigned q = 0; q < 16; ++q) {
                for (unsigned j = 0; j < 2; ++j) {
                    clause c;

                    for (unsigned k = 0; k < 3; ++k) {
                        unsigned x = random_word();

                        c.lits[k] = (int)(1 + (x >> 8) % 6) * ((x & 1) ? 1 : -1);
                    }
                    /* Keep the permanent formula SAT; conditional queries vary. */
                    if (c.lits[0] < 0 && c.lits[1] < 0 && c.lits[2] < 0) c.lits[0] = -c.lits[0];
                    cs[count++] = c;
                    assert(bsat_add_clause(s, c.lits, 3));
                }
                int a[3];
                unsigned n = q % 4;

                for (unsigned k = 0; k < n; ++k) {
                    unsigned x = random_word();

                    a[k] = (int)(1 + (x >> 8) % 6) * ((x & 1) ? 1 : -1);
                }
                if (q % 5 == 0) {
                    stop = 1;
                    assert(bsat_solve(s, a, n) == BSAT_UNKNOWN && !bsat_error(s));
                    assert(!bsat_value(s, 1));
                    stop = 0;
                }
                if (q == 8) {
                    assert(bsat_checkpoint(s));
                    assert(!bsat_value(s, 1));
                }
                int r = bsat_solve(s, a, n);

                assert(!bsat_error(s));
                assert(r == (oracle(cs, count, a, n) ? 10 : 20));
                ++queries;
                if (r == 10) {
                    unsigned bits = 0;

                    for (int v = 1; v <= 6; ++v)
                        if (bsat_value(s, v) > 0) bits |= 1u << (v - 1);
                    for (unsigned i = 0; i < count; ++i)
                        assert(satisfied(bits, cs[i].lits, 3));
                    for (unsigned i = 0; i < n; ++i)
                        assert(satisfied(bits, a + i, 1));
                } else {
                    int core[3];
                    unsigned nc = 0;

                    for (unsigned i = 0; i < n; ++i)
                        if (bsat_failed(s, a[i])) core[nc++] = a[i];
                    assert(!oracle(cs, count, core, nc));
                }
            }
            bsat_destroy(s);
        }
    for (unsigned flags = 6; flags <= 7; ++flags) {
        bsat *s = bsat_create(1, flags);

        assert(s);
        int a[] = {1, 2}, b[] = {-1, 2};

        assert(bsat_add_clause(s, a, 2) && bsat_add_clause(s, b, 2));
        assert(bsat_set_journal_limit(s, 1));
        assert(bsat_solve(s, NULL, 0) == BSAT_UNKNOWN && bsat_error(s));
        assert(!bsat_value(s, 2) && !bsat_export_query(s, NULL, NULL));
        bsat_destroy(s);
    }
    printf("PASS: %u certified probing/control oracle queries, cancellation/checkpoints, failed "
           "cores and quota failure\n",
           queries);
}
