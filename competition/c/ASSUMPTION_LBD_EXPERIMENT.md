# Assumption-aware LBD experiment — 2026-09-09

Status: gated prototype, not a production default. Frozen hypothesis and target
acceptance are in [query-context diagnosis](QUERY_CONTEXT_DIAGNOSIS.md).

The candidate ignores fixed assumption-prefix levels when computing LBD,
including their implications and dummy assumption levels. Learned clauses keep
all literals and remain consequences of the permanent formula. A transient
scoring boundary is installed around certified-policy solves and cleared after
every completed or interrupted search. Root-only and uncertified policy retain
the previous score. The compile-time BSAT_ASSUMPTION_LBD gate keeps all runtime
changes, including the extra Solver field, out of ordinary builds.

Before timing, all 66 C executables pass in release/ASan/UBSan; 53 retained/rebuilt
query certificates per build independently check, including wrong-context
rejection. The target experiment checks 8/8 queries: fresh assumption cal3 now
solves twice in 26.552 / 26.830 CPU seconds (681,570 conflicts), versus control
UNKNOWN at 60. Unit-query conflicts/proof path remain unchanged. A first retained
four-circuit pass checks 48/48 queries; cal3 takes 18.000 seconds and 482,935
conflicts. These are development wins, pending broader confirmation.

Confirmation is frozen to the original and expanded sets/budgets in
ASSUMPTION_SEARCH_MILESTONES.md. Compare pre-score prefix-retaining revision
6f793ed to the gated build in baseline/candidate/candidate/baseline order on each
host. Reject checked-solve losses or >5% aggregate CPU PAR2 regression. Inspect
per-family CPU, proof and memory tradeoffs, not just the aggregate. An independent
Linux run of the previous prefix change remains separate evidence.

The hard incremental test additionally exercises prefix-preserving and ordinary
restart policies across heap/queue decisions, chronological backtracking, budget
slices, cancellation, growing guarded contradictions and later SAT queries.

Reports under benchmark_results: query-lbd-validation-20260909.json,
query-lbd-certificates-{release,debug}-20260909.json,
query-lbd-target-20260909.json, query-lbd-retained-screen-20260909.json.
