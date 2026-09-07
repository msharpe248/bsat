# Save the phases of decision assignments

The repository's [phase-saving design](../docs/phase_saving.md) calls for saving
every assignment, including both decisions and unit propagations.

Decision assignment previously wrote the trail, value, and level directly,
bypassing `push_trail`. Propagations and assumptions used that helper, which
updates saved polarity when phase saving is enabled. Decisions did not update
saved polarity, so a random or target-phase override could be forgotten after
backtracking. The next decision could revert to an older saved phase.

Decisions now use `push_trail` as well. The actual selected phase is recorded,
the trail and reason fields are initialized consistently, and the decision
counter still increments exactly once. With phase saving disabled, the helper
leaves saved polarity alone. Random-phase probability, target-phase precedence,
initial polarity, restart scheduling, and branching options are unchanged.

Kissat's pinned [assignment implementation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/inlineassign.h)
also records phases through its shared assignment path outside probing. This
change addresses BSAT's decision omission; it does not copy Kissat's complete
phase or probing policy.

## Regression coverage

The new `test_phases` executable uses seed 2 to force a positive first random
decision over the initial negative saved phase. The saved polarity must become
positive, and after backtracking a nonrandom decision must reuse it. Seed 1 then
forces a negative decision and checks the reverse transition. Both heap and
VMTF branching are tested. Additional cases choose a positive target phase in
stable mode and check the saved fallback in focused mode, with phase saving
enabled and disabled. Trail position, decision level, reason fields, and
decision counts are also checked.

Running the regression against baseline `08ba16613b18e9117631c892ea2933d01817c177`
fails at the assertion that the first positive decision updates saved polarity.
With the fix, all 16 C test executables pass in release and ASan/UBSan builds.

Final validation also passes 5311 formula solves and 18 short-deadline runs in
each build. Formula coverage uses 47 configurations, 13 fixed inputs, and 100
randomized formulas with seed 20260922. Answers are checked against truth tables,
SAT models against original inputs, and text/binary UNSAT proofs independently.
Both deadline suites observe no overrun at the reported precision.

## Measurement design

The comparison uses the same 28 development inputs as the preceding VMTF work.
It runs the baseline and candidate with default heap branching and with `--vmtf`,
using five solving CPU seconds, ten wall seconds, and one repetition per input
and variant. SAT models and UNSAT proofs are independently checked. These are
reused development inputs, not a representative held-out competition sample.
The behavior fix changes search trajectories, so counter equality is not
expected and timing alone cannot isolate a throughput improvement.

Runs are serial on macOS arm64 with no concurrent local builds or tests.
Solving CPU limits exclude parsing, while measured process CPU includes parsing,
startup, and proof output. Certificate checking is outside solver timing. The
reports pin input/binary hashes and command templates. UNKNOWN and unverified
answers receive the harness's twenty-second PAR-2 penalty based on wall timeout.

## Initial results

| Branching | Baseline verified / 28 | Candidate verified / 28 | Baseline / candidate mean PAR-2 |
| --- | ---: | ---: | ---: |
| Default heap | 3 | 3 | 17.8588 / 17.8588 |
| VMTF | 5 | 5 | 16.6276 / 16.7247 |

The solved sets are identical and every variant has zero errors. VMTF's
multiplier-circuit solve rises from 2.548 to 4.938 process CPU seconds, while its
hamiltonian solve falls from 0.204 to 0.066 seconds. Belpyramid rises from 2.671
to 3.200 seconds. The one-repeat screen shows no competition solved-count gain,
and the multiplier result warrants further phase-policy testing. Restoring the
documented phase-saving behavior is not itself evidence of a performance win.
The multiplier change also increases conflicts from 54695 to 102842, explaining
why it cannot be treated as mere timer noise or assignment-helper overhead.

A follow-up compares the fixed VMTF solver with its usual random overrides and
with `--no-random-phase` on belpyramid, hgen, and multiplier-circuits. It uses ten
solving CPU seconds, fifteen wall seconds, and two repetitions. Both policies
verify 4 of 6 runs with zero errors. Disabling random phases lowers belpyramid's
median process CPU from 3.237 to 2.919 seconds, but increases multiplier-circuits
from 4.925 to 7.058 seconds; both miss hgen. Mean PAR-2 rises from 12.7464 to
13.3444 wall seconds. There is no evidence here for disabling random overrides,
so the existing default stays in place.

The assignment fix is retained to restore the declared phase-saving behavior
and remove duplicated bookkeeping. These samples do not demonstrate a
performance improvement; better phase diversification remains an open task.

## Evidence

- [Old/new branching comparison](benchmark_results/decision-phase-screen-20260906.json)
- [Solved sets and summaries](benchmark_results/decision-phase-summary-20260906.json)
- [Repeated phase-policy comparison](benchmark_results/decision-phase-policy-20260906.json)
- [Release deadline checks](benchmark_results/decision-phase-limits-release-20260906.json)
- [Sanitizer deadline checks](benchmark_results/decision-phase-limits-asan-20260906.json)

Build and run the unit suites with `make -j4 all test` and
`make -j4 MODE=debug all test`. Run `tests/validate.py --solver <binary> --checker
<drat-trim> --cases 100 --seed 20260922` and `tests/check_deadlines.py --solver
<binary>` for each build. Benchmark reports contain exact input paths and solver
templates for reproduction with `tests/benchmark.py`.
