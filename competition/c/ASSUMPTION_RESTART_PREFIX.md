# Retaining assumption prefixes at restarts — 2026-09-09

A compile-time prototype resolves the certified retained cal3 depth-16 query
in 40.740 CPU seconds and 1,148,517 conflicts; the unchanged portable-sort control
remains UNKNOWN at 60 seconds and 1,721,339 conflicts. The prototype's exported
query proof independently passes DRAT-to-LRAT and CakeML checking. Two further certified passes resolve and check 96/96 queries, with identical
cal3 proof bytes and 49.652 / 52.898 CPU seconds for that query.

## Change and correctness argument

An ordinary restart previously backtracked every assumption solve to level zero,
then reapplied its identical ordered assumptions. The certified facade now opts into backtracking to
`min(current decision level, number of assumptions)` instead. Restarts run only
after propagation reaches a consistent fixpoint. The retained levels contain
only assumptions already checked in this query and their implications, including
dummy levels for assumptions already true. Levels for search decisions above the
assumption prefix are discarded.

This does not turn assumptions into permanent units. Conflict analysis and
backjumping are unchanged and may cross the prefix. The next query still removes
the old assumptions through normal preparation. Learned clauses still follow the
existing assumption-as-decisions rule and certificate journal semantics. Ordinary
one-shot calls with zero assumptions keep their previous restart path. The
experimental priority-based `reuse_trail` option is independent of this prefix.
An internal `restart_assumptions` option defaults off and is enabled only by the
certified facade. There is no new public flag or ABI field.

Preserving assignments can change phase-saving and watch order, hence later
search. The observed speedup cannot be attributed entirely to fewer repeated
propagations; it is a search-policy change as well as avoided work.

## Validation and confirmation

Before target timing, all 66 C executables passed in release and ASan/UBSan
builds. Each build also passed 53 independently checked retained/rebuilt query
certificates and wrong-context rejection. All 12 target-history queries were
resolved and checked, including the negative query after conditional UNSAT.
The target journal ended the positive query at 242,749,820 bytes versus
368,744,905 bytes at the control's cutoff; these are different search endpoints,
not equal-work I/O cost measurements.

A focused regression additionally exercises forced restarts with 300 duplicate
assumptions over a guarded pigeonhole contradiction, chronological/nonchronological
backtracking, heap/queue decisions, negative and empty subsequent queries, and a
future permanent guard addition. Repeated confirmation uses the four original
pinned circuits and the same six growing depths. All 96 certified queries resolve
and check. The global prototype regresses uncertified cal3 to UNKNOWN at 60
seconds (1,324,252 conflicts), so global promotion is rejected. The final change
keeps uncertified behavior unchanged; the final scoped implementation passes the full release/ASan/UBSan suites and
53 independent certificate checks per build. Direct production-library timing
is in progress.

Evidence: `benchmark_results/assumption-restart-validation-20260909.json`,
`assumption-restart-certificates-{release,debug}-20260909.json`,
`cal3-assumption-restart-20260909.json`.

The rerun control resolves 48/48 uncertified queries and 47/48 certified queries;
its certified cal3 query again reaches 60 CPU seconds. The certified prototype
resolves 48/48 per pass. There is a tradeoff: certified gen23 depth-16 positive
CPU rises from 0.848 in the control pass to 1.378 / 1.283 in the two candidate
passes. This is not a universal per-query speedup. Promotion is based on adding
the difficult checked solve without a solve loss in this frozen circuit mix.
