# Incremental best-phase targets

The solver previously cleared its entire target array and copied the entire trail
whenever the trail reached a new maximum length. On a descent assigning one new
variable per iteration, this performs quadratic copying even for an empty formula.

The saved target now records whether it remains a prefix of the current trail.
While that prefix is valid, a new record copies only the appended literals.
Backtracking into the saved prefix invalidates it; the next strictly longer
record clears and rebuilds the entire target. Shorter or equal trails leave the
previous target untouched, including after a restart. Variable growth initializes
new target entries to UNDEF. API rebuilds start with a fresh cache, and local
search invalidates the cache when it replaces the trail.

This preserves the previous target-selection policy, including signs and absent
variables. Every new record still forces the existing CPU deadline check. Two
statistics report literals copied and target entries cleared. An uninterrupted
descent performs one initial clear and copies each assigned variable once; this
is not a claim of linear total work across arbitrary branching histories.

## Measurements

The baseline is commit `47cfdba743681a553b68d9cfb5d5da98ccdb28d6`.
Five constructed SAT inputs were selected before timing and run twice per version,
with proofs enabled, 20 CPU seconds and 25 wall seconds. All ten runs per version
returned independently verified models, with identical search and deadline-clock
counters. Median whole-process CPU seconds were:

| Formula | Baseline | Incremental |
| --- | ---: | ---: |
| Empty, 20000 variables | 0.1030 | 0.0108 |
| Empty, 50000 variables | 0.5914 | 0.0237 |
| Empty, 100000 variables | 2.2916 | 0.0444 |
| 10000 independent binary pairs | 0.1066 | 0.0142 |
| 50000 independent binary pairs | 2.3064 | 0.0653 |

The 100000-variable empty case is about 52 times faster; its candidate counters
show 100001 target entries cleared and 100000 literals copied. These deliberately
simple stress cases expose the copying cost; they do not establish competitive
performance on hard formulas.

The broader comparison uses the existing 28-input minimization and larger-values
samples, three repetitions, 10000 conflicts, 20 CPU seconds and 25 wall seconds.
Both versions verified nine of 84 runs (three distinct inputs), with zero errors;
the other runs were UNKNOWN. All twelve compared search/deadline counters matched
for every input and repetition. On the 25 inputs with baseline median CPU above
0.05 seconds, the geometric mean candidate/baseline ratio was 0.9962: essentially
flat. Individual median ratios ranged from 0.9383 to 1.0232. There is no observed
competition solved-count gain.

Timing was serial on macOS arm64 without concurrent local tests or builds.
Whole-process CPU includes startup, parsing and proof output; certificate checking
is outside timing. These are development samples, not held-out competition
families. Conflict-capped PAR-2 is diagnostic, not a competition score.

## Regression coverage and evidence

`test_targets` compares every target entry with the previous full-copy algorithm
across extensions, shorter/equal branches, changed variables and signs, restarts,
and backtracking confined to a temporary suffix. It checks variable-capacity
growth, API rebuilds, and a 20000-variable descent with exact linear copy counts.

- [Stress runs](benchmark_results/target-stress-20260906.json)
- [Competition runs](benchmark_results/target-competition-20260906.json)
- [Derived timing and counter comparison](benchmark_results/target-summary-20260906.json)
- [Stress input hashes](benchmark_results/target-corpus-20260906.json)
- [Release deadlines](benchmark_results/target-limits-release-20260906.json)
- [Sanitizer deadlines](benchmark_results/target-limits-asan-20260906.json)

Raw reports include executable/input hashes, commands, counters and checker
identity. To recreate stress inputs, write `p cnf N 0\n` for each empty formula.
For M binary pairs, write `p cnf 2M M\n` using numeric values, then lines
`1 2 0\n`, `3 4 0\n`, through `(2M-1) 2M 0\n`. Use N = 20000, 50000,
100000 and M = 10000, 50000, with the filenames in the manifest.

Final validation passed 4520 release and 4520 ASan/UBSan formula solves across
40 configurations (seed 20260914), all thirteen C test executables in both modes,
and twelve short-deadline runs per mode. Checks include independent truth-table
answers, original models, and text/binary proofs. See the
[validation record](benchmark_results/target-validation-20260906.json).
