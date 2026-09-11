/* Deliberately includes only the stable public header. Linked dynamically. */
#include "bsat.h"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>
static volatile sig_atomic_t signals;

static void
app_signal(int sig)
{
    (void)sig;
    ++signals;
}

static void *
queries(void *arg)
{
    int polarity = *(int *)arg;

    for (unsigned cycle = 0; cycle < 1000; ++cycle) {
        bsat *s = bsat_create(BSAT_ABI_VERSION, cycle & 1 ? BSAT_REUSE_LEARNTS : 0);

        assert(s);
        for (int v = 1; v <= 32; ++v) {
            int c = polarity * v;

            assert(bsat_add_clause(s, &c, 1));
        }
        assert(bsat_solve(s, NULL, 0) == BSAT_SAT && bsat_value(s, 32) == polarity * 32);
        int a = -polarity * 17;

        assert(bsat_solve(s, &a, 1) == BSAT_UNSAT && bsat_failed(s, a));
        assert(!bsat_value(s, 32));
        assert(bsat_solve(s, NULL, 0) == BSAT_SAT);
        assert(!bsat_failed(s, a));
        bsat_destroy(s);
    }
    return NULL;
}

typedef struct {
    atomic_int entered, cancel;
    bsat *solver;
} Cancel;

static int
terminate(void *p)
{
    Cancel *c = p;

    atomic_store(&c->entered, 1);
    while (!atomic_load(&c->cancel))
        sched_yield();
    return 1;
}

static void *
cancelled(void *p)
{
    Cancel *c = p;

    assert(bsat_solve(c->solver, NULL, 0) == BSAT_UNKNOWN);
    return NULL;
}

static int
wait_for_other_cpu(void *p)
{
    Cancel *c = p;

    atomic_store(&c->entered, 1);
    struct timespec pause = {0, 1000000};

    while (!atomic_load(&c->cancel))
        nanosleep(&pause, NULL);
    return 0;
}

static void *
limited(void *p)
{
    Cancel *c = p;

    assert(bsat_solve(c->solver, NULL, 0) == BSAT_SAT);
    return NULL;
}

int
main(void)
{
    assert(bsat_abi_version() == 1 && !bsat_create(0, 0) && !bsat_create(1, 128));
    struct sigaction sa = {0}, old, after;

    sa.sa_handler = app_signal;
    sigemptyset(&sa.sa_mask);
    assert(!sigaction(SIGUSR1, &sa, &old));
    pthread_t threads[4];
    int polarity[] = {1, -1, 1, -1};

    for (unsigned i = 0; i < 4; ++i)
        assert(!pthread_create(&threads[i], NULL, queries, &polarity[i]));
    for (unsigned i = 0; i < 4; ++i)
        assert(!pthread_join(threads[i], NULL));
    assert(!sigaction(SIGUSR1, NULL, &after) && after.sa_handler == app_signal);
    raise(SIGUSR1);
    assert(signals == 1);
    assert(!sigaction(SIGUSR1, &old, NULL));
    Cancel c = {0};

    c.solver = bsat_create(1, BSAT_REUSE_LEARNTS);
    assert(c.solver);
    int clause[] = {1, 2};

    assert(bsat_add_clause(c.solver, clause, 2));
    bsat_set_terminate(c.solver, &c, terminate);
    assert(!pthread_create(&threads[0], NULL, cancelled, &c));
    while (!atomic_load(&c.entered))
        sched_yield();
    atomic_store(&c.cancel, 1);
    assert(!pthread_join(threads[0], NULL));
    assert(!bsat_error(c.solver));
    bsat_set_terminate(c.solver, NULL, NULL);
    assert(bsat_solve(c.solver, NULL, 0) == BSAT_SAT);
    bsat_destroy(c.solver);
    atomic_store(&c.entered, 0);
    atomic_store(&c.cancel, 0);
    c.solver = bsat_create(1, 0);
    assert(c.solver);
    assert(bsat_set_limits(c.solver, .05, 0, 0));
    assert(bsat_add_clause(c.solver, clause, 2));
    bsat_set_terminate(c.solver, &c, wait_for_other_cpu);
    assert(!pthread_create(&threads[0], NULL, limited, &c));
    while (!atomic_load(&c.entered))
        sched_yield();
    clock_t burn = clock();

    while ((double)(clock() - burn) / CLOCKS_PER_SEC < .15) {
    }
    atomic_store(&c.cancel, 1);
    assert(!pthread_join(threads[0], NULL));
    bsat_destroy(c.solver);
    bsat *s = bsat_create(1, 0);

    assert(s);
    int bad = INT_MIN;

    assert(!bsat_add_clause(s, &bad, 1) && bsat_error(s));
    assert(bsat_solve(s, NULL, 0) == BSAT_UNKNOWN);
    bsat_destroy(s);
    s = bsat_create(1, 0);
    assert(s);
    assert(bsat_add_clause(s, NULL, 0));
    assert(bsat_solve(s, NULL, 0) == BSAT_UNSAT);
    bsat_destroy(s);
    puts("PASS: opaque shared ABI, 12000 queries across four threads, signals, atomic "
         "cancellation/retry, invalid input");
}
