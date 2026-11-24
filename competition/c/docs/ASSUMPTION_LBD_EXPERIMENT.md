# Assumption-aware LBD experiment — 2026-09-09

Status: promoted for certified solves after both matched-host confirmation gates passed. Frozen hypothesis and target
acceptance are in [query-context diagnosis](QUERY_CONTEXT_DIAGNOSIS.md).

The candidate ignores fixed assumption-prefix levels when computing LBD,
including their implications and dummy assumption levels. Learned clauses keep
all literals and remain consequences of the permanent formula. A transient
scoring boundary is installed around certified-policy solves and cleared after
every completed or interrupted search. Root-only and uncertified policy retain
the previous score. The experiment used the compile-time BSAT_ASSUMPTION_LBD gate. The final
implementation uses a separate internal assumption_lbd policy enabled by the
certified facade, independently of the restart policy; it adds no public flag or
ABI change. The transient boundary is zero outside a solve.

Before timing, all 66 C executables pass in release/ASan/UBSan; 53 retained/rebuilt
query certificates per build independently check, including wrong-context
rejection. The target experiment checks 8/8 queries: fresh assumption cal3 now
solves twice in 26.552 / 26.830 CPU seconds (681,570 conflicts), versus control
UNKNOWN at 60. Unit-query conflicts/proof path remain unchanged. A first retained
four-circuit pass checks 48/48 queries; cal3 takes 18.000 seconds and 482,935
conflicts. These target results preceded the broader confirmation below.

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

## Matched Mac confirmation

Frozen baseline/candidate/candidate/baseline runs complete. Original set: both
versions check 96/96 queries; mean query CPU is 0.847114 → 0.395114 s (-53.36%).
Expanded set: both check 62/64, with cal100 depth-8 positive remaining UNKNOWN in
both repetitions; CPU PAR2 is 0.949274 → 0.874344 s (-7.89%). No query loses a
checked solve. Both independent set gates pass.

Retained cal3 completes at 482,935 conflicts versus 1,148,517 for control; the
first control takes 39.512 s and candidate repetitions 17.758 / 17.766 s.
Expanded cal100 depth-4 positive improves 7.580 / 7.584 → 4.829 / 4.823 CPU s
(179,224 → 112,437 conflicts). Its first proof check increases 5.49 → 5.86 wall
seconds, so lower solve CPU does not imply every downstream stage is faster.
Original maximum owned capacity increases about 0.57% (62.23 → 62.58 MB); the
expanded maximum differs by the candidate's extra 8-byte Solver footprint.
Original maximum journal size drops 242.75 → 131.14 MB; expanded 117.51 → 101.81 MB.
Owned capacity, journal bytes and checker RSS are distinct resource measures.

Evidence: `benchmark_results/query-lbd-mac-confirmation-20260909/`. The summary
checks exact query sets, input identity, checked answers, CPU deadlines and
per-query losses. Six regressions validate that missing/context-changed queries,
contradictory checked answers and losses hidden by faster other queries cannot
pass. Linux confirmation also passes, as detailed below.

## Matched Linux confirmation and decision

[Run 34372161034](https://github.com/msharpe248/bsat/actions/runs/34372161034)
completed successfully, including release/sanitizer tests and independent query
certificates. Against the same prefix-retaining baseline, the original set
improves from 94/96 to 96/96 checked queries; mean CPU PAR2 falls 2.557042 →
0.694731 seconds (-72.83%). Expanded: 60/64 → 62/64 checked, CPU PAR2 1.631357 →
1.319243 (-19.13%). Both gates pass with no per-query checked losses.

Retained cal3 moves from two 60-second UNKNOWNs to checked UNSATs in 29.592 /
30.128 CPU seconds, both at 482,935 conflicts, matching the Mac candidate's
search work. cal100 depth-4 positive moves from two 10-second UNKNOWNs to checked
UNSATs in 9.539 / 9.319 seconds. cal100 depth-8 positive remains UNKNOWN.

This is not a universal improvement: Linux gen23 depth-16 positive increases
2.191 / 2.039 → 2.784 / 2.906 seconds and 6,078 → 7,524 conflicts. Its journal
grows 8.01 → 11.79 MB. Expanded maximum journal size grows 64.37 → 79.57 MB;
cal3's new proofs require about 31–32 additional wall seconds to check. Solve CPU
excludes export and checking. These are reused circuit sets, not pristine holdouts.

Evidence: `benchmark_results/query-lbd-linux-confirmation-20260909/`, including
compiler/host metadata, all eight runs, certificate checks and acceptance summary.
The successful gates justify enabling the bounded scoring policy for certified
solves. It changes learned-clause quality scores, never clause literals, logical
inference, assumption lifetime or certificate context. Independent checks provide
soundness evidence for tested queries, not a proof of the entire implementation.

## Final production validation

Runtime commit `c6b4ac84c94e231983886828d3100495a49545c6` removes the experimental
macro and enables the independent internal scoring option for certified handles.
All 66 C executables pass in release and ASan/UBSan builds. Each final build
passes 53 independently checked retained/rebuilt certificates and wrong-context
rejection. Staged installation, pkg-config, frozen ABI-v1 C and installed-header
C++ consumers pass. The gated candidate also passed 256 stateful differential
queries across public certified modes 2, 3, 6 and 7, including optional probing.

The isolated preparatory checkout omitted the shared DIMACS fixtures and stopped
at file parsing in both test modes; the complete workspace's final builds pass.
This was a validation setup error, not a solver mismatch.

Evidence: `query-lbd-production-validation-20260909.json`,
`query-lbd-production-certificates-{release,debug}-20260909.json`, and
`query-lbd-stateful-20260909.json` under benchmark_results.

Runtime CI is still running when this record is written:
[correctness](https://github.com/msharpe248/bsat/actions/runs/34375352013),
[independent integration](https://github.com/msharpe248/bsat/actions/runs/34375352037),
and [coverage fuzzing](https://github.com/msharpe248/bsat/actions/runs/34375352028).
The earlier Linux candidate workflow is green; it is separate from final runtime CI.

Direct final-production replays check 48/48 original and 31/32 expanded queries.
Input identities, results, and every conclusive proof hash/conflict count match
the gated Mac candidate. cal3 completes in 18.181 CPU seconds at 482,935 conflicts;
cal100 depth-8 positive is the sole UNKNOWN. Reports:
`query-lbd-production-{original,expanded}-20260909.json`. These replays validate
the final implementation form; they are not additional independent holdouts.
