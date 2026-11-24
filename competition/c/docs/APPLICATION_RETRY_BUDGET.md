# Deterministic cancellation recovery check

The macOS Clang sanitizer job for f5e4349 failed in the existing generated BMC
application history. Query 32 intentionally cancelled; query 33 then returned
UNKNOWN after 0.200397125 CPU seconds under its 0.2-second CPU allowance. The
harness nevertheless asserted SAT. No incorrect conclusive solver answer was
observed. The job and final query are preserved in
`benchmark_results/application-retry-ci-failure-20260908.json`.

The recovery assertion remains conclusive, but that one query now uses a
1,000,000-conflict limit with no CPU deadline, inside the runner's existing
240-second process wall timeout. This tests recovery without requiring identical
sanitizer throughput on every host. All ordinary BMC/configuration queries still
use 0.2 CPU seconds and 2,000 conflicts, and may legitimately return UNKNOWN.
The original limits are restored immediately after the recovery query.

The C driver records CPU/conflict limits in every query row. The Python runner
checks those fields for every row and records the recovery exception in its
frozen policy. The query count, formula, assumptions and SAT recovery assertion
are preserved. This changes the benchmark protocol for BMC query 33; older reports
retain their original policy and should not be silently pooled with new timings.

Validation: both release and ASan/UBSan runs complete 1,156 generated queries
(2,312 total), checking domain answers and SAT witnesses. All four mandatory
recovery queries return SAT; budgeted UNKNOWNs remain correctly accepted elsewhere.
Twelve large application query certificates pass independent checking per build
(24 total); results are recorded separately.
The solver library binary is unchanged by this test-harness fix; the service and
independent-integration measurements remain tied to the same library hash.

On the preceding revision, all four Linux GCC/Clang release/debug correctness
jobs, macOS release, and ThreadSanitizer passed. The failed macOS sanitizer
application assertion is corrected here; fresh remote CI remains pending until
its actual result is available. See the current milestone tracker for status.
