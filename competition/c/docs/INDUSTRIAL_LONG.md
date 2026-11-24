# Longer stratified industrial evaluation

The frozen sample contains 12 previously unrecorded inputs from 11 families,
with four inputs in each size band: 50 KB–1 MB, 1–10 MB and 10–100 MB.
Actual sizes range from 195 KB to 42.45 MB; the largest variable count is
698,913, and the largest clause count is 2,148,797. Selection excludes filenames
and content hashes in prior JSON history, shuffles families/files with fixed
seeds and balances families within each stratum. At-least-two-sol appears in
both the middle and large strata, so the global sample has 11 distinct families.

Each of three profiles runs every input twice: BSAT control, that same binary
with `--factor`, and pinned Kissat. The BSAT profile explicitly enables chronology,
congruence, equivalence (100-million work budget) and alternating search; VMTF
is off. BSAT uses 60 seconds of solving-thread CPU, excluding input parsing.
Kissat's pinned `--time=60` implementation uses a 60-second POSIX wall-clock
alarm set during argument parsing. The outer runner enforces 75 seconds wall
for every process. These are different clock bases; the frozen policy's
`cpu_limit` field applies to BSAT, not to Kissat. The audited record clarifies
the effective limits without changing commands or outcomes. Binary/input
hashes, options and trial order were frozen before timing. Profiles and inputs
were not tuned after observing this sample. These are evaluation profiles, not
a claim about every default configuration or the full SAT Competition corpus.

Trials run serially. The coordinator was paused for local implementation checks
and the separate memory experiment. A child-launch race made raw trial 9
(zero-based) potentially overlap validation; its exclusion and identical rerun
were specified before observing its result. The raw report is retained. Use
`industrial-long-audited-20260908.json` for analysis and the pause audit for
the original/replacement records. No outcome-based exclusion was made.

Only independently checked answers count as solved. SAT models are checked
against original input; UNSAT proofs pass DRAT-to-LRAT conversion and the
CakeML checker. UNKNOWN receives the timeout penalty and is not answer evidence.
Certificate validation time is recorded separately and in end-to-end time.

| Profile | Verified trials / 24 | Errors | Mean PAR-2 (s) | Maximum solver RSS (MB) |
| --- | ---: | ---: | ---: | ---: |
| control | 2 | 0 | 137.50 | 760.41 |
| factor | 2 | 0 | 137.50 | 866.66 |
| kissat | 6 | 0 | 118.11 | 1024.36 |

Per-input cells give verified repetitions out of two, followed by the answer.
A dash means both trials remain UNKNOWN.

| Family / input prefix | Variables | Clauses | Control | Factor | Kissat |
| --- | ---: | ---: | --- | --- | --- |
| relativized-pigeon-hole / e12874b1 | 850 | 15,645 | — | — | — |
| ramsey-numbers / 847c8843 | 171 | 7,752 | — | — | — |
| random-csp / 9b998be0 | 175 | 14,577 | — | — | 2/2 SAT |
| p-center / abadaf0e | 8,357 | 32,177 | 2/2 UNSAT | 2/2 UNSAT | 2/2 UNSAT |
| at-least-two-sol / 4ba2c1aa | 2,352 | 219,297 | — | — | — |
| planning / 16c999d0 | 50,277 | 283,903 | — | — | 2/2 UNSAT |
| sum-of-3-cubes / 7a044c99 | 33,163 | 161,461 | — | — | — |
| gray_codes / 43687a48 | 9,072 | 73,350 | — | — | — |
| at-least-two-sol / cdf559a1 | 103,530 | 1,541,429 | — | — | — |
| station-repacking / e2d2b011 | 31,294 | 802,297 | — | — | — |
| tseitin-formulas / f3abc375 | 319,200 | 1,273,608 | — | — | — |
| risc-instruction-removal-golcrest / c0bd86bd | 698,913 | 2,148,797 | — | — | — |

All 10 conclusive trials pass their independent checks. No conflicting
SAT/UNSAT answers occur on the same input. This is bounded empirical assurance,
not a proof that the solver is sound for every possible input.

The most expensive accepted validation takes 148.57 seconds
for kissat on planning; solving takes 47.33
seconds and end-to-end cost is 195.90 seconds. Solver-only
timings therefore do not describe the full cost of a certified answer.

Two repetitions and this family/size sample do not establish universal speed
rankings. All measurements are local macOS ARM64 results, not target Linux
server PMU measurements. The separate reg-n development gain remains valid
but is not counted in this fresh sample. Factoring remains opt-in.

Decision: retain factoring as an experimental option. It solves the known reg-n
input quickly, but adds no verified completions in this fresh sample and has
higher maximum RSS than control here (866.66 versus 760.41 MB). Kissat completes
planning and random CSP that both BSAT profiles leave UNKNOWN. Improving those
search gaps needs additional measured work; this batch does not establish
production readiness for arbitrary industrial inputs.

This is a shared macOS desktop, not an isolated target server. No overlapping
BSAT builds/tests/fuzzing or other solver workloads were scheduled during the
accepted timed trials. Ordinary desktop activity and operating-system effects
remain possible sources of variation. Checker identities are recorded in
`industrial-long-validation-tools-20260908.json` and match earlier independently
verified application-certificate records.
