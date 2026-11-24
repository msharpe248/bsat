# Search quality and expanded validation — 2026-09-08

This completed batch is followed by [Linux acceptance, learning ablations and
longer measurements](ACCEPTANCE_AND_SEARCH_MILESTONES.md).

Authorized: complete all four recommended work items, commit and push each
validated milestone. Freeze this policy before new solves or candidate edits.

1. **Fresh, larger holdouts.** Eight distinct CNF families selected with seed
   2026090860, 1–30 MB, excluding filenames/content hashes in 584 historical JSON
   reports. Add the only unrecorded local planning file (204.8 MB); explicitly
   relax the size bound for this stress case, rather than recycle either small
   planning input. Four larger pinned circuits are selected before solving, one
   each from beem, industry, opensource and wolf, seed 2026090861, 30–180 KB AIG.
   Their inputs are larger than all four original circuit sources. The manifests
   pin inputs and selection rules. These are research screens, not customer SLOs.
2. **Learned-clause cost.** Correct the preceding recommendation: scan-heat
   selection already failed an earlier holdout (HOT_CLAUSE_STRENGTHENING.md).
   Measure eligibility (learned size 3–64 versus larger), then test a cheaper
   selection representation only if evidence warrants it. Preserve the existing
   RUP routine and explicitly measure strengthening cost/payback. Reject changes
   without a repeatable >=10% development improvement, no lost confirmation
   solves and <=5% aggregate confirmation PAR-2 regression. Do not silently
   change the public certified mode's supported transformation set.
3. **cal3 structural diagnosis.** Use the exact depth-16 positive snapshot and
   compare existing equivalence, congruence and elimination options with one-shot
   references. Separate diagnostic one-shot transforms from retained assumption
   safety. Verify every conclusive result. A negative result is an outcome.
4. **Certified probing expansion.** Compare public flags 3/7 on the four new
   circuits at depths 0,2,8,24, two repetitions alternating mode order, five CPU
   seconds per query. Record checked answers, CPU PAR-2, journal, capacity and
   checkpoint/export costs. Exercise deeper original histories and checkpoint
   replay. Keep probing opt-in unless separate evidence warrants promotion.

One-shot holdout comparison: unchanged BSAT versus pinned Kissat, binary proofs,
10 process-CPU seconds and 15 external wall seconds, two serial repetitions,
seed 2026090863. Count late answers as outside the budget. Report UNKNOWN and
resource failures separately; no unverified conclusive result counts as solved.
Instrumented runs are not timing scores. Run timed workloads serially without
concurrent local builds/tests/fuzz. Existing independent models, DRAT-to-LRAT and
CakeML checks remain mandatory for conclusive results; a checker timeout is a
validation failure, not evidence of solver unsoundness.

Fresh holdouts become development evidence once results are inspected. No
repeated tuning against these inputs may be described as fresh confirmation.
Deployment latency/memory/durability SLOs require an actual application envelope;
the numerical limits here are experiment gates only.

1. Complete: frozen inputs and checked large-CNF baseline; see EXPANDED_HOLDOUT_BASELINE.md.
2. Complete: eligibility/payback measurements and corrected prior-experiment
   review; retain diagnostics, no speculative selector. See LEARNED_CLAUSE_ELIGIBILITY.md.
3. Complete: structural options do not close cal3; the reference also solves
   with preprocessing disabled. See CAL3_STRUCTURAL_DIAGNOSIS.md.
4. Complete: 128 larger paired circuit queries, 32 sanitizer checkpoint queries
   and 64 deeper original-circuit checkpoint queries. A checker heap/stack limit
   was reproduced and fixed without relaxing verification. Both modes preserve
   solved cases; larger-holdout probing CPU PAR-2 regresses 3.4%, so defaults stay
   unchanged. See EXPANDED_CERTIFIED_PROBING.md and CHECKER_RESOURCE_SCALING.md.
