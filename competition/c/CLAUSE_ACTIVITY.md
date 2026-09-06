# Rejected clause-activity experiment

A conflict-based recency policy was implemented, tested and benchmarked, then
removed because the measurements did not support retaining it. The resulting
production change validates the C API's `clause_decay` parameter: it must be
finite and in (0,1]. Previously NaN, infinity and out-of-range values could enter
reduction scores. The CLI does not expose this parameter.

## Experiment

The historical policy increments expanded reasons by one, does not reward the
initial conflict or a newly learned clause, and discounts retained eligible clauses
only during reduction. With decay 0.999 and reductions every 2000 conflicts, a
continuously retained eligible clause's old score takes roughly 1.39 million
conflicts to halve. Locked and glue clauses bypass that reduction-time discount.

The candidate rewarded learned conflict clauses, expanded learned reasons and
newly learned clauses using a shared increment. That increment grew after each
learned conflict, making newer uses more valuable. The default factor 0.999 gave
an effective half-life of about 693 conflicts. Reduction omitted the historical
second discount when the experiment was enabled.

This followed the shared-increment technique in
[MiniSat's clause activity methods](https://github.com/niklasso/minisat/blob/master/minisat/core/Solver.h),
while retaining BSAT's LBD-first reduction ordering. Uniform rescaling kept float
scores and the double increment finite. The candidate's decay step compared a
threshold before dividing, including for tiny positive API factors.

Dedicated tests used a real propagated conflict to check rewards for the conflict
and reason, equal-LBD reduction ranking, rescaling, extreme decay values and
repeated solves. The independent validator added three recency combinations.
The candidate's implementation and tests are preserved as an experimental patch;
`--recency-activity` is not part of the retained solver.

## Results

| Sample | Baseline | Dynamic LBD alone | Recency alone | Combined |
|---|---:|---:|---:|---:|
| Existing generated sample: verified runs | 28/30 | Not run | 28/30 | 28/30 |
| Same: mean diagnostic PAR-2 | 1.552 | Not run | 1.582 | 1.299 |
| Smaller competition sample: verified runs | 2/16 | 2/16 | Not run | 2/16 |
| Fresh random sample: verified runs | 20/24 | 20/24 | Not run | 20/24 |
| Fresh sample: mean diagnostic PAR-2 | 2.882 | 2.661 | Not run | 2.740 |

Recency alone did not improve the first sample's aggregate. The initial combined
result looked promising against baseline, but did not include dynamic LBD alone
and therefore could not isolate recency's contribution. The fresh-sample control
showed combined mode slower overall than dynamic LBD alone. For example, its
v260-3 SAT instance took median CPU time 1.215 seconds with combined mode versus
0.086 seconds with dynamic LBD alone; baseline took 1.259 seconds. All modes
failed to finish the same two fresh random inputs. There were no reported invalid
completed answers or certificates in these comparisons.

This evidence supports discarding the candidate, rather than promoting a change
based on its first favorable aggregate. It does not prove recency scoring cannot
help another solver or workload. No competition-performance improvement is claimed.

## Method and evidence

The first comparison reused the 15-instance generated development sample from the
dynamic-LBD milestone. The competition screen used the existing sixteen smaller
competition inputs. The fresh comparison used twelve random 3-SAT formulas from
seed 20260912, with four formulas each at 180, 220 and 260 variables and density
4.26. Seed, sizes and density were fixed before measurement. Repeated pigeonhole
inputs from the generator were excluded. These are new instances of known
synthetic families, not held-out application families.

Generated comparisons used two repetitions, five CPU seconds and seven wall
seconds. The competition screen used one repetition, three CPU seconds and five
wall seconds. Runs were serial without concurrent local builds or tests. Models
and UNSAT proofs were independently checked outside timing. UNKNOWN results are
not completed answers; these short-budget PAR-2 values are diagnostics, not
competition scores. Search trajectories differ between policies.

- [First generated comparison](benchmark_results/clause-activity-stress-20260906.json)
- [Competition comparison](benchmark_results/clause-activity-competition-20260906.json)
- [Fresh random comparison](benchmark_results/clause-activity-fresh-20260906.json)
- [Fresh input manifest](benchmark_results/clause-activity-fresh-manifest-20260906.json)
- [Generator, with optional seed argument](benchmark_results/lbd-stress-corpus-20260906.py)
- [Rejected implementation and tests](benchmark_results/clause-activity-rejected-20260906.patch)

The patch applies to baseline commit `cdb57a91a9f67c8cbacc93a281e075ff7bd5bae6`.
Exact commands, executable/input hashes, counters, resource use and proof-checker
hashes are recorded in the raw results.

## Retained validation fix

`test_options` checks valid factors including DBL_MIN and 1 through construction,
model validation and repeated solves. It rejects zero, negative factors, values
above one, infinities and NaN. The solver's search/retention implementation remains
otherwise identical to the baseline. Earlier milestone records also correct an
executable-count mistake: the last test's 11/11 summary counted watch-test cases;
nine C test executables were run in those milestones, not eleven. The new options
test brings the current suite to ten executables.

The rejected candidate passed 4520 release and 4520 ASan/UBSan independent
validation solves across 40 configurations (seed 20260912). These results apply
to the candidate, not the subsequently restored solver.
[Experimental validation record](benchmark_results/clause-activity-experimental-validation-20260906.json).
After restoring the solver and retaining only the input-validation fix, all ten
C test executables passed in release and ASan/UBSan modes.
The retained release solver also passed 4181 independent formula-validation
solves across the original 37 configurations. The sanitizer unit suite was rerun;
the complete sanitizer formula matrix was not repeated after removing the
experiment. [Final validation record](benchmark_results/clause-activity-final-validation-20260906.json).
