# Longer frozen development subset — 2026-09-08

BSAT and Kissat each complete 4/8 runs on the four-input, two-repeat screen.
All eight conclusive answers pass independent validation; there are no errors.
The longer allowance reveals solvable cases, but BSAT remains slower on both
solved inputs. No runtime policy or default changes in this milestone.

The [frozen policy](benchmark_results/longer-subset-20260908.policy.json) sets
60 process CPU seconds, a separate 90-second wall fuse, two serial repetitions
and seed 2026090868. Both solvers write binary proof streams. These are already
inspected development inputs, not fresh holdouts. Local macOS ARM64 timing did
not overlap local builds, tests or fuzzing; remote Ubuntu acceptance ran on a
separate host. Raw results pin executable/input hashes. The reported dirty tree
contains documentation/experiment artifacts and unrelated `.zvec-grep/`; no C
runtime source or measured binary was changed during the runs.

| Input | BSAT outcomes; median CPU | Kissat outcomes; median CPU |
|---|---|---|
| Planning `16c999` | 2 UNKNOWN; 60.144 s | 2 UNKNOWN; 60.040 s |
| Exact cal3 depth-16 positive snapshot | 2 checked UNSAT; 22.703 s | 2 checked UNSAT; 1.054 s |
| Influence maximization `931621` | 2 checked SAT; 56.892 s | 2 checked SAT; 24.496 s |
| Scheduling `c05d4d` | 2 UNKNOWN; 60.250 s | 2 UNKNOWN; 60.060 s |

The planning case here is the previously difficult `16c999`, **not** the larger
`2e0588` planning case BSAT solved in about 6.1 seconds in the
[earlier holdout](EXPANDED_HOLDOUT_BASELINE.md). The cal3 snapshot has the output
assumption as a permanent unit. Its fresh one-shot solving policy differs from
the public certified retained-query history; this result does not erase that
history's 30-second UNKNOWN result.

Mean CPU PAR-2, assigning each unfinished run 120 seconds, is **79.899 seconds
for BSAT versus 66.388 for Kissat**. Mean wall PAR-2, using the separate 180-second
penalty, is 110.742 versus 96.425 seconds. Peak solver RSS is 84,279,296 versus
131,481,600 bytes. RSS excludes validation. Equal solved counts do not establish
equal performance or representative competition ranking.

BSAT's repeated cal3 proofs have identical hashes and each verifies through
DRAT-to-LRAT and CakeML; median checking adds **7.766 wall seconds**, versus
0.592 for Kissat. Checking is outside solver timing and uses explicit
2,048/512 MB checker heap/stack settings with a 600-second deadline. SAT models
are checked against the original clauses. CPU-limit terminations remain UNKNOWN;
the observed soft-limit overshoot is recorded, not rounded into an exact bound.

The [raw report](benchmark_results/longer-subset-20260908.json) and
[derived summary](benchmark_results/longer-subset-summary-20260908.json) retain
per-run times, verification outcomes, proof hashes and counters when available.
Externally killed runs may lack final counters, and their proof prefixes are not
accepted UNSAT certificates. Reproduce with `tests/benchmark.py`, the frozen
manifest and recorded command templates, setting `BSAT_DRAT_TRIM`,
`BSAT_CAKE_LPR`, `BSAT_CAKE_HEAP_MB=2048` and `BSAT_CAKE_STACK_MB=512`.
