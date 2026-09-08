# reg-n reference diagnosis

The frozen experiment in `benchmark_results/regn-reference-policy-20260908.json`
tests Kissat feature ablations on the previously timed-out reg-n instance. It
uses two seeded, serial repetitions per profile, a 12-second wall limit, and
independent DRAT-to-LRAT plus CakeML verification for conclusive UNSAT results.
Solver time excludes certificate checking; end-to-end time is recorded too.

The input has 4,608 variables and 825,728 clauses. Of those clauses, 696,320
have length three and 126,976 have length two; 823,296 are entirely negative.
This is a highly structured formula, not evidence about average SAT workloads.

The successful default and elimination-disabled runs each report 318 factored
variables and 5,263 conflicts. Disabling simplification still permits initial
factoring and solves in 5,338 conflicts. Both factoring-disabled runs time out.
Disabling probing also times out; that switch disables a pipeline containing
factoring, so it does not isolate failed-literal probing as the missing feature.

Inspection of the pinned Kissat source explains the option interaction:
`src/probe.c` calls `kissat_factor` during initial probing when
`preprocessfactor` is enabled; `src/factor.c` independently checks `factor`.
Thus `--simplify=false` is not a control that removes all preprocessing.

The next algorithm to prototype for this family is bounded variable addition
(factoring repeated clause structure). It needs fresh-variable allocation,
certificate additions/deletions, original-input model validation, allocation
failure handling and bounded/cancellable work. Keep it opt-in until independent
proof checks and fresh-family comparisons pass. This experiment diagnoses the
reference advantage; it neither implements factoring in BSAT nor establishes
that factoring alone is sufficient to close BSAT's gap.

Full per-run evidence is in `benchmark_results/regn-reference-20260908.json`.

All 14 runs completed the acceptance audit: eight independently verified UNSAT
answers, six UNKNOWN results, no execution or validation errors. Default,
no-sweep, no-eliminate and no-simplify each solve both repetitions in 1.667–1.807
seconds. No-factor, no-probe and plain each hit both 12-second wall limits.
These are two repetitions on one input, with the binary and input hashes pinned.
