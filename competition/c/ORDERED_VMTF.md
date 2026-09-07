# Preserve queue order across conflict bumps

The initial `--vmtf` policy moved variables immediately as conflict analysis
visited them. That made graph traversal order determine the relative order of
all variables involved in one conflict. The updated policy collects those
variables, sorts them by their previous queue timestamps, and then moves them
oldest first. Their previous relative order is preserved at the queue front.

This follows the ordering principle in Kissat's pinned
[bump implementation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/bump.c).
BSAT still differs in mode scheduling, phase policy, heap maintenance and other
search techniques. The default VSIDS policy is unchanged; `--vmtf` remains an
experimental option.

## Implementation and tests

Conflict analysis records each newly seen variable in `analyze_stack`, which
already has capacity for every variable. The minimizer reuses that scratch only
after analysis returns. An in-place heapsort orders the variable IDs using their
unchanged timestamps; queue mutations start only after sorting. No per-conflict
allocation or additional per-variable storage is needed. Sorting and movement
poll resource limits, and the pending count resets at every analysis boundary.
Timestamp rollover during movement preserves the order computed beforehand.

The new unit test compares 100 randomized batches over 257 variables against a
simple reference: select the involved variables in their existing order, then
append all uninvolved variables in their existing order. It also forces
timestamp rollover and checks an empty batch. Existing tests cover actual
binary conflicts, backtracking, assumptions, API rebuilds, allocation growth,
and interruption. Both release and ASan/UBSan unit suites passed.

Final validation passed all 15 C test executables, 5311 independent formula
solves, and 18 short-deadline checks in each of release and ASan/UBSan builds.
Formula coverage uses 47 configurations, 13 fixed formulas, and 100 randomized
formulas with seed 20260920. Answers are compared with truth tables; SAT models
are checked against original inputs and text/binary proofs are independently
checked. These tests cover this change without establishing correctness for
every possible competition input.

## Targeted comparison

The three development inputs are precisely those that Kissat solved and BSAT
missed in the earlier three-second screen. Each solver gets ten solving CPU
seconds, fifteen wall seconds, and two repetitions. SAT models and UNSAT proofs
are independently checked. This selection deliberately targets known weaknesses;
it is not an unbiased competition sample.

| Variant | Verified runs / 6 | Mean PAR-2 (wall seconds) |
| --- | ---: | ---: |
| Default BSAT (`c2d01be`) | 2 | 22.4167 |
| Initial VMTF (`ab6294d`) | 2 | 21.5377 |
| Ordered VMTF | 4 | 12.0988 |
| Pinned Kissat | 6 | 1.1186 |

Ordered VMTF verifies the multiplier-circuit SAT input in both repetitions,
with median process CPU time 2.755 seconds; both earlier BSAT variants exhaust
their ten-second solving limit. On belpyramid, median process CPU falls from
4.596 seconds with initial VMTF to 3.404 seconds with ordered VMTF (default
BSAT: 7.230 seconds). Every BSAT variant still misses hgen at ten seconds,
where Kissat takes about 1.2 seconds. Kissat also remains substantially faster
on the other two inputs.

## Broader development screen

Across the same 28 development inputs at five solving CPU seconds, ten wall
seconds, and one repetition, initial VMTF verifies 4 inputs and ordered VMTF
verifies 5. The multiplier-circuit input is the extra solve; no previous solve
is lost. Mean PAR-2 improves from 17.3108 to 16.6703 wall seconds. Both variants
have zero errors. Maximum process RSS rises from 59.3 MB to 75.0 MB on these
runs, despite unchanged scratch allocation: altered search paths can change
the retained clause database. This small reused sample is evidence for keeping
the opt-in improvement, not for changing the default branching policy.

## Fresh instances

Before measuring them, three additional input filenames were checked against
the stored benchmark reports and had no occurrences: belpyramid `0205e2d...`
and `0f269188...`, and multiplier-circuits `362d153...`. These are new instances
from already studied families, not held-out families. No further tuning followed
their results. With ten solving CPU seconds, fifteen wall seconds and two
repetitions, both VMTF versions verify 0 of 6 runs. Kissat verifies the multiplier
instance twice (2 of 6); all versions miss both belpyramid instances. There are
zero errors. The development gain has therefore not generalized to this small
fresh sample, and competition parity remains unachieved.

Process CPU includes parsing/startup/proof output; the solving limit excludes
parsing. Failed or unfinished runs receive twice the wall timeout as their PAR-2
penalty. Certificate checking is outside solver timing. Runs are serial on
macOS arm64, with no concurrent local builds or tests. Timing varies on this
host; the repeatable verified solve is stronger evidence than a small timing
difference. The reports pin executable/input hashes, commands and limits.

## Evidence

- [Targeted repeated comparison](benchmark_results/vmtf-ordered-gap-20260906.json)
- [28-input development comparison](benchmark_results/vmtf-ordered-broad-20260906.json)
- [Fresh-instance comparison](benchmark_results/vmtf-ordered-fresh-20260906.json)
- [Release deadline checks](benchmark_results/vmtf-ordered-limits-release-20260906.json)
- [Sanitizer deadline checks](benchmark_results/vmtf-ordered-limits-asan-20260906.json)

Reproduce the comparisons with `tests/benchmark.py`, building the revisions
named above and using the command templates and exact input paths in each JSON
report. Build unit tests with `make -j4 all test` and
`make -j4 MODE=debug all test`. Formula validation uses
`tests/validate.py --solver <binary> --checker <drat-trim> --cases 100 --seed 20260920`;
resource-limit validation uses `tests/check_deadlines.py --solver <binary>`.
