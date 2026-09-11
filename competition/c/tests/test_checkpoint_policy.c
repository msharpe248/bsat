#include "../examples/checkpoint_policy.h"
#include <assert.h>
#include <stdio.h>

int
main(void)
{
    checkpoint_policy p;

    assert(!checkpoint_policy_init(&p, 100, 100));
    assert(checkpoint_policy_init(&p, 10000, 1000));
    for (unsigned i = 0; i < 64; ++i)
        checkpoint_policy_observe(&p, 2500, 0.001);
    assert(checkpoint_policy_due(&p, 2500) == 1);
    checkpoint_policy_completed(&p, 0, 1);
    for (unsigned i = 0; i < 64; ++i)
        checkpoint_policy_observe(&p, 2500, 0.001);
    assert(checkpoint_policy_due(&p, 2500) == 0); /* expensive rebuild is deferred */
    assert(checkpoint_policy_due(&p, 5000) == 1); /* headroom overrides cost */
    checkpoint_policy_observe(&p, 8500, 0.001);
    assert(checkpoint_policy_due(&p, 8500) == -1); /* changed regime's burst too large */
    assert(checkpoint_policy_init(&p, UINT64_MAX, 1));
    checkpoint_policy_observe(&p, UINT64_MAX, 0);
    assert(checkpoint_policy_reserve(&p) == UINT64_MAX && checkpoint_policy_due(&p, 0) == -1);
    puts("PASS: checkpoint cost deferral, quota precedence, changed bursts and overflow");
}
