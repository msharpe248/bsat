#include "../include/reduction_sort.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int
cmp(const void *x, const void *y)
{
    const ClauseScore *a = x, *b = y;

    if (a->lbd != b->lbd) return a->lbd < b->lbd ? -1 : 1;
    return (a->activity < b->activity) - (a->activity > b->activity);
}

static uint32_t
random32(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static void
check(const ClauseScore *a, const ClauseScore *original, size_t n)
{
    bool *seen = calloc(n ? n : 1, sizeof *seen);

    assert(seen);
    for (size_t i = 0; i < n; ++i) {
        if (i) assert(cmp(a + i - 1, a + i) <= 0);
        assert(a[i].cref < n && !seen[a[i].cref]);
        seen[a[i].cref] = true;
        assert(a[i].lbd == original[a[i].cref].lbd);
        assert(a[i].activity == original[a[i].cref].activity);
    }
    free(seen);
}

int
main(void)
{
    static const size_t sizes[] = {0,  1,  2,   6,   7,   8,   9,   39,  40,  41,   42,   63,
                                   64, 65, 127, 128, 255, 256, 511, 512, 513, 1023, 4095, 4096};
    uint64_t hash = UINT64_C(1469598103934665603);
    unsigned cases = 0;

    bsat_sort_clause_scores(NULL, 0);
    for (unsigned k = 0; k < sizeof sizes / sizeof *sizes; ++k)
        for (unsigned shape = 0; shape < 8; ++shape) {
            size_t n = sizes[k];
            ClauseScore *a = malloc((n ? n : 1) * sizeof *a),
                        *orig = malloc((n ? n : 1) * sizeof *orig),
                        *b = malloc((n ? n : 1) * sizeof *b);

            assert(a && orig && b);
            uint32_t state = 42;

            for (size_t i = 0; i < n; ++i) {
                uint32_t key = random32(&state);

                if (shape == 1) key = (uint32_t)i;
                if (shape == 2) key = (uint32_t)(n - i);
                if (shape == 3) key = 7;
                if (shape == 4) key = (uint32_t)(i < n / 2 ? i : n - i);
                if (shape == 5) key = (uint32_t)(i % 3);
                if (shape == 6) key = i % 2 ? UINT32_MAX : 0;
                orig[i] = (ClauseScore){(CRef)i, key % 19, (float)(shape == 3 ? 0 : key % 5)};
                if (shape == 7) orig[i].lbd = key;
            }
            memcpy(a, orig, n * sizeof *a);
#ifdef BSAT_CAPTURE_SYSTEM_SORT
            qsort(a, n, sizeof *a, cmp);
#else
            bsat_sort_clause_scores(a, n);
#endif
            check(a, orig, n);
            for (size_t i = 0; i < n; ++i) {
                hash ^= a[i].cref;
                hash *= UINT64_C(1099511628211);
            }
            for (unsigned depth = 0; depth < 2; ++depth) {
                memcpy(a, orig, n * sizeof *a);
                memcpy(b, orig, n * sizeof *b);
                bsat_sort_clause_scores_depth(a, n, depth);
                bsat_sort_clause_scores_depth(b, n, depth);
                check(a, orig, n);
                assert(!memcmp(a, b, n * sizeof *a));
                ++cases;
            }
            free(a);
            free(b);
            free(orig);
        }
    printf("Golden tie-order checksum: %llu\n", (unsigned long long)hash);
#ifndef BSAT_CAPTURE_SYSTEM_SORT
    /* Frozen from the Mac system sort before adopting the portable routine. */
    assert(hash == UINT64_C(12777054334156941331));
#endif
    printf("PASS: %u forced-depth sorting/permutation/determinism cases\n", cases);
    return 0;
}
