# Evaluate per-decision random phases

The preceding saved-phase experiment improved battleship, but the existing
`--no-random-phase` switch matched or beat that prototype on the targeted inputs.
This milestone evaluates the simpler policy across a broader development sample
before deciding whether to change the default.

## Method

The corpus is the union of the 28 reused development inputs and the 16
additional inputs selected in the default-branching milestone: 44 distinct
inputs, fixed before timing. Each profile runs once per input in seeded serial
order, with ten solving CPU seconds and fifteen wall seconds. Both profiles use
the same preserved BSAT executable; the only difference is `--no-random-phase`.
Phase saving, probing, heap ordering, restarts, target rephasing and reduction
keep their existing settings. No local builds or tests run concurrently.

The runner verifies every input against the corpus manifest before starting.
SAT models and UNSAT proofs are checked independently outside solver timing.
Process CPU includes startup, parsing and proof output; the solving limit
excludes parsing. PAR-2 gives unfinished runs twice the wall timeout. These
short, selected development inputs are not a held-out competition evaluation.
The two corpus subsets are reported separately as well as together.

The measured baseline is the retained C implementation at `c7d44b6` (runtime last
changed in `47d38cb`), executable SHA256
`8237cde01317a46a16053817485fb4f77aed08d2f96e3dbaf11883a50fc49a7b`.
Random phases use a fixed 1% probability and the existing reproducible RNG seed.
There is no adaptive stuck-state detection or frequency adjustment in the C
implementation.

## Documentation corrections

The C feature summary incorrectly described adaptive random-phase frequency.
It now describes the actual fixed-probability implementation. The documentation
index no longer equates randomization with completeness, and the 2025 Python
phase-selection report is explicitly scoped as historical evidence. A timeout
under a conflict limit does not establish an infinite loop or a correctness
requirement to randomize. Unsupported common-frequency claims about other
solvers have also been removed from that historical overview.

## Evidence

- [Fixed 44-input manifest](benchmark_results/no-random-corpus-20260907.json)
- [Complete comparison](benchmark_results/no-random-broad-20260907.json)
- [Earlier targeted comparison](benchmark_results/saved-phase-targeted-20260907.json)

## Results and default decision

| Corpus | Previous default | No random phases | Previous PAR-2 | No-random PAR-2 |
| --- | ---: | ---: | ---: | ---: |
| Reused inputs | 4/28 | 4/28 | 25.9653 | 25.9253 |
| Additional inputs | 3/16 | 3/16 | 24.8978 | 24.6166 |
| Combined | 7/44 | 7/44 | 25.5771 | 25.4494 |

There are no gained or lost solves and zero errors. On the matched solved
inputs, battleship drops from 5.801 to 2.458 process CPU seconds, belpyramid from
6.754 to 5.619, and the additional Hamiltonian input from 0.356 to 0.164.
The older, very short Hamiltonian input regresses from 0.013 to 0.075 seconds.
P-center, scheduling and multiplier-verification remain very short in both
profiles. Peak process RSS is 102.14 MB with the previous default and 84.30 MB
without random phases. These are observed maxima across the sample, not a memory
bound or a guarantee for each instance.

The substantial battleship and belpyramid gains repeat the preceding targeted
comparison. Retain the simpler no-random policy as the default: it preserves all
measured solves and improves total time on the completed cases despite the
short-case regression. Overall wall PAR-2 improves by about 0.5%, since the
37 unresolved inputs dominate that score. This does not establish competition
parity or a universal improvement across unseen inputs.

`default_opts().random_phase` is now false. `--random-phase` or an explicit true
API option restores the previous policy, including its default probability of
0.01. Setting `--random-prob` alone changes the configured probability, not the
enable flag. The change applies to both heap and VMTF; earlier randomized VMTF
profiles can be reproduced by explicitly adding `--random-phase`.

[Subset summary and solve changes](benchmark_results/no-random-summary-20260907.json).

## Validation

The new default passes all 19 C test executables in both release and ASan/UBSan
builds. The phase regression checks that an ordinary default decision neither
uses a random override nor consumes RNG state, and that explicitly enabling
random phases restores the expected seeded choice. Existing randomized phase
and binary-minimization integration fixtures now request their intended random
policy explicitly instead of depending on a global default.

The independent matrix adds four explicit randomized configurations, including
VMTF/binary proofs, SCC/BVE/BCE, inprocessing and local search. All 58
configurations pass on 13 fixed and 50 random formulas (seed 20261001): 3654
truth-table/model/proof checks per release and sanitizer build. Both builds also
pass 24 short-deadline cases, including the explicit randomized VMTF policy;
maximum observed overrun is 0.001 seconds at the reported precision.

A final six-input check at 10000 conflicts compares three policy pairs:
old no-random versus new default, old randomized heap versus new explicit
`--random-phase`, and old randomized VMTF versus new `--vmtf --random-phase`.
All 18 pairs match status, verification outcome, and every recorded search and
arena-memory counter except PID, reported time/rates and deadline-clock reads.
There are zero errors. This verifies policy compatibility, not a new timing
claim; some runs intentionally stop at the conflict cap.

- [Final policy comparison](benchmark_results/no-random-final-modes-20260907.json)
- [Counter comparison](benchmark_results/no-random-final-modes-summary-20260907.json)
