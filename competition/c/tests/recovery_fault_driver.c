#include "../examples/recoverable_service.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
static size_t calls, fail_at;
static int injected;

static int
reject(void)
{
    if (++calls != fail_at) return 0;
    injected = 1;
    errno = ENOMEM;
    return 1;
}

void *
fault_malloc(size_t n)
{
    return reject() ? NULL : malloc(n);
}

void *
fault_calloc(size_t n, size_t z)
{
    return reject() ? NULL : calloc(n, z);
}

void *
fault_realloc(void *p, size_t n)
{
    return reject() ? NULL : realloc(p, n);
}

static size_t
attempt(size_t cutoff)
{
    recoverable_service s;

    assert(recovery_init(&s, 8192, 65536));
    recovery_set_cancel(&s, NULL, NULL);
    /* Exactly one model (all true), with nontrivial propagation/learning. */
    for (unsigned bits = 0; bits < 63; ++bits) {
        int c[6];

        for (unsigned v = 0; v < 6; ++v)
            c[v] = (bits & (1u << v)) ? -(int)(v + 1) : (int)(v + 1);
        assert(recovery_add(&s, c, 6));
    }
    calls = 0;
    fail_at = cutoff;
    injected = 0;
    int ready = recovery_rebuild(&s), result = BSAT_UNKNOWN;

    if (ready) {
        int unit = 7;

        assert(recovery_add(&s, &unit, 1));
        result = recovery_solve(&s, NULL, 0);
    } else {
        int unit = 7;

        assert(recovery_add(&s, &unit, 1));
    }
    size_t total = calls;

    if (injected)
        assert(result == BSAT_UNKNOWN && !s.worker && !recovery_value(&s, 1));
    else
        assert(result == BSAT_SAT);
    fail_at = 0;
    assert(recovery_rebuild(&s) && recovery_solve(&s, NULL, 0) == BSAT_SAT);
    for (int v = 1; v <= 7; ++v)
        assert(recovery_value(&s, v) == v);
    int a = -1;

    assert(recovery_solve(&s, &a, 1) == BSAT_UNSAT);
    assert(recovery_solve(&s, NULL, 0) == BSAT_SAT); /* Assumptions were not replayed. */
    recovery_destroy(&s);
    return total;
}

int
main(void)
{
    size_t n = attempt(0);

    for (size_t at = 1; at <= n; ++at) {
        attempt(at);
        assert(injected);
    }
    printf("PASS: %zu allocation-failure cutoffs across worker creation, replay, forwarded "
           "additions and solving; all recovered\n",
           n);
}
