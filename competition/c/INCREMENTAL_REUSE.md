# Conservative learned-clause reuse

`SolverOpts.reuse_learnts` defaults to false. When enabled, compatible repeated
calls keep the arena, learned clauses, root consequences and variable activities.
Preparation backtracks to level zero, replays root assignments, clears the old
failed-assumption result and resets per-call budgets and restart bookkeeping.
Input growth and new clauses use the same preparation. Assumptions remain
one-call decisions; their learned consequences are entailed by permanent input.

The fast path requires no proof, elimination state, BVE, BCE, equivalence rewrite,
congruence, local search or inprocessing; it also requires no interruption and
no previous UNSAT result under assumptions. Those cases rebuild from original
input. Errors remain poisoned. Unconditional UNSAT may be cached. Portfolio
solves retain their established rebuild policy. This is a scoped opt-in API
feature, not transparent reuse across every preprocessing combination.

Validation on macOS ARM64 / Apple Clang:

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
