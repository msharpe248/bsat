# Certified incremental cal3 follow-up — 2026-09-09

These baseline measurements predate the
[certified assumption-prefix improvement](ASSUMPTION_RESTART_PREFIX.md).

The portable reduction default does not close the certified incremental gap.
The updated serial Mac measurements use the public facade through the existing
test diagnostic library (control changes no options; phase accounting disabled),
the pinned growing cal3 encoding at depths 0,1,2,4,8,16 and both output polarities.
Each query receives 60 solving CPU seconds. Same-process retained CaDiCaL has
the same CPU allowance and a 90-second wall fuse. Fresh Kissat and independent
model/DRAT-to-LRAT/CakeML checks run outside query timing. No local builds or tests
run concurrently with measurements. Each report records input and tool hashes.

| Positive depth-16 query | Result | CPU seconds | Conflicts |
|---|---|---:|---:|
| Retained uncertified | UNSAT, independently confirmed | 46.928 | 1,400,032 |
| Retained certified | UNKNOWN | 60.000 | 1,721,339 |
| Fresh certified (only depth 16) | UNKNOWN | 60.000 | 1,623,424 |
| Retained certified policy, journal disabled diagnostically | UNKNOWN | 60.000 | 1,757,838 |

Retained CaDiCaL resolves the matching retained queries in approximately 0.66 CPU
seconds. A fresh handle does not remove the timeout. Removing journal writing
only modestly changes work completed and does not add a solve; certificate I/O
alone is not a demonstrated explanation. These are single-run diagnostics, not
precise overhead estimates. The uncertified mode also enables probing, so its
search is not a journal-only ablation.

The certified control performs 10,049 restarts, creates 368,744,905 journal bytes
including earlier queries, and returns UNKNOWN without an accepted UNSAT claim.
The following negative query returns SAT and its model checks. Independent
reference proofs resolve the positive exact query, but do not turn BSAT UNKNOWN
into a BSAT solve. All conclusive results in these histories are checked or
independently confirmed; this finite validation is not a general soundness proof.

A bounded restart-prefix implementation experiment is specified in
[the current milestones](INCREMENTAL_PERFORMANCE_MILESTONES.md). It targets
repeated assumption propagation; the resulting certified-only change and validation are recorded there.

Raw evidence under `benchmark_results/`:
`cal3-portable-retained-20260909.json`, `cal3-portable-fresh-20260909.json`,
`cal3-portable-nojournal-20260909.json`.
