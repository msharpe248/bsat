# Planning search comparison — 2026-09-08

Follow-up: [eligibility investigation](LEARNED_CLAUSE_ELIGIBILITY.md) corrects
this report's proposed hot-clause selection experiment: that policy had already
been tested and rejected. The new measurements explain its limited size coverage.

The two frozen planning instances remain UNKNOWN for BSAT and Kissat under the
same external CPU and wall limits. BSAT reaches 100,000 conflicts faster than
Kissat on both, but performs different search and simplification work. These
results do not establish a solving advantage or justify a new default policy.
Propagation remains the largest measured BSAT cost; improving the consequences
learned per unit of work deserves investigation alongside its implementation.

## Equal process budgets

`planning-equal-budget-20260908.json` records two serial repetitions of three
configurations on each input: ordinary BSAT, the existing planning profile
(`--chrono --congruence --equiv --equiv-budget 100000000 --alternating`), and
pinned Kissat. All write binary proof streams. Each process receives 15 CPU
seconds and 20 wall seconds, including parsing and proof output. None solves;
all 12 runs hit the external CPU limit, without harness errors. Actual CPU spans
15.007–15.066 seconds, reflecting external enforcement granularity.

Maximum harness-measured RSS across these runs is 126.7 MB for ordinary BSAT,
167.0 MB for the planning profile and 72.7 MB for Kissat. These are observed
peaks, not enforced memory ceilings. UNKNOWN proof streams are incomplete
prefixes and are not certificates of UNSAT.

## Fixed conflict work

`planning-fixed-conflicts-20260908.json` uses 100,000 conflicts and a 60-second
wall guard, without a CPU limit. All 12 runs stop normally at exactly 100,000
conflicts, remain UNKNOWN and have no harness errors. Within each solver/input
pair, repeated propagation/decision counts and proof-prefix hashes agree.
CPU below is the median child-process CPU, including parsing and output; it is
not the solver's narrower internal search timer.

| Input prefix | Configuration | CPU seconds | Propagations / conflict | Decisions | Peak RSS MB |
|---|---|---:|---:|---:|---:|
| 16c999 | BSAT default | 4.333 | 1,205.0 | 392,153 | 61.7 |
| 16c999 | BSAT planning | 4.717 | 1,180.3 | 308,452 | 115.9 |
| 16c999 | Kissat | 9.004 | 2,111.8 | 639,076 | 70.3 |
| 21b173 | BSAT default | 1.197 | 146.7 | 366,389 | 16.8 |
| 21b173 | BSAT planning | 1.502 | 144.2 | 294,464 | 18.0 |
| 21b173 | Kissat | 2.677 | 365.3 | 536,065 | 22.0 |

The inputs contain respectively 50,277 variables / 283,903 clauses and 3,084
variables / 26,019 clauses. CPU ranges are 4.311–4.354 / 4.652–4.781 /
8.661–9.347 seconds on the larger input, and 1.190–1.205 / 1.485–1.520 /
2.464–2.890 on the smaller input, in table order. Two repetitions give limited
precision. RSS comes from the harness; Kissat's own printed RSS has a macOS
unit inconsistency and is not used.

At this stopping point the planning profile reduces decisions but takes about
9% and 25% more CPU than ordinary BSAT. It does not establish better progress
toward a solution. Nor does Kissat's slower time to this conflict count establish
worse solving: its statistics report 33,048 / 891 eliminated variables, 20,247 /
925 factored variables and 8,679 / 13,062 vivified clauses on the two inputs.
Those transformations, search trajectories and counting conventions differ.
Conflicts and propagations are not interchangeable units of useful work across
solvers. No matched-search comparison of their propagation implementations was
performed.

## Attribution and next hypothesis

The existing Linux software-stack profile attributes 70.06% of samples to
propagation, 6.4% to analysis, 5.0% to backtracking and 3.02% to recursive
minimization. Hardware PMU events were unavailable; this is not evidence of
cache-miss or branch-misprediction causality.

Existing intrusive 100,000-conflict clause accounting finds learned clauses
responsible for about 60% / 86% of replacement literal scans. About 72% / 86%
of scans are in size-9-or-larger clauses. This supports investigating the amount
and quality of long learned-clause work. It does not support optimizing only
original-clause headers; that layout experiment already failed its timing gate.

The existing vivifier scans up to 100 learned candidates per interval, skips
locked clauses and sizes outside 3–64, and tests individual literal deletions.
Earlier [prefix](PREFIX_VIVIFICATION_EXPERIMENT.md),
[assumption reuse](VIVIFICATION_REUSE_EXPERIMENT.md) and
[rolling omission](ROLLING_OMISSION_EXPERIMENT.md) experiments were rejected;
one lost previously solved cases. Repeating those kernel changes is not the
recommendation.

The next bounded experiment should test candidate selection for this existing
proof-producing strengthening routine: prioritize repeatedly scanned learned
clauses over the rotating cursor, with a tightly bounded work allowance and
preservation of assumption validity. First measure what fraction of costly
clauses lies within the existing size limit; do not blindly raise that limit.
Freeze fresh planning
holdouts and compare solved cases and CPU PAR-2 before considering a default
change. Log simplification cost, clause shrinkage, later scan work, memory and
proof growth. Reject a candidate that merely reaches a conflict threshold faster
without improving end-to-end outcomes. A PMU-enabled target host would separately
allow a justified cache/layout experiment.

## Reproduction and provenance

Both new JSON reports under `benchmark_results/` record exact commands, input
hashes, binary hashes, randomized serial order, platform and stopping conditions.
They use the unchanged CLI binary with SHA-256
`cdef7bd63217c941ebe30cfc27ef00fafd2d88e362fdf950c856b952c66e0a57`.
Use `tests/benchmark.py` with their command templates and respectively
`--external-wall-only --cpu-limit 15 --timeout 20 --repeats 2` or
`--timeout 60 --repeats 2` plus the recorded solver conflict options. Seeds are
2026090854 and 2026090855. Independent checking is configured but cannot turn
these unfinished proof prefixes into conclusive answers.

Earlier attribution is archived in `clause-attribution-20260908.json` and
`planning-linux-profile-20260908/`, described in
[the planning layout experiment](PLANNING_LAYOUT_EXPERIMENT.md). This milestone
promotes no planning runtime or default-policy change.
