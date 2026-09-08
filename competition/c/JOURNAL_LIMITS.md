# Bounded certificate sessions

`bsat_set_journal_limit(s, bytes)` sets an exact ceiling for accepted bytes in a
certified handle's journal; zero is unlimited. An addition is sized before any
of its bytes are written. If it cannot fit, the handle enters persistent error,
the query returns UNKNOWN, and exports are forbidden. Discard that handle.
`bsat_get_journal_bytes` remains readable after error. Reducing the ceiling below
existing bytes is rejected without modifying the handle.

Call `bsat_checkpoint(s)` proactively to reclaim the accumulated journal and
learned database. It rebuilds from permanent original input, then replaces the
journal with an empty temporary file. It invalidates the latest answer and
statistics. CPU budgets and cancellation apply to rebuilding; failure never
installs an empty journal over retained learning. An allocation or I/O error is
persistent; ordinary cancellation can be retried. Limits and callbacks survive.
The operation also works on non-certified handles to discard learned state.

For a bounded session: monitor journal bytes between queries, checkpoint at an
application-selected watermark, and reserve enough headroom below the ceiling
for the next query. A single query may still exceed the ceiling. No bytes are
silently dropped and no incomplete certificate is presented as conclusive.

This implements checkpointing by discarding learning, not proof compaction that
preserves learning. It bounds the journal and its per-export prefix, while
trading away reuse at checkpoints. The limit excludes original CNF, exported
copies, the two-byte query-local UNSAT suffix, stdio/filesystem overhead, and
files the caller elects to keep. Export remains synchronous outside solve CPU
budgets. Retaining every export indefinitely still requires caller retention
policy or external storage limits.

Tests cover an 8,192-query session with 64 checkpoints and a 512-byte ceiling;
exact empty-clause quota boundaries; rejection of multi-buffer records before
their first byte; persistent errors; cancellation/retry; and answer/statistic
invalidation. The certificate driver now exports 53 independently checked
queries, including hard conditional UNSAT and SAT immediately after a checkpoint.

Validation: release and ASan/UBSan unit/embedding/installed-package suites pass;
all 53 certificates per build pass independent checks and wrong-context rejection.
The sanitizer allocation campaign passes 3,127 injected failures including
checkpoint rebuilding. Updated public fuzzing completes 46,200 runs in 31 seconds
without a finding. Evidence is in `benchmark_results/journal-checkpoints-*`.
