# Dynamic learned-clause quality

`--dynamic-lbd` re-evaluates the LBD of learned clauses encountered as conflicts
or expanded reasons during first-UIP analysis. LBD counts distinct non-root
decision levels represented in a clause. When a later conflict uses the clause
with fewer levels, its stored score decreases. Existing reduction ranking and
glue retention then use that improved score.

The option defaults to disabled. It changes learned-clause retention and search
trajectories, and requires workload-specific evaluation.

## Implementation and reference

Scores only decrease. Original clauses and clauses already within the configured
glue threshold are skipped. The calculation uses separate decision-level marks,
so it cannot overwrite the variable marks used by first-UIP analysis. No clause
literals, reasons, proof additions or restart averages are changed directly.
`LBD improvements` reports the number of successful score decreases.

[Glucose conflict analysis](https://github.com/audemard/glucose/blob/master/core/Solver.cc)
recomputes learned-clause LBD and improves retention when the score falls. BSAT's
experiment adopts the score update, but does not reproduce Glucose's temporary
freezing or permanent-clause strategy. It accepts any strict score decrease;
the referenced Glucose implementation requires a decrease greater than one.
These differences matter when interpreting performance comparisons.

## Tests

A deterministic propagation graph produces a learned reason and a learned conflict
clause spanning two decision levels, after both were assigned historical LBD four.
Analysis must lower both scores to two, preserve the asserting clause and backjump,
and clear its scratch marks. After backtracking, an aggressive reduction keeps the
new glue clauses. Controls with the option disabled or original clauses preserve
the old scores. The validator additionally exercises dynamic LBD with deletion
at every conflict, and with binary proofs plus equivalence/BVE/BCE preprocessing.

## Benchmark protocol

The initial screen uses the existing 28 development competition instances, one
run per version, three CPU seconds and five wall seconds. Both versions verified
three instances, with no reported errors. Peak RSS was 58408960 bytes for the
baseline and 64667648 for the candidate. This screen does not demonstrate an
improvement in solved count. It preceded addition of the diagnostic update counter;
the recorded executable hashes distinguish it from the final build.

A separate reproducible stress sample contains four random 3-SAT formulas at each
of 180, 220 and 260 variables, at clause/variable ratio 4.26, plus pigeonhole
formulas with seven, eight and nine holes. The generator uses seed 20260911 and is
saved with the evidence. The sizes and seed were chosen before measuring these
instances. They are development tests, not disjoint held-out competition families.

Stress comparisons run the committed baseline and dynamic candidate with five
CPU seconds, a seven-second wall limit and two repetitions. Kissat is included
as an external reference with the same wall limit; it does not have BSAT's internal
CPU cap, so timeout scores are not a strict equal-resource competition comparison.
All timing runs are serial without concurrent builds or tests. SAT models and
UNSAT certificates are independently checked outside the solver timing.

## Stress results

| Measure | Baseline | Dynamic LBD | Kissat |
|---|---:|---:|---:|
| Verified runs | 28/30 | 28/30 | 30/30 |
| Mean diagnostic PAR-2, seconds | 1.543 | 1.333 | 0.370 |
| Maximum RSS, bytes | 7716864 | 8585216 | 11812864 |

Both BSAT versions failed to finish the nine-hole pigeonhole instance in either
repeat. One dynamic run reached the external wall limit with 4.86 CPU seconds
and no final counters; this is not a completed answer. Kissat solved it in about
0.25 CPU seconds. There were no reported invalid completed answers or certificates.

Across the fourteen instances completed by both BSAT versions, the geometric
mean ratio of median dynamic/baseline CPU time was 0.8968 (10.3% lower). The sum
of those medians fell from 8.846 to 5.567 seconds, largely due to the hardest
random UNSAT cases. These descriptive aggregates exclude the unsolved instance
and include very short runs. Search trajectories differ, so they do not measure
throughput at identical work.

| Selected instance | Baseline median CPU | Dynamic median CPU |
|---|---:|---:|
| Random v260-2, UNSAT | 4.700 s | 2.167 s |
| Random v260-3, UNSAT | 1.839 s | 1.162 s |
| Pigeonhole 9 into 8, UNSAT | 0.550 s | 0.299 s |
| Random v220-1, SAT | 0.142 s | 0.327 s |
| Random v220-2, SAT | 0.377 s | 0.550 s |

The mixed results support retaining an opt-in experiment, not changing the default
or claiming competition parity. The competition-file screen had no solved-count
gain, and the external reference solved the hardest stress instance that BSAT
could not. Further evaluation should use longer limits and previously unmeasured
application families before selecting a default retention policy.

## Reproduction and evidence

- [Initial competition screen](benchmark_results/dynamic-lbd-competition-screen-20260906.json)
- [Final stress comparison](benchmark_results/dynamic-lbd-stress-comparison-20260906.json)
- [Per-instance derived summary](benchmark_results/dynamic-lbd-summary-20260906.json)
- [Stress generator](benchmark_results/lbd-stress-corpus-20260906.py)
- [Input manifest](benchmark_results/dynamic-lbd-stress-manifest-20260906.json)

The baseline executable came from commit
`258cb3eead12ae0552d81cd4e431e00a886b6506`. The final candidate SHA256 is
`f15d323fc53934716f12b4db5cdf9e3e10c099a935bd708ccc585bbaba51878a`.
Commands, platform, input hashes, proof-checker hash and external solver hash are
stored in each raw comparison. The final executable matches the measured stress
candidate. Final validation passed 4181 release and 4181 ASan/UBSan solves across 37
configurations, plus both unit suites. See the
[validation record](benchmark_results/dynamic-lbd-validation-20260906.json).
