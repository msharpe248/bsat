# Targeted performance investigation — 2026-09-08

Authorized scope: investigate cal3, certified gen23 and planning propagation;
commit and push each validated milestone. This is an investigation, not a promise
to promote a new heuristic. Production defaults stay unchanged without evidence.

Frozen before running new experiments:

1. **cal3 search diagnosis.** Replay the existing depths 0,1,2,4,8,16 and both
   output polarities. Compare retained BSAT/CaDiCaL with the same solver-thread
   CPU allowance, then examine the difficult query with a longer allowance.
   Record search work and all budget outcomes, verify conclusive answers.
2. **Certified gen23.** Replay the same histories across public flag combinations.
   A test-only journal-off control preserves certified search options to separate
   proof emission from changed search. Require identical conclusive statuses,
   work counters and models across the journal-on/off control before attributing
   timing differences to proof emission. Repeat three times in alternating order.
3. **Planning.** Compare frozen planning inputs with BSAT and Kissat under equal
   external CPU/wall budgets, and use fixed-conflict diagnostics to distinguish
   search progress from propagation cost. Preserve UNKNOWN and PMU unavailability.

Any candidate promotion requires independently checked answers, release/sanitizer
regressions, and at least a 10% repeatable gain on the target with no loss of
solved cases or >5% aggregate regression on a separate confirmation history/corpus.
These are experiment acceptance thresholds, not deployment SLOs. If no candidate
meets them, commit the negative result and a concrete next hypothesis.

Timing runs are serial without concurrent local builds/tests/fuzzing. Instrumented
phase/counter runs are diagnostic only. Fresh proof checks are outside solve timing.

1. cal3: complete. Equal-budget reference gap confirmed, 30-second control and
   phase/restart/retention switches remain UNKNOWN. See CAL3_SEARCH_DIAGNOSIS.md.
2. gen23: complete. Ordinary-budget certified probing passes the frozen target
   and confirmation gates; exposed as an opt-in with unchanged defaults. See
   GEN23_CERTIFIED_DIAGNOSIS.md for proof checks, public API parity and regressions.
3. planning: measurements complete; final analysis pending.
