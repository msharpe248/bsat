# Generation-stamped LBD experiment

The prototype replaces one-byte decision-level marks with 32-bit generation
stamps. Each LBD calculation increments a generation and counts distinct non-root
levels in one scan; it no longer scans the clause again to clear those marks.
On unsigned rollover, the complete mark array is cleared before generation one
is reused. This changes scratch storage, not the definition of LBD or search
policy. Allocation grows by three bytes per decision-level slot plus a generation
counter. The number of slots includes dummy assumption levels.

Glucose 4.2.1 also uses generation flags for scratch membership in its
[binary-resolution minimizer](https://github.com/audemard/glucose/blob/4.2.1/core/Solver.cc#L523).
This experiment applies that general technique to BSAT's separate LBD marks;
it does not copy Glucose's minimization or clause-retention policy.

## Validation

Release and ASan/UBSan C suites exercise dynamic LBD using real propagation and
conflict analysis. The regression now repeats its learned/original clause and
option matrix with fresh marks, immediate generation rollover, and rollover
between the conflict and reason calculation. Stale generation-one marks are
seeded explicitly. Expected LBD, asserting literal, backjump, variable-mark
cleanup and clause-retention results remain checked.

The prototype passes all 26 C executable suites in release and ASan/UBSan
builds. Both builds also pass 2,709 independent truth-table, original-model,
text/binary RUP and external DRAT checks with seed 20261008 and 30 random cases.
These are local checks, not a claim that remote CI ran.

## Measurements and decision

[Raw results](benchmark_results/lbd-generation-work-20260907.json) compare
baseline `b967c72` with the tested working-tree prototype. The record includes
executable hashes, input hashes, commands, timings and verification outcomes.
Both executable hashes were checked against the measured binaries before
restoring the baseline sources.

The fixed 28-input development corpus runs three shuffled repetitions per
version, using default search, a 30,000-conflict cap, three solving CPU seconds
and five wall seconds. Timing runs serially after validation, with no concurrent
builds or tests. SAT models and UNSAT proofs are checked outside solver timing.
The CPU limit excludes parsing; measured process CPU includes startup, parsing
and proof output. These are development measurements, not a held-out competition
score.

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 9/84 | 8.9378 s | 58,916,864 bytes | 0 |
| Generation stamps | 9/84 | 8.9391 s | 59,179,008 bytes | 0 |

Both versions solve the same three inputs in all three repetitions. The
conflict cap makes PAR-2 a diagnostic rather than a competition score.

The [derived comparison](benchmark_results/lbd-generation-summary-20260907.json)
compares every printed counter except PID, CPU time, per-second rates and clock
reads. Twenty-two inputs have identical remaining counters across all six runs.
Their geometric mean ratio of candidate/baseline median process CPU is
**0.9956**, about 0.4% lower. Individual timings are mixed, and some short runs
are especially noisy. This does not establish a useful general speedup.

Six inputs are excluded from that equal-work aggregate because time or external
wall cutoffs prevent identical work across all repetitions: belpyramid, crafted
CEC, ensemble computation, hardware model checking, influence maximization and
multiplier circuits. Their raw outcomes remain in the record.

**Reject the prototype.** The added scratch-array footprint and rollover logic
have not produced a compelling measured benefit or an additional solve. Restore
the one-byte marks and per-clause cleanup; no production option is added.
The tested implementation and rollover regressions are preserved in an
[applicable patch](benchmark_results/lbd-generation-rejected-20260907.patch)
against `b967c72`, so the experiment can be reproduced without retaining its
runtime cost. Applying that patch changes only the solver header, core and
regression test. The restored production sources pass all 26 C suites in both
build modes.
