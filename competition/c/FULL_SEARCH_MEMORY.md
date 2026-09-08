# Full-search memory and throughput confirmation

This follow-up uses the exact pre/post-memory executables and hashes from
`MEMORY_LIFETIMES.md`, isolating the earlier watch-capacity and allocation-lifetime
changes. It runs the 622,818-variable / 2,552,775-clause instance for 120 seconds
of solver CPU, with a 145-second wall limit and two repetitions per executable.
The explicit profile enables chronology, congruence, equivalence, alternating
search, VMTF, accounting and binary proof output. Timed workloads run serially,
with no concurrent local builds, tests, fuzzing or other solver runs.

A separate diagnosis comparison raises the fixed-work budget from 20,000 to
100,000 conflicts, with binary, text and no proof output and two repetitions.
Status, selected search counters and proof-prefix hashes must match across both
executables in every format. UNKNOWN proof prefixes are trace guards only.

RSS is peak solver-process memory. Owned capacities describe requested retained
storage, not allocator overhead or a whole certificate-checking pipeline.
Accounting timings are intrusive and inclusive. The two-repetition measurements
bound the evidence for these specific observed tradeoffs; they cannot guarantee a memory or
speed improvement for arbitrary inputs and profiles.

All four full-length runs return UNKNOWN without an error. Retained watch
capacity falls from 192.59 MB to 118.33–118.36 MB; total retained capacity falls
from 534.59 MB to 460.88–460.92 MB (about 14%). Congruence temporary capacity
falls from 480.64 to 459.67 MB, and reconstruction overlap from 737.86 to
610.99 MB. Those savings persist through more than 2.29 million conflicts and
1,145 reductions per run, beyond the earlier 1,000-conflict experiment.

Peak RSS is mixed: before 1,064.86 / 1,194.51 MB; after 933.94 / 1,248.48 MB.
The ranges overlap, and the largest after-run exceeds the largest before-run.
Two repetitions on this macOS host do not establish a reliable peak-RSS
reduction or isolate the cause of its variation. Report the deterministic
retained-capacity savings separately; do not promise a fixed RSS percentage.

All 12 longer fixed-work diagnosis runs match result, selected counters and
proof bytes. CPU seconds are:

| Format | Before, two runs | After, two runs |
| --- | --- | --- |
| Binary | 4.400 / 4.653 | 4.401 / 4.499 |
| Text | 4.568 / 4.584 | 4.636 / 4.579 |
| None | 4.093 / 4.064 | 4.055 / 4.085 |

The earlier roughly 10–15% short text-proof regression does not recur at this
longer work budget; the mean text difference is about 0.7%. These runs support
keeping the lower-capacity allocation policy, with no general speed or peak-RSS
claim. Records and frozen protocols are `memory-full*-20260908.json` and
`memory-throughput-long-20260908*`. Actual Linux target-server PMU and RSS
measurements remain outside this local macOS evidence.
