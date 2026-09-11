/* 256-variable parity chains have exactly four models. Enumerate those models
   independently, filter added clauses/assumptions, and check every SAT output. */
#include "bsat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define N 256

static unsigned
rnd(unsigned *r)
{
    *r ^= *r << 13;
    *r ^= *r >> 17;
    *r ^= *r << 5;
    return *r;
}

static int
satisfied(const unsigned char *m, int lit)
{
    return m[abs(lit)] == (lit > 0);
}

int
main(void)
{
    unsigned cycles = 128;
    const char *env = getenv("BSAT_SOAK_CYCLES");

    if (env) {
        char *end;
        unsigned long n = strtoul(env, &end, 10);

        assert(*env && !*end && n && n <= 100000);
        cycles = (unsigned)n;
    }
    unsigned long sat = 0, unsat = 0;

    for (unsigned cycle = 1; cycle <= cycles; ++cycle) {
        unsigned random = cycle;
        unsigned char models[4][N + 1] = {{0}}, parity[N + 1] = {0};

        for (unsigned v = 3; v <= N; ++v)
            parity[v] = rnd(&random) & 1;
        for (unsigned m = 0; m < 4; ++m) {
            models[m][1] = m & 1;
            models[m][2] = (m >> 1) & 1;
            for (unsigned v = 3; v <= N; ++v)
                models[m][v] = models[m][v - 1] ^ models[m][v - 2] ^ parity[v];
        }
        bsat *s = bsat_create(1, cycle & 1 ? BSAT_REUSE_LEARNTS : 0);

        assert(s);
        for (unsigned v = 3; v <= N; ++v)
            for (unsigned bits = 0; bits < 8; ++bits) {
                if (((bits & 1) ^ ((bits >> 1) & 1) ^ ((bits >> 2) & 1)) == parity[v]) continue;
                int c[3];

                for (unsigned j = 0; j < 3; ++j)
                    c[j] = (bits & (1u << j)) ? -(int)(v - j) : (int)(v - j);
                assert(bsat_add_clause(s, c, 3));
            }
        unsigned active = 15;

        for (unsigned query = 0; query < 128; ++query) {
            if (query % 16 == 0) {
                int c[3];

                for (unsigned j = 0; j < 3; ++j) {
                    unsigned x = rnd(&random);

                    c[j] = (1 + (int)(x % N)) * (x & 256 ? -1 : 1);
                }
                c[0] = abs(c[0]) * (models[0][abs(c[0])] ? 1 : -1); /* Preserve model zero. */
                assert(bsat_add_clause(s, c, 3));
                for (unsigned m = 0; m < 4; ++m)
                    if (!(satisfied(models[m], c[0]) || satisfied(models[m], c[1]) ||
                          satisfied(models[m], c[2])))
                        active &= ~(1u << m);
            }
            int a[4];
            unsigned count = rnd(&random) % 5, possible = active;

            for (unsigned j = 0; j < count; ++j) {
                unsigned x = rnd(&random);

                a[j] = (1 + (int)(x % N)) * (x & 256 ? -1 : 1);
            }
            for (unsigned m = 0; m < 4; ++m)
                for (unsigned j = 0; j < count; ++j)
                    if (!satisfied(models[m], a[j])) possible &= ~(1u << m);
            int result = bsat_solve(s, a, count);

            assert(!bsat_error(s));
            assert(result == (possible ? BSAT_SAT : BSAT_UNSAT));
            if (possible) {
                unsigned match = possible;

                for (unsigned v = 1; v <= N; ++v) {
                    int value = bsat_value(s, (int)v);

                    assert(value);
                    for (unsigned m = 0; m < 4; ++m)
                        if ((value > 0) != models[m][v]) match &= ~(1u << m);
                }
                assert(match);
                ++sat;
            } else
                ++unsat;
        }
        bsat_destroy(s);
    }
    printf("PASS: %u instances, %lu SAT and %lu UNSAT queries, 256 variables, independent "
           "four-model oracle\n",
           cycles, sat, unsat);
}
