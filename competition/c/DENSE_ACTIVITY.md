# Rejected dense activity layout

A separate variable-activity array was implemented and measured, then reverted.
The initial small improvement did not hold consistently across the longer,
broader comparison. The retained changes are heap regression tests and profiling
and benchmark evidence. The solver still stores scores in `VarInfo.activity`.

The candidate moved scores into a dense double array, preserving heap algorithms,
comparison order, score updates and rescaling. On arm64 this changed `VarInfo`
from 40 to 32 bytes plus eight bytes per variable in the separate array, without
reducing total per-variable storage. Allocation, zero initialization, growth,
freeing and API rebuild paths were updated for the experiment. No new layout or
ABI change is retained.

## Profiling evidence

A separate optimized build with debug information was sampled on three larger
competition inputs for three seconds at one-millisecond intervals, while the
solver used a six-second CPU limit. The build used C11, `-O3 -g -march=native`,
without LTO; timing comparisons below use the ordinary release build with LTO.
Sampling is diagnostic and is not used as a before/after speed measurement.

On belpyramid, `solver_decide` accounted for 993 of 2544 samples (39.0%), largely
heap work in the optimized instruction ranges. Propagation accounted for 843
samples and backtracking for 260. On the multiplier circuit and crafted CEC,
propagation dominated instead. This motivated testing a data-layout change across
multiple families rather than assuming decision selection dominated every workload.

The belpyramid profile also attributed 400 samples to `getrusage`, used by CPU-time
checks. Deadline-check scheduling is a separate follow-up candidate; this milestone
does not change resource-limit behavior.

- [Belpyramid profile](benchmark_results/dense-activity-profile-belpyramid-puzzle-20260906.txt)
- [Multiplier profile](benchmark_results/dense-activity-profile-multiplier-circuits-20260906.txt)
- [Crafted CEC profile](benchmark_results/dense-activity-profile-crafted-cec-20260906.txt)

The profile excerpts omit the trailing system-library image inventory. Inputs are
identified in the larger-corpus manifest. The profiling source was baseline commit
`562424ea7133c1c29d0ff536e1c80514a4632a35`.

## Initial comparison

The existing twelve-family larger corpus was run three times per version, with
10000 conflicts, a 20-second CPU cap and a 25-second wall cap. Eleven recorded
search counters matched across all repetitions, including decisions, propagations,
conflicts, learned literals, minimization inspections, reductions and total work.
Both versions verified three of 36 runs, with no reported errors; the remaining
runs were UNKNOWN at the conflict limit.

For the eleven instances with baseline median CPU time above 0.05 seconds, the
geometric mean candidate/baseline ratio was 0.9821 (1.8% less CPU). Belpyramid's
median fell from 3.428 to 3.304 seconds (3.6%); all eleven per-instance median ratios
were below one. The very short preprocessing UNSAT instance is excluded only from
the timing aggregate. Maximum process RSS was 42287104 versus 44548096 bytes;
there is no measured memory-use improvement.

[Raw initial comparison](benchmark_results/dense-activity-screen-20260906.json).

## Longer comparison and decision

The broader run used 28 development inputs, two repetitions per version,
30000 conflicts, a 20-second CPU cap and a 25-second wall cap. All eleven checked
search counters again matched on all inputs. Both versions verified six of 56
runs, with no reported errors. Maximum RSS was 48447488 versus 50790400 bytes.

For the 25 instances with baseline median CPU above 0.05 seconds, the geometric
mean CPU ratio was 0.9902, approximately 1% lower. However, several per-family
regressions were substantial and mostly repeatable:

| Family | Baseline median CPU | Dense scores | Ratio |
|---|---:|---:|---:|
| Belpyramid | 7.921 s | 7.670 s | 0.968 |
| Multiplier circuit | 5.409 s | 4.413 s | 0.816 |
| Fermat | 1.117 s | 0.877 s | 0.785 |
| Crafted CEC | 6.145 s | 7.024 s | 1.143 |
| Grandtour | 0.457 s | 0.567 s | 1.240 |
| Mechanical master key | 0.938 s | 1.163 s | 1.240 |
| Larger scheduling | 1.174 s | 1.459 s | 1.243 |

The experiment does not isolate score-array locality from the changed layout of
other variable metadata or compiler effects. These regressions make the small
aggregate improvement a poor basis for a default change. The layout was reverted;
there is no claimed production performance gain from this experiment.

- [Longer raw comparison](benchmark_results/dense-activity-long-20260906.json)
- [Per-instance summary](benchmark_results/dense-activity-summary-20260906.json)
- [Rejected source patch](benchmark_results/dense-activity-rejected-20260906.patch)

## Validation and measurement limits

The new heap regression checks score preservation over several capacity increases,
zero initialization, repeated extraction/backtracking, large-score rescaling during
real conflict analysis, and score reset on formula rebuild. Existing solver and
independent certificate tests exercise the unchanged search policies.

Timing runs are serial without concurrent local builds or tests. Certificate
checking is outside solver timing. These are development families, not a disjoint
held-out competition set. Conflict-capped PAR-2 values are diagnostics, not
competition scores. UNKNOWN results are not completed answers. Matching recorded
counters supports comparison at equal search work, but is not a formal proof that
every internal operation is identical.

Both the candidate and the restored solver passed the release and ASan/UBSan
unit suites. The retained suite now contains eleven C test executables. The
restored release executable has the same SHA256 as the baseline, confirming no
solver implementation change remains. No new full formula-validation matrix was
run for this discarded layout.
[Validation record](benchmark_results/dense-activity-validation-20260906.json).
