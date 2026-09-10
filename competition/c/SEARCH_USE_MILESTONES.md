# Search usefulness and simplification opportunity — 2026-09-10

Baseline 04f8323; production runtime c6b4ac8. Commit/push each milestone.

1. Extend compile-only diagnostics to cover learned binary propagation and
   conflict participation, clause age and usefulness at deletion/retention.
   Include deleted clauses to avoid survivor bias. Separate diagnostic timings
   from performance; verify fixed-work answer/proof/search parity where possible.
   Diagnose certified retained cal3 (depths 0,1,2,4,8,16) and cal100 (0,2,4),
   flags 3, 60 CPU seconds; independent answers required. No repeated policy grid.
2. Frozen six-file holdout is `benchmark_results/search-use-holdout-20260910.json`.
   Seed 2026091001, one input per family, 10 KB–20 MB, excluding filenames and hashes
   in all recorded JSON history. These are fresh files, not necessarily fresh
   families. Use imported DIMACS directly, without BSAT AIG preparation; upstream
   preprocessing is unknown. Do not inspect holdout outcomes until the candidate
   and gates are frozen. Time budget 15 CPU / 20 wall seconds per solve, two
   serial baseline/candidate repeats; independent certificates and memory recorded.
3. Audit simplification opportunities offline before another implementation:
   bounded full literal-occurrence indexing for source clauses of size 2–4,
   targets 3–16, exact signed SSR checks, root assignment classification, work
   limits and input hashes. Report witnesses and independently verify each
   proposed removal by direct subset checks. Do not change solver clauses or
   infer search speed from opportunity counts. Use development inputs first;
   inspect holdout opportunity counts only after freezing the candidate.
4. Use diagnostic evidence to freeze one targeted search-policy candidate, with
   an explicit distinction from prior clause-activity/protect-used experiments.
   If no defensible new policy emerges, record that finding rather than replay
   a rejected grid. For a candidate require paired baseline/candidate/candidate/
   baseline cal3/cal100 histories, 60 query CPU, independent checking up to 600
   seconds/stage, 2048/512 MiB checker heap/stack. Require no checked loss and >5%
   combined solve/export/check CPU benefit; reject >5% regression on either target.
   Evaluate holdout even on target rejection, with no new tuning. Promotion also
   requires no holdout checked loss or >5% CPU PAR2 regression, macOS/Linux paired
   confirmation, appropriate correctness/failure tests, memory and complete
   transaction acceptance. Rejected changes are archived and removed.
5. Keep docs current, publish counts and limitations, and commit/push outcomes.

Status: protocol and fresh input selection frozen before new measurements.


## Diagnostic evidence and frozen candidate

The 18 retained diagnostic queries all independently check. Full-history clause
use counts (including deletions) show 443,180/479,630 cal3 and 102,197/114,497
cal100 deleted clauses were analyzed since the last reduction. A broad reprieve
would therefore protect most candidates. Current LBD is lower than the stored
score at 4,994,533/7,050,085 cal3 and 672,517/968,222 cal100 eligible analysis
reuse events. Counts are events, not unique clauses or counterfactual benefits.

Freeze one new candidate: certified-only dynamic LBD refresh, clamping refreshed
scores to at least `glue_lbd + 1`. Existing glue clauses stay glue; score refresh
never creates new permanently protected glue. Trigger at analyzed conflicts and
expanded reasons; retain the existing comparator, reduction capacity and schedule.
Unlike prior recency activity and LBD<=6 reprieves, this updates measured level
spread and grants no reduction exemption. Unlike existing `--dynamic-lbd`, it
cannot create permanent glue protection. Compile-time `BSAT_CERTIFIED_REFRESH`,
requiring the certified policy and proof journal; no new public option. Candidate
and all gates are frozen before holdout outcomes are inspected.

Validation so far: all 68 C diagnostic executables, focused ASan/UBSan clause-use
and analysis regressions, 48 public facade parity checks with accounting enabled,
and 200 random exhaustive SSR relation/truth-table cases pass. Diagnostic target
proofs and conflict counts match the production baseline. Expanded headers and
inclusive timers mean diagnostic CPU is not a production performance score.
The offline audit finds zero opportunities on complete cal3 and smaller planning
scans; the larger planning scan exhausts its 1,000,000-unit bound with zero found,
which does not establish absence. No destructive simplifier is warranted yet.
