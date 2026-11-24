# Bounded vivification ordering experiment

Rejected the runtime change. Ordering the existing 100-clause window by shorter
size, then lower LBD, retained the same six verified answers out of ten runs but
worsened mean wall PAR-2 from 17.0671 to 17.4374 seconds. Production keeps its
existing order. Inprocessing remains opt-in.

The candidate uses fixed 100-entry stack arrays and stable insertion sorting;
it does not scan/sort the entire learned database. It keeps the existing RUP
literal-deletion trials, proof addition/replacement ordering and propagation
budgets. Selecting the window up front changes cursor advancement on early exits,
and reordered trials can change watches, phases and later search. Consequently,
this is a search-policy experiment, not a claim of equivalent execution traces.

Before timing, release and ASan/UBSan builds each passed all 48 C executables and
4,876 independently checked solves (seed 20261333). The release candidate also
passed allocation-failure tests and 24 structured/metamorphic long-search cases
with independently checked SAT models and UNSAT proofs. Finite testing supports
regression confidence, not universal solver soundness.

The frozen policy uses the same five development families as the prior reuse
experiment, two repetitions, 15 solving CPU seconds and 20 external wall seconds,
with binary proofs and the same independent checker. No local builds or fuzz
campaigns run during timing. Both versions verify 6/10 runs with zero errors.

- Battleship conflicts rise from 17,491 to 257,207. Process CPU changes from
  0.104/0.182 seconds to 1.832/1.840 seconds; the search regression is substantial.
- Belpyramid UNSAT improves from 34,493 to 32,150 conflicts and from 4.400/4.321
  CPU seconds to 3.727/3.773. Both repetitions' proofs are verified.
- Hamiltonian has identical conflicts and completes before this policy matters.
  Hardware verification and ktf remain UNKNOWN in every run.

Wall timings show scheduling variation (notably one baseline Battleship run), so
small wall deltas should not be overinterpreted. The larger, repeatable search
regression outweighs the Belpyramid improvement. This confirms that shorter/low-LBD
priority alone is insufficient here. It does not reject all bounded selection
policies or establish results on unseen inputs or target Linux hardware.

Reproduction records are in `benchmark_results/selection-policy-20260907.json`,
`selection-targeted-20260907.json` and `selection-validation-20260907.json`.
`selection-rejected-20260907.patch` archives the full candidate against the current
production solver source. Apply only in an isolated experiment. Earlier prefix
and assumption-reuse failures are documented in `VIVIFICATION_REUSE_EXPERIMENT.md`.
