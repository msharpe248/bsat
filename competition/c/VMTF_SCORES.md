# Skip inactive score updates in VMTF search

VMTF decisions depend on queue order, not variable activity. Nevertheless, the
previous implementation updated VSIDS/LRB activity, repaired the activity heap,
decayed its increment, and periodically rescaled every variable's score during
VMTF search. Those operations did not contribute to its branching decisions.

The conflict bump now records a VMTF variable and returns before numeric score
maintenance. Increment decay also skips VMTF mode. This follows the separation
of queue bumps and score bumps in Kissat's pinned
[bump implementation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/bump.c).
BSAT's heap storage, initial insertion, and backtrack bookkeeping remain; this
change removes search-time score work, not the heap allocation. It adds no new
option and does not change the default branching policy.

## Behavioral coverage

A new unit regression solves the same pigeonhole formula with VMTF under
ordinary scores and under LRB with an enormous starting increment and much
faster decay. Results, conflicts, decisions, propagations, learned-literal
counts, trail contents, and complete queue order must match. This guards the
independence of queue search from inactive numeric settings. Existing regressions
cover ordering, rollover, allocation growth, actual conflicts, API rebuilds,
assumptions, and deadlines. All 15 C test executables passed in both release and
ASan/UBSan builds before timing.

Final validation passed 5311 formula solves and 18 short-deadline checks in each
build mode. The formula suite covers 47 configurations, 13 fixed inputs, and
100 randomized formulas with seed 20260921. It compares answers with truth
tables, checks original-input SAT models, and independently verifies text/binary
UNSAT proofs. The release deadline checks observed at most 0.003 seconds of
overrun; sanitizer checks observed no overrun at the reported precision.

## Fixed-work timing

The baseline is `71bd4440ce32798a0f3d30067018e1922834df55`. Every timed solver has a
50000-conflict limit, a 20-second solving CPU limit, a 25-second wall limit, and
two repetitions. The control label invokes the exact same baseline executable;
its variation measures host noise. Comparisons use the geometric mean of
per-input ratios of median process CPU time, including parsing/startup and proof
output. Independent certificate checking is outside solver timing.

| VMTF sample | Inputs | Candidate / baseline CPU | Identical control / baseline |
| --- | ---: | ---: | ---: |
| Initial families | 12 | 0.9707 | 1.0106 |
| Additional families | 6 | 0.9772 | 1.0207 |

The observed improvement is modest, roughly 2–3% in each sample. Individual
results are mixed and noisier: the initial discrete-logarithm input improves
about 13%, while the additional grandtour input is slower than the baseline
label but faster than its identical control. These results support removing
unused work; they do not establish a speedup on every input.

All 36 non-timing CLI statistics, reported statuses, and verification outcomes
match across each input's runs. There are zero errors. The initial sample
verifies two runs per variant; the additional sample verifies none. Most runs
stop at the fixed conflict limit. This isolates throughput and demonstrates no
new solved-count gain. All inputs were used in earlier solver-policy experiments;
the six additional families were not in the first timing sample, but they are
not fresh competition instances. Runs were serial without concurrent local
builds or tests, on macOS arm64. Input and binary hashes are pinned in the reports.

An initial four-family default-heap screen also matches all 36 statistics and
has zero errors. Candidate/baseline geometric-mean CPU is 0.9862, versus 1.0104
for the identical control. Individual medians range from about 15% faster to
7% slower; this small, variable sample does not establish a default-mode speedup.

After correctness validation finished, the two apparently slower default cases
were repeated five times per label. Hardware-model-checking's candidate/baseline
median ratio was 1.0144, almost the same as the identical control's 1.0126.
Multiplier-circuits' ratio was 0.9987, while its identical control was 0.8627.
The initial 6–7% slowdowns did not repeat against the baseline label. This large
control variation reinforces the limitation: no reliable default-mode timing
change is established. All 36 statistics still match and there are zero errors.

## Evidence

- [Initial 12-family timing](benchmark_results/vmtf-score-development-20260906.json)
- [Additional six-family timing](benchmark_results/vmtf-score-additional-20260906.json)
- [Initial default-heap check](benchmark_results/vmtf-score-default-20260906.json)
- [Five-repeat default-heap follow-up](benchmark_results/vmtf-score-default-repeat-20260906.json)
- [Ratios and counter comparisons](benchmark_results/vmtf-score-summary-20260906.json)
- [Release deadlines](benchmark_results/vmtf-score-limits-release-20260906.json)
- [Sanitizer deadlines](benchmark_results/vmtf-score-limits-asan-20260906.json)

Reproduce timing with `tests/benchmark.py`, the solver templates and input paths
in the reports, and separately built baseline/candidate executables. Ratios use
each input's median process CPU before taking a geometric mean across inputs.
Reproduce validation with `make -j4 all test` and `make -j4 MODE=debug all test`,
then run `tests/validate.py --solver <binary> --checker <drat-trim> --cases 100
--seed 20260921` and `tests/check_deadlines.py --solver <binary>` for each build.
