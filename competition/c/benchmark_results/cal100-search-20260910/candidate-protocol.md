# cal100 depth-8 search scaling — 2026-09-10

Status: investigation in progress. Baseline: formatting-only revision `1b424f6`.

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
