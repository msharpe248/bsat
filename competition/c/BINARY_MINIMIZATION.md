# Binary-resolution learned-clause minimization experiment

The prototype adds an opt-in `--binary-minimize` pass after conventional
minimization and before final LBD recording and backjump selection. For learned
clause `(a | b | rest)` and live binary `(a | ~b)`, resolution proves
`(a | rest)`. The asserting literal `a` is preserved. Several tails can be
removed using separate binaries containing the same asserting literal.

The local resolution rule and gates follow
[Glucose 4.2.1](https://github.com/audemard/glucose/blob/4.2.1/core/Solver.cc#L523):
only clauses of at most 30 literals and LBD at most 6 are considered. Unlike
Glucose's assignment-based membership check, BSAT uses exact signed scratch
marks and stable compaction of remaining literals. It handles both implicit
input and tagged arena binary watches, ignores long-clause blockers, and relies
on eager watch deletion. It does not follow binary implication chains.

The pass scans at most `--minimize-budget` watch entries, in addition to the
ordinary minimizer's independently bounded traversal. Each inspected watch
charges the minimization-inspection counter; deadline polling occurs at entry
and every 1024 cumulative inspections. Zero budget, `--no-minimize`, cancellation
or an expired deadline disable the pass. Partially proved removals remain valid
on interruption and all scratch marks are cleared. New counters report binary
watch checks and removed literals. LBD and backjump level are recomputed after
removal, before proof output and asserting enqueueing.

## Validation

Release and ASan/UBSan builds pass all 19 C test executables. New direct tests
cover 16 polarity patterns, both binary-watch forms, zero/one/sufficient budgets,
duplicate hits, deleted binaries, wrong-polarity and long-clause blockers,
asserting-literal preservation, unit results, option gates and cancellation.
Exhaustive truth tables independently verify every direct strengthening.

Two eight-variable SAT/UNSAT regressions assert that the actual solver performs
at least one binary-resolution removal. An additional 16 truth-table/model/proof
checks cover those two formulas in both builds, using text and binary proofs,
iterative minimization and aggressive reduction. Each build also passes 21
short-deadline checks (at most 0.001 seconds observed CPU overrun).

The expanded matrix has 54 configurations and 63 formulas (13 fixed, 50 random,
seed 20260929), for 3402 truth-table/model/proof checks per build. The new profiles
cover VMTF, text/binary proofs, SCC/BVE/BCE, inprocessing, aggressive reduction,
iterative minimization and zero/one budgets.

## Performance method

Compare retained VMTF with VMTF plus the new pass, using four reused development
inputs (belpyramid, hgen, maximum-constraint-partition and multiplier-circuits),
twice per profile. Limits are ten solving CPU seconds and fifteen wall seconds.
Runs are serial with no concurrent local builds/tests. SAT models and UNSAT
proofs are independently checked outside solver timing. Process CPU includes
startup, parsing and proof output; the solving limit excludes parsing. PAR-2 uses
twice the wall timeout for unsolved runs. This is a development screen, not a
held-out competition score.

Baseline source is 5203e94; the preserved executable has SHA256
f20f508873ddcd68ecf614c979fee2fc18c5d26f9698e025778abf55adfd7e05.

## Targeted result

Both profiles verify 4/8 runs with zero errors. Mean wall PAR-2 is 17.6677 at
baseline versus 17.2887 with binary resolution. Multiplier-circuits drops from
4.906–4.925 process CPU seconds and 102842 conflicts to 2.659–2.776 seconds and
61602 conflicts. The candidate removes 309 additional literals in each run.
Belpyramid moves in the other direction: 3.527–3.593 seconds and 36652 conflicts
become 4.248–4.453 seconds and 41975 conflicts, with 1091 binary-resolution
removals. Hgen and maximum-constraint-partition remain UNKNOWN twice in both
profiles. Hgen has no binary-resolution removals; maximum-constraint-partition
has 5455–5532 in the candidate, without a verified solve at this limit.

The broader comparison uses the same 28 reused development inputs as preceding
milestones, once per version, with the same CPU/wall limits and certificate
checking. This checks for solved-instance tradeoffs beyond the targeted speedup.

## Broader and additional results

The 28-input comparison verifies the same 5/28 inputs in both versions, with no
gained or lost solves and zero errors. Mean wall PAR-2 is 25.0157 versus 24.9650,
a roughly 0.2% improvement. The multiplier improvement repeats (5.034 to 2.671
process CPU seconds), as does the belpyramid regression (3.539 to 4.250).
Hamiltonian also slows from 0.066 to 0.148 seconds; both versions solve it.
Peak process RSS is 90.96 versus 91.16 MB.

A follow-up selects the other available multiplier-circuit file and the three
smallest belpyramid files absent from the 28-input sample, by file size and then
name, before timing them. Two repetitions each verify 4/8 in both versions,
with zero errors. Mean wall PAR-2 is 15.5913 versus 15.6186. The additional
circuit and one belpyramid input remain UNKNOWN in every run. On the smallest
belpyramid input, CPU rises from 0.245–0.266 to 0.272–0.299 seconds and conflicts
from 17696 to 19689. Another solved belpyramid input has fewer conflicts
(29599 to 28610) but slightly higher CPU (1.658–1.706 to 1.750–1.764 seconds).
The original circuit speedup has not been established as a family-wide gain.

Retain the pass as an experimental option for its repeatable improvement on the
original multiplier input, with no solved-instance loss on these measured
samples. Leave it disabled by default: the aggregate result is small, some
solved inputs slow down, and additional solves or competition parity are not
established. The final help text clarifies that the work cap is per pass; this
wording-only change follows the timed builds and does not change the algorithm.

- [Targeted repeated comparison](benchmark_results/binary-minimize-targeted-20260907.json)
- [28-input comparison](benchmark_results/binary-minimize-broad-20260907.json)
- [Additional inputs](benchmark_results/binary-minimize-additional-20260907.json)

After the help clarification, both final builds again pass all 19 C test
executables. A five-input default-disabled check at 10000 conflicts matches
baseline search and memory counters, excluding PID, reported time/rates and
clock reads; both new counters stay zero. Both versions verify 1/5 runs, with
zero errors. This is a behavior check, not a default-path speedup claim.
[Disabled-option comparison](benchmark_results/binary-minimize-disabled-20260907.json).
