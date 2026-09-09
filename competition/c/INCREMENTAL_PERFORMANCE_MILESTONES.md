# Incremental performance follow-up — 2026-09-09

Authorized scope: broaden Linux confirmation of the portable reduction order,
remeasure retained certified cal3, and implement a targeted change only when
controlled evidence identifies a bottleneck. Update documentation and commit and
push each milestone.

1. Freeze a broader Linux regression corpus and same-runner baseline/current
   comparison before timing. Eight named families, lexicographically first local
   SAT Competition 2025 filename no larger than 2 MB in each: battleship,
   belpyramid-puzzle, hamiltonian, coloring, cryptography, argumentation,
   scheduling, tseitin-formulas. Selection uses names and sizes, not outcomes.
   These are existing repository datasets; no pristine holdout claim.
   Two repetitions, 30 process CPU / 45 wall seconds, seed 2026090910;
   independent model or DRAT/LRAT/CakeML checking (600 seconds per stage,
   2048 MB heap / 512 MB stack). Compare pinned pre-sort commit 67d80b5
   against current default with portable -O3 -flto on the same Linux runner.
   Gate: no invalid answer, no checked solve loss, aggregate CPU PAR2 regression
   at most 5%. UNKNOWN remains unfinished. Inspect per-family regressions even
   if aggregate passes. This screen is smaller than deployment acceptance.
2. Remeasure cal3 public-facade retained uncertified/certified histories at 60
   query CPU seconds, with independent retained CaDiCaL and checked witnesses.
   Then compare fresh certified state and journal-free diagnostic execution
   under the same budget to separate retained search and certificate costs.
3. Select one implementation hypothesis from this evidence, validate soundness
   and repeat frozen confirmation. Reject unsuccessful experiments explicitly;
   do not change defaults merely to complete the milestone.

Status: broader Linux screen complete, gate passed with 4/16 checked runs in
each version and +0.037% CPU PAR2; six unresolved families limit the evidence.
Retained/fresh/journal baseline complete: see [updated diagnosis](CAL3_INCREMENTAL_FOLLOWUP.md).
Certified restart-prefix implementation and final confirmation in progress.

## Bounded implementation experiment (frozen before candidate timing)

The retained certified control executes 10,049 restarts in its 60-second query.
The search loop currently backtracks every assumption restart to level zero,
then reapplies the same assumption prefix. Test preserving the established
prefix, capped at the current decision level and number of assumptions. Ordinary
conflict backjumps remain unrestricted and new queries still backtrack to zero.
This may avoid repeated propagation, but changes saved-phase/watch behavior and
therefore needs measured search and correctness checks, not a presumed speedup.

Use a separate compile-time experimental build. Require release/sanitizer and
independently checked retained certificates before timing. Test the same retained
cal3 history with the same 60-second allowance first. If it adds no checked solve,
reject it for this target; if it succeeds, confirm repeated cal3 and all four
pinned industrial circuit histories before any default promotion. Preserve the
baseline and archive a rejected patch instead of leaving an unused switch.

Confirmation refinement: the global prototype resolves all certified queries but
loses the uncertified cal3 solve (UNKNOWN at 60 seconds versus the earlier control
solve at 46.928). Do not promote it globally. Scope prefix retention to the
certified facade via an internal option, preserving existing uncertified and
one-shot policy. Revalidate this final implementation and compare the unchanged
control across all four circuits. The uncertified regression stays in the record.
