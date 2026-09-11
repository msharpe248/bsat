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
