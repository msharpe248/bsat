# cal3 retained-query investigation — 2026-09-08

Historical measurements below predate portable reduction. See the
[updated 60-second incremental investigation](CAL3_INCREMENTAL_FOLLOWUP.md).

The depth-16 positive-output query remains unresolved by BSAT. Equal CPU budgets
confirm a real gap: retained CaDiCaL proves the query within a second, while BSAT
does not finish in 30 seconds. The tested existing search switches do not close it.
No production default changes are justified by this investigation.

## Method and acceptance

The pinned circuit, encoding and depths are unchanged from
[industrial histories](INDUSTRIAL_CIRCUIT_HISTORIES.md). Permanent clauses grow
through frames 0,1,2,4,8,16, with positive/negative output assumptions at each step.
The comparison gives each retained solver five solving-thread CPU seconds per
query; the longer control gives each 30. CaDiCaL's Python termination callback
also has a 60/90-second wall fuse. Its callback/FFI cost is included in measured
query CPU. Fresh Kissat and certificate checking remain validation-only stages,
outside measured queries, with their previously documented limits.

Actual conclusive answers are validated even if observed late, but
`within_cpu_budget` and `reference_within_cpu_budget` never award a timed solve
whose measured CPU exceeds its allowance. All runs are serial without local
build/test contention. These are development diagnostics, not competition scores.

The separate `make query-diagnostics` library uses the production public facade
with pre-input test-only option selection and internal work snapshots. It is not
installed or linked into the production library. Release and ASan/UBSan parity
tests compare 48 calls per build, including exact work, models, exported CNFs and
proof bytes, and verify that production exports no diagnostic functions.

## Difficult query results

| Mode / test | CPU seconds | Conflicts | Result |
|---|---:|---:|---|
| Retained, uncertified control | 5.000 | 147,840 | UNKNOWN |
| Retained, certified control | 5.000 | 123,470 | UNKNOWN |
| Certified control, longer allowance | 30.000 | 735,968 | UNKNOWN |
| Certified, rephasing disabled | 5.000 | 100,477 | UNKNOWN |
| Certified, alternating search | 5.000 | 110,852 | UNKNOWN |
| Certified, chronological backtracking | 5.000 | 125,332 | UNKNOWN |
| Certified, reduction interval grows by 1,000 | 5.000 | 78,606 | UNKNOWN |
| Fresh certified handle, only depth-16 query | 5.000 | 133,680 | UNKNOWN |

Retained CaDiCaL takes 0.657 s alongside the uncertified control and 0.814 s
alongside the certified control, with independently checked reference UNSAT.
At depth 8, BSAT resolves the positive query in 0.067–0.088 s; the difficult
behavior emerges with deeper unrolling rather than every query being slow.
Starting a fresh BSAT handle does not remove the depth-16 timeout.

The 30-second BSAT query generates 735,968 clauses and deletes 732,166, and leaves
153,979,671 journal bytes including earlier query learning. Clause counts show
substantial churn, but do not by themselves establish that the deleted clauses
were useful. Growing the reduction interval cuts reductions on the five-second
query to ten while also reducing throughput to 78,606 conflicts. That test changes
the search trajectory, so fewer conflicts cannot be interpreted as convergence.

## What this establishes

The failure cannot be explained solely by a five-second cutoff or stale retained
history. Several inexpensive policy switches and a less aggressive deletion
schedule fail to recover it. We need to investigate the *quality* of learned
consequences and the circuit's deeper constraints; shaving a few percent from
propagation cost has no demonstrated path to closing this particular gap.

A useful next hypothesis is whether equivalence/structural reasoning or stronger
learned-clause retention criteria can produce the consequences the current search
misses. That remains a hypothesis, not an implemented improvement. Any such pass
must preserve future assumption semantics and produce checked certificates.

Evidence in `benchmark_results/`: `cal3-equal-cpu-20260908.json`,
`cal3-long-cpu-20260908.json`, `cal3-no-rephase-20260908.json`,
`cal3-alternating-20260908.json`, `cal3-chrono-20260908.json`,
`cal3-growing-reduce-20260908.json`, and `cal3-fresh-assumption-20260908.json`.
Each contains tool/input hashes, statuses, counters, CPU and independent checks.
Phase arrays are zero in these uninstrumented runs. Detailed scan counters and
GC counters have core-lifetime scope; ordinary query statistics reset per solve.
