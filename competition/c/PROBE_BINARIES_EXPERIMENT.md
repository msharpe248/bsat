# Rejected experiment: binary consequences from probing

This implementation was rejected after losing two development solves. No new
production option is retained. The patches and measurements below preserve the
experiment for future work.

The prototype follows the single-assumption probing approach described in the
[SAT Handbook preprocessing chapter](https://fmv.jku.at/papers/BiereJarvisaloKiesl-SAT-Handbook-2021-Preprocessing-Chapter-Manuscript.pdf).
When a long clause propagates a literal under one assumption, that implication
can be retained as a binary consequence. This implementation uses the probe's
assumption as the antecedent; it does not compute the closer dominators used
by more elaborate lazy hyper-binary resolution algorithms.

The optional `--probe-binaries` prototype retains at most 64 clauses per probe
and 4096 per solve. It samples only implications with long arena reasons,
skipping binary reasons. Candidate scans and duplicate-watch inspections consume
the existing preprocessing work budget. Deadline checks also cover these loops
and insertion. If the budget expires, uncommitted candidates are discarded.

A probe runs under one temporary assumption and root assignments. Each emitted
clause is `(negated_assumption | consequence)`, which is RUP against the current
formula. Clauses are emitted only after backtracking, outside watch traversal.
They use the existing internal-add path, retained arena records and implicit
binary watches, like SCC-exposed binaries. They do not enter the preserved
original input. This permits existing elimination and SCC passes to process them.
No extraction occurs during multi-assumption vivification or failed probes.

Baseline source: `40001458869801fec3b18590784c2793bb112229` (runtime unchanged
from `092a94e`). The prototype passes all 17 C test executables in release and
ASan/UBSan builds. Regression cases cover long-clause consequences, root units,
duplicate suppression, budgets 0 through 99, heap/VMTF branching, assumptions,
and rebuilds. Each build passes 6210 validator solves: 54 configurations across
15 fixed and 100 random formulas, seed 20260923, with truth-table answers,
original-model checks and text/binary RUP proofs. External drat-trim checks
UNSAT certificates. Each build also passes 24 short-deadline cases; maximum
observed overruns were 0.002 seconds in release and 0.001 seconds in debug.

Performance uses VMTF with a ten-second solving CPU limit and fifteen-second
wall timeout. The targeted sample reuses belpyramid, hgen and multiplier-circuits,
twice each. Baseline verifies 4/6 runs, mean wall PAR-2 12.6369; the candidate
verifies 2/6, PAR-2 21.0960. Both miss hgen. The candidate misses multiplier
in both runs despite baseline process CPU of 4.248 and 4.678 seconds. It adds
981 binaries there and reaches the 4096 cap on belpyramid. All reported answers
are independently checked, with zero errors.

These are reused development inputs, not held-out competition results. Runs
are serial without concurrent BSAT builds or tests. Process CPU includes startup,
parsing and proof output, while the solving CPU limit excludes parsing.
Independent checking is outside solver timing. UNKNOWN and unverified answers
receive twice the wall timeout as PAR-2. Heap performance was not benchmarked.

## Broader comparison and decision

The same 28 reused development inputs were compared once per solver with the
same limits. Baseline verifies 6/28, mean wall PAR-2 24.1500; the candidate
verifies 4/28, PAR-2 25.8274. There are no gained solves and two lost solves:
multiplier-circuits and maximum-constraint-partition. Both versions report zero
errors. The latter input finishes near the baseline deadline (9.5 wall seconds),
so these short runs are sensitive to machine load. The paired experiment still
provides no evidence for retaining this prototype.

This result rejects the tested implementation and limits, not hyper-binary
resolution in general. Candidate scanning shares the failed-literal work budget,
and new watches change propagation order and phase updates. These effects were
not isolated. Closer antecedents, different scheduling, and cheaper duplicate
handling remain possible alternatives, without a measured benefit here.

After restoration, all 16 production C test executables pass in both release
and debug builds. The rebuilt release executable is byte-identical to the saved
baseline (SHA-256 `1f6ca7fd8078bef247a8adc063a0dcd3f79d163b521b4e84c33278e68290cc04`).
The archived patch passes `git apply --check --unidiff-zero` against the restored
source. Production solver and test sources are unchanged.

## Archived evidence

- [Prototype and regressions](benchmark_results/probe-binaries-prototype-20260906.patch)
- [Validation and deadline record](benchmark_results/probe-binaries-validation-20260906.json)
- [Targeted timing](benchmark_results/probe-binaries-targeted-20260906.json)
- [Broad timing](benchmark_results/probe-binaries-broad-20260906.json)

## Reproduction

The prototype patch includes the C implementation, the dedicated regression,
seven added validator configurations, two fixed formulas, and two added deadline
modes. Apply its zero-context diff independently to the baseline with
`git apply --unidiff-zero <patch>` from the repository root, then run:

```sh
make -C competition/c -j4 all test
make -C competition/c -j4 MODE=debug all test
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /path/to/drat-trim --cases 100 --seed 20260923
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug --checker /path/to/drat-trim --cases 100 --seed 20260923
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat_debug
```

Use `competition/c/tests/benchmark.py` with the reports' input paths, command
templates, limits and repetitions. Reports pin input and executable hashes;
local dataset paths and the temporary baseline binary must be recreated on
another machine. Keep performance timing separate from local builds and tests.
