# cal100 depth-8 search scaling — 2026-09-10

Status: complete. The ternary fast path failed its frozen local gate and was
removed. Production search policy is unchanged. Baseline: formatting-only
revision `1b424f6`.

## Frozen investigation

- Reproduce certified retained cal100 at depths 0, 2, 4, 8, both query polarities,
  flags 3, with 60 query CPU seconds. Run two serial baseline repetitions.
  This extends the earlier 10-second depth-8 screen; UNKNOWN remains unfinished.
- Keep cal3 at depths 0, 1, 2, 4, 8, 16 as the regression target, with the same
  60-second query budget. Use the existing hash-pinned circuit manifests.
- Check SAT models against the exact clauses and circuit simulation; check UNSAT
  certificates with the existing DRAT-to-LRAT and CakeML chain. Run retained
  CaDiCaL and fresh Kissat references serially under documented bounds.
- Separate uninstrumented timing from intrusive accounting. Diagnose at a
  200,000-conflict per-query cap with sufficient CPU headroom; compare matching
  histories and work limits before attributing differences to instrumentation.
  Inclusive phase times overlap and must not be added as exclusive costs.
- Measure propagation visits/scans, analysis/minimization work, reductions,
  restart frequency and discarded trail work. Distinguish query-local counters
  from cumulative counters across the retained history.
- Select at most one implementation candidate from the diagnosis. Freeze its
  hypothesis and gates before timing it; do not search a parameter grid.
- A candidate must preserve checked answers, pass focused semantic tests,
  release/ASan/UBSan tests and independent certificate validation. Promotion
  requires repeated cal100 improvement, no cal3 checked loss or >5% total
  solve/export/check CPU regression, and a frozen independent confirmation
  corpus with no checked loss or >5% aggregate CPU PAR2 regression. Cross-host
  confirmation is required before enabling a production search-policy change.
- If the evidence does not support a candidate, retain the diagnosis and useful
  tests without changing production search policy. Archive rejected experiments.

Raw reports will be recorded under `benchmark_results/cal100-search-20260910/`.
Tool, input, source and binary hashes identify each measurement. Timing excludes
encoding and additions unless explicitly stated; checker/export costs and proof
growth are reported separately. Public development inputs are not customer SLOs.

## Frozen candidate: ternary propagation specialization

The 200,000-conflict comparison preserves all eight query outcomes, public
search counters, learning/reduction counters, journal sizes and completed proof
hashes. The legacy `literal_inspections` counter also charges arena copying;
expanded diagnostic headers change it, so it is not a cross-layout parity metric.

At cal100 depth 8 the instrumented propagation interval is 14.03/18.70 seconds;
212.27/291.63 million replacement scans visit original clauses. The circuit
encoder produces binary and ternary gate clauses. Restart-discarded assignments
are 23.58/436.41 million propagations; this is not a direct replay measurement.
Learned literals per conflict fall from 192.59 at depth 4 to 74.84 at depth 8.

Test one compile-time `BSAT_TERNARY_PROPAGATION` candidate: specialize the single
replacement literal of a three-literal clause, bypassing circular-search cursor
loads, bounds normalization and loop bookkeeping. Preserve literal/watch order,
reason levels, cursor updates, budget charges and interrupted-watch restoration.
Detailed accounting uses the general path, allowing the parity tests to compare
the fast path against the instrumented reference implementation.

Before timing, require the full candidate release/ASan/UBSan suites, focused
mixed-watch and restart/cancellation tests, independent random-formula proof
validation, and fixed-conflict exact-search/proof parity on cal100. Measure two
serial candidate repetitions at the frozen 60-second cal100/cal3 budgets. A
timed UNKNOWN never counts as a solve or as a proof. If the target fails to add
a solve, require >5% repeatable fixed-work CPU benefit to justify broader
confirmation; equal-conflict timings are only kernel evidence, not convergence.
Reject after the first candidate screen if it fails that gate. No follow-up
parameter or implementation grid. Any promotion still requires the original
cal3, independent-holdout and cross-host gates above.

## Baseline and diagnosis

Both uninstrumented cal100 repetitions check 7/8 queries. The positive depth-8
query remains UNKNOWN at 60 CPU seconds; all other queries check. Retained
CaDiCaL completes that query in 27.10/26.92 CPU seconds, at 725,006 conflicts.
The existing harness independently checks conclusive results and original SAT
models, including circuit simulation. Both cal3 baseline histories check 12/12
queries; depth-16 positive takes 18.12/18.14 seconds and 482,935 conflicts.

| Positive cal100 query | Ordinary baseline CPU | Conflicts | Propagations/conflict |
| --- | ---: | ---: | ---: |
| Depth 2 | 0.247 / 0.256 s | 6,905 | 766 |
| Depth 4 | 4.790 / 4.920 s | 112,437 | 966 |
| Depth 8 | 60 s, both UNKNOWN | 781,164 / 778,949 | about 2,083 |

