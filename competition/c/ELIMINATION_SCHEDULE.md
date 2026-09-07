# Dynamic variable-elimination scheduling

The dynamic queue is **rejected**: it adds no verified solves in the seven-input
screen and repeatedly regresses on belpyramid, even with a larger budget. The
retained changes are three elimination statistics and independent BVE model and
cutoff/resume tests. The implementation and candidate-only scheduling oracle are
preserved in the [experimental patch](benchmark_results/elim-schedule-rejected-20260907.patch).

## Motivation

The existing BVE pass visits variables once in numeric order. A rejected variable
is not reconsidered if later eliminations make it cheaper. The candidate uses a
pass-local priority queue with live positive-occurrence × negative-occurrence
costs, prioritizes pure variables, and uses variable ID to break equal-cost ties.
Changes to neighboring occurrence counts update priorities and requeue previously
rejected candidates. Variables already assigned or eliminated are skipped.

This follows the scheduling idea in [MiniSat's simplifier](https://github.com/niklasso/minisat/blob/master/minisat/simp/SimpSolver.h),
which uses a product of occurrence counts and updates its elimination heap after
formula changes. BSAT's implementation retains its existing occurrence cap,
resolution-growth test, staged resolvents, proof emission and model extension;
it does not implement MiniSat's entire simplification pipeline.

The discovery screen on larger hardware-verification and ktf inputs tried both
default search and existing BVE with probing disabled and a 100-million-work
budget. All four runs reached the 15-second solving CPU limit with UNKNOWN;
no additional solve was established. This is a motivation for improving BVE's
schedule, not evidence that merely increasing its budget closes the search gap.
[Discovery results](benchmark_results/elim-schedule-discovery-20260907.json).

## Implementation and resource behavior

Queue storage is local to one pass: a four-byte heap entry and a twelve-byte
count/position entry per variable, freed on every exit. Products are calculated
in 64 bits. Initialization and occurrence updates charge work and check limits.
Updates scan the removed parents and newly stored resolvents; no formula-wide
rescan is needed after each successful elimination. Deleted parent arena storage
remains available during this pass. An interruption during priority bookkeeping
can discard the local queue after the already-committed, valid elimination.
A later pass rebuilds occurrences and priorities from the remaining formula.

Preprocessing budgets, phase order, and the default-disabled `--elim` option are
unchanged. The current shared budget can be consumed by probing or occurrence
construction before BVE attempts a pivot; benchmark profiles disable probing and
set an explicit larger budget to expose scheduling behavior. New CLI statistics
report eliminated variables, added resolvents and removed clauses.

## Verification

The archived candidate test compares 512 small formulas with an independent linear scheduling
oracle that recomputes live counts by scanning the whole formula. It compares
elimination order and emitted text/binary proof bytes, and checks every residual
model against the original formula after model extension. It exercises 89
retries of previously rejected variables. Another 1,539 cutoff/resume cases check
scratch cleanup and final truth-table/model agreement after partial preprocessing.
The oracle shares the existing resolution primitive; independent full-solver
formula and proof validation therefore remains necessary.

Both release and ASan/UBSan debug builds pass **42 C test executables** and
**3,139 independently checked solves**, 73 configurations and seed 20261202.
Both also pass 42 deadline cases with maximum observed CPU overrun 0.000 seconds.
[Validation record](benchmark_results/elim-schedule-validation-20260907.json).
Three deliberate mutations—sum instead of product priority, stale removed-parent
counts, and missing upward heap repair—are rejected by the fresh-count oracle.
[Mutation record](benchmark_results/elim-schedule-mutations-20260907.json).

With BVE disabled, twelve text/binary proof comparisons on six reused competition
inputs preserve proof bytes and all 24 selected counters at 1,000 conflicts.
[Default trace guard](benchmark_results/elim-schedule-default-traces-20260907.json).
Incomplete prefixes are not certificates for UNKNOWN answers. These finite tests
do not establish universal soundness.

## Performance and decision

The [seven-input comparison](benchmark_results/elim-schedule-targeted-20260907.json)
uses `--elim --no-probing --preprocess-budget 100000000 --time 15` for both
schedules, text proofs, a 20-second external wall limit and two repetitions per
input, seed 20261203. The baseline differs from `d4538d2` only by the same three
statistics lines added to the candidate. **Both verify 6/14 runs**, with zero
errors. They solve battleship, belpyramid and hamiltonian twice each. Both reach
the time limit on hardware-verification, ktf, hamiltonian-cycle and
multiplier-circuits. Mean wall PAR2 worsens from 23.557 to 24.343 seconds; maximum
RSS rises from 70,074,368 to 71,516,160 bytes.

| Input | Numeric-order variables eliminated | Queue variables eliminated |
| --- | ---: | ---: |
| battleship | 0 | 0 |
| belpyramid | 20,328 | 12,672 |
| hamiltonian | 0 | 0 |
| hamiltonian-cycle | 0 | 0 |
| hardware-verification | 5,422 | 5,659 |
| ktf | 14,622 | 16,440 |
| multiplier-circuits | 2,448 | 2,448 |

Counts repeat exactly within each profile. More elimination on hardware and ktf
does not produce a completed answer at this limit. Battleship and hamiltonian
have unchanged search counters and no eliminated variables; their varied wall
times must not be attributed to changed elimination behavior.

Belpyramid is the material regression: median process CPU is **1.395 seconds
numeric versus 5.626 seconds queue**, with 27,936 versus 39,687 conflicts.
Numeric order removes 142,182 clauses while the queue removes 55,745.

## Budget diagnosis and confirmation

A [one-conflict diagnostic](benchmark_results/elim-schedule-budget-diagnostic-20260907.json)
shows 100,017,456 total work units for queued belpyramid versus 5,874,501 for
numeric order. The queue reaches the 100-million preprocessing allowance.
These counts include work before and after BVE through the first conflict; they
are not an isolated preprocessing CPU measurement. The other two diagnostic
inputs, hardware and ktf, stay well below the allowance in both schedules.
All six diagnostic runs return UNKNOWN; their prefixes are not certified answers.

With a [one-billion allowance](benchmark_results/elim-schedule-expanded-budget-20260907.json),
the queue reaches the first conflict after 343,254,049 total work units, below
the new limit, and eliminates 19,404 variables. Numeric order remains at
5,874,501 work units and 20,328 eliminated variables. Both runs are UNKNOWN at
the one-conflict cap. This permits a direct test without the earlier budget cutoff.

The [full-solve confirmation](benchmark_results/elim-schedule-budget-confirmation-20260907.json)
uses the one-billion allowance, the same 15-second solving CPU/20-second wall
limits, two repetitions per schedule and seed 20261206. Both verify both UNSAT
answers. Median CPU remains **1.2446 versus 4.1463 seconds**, about **3.33 times
slower with the queue**. Conflict counts are 27,936 versus 39,579. Peak RSS is
36,126,720 versus 44,498,944 bytes. The slowdown is therefore not explained solely
by the earlier budget cutoff. This rejects the dynamic queue as a replacement
for the existing BVE schedule.

All performance jobs ran serially, separately from builds, validation and
sampling. CPU includes parsing and proof output; independent checking is outside
solver timing. UNKNOWN incurs twice the wall limit in PAR2. Inputs are reused
development cases from SAT Competition 2025 on one macOS arm64 machine, not a
held-out competition evaluation. This experiment adds no evidence of closing
the previously recorded larger-problem solve gap against Kissat.

## Retained work and reproduction

`elim.c` is restored exactly to `d4538d2`. The three printed elimination counters
remain. `tests/test_elim_preprocess.c` retains 512 independent original-truth and
residual-model cases plus 1,539 cutoff/resume cases, without asserting the
rejected scheduling policy. Restored release and ASan/UBSan suites are recorded
separately in the validation JSON. The 3,139-per-build validation above applies
to the candidate. The earlier milestone's checks apply to the unchanged restored
algorithm; they are not new validation runs from this experiment.

Apply the archived patch with `git apply --unidiff-zero` to reproduce the
candidate and its fresh-count scheduling oracle. Build/test using
`make -C competition/c all test`, repeating with `MODE=debug`. Raw JSON records
exact solver commands, input/executable/checker hashes, limits and seeds. The
baseline executable is preserved at `/tmp/bsat-elim-schedule-baseline`; the tested
candidate is preserved at `/tmp/bsat-elim-schedule-candidate`. Recorded commands
show the actual paths used before restoring the numeric implementation.
