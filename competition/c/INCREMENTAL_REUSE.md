# Conservative learned-clause reuse

`SolverOpts.reuse_learnts` defaults to false. When enabled, compatible repeated
calls keep the arena, learned clauses, root consequences and variable activities.
Preparation backtracks to level zero, replays root assignments, clears the old
failed-assumption result and resets per-call budgets and restart bookkeeping.
Input growth and new clauses use the same preparation. Assumptions remain
one-call decisions; their learned consequences are entailed by permanent input.

The fast path requires no ordinary proof stream, elimination state, BVE, BCE, local search or
inprocessing; it also requires no interruption. Those cases rebuild from original
input. Congruence's entailed additions are compatible. Enabling equivalence
processing is compatible only until an actual rewrite creates reconstruction
state, which forces a rebuild. Conditional UNSAT now retains entailed learned
clauses: a separate permanent-root-inconsistency flag prevents confusing a
failed assumption with a base-formula contradiction. Errors remain poisoned.
Unconditional UNSAT may be cached. Portfolio
solves retain their established rebuild policy. This is a scoped opt-in API
feature, not transparent reuse across every preprocessing combination.

Initial implementation validation on macOS ARM64 / Apple Clang (before the
conditional-UNSAT extension):

- All 51 C test executables pass in release and ASan/UBSan builds.
- Each build checks 8,192 stateful eight-variable queries against an independent
  truth-table oracle, with 3,268 fast preparations and preprocessing fallbacks.
- Explicit retention checks preserve arena/learned clauses across variable and
  clause additions. Single-conflict slices resume to a checked SAT model.
- Both builds pass 2,016 individual allocation failures across 14 paths/formulas,
  including variable growth after reuse. All 69 release short-deadline cases pass.
- A 121-second ASan/UBSan API fuzz campaign executes 76,929 cases with no finding;
  peak RSS is 507 MiB. The grammar remains bounded to six variables / 512 bytes.

Performance evidence is in `benchmark_results/incremental-reuse-v2-20260908.json`.
The frozen protocol uses two inputs, three repetitions and 20 queries per
process. Queries alternate base solves and assumptions consistent with the
initial model. Exact initial-model strings must match between variants. Each
completed process's final model is independently checked; intermediate models
are checked internally. This is a development comparison, not a held-out result.
The initial report is retained: its timeout runs lost buffered model strings,
so the v2 driver flushes the guard before the query loop and repeats the protocol.

In v2, all 12 initial-model guards match across each input's variants/repetitions.
Reuse completes all six runs with independently valid final models; rebuild
completes the three tiny-fixture runs and times out on all three battleship runs
at 30 seconds. Reuse finishes battleship in 6.108–6.212 seconds wall time; peak
RSS across the protocol is 32.9 MB versus rebuild's 65.3 MB. The tiny fixture's
sub-millisecond repeated-query times are below a useful general speed claim.
Timeouts are UNKNOWN, not independently verified answers. These two workloads
do not establish industrial incremental performance generally.

## Conditional UNSAT and compatible preprocessing

The extension passes all 55 C test executables in release and ASan/UBSan builds.
The eight-variable stateful oracle checks 8,192 queries per build with 8,007
fast preparations, now mixing congruence and equivalence options. Focused
tests cover permanent empty clauses, root contradictions, conditional failure,
truth-table queries after actual congruence additions, and reconstruction
fallback after actual equivalence substitution.

Both builds pass 2,583 injected allocation failures, 12,000 concurrent opaque-ABI
queries and 16,384 public-API soak queries. All 69 short-deadline cases pass.
The hard growing histories pass with 360 non-cancellation queries, 296 UNKNOWN
slices, 1,925,176 conflicts and 59,898 reductions. They use slightly more total
conflicts than the pre-extension baseline: retaining state changes later search
and is not guaranteed to improve every history. Two representative 456-variable
conditional UNSAT queries also pass the independent LRAT/CakeML chain; a SAT
query passes original-query model validation. Reproduce those checks with
`tests/check_hard_certificates.py`; hashes and results are recorded in
`benchmark_results/hard-query-certificates-20260908.json`.

A 121-second ASan/UBSan API campaign completes 70,345 executions without a
finding, including compatible congruence/equivalence reuse and allocation
failure paths (peak fuzzer RSS 511 MiB).

The frozen repeated-query comparison uses a 73-variable guarded PHP(9,8)
formula, ten conditional-UNSAT/base-SAT pairs, two repetitions and the same
binary with reuse disabled/enabled. Every answer has a structural oracle and
every returned base model is scanned against the original clauses. Rebuild
uses 253,510 conflicts and 1.943/1.946 seconds CPU per process; reuse uses
25,351 conflicts and 0.193/0.193 seconds, retaining state for 19 preparations.
This roughly 10x result is specific to repeating the same hard query. It is
not a general incremental speed claim. Driver, binary and policy hashes are
in `benchmark_results/conditional-reuse-policy-20260908.json`; complete rows
are in `conditional-reuse-20260908.json`.
Twelve additional single-query runs at 20,000 conflicts preserve the pre-extension
status, selected search counters and binary/text proof bytes; see
`benchmark_results/reuse-single-query-trace-20260908.json`. UNKNOWN prefixes
are used only for these trace comparisons.

Public `BSAT_CERTIFICATES` handles now support a separate append-only RUP
journal without disabling compatible reuse. See `RETAINED_CERTIFICATES.md`
for exact query binding, independent verification and storage tradeoffs.
