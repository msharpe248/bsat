# Search usefulness and simplification opportunities — 2026-09-10

Status: complete. One evidence-driven score-refresh candidate was implemented,
validated, measured and rejected. Production search policy is unchanged.
Observational diagnostics, regression tests, six portable DIMACS fixtures and
measurement improvements remain. Each milestone was committed and pushed.

## Frozen scope and evidence

The protocol was committed at c148308 before measurements; the candidate was
frozen at 76fcd3c before inspecting holdout outcomes. The baseline production
policy is c6b4ac8. The implementation was measured at 894aa67 on macOS and df61412
on Linux; df61412 fixes only the workflow's checker environment scope.

- Measure learned clause propagation, analysis/conflict use, age and reduction
  outcomes, including deleted clauses. Keep intrusive diagnostics out of timing
  scores and verify search/proof parity against the ordinary build.
- Freeze six previously unrecorded DIMACS files, one per dataset family directory,
  10 KB–20 MB, seed 2026091001, excluding recorded filenames and hashes. These are
  fresh files, not necessarily fresh families. Use imported DIMACS directly,
  without BSAT's AIG preparation; upstream preprocessing is unknown.
- Audit small-witness SSR offline using full literal-occurrence indexing, bounded
  root closure and signed subset checks. Do not modify solver clauses or infer
  performance gains from opportunity counts.
- Evaluate one candidate in baseline/candidate/candidate/baseline order on cal3
  depths 0,1,2,4,8,16 and cal100 depths 0,2,4, flags 3, 60 query CPU seconds.
  Require no checked loss, >5% combined solve/export/check CPU benefit and no
  >5% regression on either target. Independently check conclusive answers.
- Run the frozen DIMACS holdout at 15 query CPU / 20 solve wall seconds, two runs
  per build, even if targets reject. Promotion also requires holdout nonregression,
  macOS/Linux confirmation, memory and complete transaction acceptance. No grid
  of follow-up parameter settings after rejection.

The [selection manifest](benchmark_results/search-use-holdout-20260910.json)
records all history exclusions. Compressed, hash-checked portable inputs are in
[fixtures/search_use](tests/fixtures/search_use/manifest.json).

## What the diagnostics establish

All 18 retained diagnostic queries independently check. Their proof hashes and
conflict counts match the production baseline. The counters show:

| Cumulative retained-history observation | cal3 | cal100 |
|---|---:|---:|
| Deleted clauses | 479,630 | 114,497 |
| Deleted clauses analyzed since previous reduction | 443,180 | 102,197 |
| Eligible analysis reuse events with lower current LBD | 4,994,533 / 7,050,085 | 672,517 / 968,222 |

A blanket recent-use reprieve would cover most deleted clauses. Lower current
LBD is common, but is not evidence that retaining those clauses improves search.
Deletion totals count clauses; retained totals count repeated reduction visits;
LBD counters count reuse events, not unique clauses. Mandatory assertions when
clauses are first learned are excluded from reuse. The observations include
binary learned clauses and preserve metadata across arena compaction.

[Diagnostic summary](benchmark_results/search-use-diagnosis-20260910.json) links
to the adjacent raw target reports. Expanded headers and inclusive timers make
these diagnostic CPU numbers unsuitable as production performance scores.

## Candidate and measured rejection

The candidate refreshes LBD when certified conflict analysis uses a learned
conflict or expanded reason, clamping refreshed scores to `glue_lbd + 1` or higher.
It cannot create newly permanent glue protection. Existing glue clauses, clause
contents, proof rules, the comparator and reduction schedule are unchanged.
This differs from existing dynamic LBD, which can promote clauses to glue, and
from previously evaluated recency activity and low-LBD reprieves.

| Target ABBA screen | Baseline checked | Candidate checked | Baseline CPU PAR2 | Candidate CPU PAR2 | Change |
|---|---:|---:|---:|---:|---:|
| macOS ARM64 | 36/36 | 34/36 | 129.72 s | 308.89 s | +138.12% |
| Linux x86-64 | 36/36 | 34/36 | 187.69 s | 344.55 s | +83.57% |

Both candidate repetitions lose cal3 positive depth 16 at the 60-second limit.
The first macOS baseline solves it in 23.20 seconds; the candidate's journal
reaches about 315 MB before UNKNOWN, versus about 131 MB for baseline. cal100
also regresses: about 6.48 versus 19.84 seconds in the first macOS comparison,
and about 2.9 times the aggregate solve CPU on Linux. Both per-target gates fail.
The candidate changes search and performs score updates; this is a measured
regression, not a no-op or a failure to activate the feature.

