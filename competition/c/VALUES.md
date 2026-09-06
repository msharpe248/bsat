# Compact assignment storage — September 2026

Truth values now live in one authoritative `uint8_t` array indexed by variable.
Propagation reads this dense array instead of loading values from a 48-byte
`VarInfo` record. Reordering the remaining metadata reduces that record to 40
bytes on the measured arm64 ABI. Combined value/metadata storage falls from 48
to 41 bytes per allocated variable (14.6%); this is not a claim about total RSS.

Assignment, propagation, backtracking, decisions, preprocessing reconstruction,
local-search model transfer, minimization and final model validation all use the
same array. There is no second value cache to become stale. Growth preserves live
assignments and initializes each new variable to UNDEF. Destruction frees the
array, including partial allocation-failure paths. `solver_model_value` also now
returns UNDEF for variable zero, including on an empty solver.

Use `solver_model_value` in embedding code. Direct accesses to the removed
`vars[v].value` field require updating, and code using internal structures must
be recompiled. Search policy and watch order are unchanged.

## Evidence and methodology

The baseline is the hardened working tree including the opt-in minimization
experiment, preserved before this layout change. Its executable SHA-256 is
`40ee4eb71e7083fae6f2250160966f64ca8c3ea682a76aaaccf3fd506e32c24b`.
It is not Git HEAD. Both versions use the established default minimizer.
The [source diff](benchmark_results/values-source-20260906.patch) captures the
core/header changes from that preserved baseline. Reverse it in a separate copy
of this source state to recover the pre-change core, then force a rebuild;
do not reverse it in an actively edited checkout.

Measurements use the serial, seeded benchmark harness with proof output, three
repetitions per input/version and independent checking of completed models and
UNSAT certificates. No tests or builds ran concurrently with timed benchmarks.
This is an Apple arm64 macOS development machine, not a controlled competition
host. CPU results include process startup, parsing and proof writing, and exclude
the independent checker. Conflict limits make these throughput diagnostics;
their PAR-2 fields must not be read as competition scores.

The first sample reuses the sixteen-family
[manifest](benchmark_results/minimization-corpus-20260906.json), with 30000
conflicts, a three-second internal CPU limit and eight-second external wall limit.
All decision, conflict, propagation, learned-literal, minimized-literal and
literal-inspection counters match between versions and repetitions. Fourteen
families reach exactly 30000 conflicts; the geometric mean candidate/baseline
ratio of median process CPU times is 0.9653 (3.5% less time). The two remaining
instances solve at 1039 and 22 conflicts, unchanged, with verified models.
Raw results: [sixteen-family run](benchmark_results/values-work-20260906.json).

A [larger sample](benchmark_results/values-large-corpus-20260906.json) selects
the first twelve families by file size after requiring at least 1 MB and 10000
variables, taking the smallest qualifying file per family. Selection preceded
candidate performance measurements. These are larger follow-up inputs, not
disjoint held-out families. They contain 11607–51108 variables.

The initial larger screen uses 3000 conflicts, five internal CPU seconds and ten
external wall seconds. Ten families have complete identical search counters and
reach the conflict bound; their geometric mean CPU ratio is 0.9640. A separate
instance is verified UNSAT during preprocessing. The multiplier-circuit instance
hits the external timeout in both versions without final counters, so it is
excluded from equal-work timing comparisons. This also exposes a resource-limit
boundary that needs investigation; a requested internal time cap did not ensure
timely process completion on that input.

The [longer run](benchmark_results/values-large-work-20260906.json) repeats the
larger sample with 30000 conflicts, ten internal CPU seconds and fifteen external
wall seconds. The same ten families reach the conflict bound with identical
search counters across all six runs per instance. Their geometric mean CPU ratio
is **0.9586 (4.1% less CPU time)**. All ten improve; individual median reductions
range from 0.6% to 7.2%. The preprocessing UNSAT instance remains verified, and
the multiplier-circuit case again externally times out without counters.

| Larger family | Baseline median CPU seconds | Compact values | Reduction |
|---|---:|---:|---:|
| Scheduling | 1.560 | 1.505 | 3.5% |
| Unknown cases | 2.365 | 2.224 | 6.0% |
| Hardware model checking | 4.721 | 4.544 | 3.8% |
| Crafted CEC | 7.968 | 7.396 | 7.2% |
| Belpyramid puzzle | 8.317 | 8.265 | 0.6% |
| Ensemble computation | 2.318 | 2.226 | 4.0% |
| Fermat | 1.358 | 1.310 | 3.5% |
| Maximum constraint partition | 0.803 | 0.784 | 2.4% |
| Influence maximization | 3.820 | 3.556 | 6.9% |
| Discrete logarithm | 2.034 | 1.965 | 3.4% |

The [derived summary](benchmark_results/values-summary-20260906.json) records
per-instance medians, exclusions, comparison counters and the final executable
hash, which matches the measured candidate. Across the three experiments there
are 240 process runs, 24 independently verified completed answers and zero
reported errors. UNKNOWN results are not validated SAT/UNSAT answers. This is
evidence of a modest implementation speedup at equal search work, not an increase
in solved count or proof of competition parity. The compact layout is retained
as the default based on consistent improvements and unchanged search counters.

After timing completed, a [diagnostic](benchmark_results/values-limit-diagnostic-20260906.json)
on the multiplier-circuit case requested a
0.1-second internal limit. Default minimization exceeded a three-second external
limit, while `--no-minimize` and `--iterative-minimize` returned UNKNOWN at about
0.1 CPU seconds. These controls change search trajectories, so they implicate
the default minimization path without constituting a profile of the stall. The
legacy recursive minimizer has no internal cancellation/work check; bounding
that path was subsequently [fixed and measured](MINIMIZATION_LIMITS.md). The
measurements above retain their original pre-fix executable hashes. Diagnostic wall timings overlapped
validation and are not included in the performance comparison.

## Correctness checks

Release and ASan/UBSan unit suites pass. The added assignment-lifecycle regression
keeps root and decision assignments live across multiple capacity doublings,
checks new variables remain unassigned, backtracks without losing root values,
then solves repeatedly under assumptions. It also checks invalid model indices.
Existing tests exercise 800 exhaustive-oracle API calls, model reconstruction,
local search, minimization, clause relocation and garbage collection.

Final independent validation passed **2756 release and 2756 ASan/UBSan solves**
on fresh seed `20260908`. Each build ran 100 random plus six fixed formulas under
26 configurations, including both minimizers, budgets zero/one, BVE/BCE,
vivification, aggressive deletion, local search, alternating search and binary
proofs. Answers are checked against a truth table, SAT models against the
original formula, and UNSAT proofs with both the Python RUP checker and pinned
DRAT-trim. These checks cover the storage migration but do not establish
correctness on every industrial formula or resolve the legacy timeout defect.

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat \
  --cases 100 --seed 20260908 --checker /path/to/drat-trim
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug \
  --cases 100 --seed 20260908 --checker /path/to/drat-trim
```

## Related implementation

[Kissat's internal solver representation](https://github.com/arminbiere/kissat/blob/master/src/internal.h)
also separates values from assignment metadata. BSAT's implementation retains
its existing variable-indexed truth encoding and changes storage only; it does
not adopt Kissat's literal-value representation or propagation scheduling.