The fixed-work diagnostic at depth 8 stops at 200,000 conflicts, after exactly
the same preceding queries as its control. It processes 436.41 million
propagations (2,182 per conflict), versus 108.58 million for the completed
depth-4 query. Scans per propagation actually fall from 1.89 to 0.67. Original
clauses rise from 27.2% to 72.8% of replacement scans; the learned-clause share
falls. The size of the learned clauses therefore does not explain the rising
propagation work in this measurement.

| Instrumented observation | Depth 4 | Depth 8, 200k conflicts |
| --- | ---: | ---: |
| Propagation interval / search interval | 65.6% | 75.0% |
| Analysis interval / search interval | 14.5% | 6.8% |
| Minimization plus LBD interval / search interval | 3.4% | 0.9% |
| Restarts | 735 | 1,199 |
| Assignments removed at restarts / propagations | 3.75% | 5.40% |
| Learned literals / conflict | 192.59 | 74.84 |

Phase intervals are intrusive and inclusive; these percentages are not an
exclusive CPU breakdown. Discarded assignments do not establish how many equal
assignments were replayed or their cost. CaDiCaL propagation counters count search
propagation under different preprocessing and accounting conventions. Its roughly
238 million propagations at depth 8 versus BSAT's roughly 1.63 billion are useful
context, not a matched per-instruction comparison.

The next algorithmic investigation should target why the deeper query requires
so much original-clause propagation. This evidence weakens another blanket
learned-score/minimization change or restart-replay optimization as the immediate
answer. It does not yet identify a proven structural simplification or branching
change that reduces that work.

## Candidate outcome

The fixed-work order was baseline, candidate, candidate, baseline, at 200,000
conflicts per query. Depth-8 CPU was 15.773, 15.425, 15.561, 15.088 seconds.
The candidate median is **0.41% slower**, failing the required repeatable >5%
benefit. All four histories preserve the same search and same-layout resource
work counters, journal sizes and completed proof hashes.

| Full target screen | Baseline checked | Candidate checked | Solve/export/check CPU PAR2 change |
| --- | ---: | ---: | ---: |
| cal100, depths 0/2/4/8, two histories | 14/16 | 14/16 | +1.16% |
| cal3, depths 0/1/2/4/8/16, two histories | 24/24 | 24/24 | +13.92% |

Both candidate cal100 depth-8 runs remain UNKNOWN at 60 seconds. Candidate cal3
depth-16 positives take 19.71/22.20 seconds, still at 482,935 conflicts. The full
screens follow the baseline histories rather than interleaving them, so their
timing differences are not a drift-controlled causal estimate. The interleaved
fixed-work gate already fails independently. UNKNOWN receives a 120-second CPU
PAR2 penalty; references, encoding and additions are excluded from this score.
It is not full transaction CPU.

The prototype was archived and removed. No independent holdout or cross-host
promotion run was needed after the local rejection. There is no supported new
runtime flag or production optimization from this experiment.

## Retained changes and reproduction

- Diagnostic-only restart trail/level totals and minimization/LBD timing.
- Test-facade exports for existing propagation counters and phase call counts.
- A bounded replay harness that pins queries to previously checked input hashes
  and independently verifies each new conclusive BSAT answer.
- A summary tool that distinguishes query-local counters from retained database
  counters and rejects mismatched query sets, search work or proofs.
- Tests for restart accounting across changed assumptions and 24 ternary scan
  cutoffs, including invalid cursors and resumed unit/conflict/replacement paths.

Candidate validation passes 70 release and 70 ASan/UBSan C executables, 11,316
independently checked CLI solves, and focused diagnostic ASan/UBSan accounting
parity checks. The final restored build validation is recorded separately in
`final-validation.json` beside the raw reports.

Evidence under [cal100-search-20260910](benchmark_results/cal100-search-20260910):

- `baseline.json`, `cal3-baseline.json`: repeated default histories and references.
- `fixed-summary.json`: matched control/accounting search diagnosis.
- `candidate-summary.json`: the reproducible frozen rejection gate; regenerate
  with `python3 benchmark_results/cal100-search-20260910/summarize_candidate.py`
  from this C solver directory.
- `candidate-runs.json`: exact commands, library hashes and completed exit codes.
- `baseline-manifest.json`, `diagnostic-manifest.json`, `candidate-policy.json`:
  source/compiler/binary identities and pre-measurement policies. Immutable
  `baseline-protocol.md` and `candidate-protocol.md` preserve the frozen text.
- `ternary-prototype.patch`: rejected candidate only, applicable to the retained
  source with `git apply` from the repository root. Compile with
  `-DBSAT_TERNARY_PROPAGATION` to reproduce; it is not a supported build option.