For conclusive target queries, CPU cost includes solving, export and BSAT's
independent verification chain. UNKNOWN/late queries receive the frozen 120-second
PAR2 penalty. This is not wall latency or a complete service transaction cost.
Retained CaDiCaL and fresh Kissat run on the same respective host as references;
reference checking is excluded from BSAT's reported cost.

The fresh DIMACS screen checks 2/12 runs in each build on each host. Only the
root-UNSAT p-center repetitions finish; the other ten are UNKNOWN at 15 seconds.
There is no new solve or informative aggregate speed gain. These short, mostly
censored results do not establish general competitiveness.

Raw reports: [macOS targets](benchmark_results/refresh-targets-mac-20260910/summary.json),
[macOS holdout](benchmark_results/refresh-holdout-mac-20260910.json),
[Linux summary](benchmark_results/refresh-linux-20260910/summary.json).
[Linux run 34426516845](https://github.com/msharpe248/bsat/actions/runs/34426516845)
completed successfully; success means the experiment ran, not that its candidate
passed performance gates.

## Simplification opportunities

The full-occurrence audit finds no small-witness SSR opportunities in the complete
cal3 snapshot or smaller planning scan. The larger planning scan reaches its
1,000,000-unit bound with no match; absence is not established there.

In the fresh circuit-multiplier input, it finds 74 independently verified target
relations before the cap, including 47 root-unassigned targets after complete
root closure. The formula is not root UNSAT. This is the concrete lead for future
simplification work, but no SSR speedup has been measured on it.

The p-center input has 57 valid relations, including 43 root-unassigned targets,
but root closure already proves UNSAT, so these offer no search benefit. The
other four holdout scans complete with no matches. Reports distinguish valid
relations from opportunities on formulas that still need search. Each relation
records original source/target indices and the removed pivot, with input hashes,
root-closure status and explicit work bounds. The audit is not a fixpoint pass.

[Development audit](benchmark_results/ssr-opportunities-development-20260910.json),
[holdout audit](benchmark_results/ssr-opportunities-holdout-20260910.json).

## Resources and measurement boundaries

| Linux isolated embedding peak RSS | Baseline | Candidate |
|---|---:|---:|
| cal3 through depth 16 | 37.11 MiB | 38.09 MiB |
| cal100 through depth 4 | 133.42 MiB | 136.57 MiB |

Each target replay uses a fresh process containing Python encoding/hash/model
validation and one native solver, against exact independently checked contexts.
The candidate cal3 replay remains UNKNOWN. These single-run peaks exclude proof
checkers; they are neither native-only RSS nor total worker/checker memory.
No complete containment promotion run was required after the target rejection;
the existing production transaction acceptance evidence remains unchanged.

The archived holdout `complete_cpu_par2_sum` is a legacy sum of loading, solving,
export and checking phases; it excludes some harness bookkeeping and startup.
It must not be called full transaction CPU. Future DIMACS reports additionally
measure the fresh worker and its waited-for checker descendants using the parent
process child-CPU delta. This includes startup, bookkeeping, cleanup and result
serialization; parent orchestration CPU remains outside the measurement. A separate
within-worker measurement covers input loading through solver teardown and helps
check the accounting. Eight SAT/UNSAT worker transactions validate the new path
on macOS without claiming a performance gain from these tiny fixtures. The rejection does not depend on these small cost
boundary differences: it loses checked target answers on both hosts.

## Validation and retained changes

Candidate release and ASan/UBSan builds pass 69 C executables, 53 independently
checked certificates per build, 3,283 allocation cutoffs, 22 proof-output cutoffs
and two deferred-flush failures on macOS and Linux. Tests cover changed assumptions,
future clauses, conditional UNSAT followed by SAT, cancellation and checkpoint.
The focused score test checks the non-glue floor, untouched proof output and
scratch cleanup. Finite tests do not prove universal solver soundness.

The restored production build passes all 69 release and ASan/UBSan tests. The
expanded diagnostic layout also passes all 69 under ASan/UBSan; its earlier
release suite passes all 68 executables present before the gated candidate test
was added. Public facade parity includes 48 accounting-enabled queries. Python
coverage includes 200 random formulas with exhaustive five-variable entailment,
root/normalization/budget cases, six target-gate tests, five DIMACS-gate tests and
six portable fixture round trips. All measured holdout identities and deadlines
were checked against their frozen artifacts.

[Validation record](benchmark_results/search-use-validation-20260910.json).
The [archived patch](benchmark_results/certified-refresh-prototype-20260910.patch)
and manual `c-certified-refresh.yml` workflow reproduce the candidate.
`BSAT_CERTIFIED_REFRESH` is not a supported production build option. Retained
changes are observational diagnostics, regression coverage, portable holdouts,
independent opportunity auditing and more complete cost accounting. No new
search policy is promoted. Deployment-specific workloads and SLOs remain missing.
