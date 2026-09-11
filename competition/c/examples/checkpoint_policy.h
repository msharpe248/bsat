/* Example application policy, not part of the public ABI. Observations are
   estimates: an unseen proof burst can still exhaust the hard journal quota. */
#ifndef CHECKPOINT_POLICY_H
#define CHECKPOINT_POLICY_H
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

typedef struct {
    uint64_t quota, floor, peak_burst, previous_bytes, queries;
    double query_cpu, checkpoint_cpu;
} checkpoint_policy;

static inline bool
checkpoint_policy_init(checkpoint_policy *p, uint64_t quota, uint64_t floor)
{
    if (!p || !floor || floor >= quota) return false;
    *p = (checkpoint_policy){.quota = quota, .floor = floor};
    return true;
}

static inline void
checkpoint_policy_observe(checkpoint_policy *p, uint64_t bytes, double cpu)
{
    uint64_t burst = bytes >= p->previous_bytes ? bytes - p->previous_bytes : 0;

    if (burst > p->peak_burst) p->peak_burst = burst;
    p->previous_bytes = bytes;
    ++p->queries;
    if (isfinite(cpu) && cpu > 0) p->query_cpu += cpu;
}

static inline uint64_t
checkpoint_policy_reserve(const checkpoint_policy *p)
{
    uint64_t twice = p->peak_burst > UINT64_MAX / 2 ? UINT64_MAX : 2 * p->peak_burst;

    return twice > p->floor ? twice : p->floor;
}

/* -1: observed burst cannot fit safely; grow quota or rebuild a new handle.
    1: checkpoint before next query. 0: retain learning.
   The soft target is 25% quota, but a measured expensive rebuild is deferred
   until amortized to 1% of observed query CPU. Headroom always overrides cost. */
static inline int
checkpoint_policy_due(const checkpoint_policy *p, uint64_t bytes)
{
    uint64_t reserve = checkpoint_policy_reserve(p);

    if (reserve >= p->quota || bytes > p->quota) return -1;
    if (bytes >= p->quota - reserve) return 1;
    return bytes >= p->quota / 4 && p->queries >= 64 && p->checkpoint_cpu <= p->query_cpu * 0.01;
}

static inline void
checkpoint_policy_completed(checkpoint_policy *p, uint64_t bytes, double cpu)
{
    p->previous_bytes = bytes;
    p->queries = 0;
    p->query_cpu = 0;
    p->checkpoint_cpu = isfinite(cpu) && cpu > 0 ? cpu : 0;
    /* Keep the maximum observed burst across rebuilds and workload changes. */
}
#endif
