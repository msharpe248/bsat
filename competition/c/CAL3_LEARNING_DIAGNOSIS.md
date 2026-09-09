# cal3 learning-policy diagnosis — 2026-09-08

This is a development ablation of the exact depth-16 positive one-shot snapshot,
not a retained API comparison or a default-promotion experiment. The frozen
matrix uses existing options: seven BSAT configurations and six pinned CaDiCaL
configurations, two repetitions each, ten process CPU seconds and a fifteen-second
wall fuse. A separate 10,000-conflict screen collects completed diagnostic counters.
Both screens use binary proofs and independently check every conclusive answer.

## What the source audit establishes

BSAT's `solver_analyze` derives a first-UIP clause. The default recursive minimizer
has a depth-128 guard, per-clause inspection budget, abstract-level filter and
conservative treatment of implicit binary reasons. The opt-in iterative traversal
follows those reasons and shares successful dependencies across source literals.
A prior candidate adding binary reasons to the recursive default was already
[rejected for regressions](RECURSIVE_BINARY_MINIMIZATION_EXPERIMENT.md).

The separate optional binary-resolution pass only considers clauses of at most
30 literals with LBD at most six. It resolves binaries containing the asserting
literal. This is different from CaDiCaL's same-decision-level UIP shrinking,
which can replace blocks of literals and integrate recursive removal. CaDiCaL's
`--no-minimize` leaves shrinking active; the matrix therefore disables each pass
separately and both together. Its recursive minimizer also uses trail/level
filters and remembers unsuccessful attempts as well as successful removals.

BSAT reduction protects binaries, low-glue clauses and locked reasons; remaining
candidates are ranked by LBD/activity. Keeping 90% still permits the independent
maximum-LBD deletion rule. The separate billion-conflict reduction interval
suppresses reduction throughout this bounded experiment. CaDiCaL has different
usage/tier-based protection, schedules and ranking, so equal retention fractions
would not represent equivalent clause databases.

There is an exact counter mismatch in the earlier structural diagnosis:
CaDiCaL increments `learned.literals` **before** shrinking/minimization, while
BSAT increments `learned_literals` **after** both minimization passes. Their raw
totals are not matched measurements. Even corrected totals would follow different
conflicts, restarts and formulas; equal conflict counts are not equal work.

Sources: BSAT `src/solver.c` and `src/minimize.c`; pinned CaDiCaL
`c60730422e758ef1cebe7aeddf2dda31c996bf04`, `src/analyze.cpp`, `minimize.cpp`,
`shrink.cpp`, `reduce.cpp` and `options.hpp`. The benchmark records binary hashes.

## Measurements

All 14 BSAT timed runs remain UNKNOWN; all 12 CaDiCaL timed runs produce
independently checked UNSAT. No errors occur. CaDiCaL proves the query even when
both minimization and shrinking are disabled, or separately when reduction is
disabled. These ablations do not identify any one of those features as the
complete explanation of BSAT's gap.

| Timed configuration | Checked / runs | Median process CPU |
|---|---:|---:|
| BSAT default | 0/2 | 10.013 s |
| BSAT no minimization | 0/2 | 10.017 s |
| BSAT iterative | 0/2 | 10.006 s |
| BSAT binary resolution | 0/2 | 10.013 s |
| BSAT iterative plus binary | 0/2 | 10.007 s |
| BSAT retain 90% | 0/2 | 10.020 s |
| BSAT no reduction in screen | 0/2 | 10.010 s |
| CaDiCaL default | 2/2 | 0.700 s |
| CaDiCaL plain | 2/2 | 2.593 s |
| CaDiCaL plain, no minimization | 2/2 | 2.888 s |
| CaDiCaL plain, no shrinking | 2/2 | 2.341 s |
| CaDiCaL plain, neither minimization nor shrinking | 2/2 | 3.146 s |
| CaDiCaL plain, no reduction | 2/2 | 2.950 s |

The fixed-conflict screen returns 26 UNKNOWNs with no errors. BSAT reports
10,000 conflicts; CaDiCaL reports 10,000 or 10,001 depending on its polling point.
Within each configuration, repeated proof-prefix hashes and the listed integer
counters agree. Prefixes are not accepted UNSAT certificates.

| BSAT at 10,000 conflicts | Learned literals after minimization | Removed by minimization | Deleted clauses |
|---|---:|---:|---:|
| Default | 766,096 | 36,401 | 7,526 |
| No minimization | 731,636 | 0 | 7,456 |
| Iterative | 258,674 | 275,073 | 7,500 |
| Binary resolution | 664,695 | 29,080 | 7,427 |
| Combined | 260,388 | 229,939 | 7,513 |
| Retain 90% | 860,016 | 52,960 | 2,419 |
| No reduction | 759,877 | 36,316 | 0 |

Iterative minimization lowers the learned-literal total by about 66% and counts
2,057,673 binary-reason steps, but does not finish within ten CPU seconds. It
also takes 0.508 CPU seconds to reach 10,000 conflicts versus default's 0.422.
Disabling minimization changes the trajectory enough to produce a smaller total
than default despite removing no literals. This demonstrates why aggregate
clause size cannot identify a winning minimizer.

Retaining more reduces deletions as intended, but the timed peak solver RSS
rises from 23.66 MB to 85.51 MB; suppressing reduction reaches 85.38 MB. Neither
adds a solve. These measurements support leaving the defaults unchanged, not
claiming the alternative modes can never help another workload.

CaDiCaL plain records 326,777 pre-minimization learned literals at its conflict
stop, 87,325 removed by shrinking and 71,323 removed inside the shrinking pass.
With both passes disabled it records 364,399 pre-minimization literals and zero
removals, yet still proves the full query in 3.146 CPU seconds. It retains other
search mechanisms, including on-the-fly self-subsumption: that completed run
records 2,525 such strengthenings. BSAT's similarly named routine instead
removes subsumed clauses after learning a small clause; it is not that same
conflict-analysis transformation.

## Decision and reproduction

No runtime change is promoted. The source audit and controlled negative results
narrow the next investigation to the remaining conflict-analysis and search
trajectory differences. They do not establish self-subsumption, branching or
restarts as the missing cause; those would require their own frozen ablations.
The separate [longer screen](LONGER_DEVELOPMENT_SUBSET.md) already shows BSAT can
prove this one-shot query with 22.7 CPU seconds, so ten-second UNKNOWN is a
performance result, not evidence of an incorrect answer.

Run `tests/benchmark_cal3_learning.py` with `--bsat`, `--cadical`, `--input` and
`--output`; add `--conflicts 10000` for the diagnostic. Set `BSAT_DRAT_TRIM` and
`BSAT_CAKE_LPR` to the pinned checker tools. The driver freezes its exact commands,
input/binary hashes and seed before launching each serial screen. Checking has
its own 120-second deadline and default 512/128 MB checker settings. Local timing
did not overlap local tests/builds; documentation edits and remote CI are separate.
These experimental workspaces report dirty because of docs/results and unrelated
`.zvec-grep/`; the measured C binary was unchanged.

Evidence: [timed runs](benchmark_results/cal3-learning-20260908.json),
[fixed-conflict runs](benchmark_results/cal3-learning-fixed-work-20260908.json),
[derived summary](benchmark_results/cal3-learning-summary-20260908.json), and
adjacent `.policy.json` files. Finite checked runs do not prove universal solver
soundness or justify a representative performance claim.
