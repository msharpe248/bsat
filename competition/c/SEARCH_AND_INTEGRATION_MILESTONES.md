# Search and integration batch

> Historical milestone report. Its claims apply to the recorded revisions. Use
> [PRODUCTION_READINESS.md](PRODUCTION_READINESS.md) for current capabilities,
> evidence limits and release gates.

Authorized: evaluate combined phase/factoring policies on fresh inputs; test
bounded target refresh and diversification; measure planning propagation/search
bottlenecks; independent incremental differential histories and an upstream IPASIR
application; larger long-running service sessions. Commit and push each validated
milestone. Preserve unrelated `.zvec-grep/` state.

1. Combined holdout: complete; COMBINED_HOLDOUT.md. Combined defaults rejected.
2. Phase refresh: complete; PHASE_REFRESH.md. Tested prototypes archived without production promotion.
3. Planning propagation: complete; PLANNING_PROPAGATION.md. Statistics corrected; scan prototype rejected.
4. Differential histories and upstream client: complete; INDEPENDENT_INTEGRATION.md. 1,024 queries and 65 client cases pass.
5. Service session tails: complete; SERVICE_SESSION_TAILS.md. 49,152 conclusive queries and 384 exact-context exports pass.

Timed workloads run serially, without concurrent local builds, tests or fuzzing.
All conclusive benchmark answers require independent checks. A short holdout
screen can reject a policy but cannot alone justify production defaults. Report
UNKNOWN separately from errors and preserve negative or inconclusive results.

All five evaluation/implementation scopes are complete. Production changes are
correct blocker statistics, an isolated diagnostic build, and durable independent
integration/service regression coverage. The phase and scan-loop prototypes are
archived after inconclusive or mixed performance; defaults are not promoted.
Planning and a generally better phase policy remain unresolved algorithmic gaps.
The service benchmark demonstrates a workload-specific checkpoint policy benefit.

Local release/diagnostic/ASan/UBSan checks pass as recorded in the linked reports.
Linux independent integration and coverage fuzzing pass for 15716a7. The f5e4349 correctness run passed Linux GCC/Clang release/debug, macOS release,
and ThreadSanitizer, but exposed an existing macOS sanitizer application-harness
assertion that required SAT under a 0.2-second retry budget. This is corrected and
locally revalidated in APPLICATION_RETRY_BUDGET.md. Fresh remote correctness and
the service-smoke update remain pending at this snapshot. Two superseded
pre-diagnostic correctness runs were cancelled to free capacity.
