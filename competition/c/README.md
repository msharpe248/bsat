# BSAT C solver

BSAT is a CDCL solver under active development. The September 2026 hardening
work fixes known wrong-answer and memory-management defects and adds independent
certificate validation. Passing these tests does not establish production
readiness for arbitrary workloads. See [HARDENING.md](HARDENING.md) for changes,
validation evidence, research references, and remaining limitations.

## Build and verify

Requires a C11/POSIX compiler, make, and Python 3 for validation and benchmarks.

```sh
cd competition/c
make all test
make all test MODE=debug
python3 tests/validate.py --solver bin/bsat --cases 200
python3 tests/validate.py --solver bin/bsat_debug --cases 100 --checker /path/to/drat-trim
```

Object and unit-test directories are separated by build mode. The debug build
uses assertions, AddressSanitizer and UndefinedBehaviorSanitizer. CI checks Linux
and macOS, and builds a pinned independent `drat-trim` checker. Regression tests
include malformed DIMACS, clauses crossing line boundaries, empty clauses,
assumption sequences, clause reduction, locked reasons, and garbage collection.
The Python validator uses its own truth-table oracle, model parser and RUP
checker, tries option combinations, and minimizes failing formulas.

## Run

```sh
./bin/bsat input.cnf
./bin/bsat --proof proof.drat input.cnf
./bin/bsat --binary-proof --proof proof.drat input.cnf
/path/to/drat-trim input.cnf proof.drat
```

Exit codes: 10 SAT, 20 UNSAT, 0 UNKNOWN/resource limit, 1 input/internal/I/O error.
Every returned SAT model is checked against the retained original input. Proof
logging covers search, minimization, probing, equivalence substitution, BVE, BCE
deletion and vivification;
UNSAT certificates end with an empty clause. Text and binary DRAT are supported.
The search algorithm is the same with proof logging enabled or disabled.

DIMACS input uses a fixed 64 KiB reader to reduce per-byte stdio overhead.
Embedded NULs in tokens are rejected and stream failures report file errors.
See [BUFFERED_INPUT.md](BUFFERED_INPUT.md) for boundary tests and parse-only
and end-to-end measurements.

DIMACS requires one `p cnf` header, in-range variables, an exact clause count and
zero-terminated clauses. Clauses may span lines, and multiple clauses may share
one line. Duplicates and tautologies are normalized. Numeric overflow and
unfinished clauses are errors. Empty formulas are SAT; empty clauses are UNSAT.

## Search controls

Random phase overrides are disabled by default. Enable the previous random-phase
policy with `--random-phase`; `--random-prob` sets its probability (default 0.01)
but does not enable it on its own. `--no-random-phase` explicitly disables it.
Include `--random-phase` when reproducing earlier VMTF or binary-minimization
profiles that used the former default.
The 44-input comparison preserved all seven verified solves and improved several
substantial solved cases, while slowing one very short case. See
[NO_RANDOM_PHASE_EVALUATION.md](NO_RANDOM_PHASE_EVALUATION.md) for the tradeoffs.

Defaults use VSIDS, phase saving, deterministic random diversification,
LBD-based EMA restarts, bounded failed-literal probing, clause minimization,
bounded subsumption, learned-clause reduction, and circular watch scanning.
Binary propagation retains compact implicit watches; every original clause also
has an arena record for preprocessing and bookkeeping.
Learned binaries use tagged watches for direct binary propagation while retaining
arena reasons and minimization behavior. See
[TAGGED_BINARY_WATCHES.md](TAGGED_BINARY_WATCHES.md) for validation, a faster
binary-chain microbenchmark, and the unchanged development solve count.

Best-phase targets append only new literals while the saved trail prefix remains
valid. Backtracking into that prefix triggers a full rebuild at the next record.
This preserves target choices and removes quadratic copying on uninterrupted
descents. See [TARGET_SAVING.md](TARGET_SAVING.md) for tests and measurements.

Clause normalization uses a fixed 16-literal temporary buffer for short clauses
and heap storage for longer ones. Caller arrays and original-input snapshots
remain unchanged. See [CLAUSE_NORMALIZATION.md](CLAUSE_NORMALIZATION.md) for tests
and the incremental loading-speed measurement.

Assignment-trail entries contain only a literal (four bytes per slot). Decision
levels remain authoritative in variable metadata, avoiding a redundant level
copy in the trail. See [COMPACT_TRAIL.md](COMPACT_TRAIL.md) for validation,
storage savings and performance measurements.

Truth assignments use a dense byte array, separate from variable metadata, to
reduce cache traffic during propagation. Search order is preserved. See
[VALUES.md](VALUES.md) for before/after measurements and validation.

```sh
./bin/bsat --seed 42 --time 30 input.cnf
./bin/bsat --glucose-restart-avg input.cnf
./bin/bsat --luby-restart --luby-unit 100 input.cnf
./bin/bsat --elim --bce --preprocess-budget 1000000 input.cnf
./bin/bsat --equiv --equiv-budget 1000000 input.cnf
./bin/bsat --inprocess --inprocess-interval 10000 input.cnf
./bin/bsat --alternating input.cnf
./bin/bsat --no-circular --subsume-budget 0 input.cnf
```

Unsuccessful local-search attempts preserve the CDCL trail. Walks keep root
assignments fixed while allowing non-root decisions to change in their separate
assignment. See [LOCAL_SEARCH_STATE.md](LOCAL_SEARCH_STATE.md) for tests and the
comparison against forced root backtracking and plain CDCL.

Local-search flip scores now maintain exact break-minus-make contributions for
all affected variables. See [LOCAL_SEARCH_SCORES.md](LOCAL_SEARCH_SCORES.md) for
the regression oracle and measured comparisons. Build the standalone walk driver
with `make local-search-benchmark`, then run
`./bin/local_search_benchmark_release input.cnf 20000 1` (flip budget and seed).
It emits a checked SAT model or UNKNOWN; local search remains opt-in.
Unsatisfied-clause selection uses an ordered prefix-count tree to preserve walk
choices while avoiding a formula scan per flip. See
[LOCAL_SEARCH_SELECTION.md](LOCAL_SEARCH_SELECTION.md) for tests, memory cost and
standalone/full-solver measurements.
Walk initialization computes flip scores during the clause scan; see
[LOCAL_SEARCH_INITIALIZATION.md](LOCAL_SEARCH_INITIALIZATION.md) for validation
and repeated-initialization measurements.
Local-search deadline checkpoints take a fresh CPU reading every 256 flips. See
[LOCAL_SEARCH_BUDGETS.md](LOCAL_SEARCH_BUDGETS.md) for the cached-deadline regression
and preliminary flip-budget comparisons.
The [broader budget evaluation](LOCAL_SEARCH_BUDGET_EVALUATION.md) compares
10,000 and 100,000 flips across 44 fixed inputs. Statistics also report total
local-search flips, so configured limits can be compared with actual work.
An [inverse-square probabilistic walk experiment](PROBABILISTIC_WALK_EXPERIMENT.md)
was rejected after repeated structured-instance regressions; its tested patch and
positive and negative results are preserved for reproducibility.
Experimental `--ls-save-phases` feeds improved walks back into non-root saved
phases when local search and phase saving are enabled. See
[WALK_PHASE_FEEDBACK_EXPERIMENT.md](WALK_PHASE_FEEDBACK_EXPERIMENT.md) for the
reproduced targeted gains, broader results, and remaining Kissat performance gap.

Successful local-search model transfer resets assignment positions, reasons and
the propagation cursor consistently. See
[LOCAL_SEARCH_TRANSFER.md](LOCAL_SEARCH_TRANSFER.md) for its regression and
focused model/proof validation command.

Use `--help` for the exact CLI names, including the time-limit option. Preprocessing
has a deterministic literal-work budget; zero disables it. The time limit is CPU
time for solving, including preprocessing, checked within long operations.
Clock reads are amortized between bounded poll/work intervals; cancellation and
work limits are checked on every poll. Target copying and solver return force
a CPU-time check. See [DEADLINE_CHECKS.md](DEADLINE_CHECKS.md) for responsiveness
tests, measured speedups and the limits of this polling scheme.
BVE, BCE, vivification, local search, and alternating modes remain opt-in.
Alternating mode combines stable Luby intervals and partial target assignments
with focused LBD restarts; it is experimental and does not implement every
Kissat scheduling or branching technique. `--lrb` is the existing recency-weighted
activity heuristic, not a complete MapleSAT LRB implementation.

Opt-in `--vmtf` selects variables from a conflict-recency queue. It overrides heap
selection in both focused and stable modes, while retaining heap bookkeeping.
Ordered conflict bumps improved the development sample, but this policy remains
experimental. See [ORDERED_VMTF.md](ORDERED_VMTF.md) for the latest measurements and
[VMTF.md](VMTF.md) for the initial implementation, validation, and
comparison with the existing phase policy and Kissat.

Experimental `--vmtf --reuse-trail` retains high-priority decision prefixes at
restarts. It repeatedly improved one multiplier-circuit input by about 8.8×,
but regressed on Hamiltonian inputs; the 44-input screen preserved 7/44 solves.
It is disabled by default, and using reuse with heap ordering regressed in the
targeted screen. See [RESTART_REUSE_EXPERIMENT.md](RESTART_REUSE_EXPERIMENT.md)
for the implementation, verification and complete comparisons.

VMTF skips inactive numeric score updates during conflicts. Fixed-work timing
shows a modest throughput improvement with matching search counters; see
[VMTF_SCORES.md](VMTF_SCORES.md) for measurements and timing controls.

Phase saving records decisions as well as propagated assignments, retaining
random and target-phase choices after backtracking. See
[DECISION_PHASES.md](DECISION_PHASES.md) for the regression and performance tradeoffs.
An experiment suppressing phase updates during probes was rejected after
targeted comparisons; see [PROBE_PHASE_EXPERIMENT.md](PROBE_PHASE_EXPERIMENT.md).
Fixed and growing random-decision bursts also failed to increase verified solves;
see [RANDOM_DECISION_EXPERIMENT.md](RANDOM_DECISION_EXPERIMENT.md) for the archived
prototypes, tests, and comparisons.
Retaining binary consequences from probing lost two development solves and was
rejected; see [PROBE_BINARIES_EXPERIMENT.md](PROBE_BINARIES_EXPERIMENT.md).
Growing reduction intervals improved multiplier-circuits but lost another solve,
including at a longer limit. See
[GROWING_REDUCTION_EXPERIMENT.md](GROWING_REDUCTION_EXPERIMENT.md) for both tested
schedules, the integration regression, and the rejection decision.

Opt-in `--binary-minimize` applies bounded binary resolution to learned clauses
of at most 30 literals and LBD at most 6. It uses up to `--minimize-budget`
additional watch inspections after the ordinary minimizer, preserves the
asserting literal, and recomputes LBD and backjump level. Zero budget or
`--no-minimize` disables both passes. It nearly halved runtime on one multiplier
input, but showed no additional solves on the broader development sample and
slowed some other inputs. See [BINARY_MINIMIZATION.md](BINARY_MINIMIZATION.md)
for validation and measured tradeoffs. It is experimental and disabled by default.

Opt-in `--iterative-minimize` follows both implicit binary and arena reasons using
an iterative traversal with shared successful dependency checks. The
`--minimize-budget` option caps antecedent inspections per learned clause
(default 10000; zero disables either minimizer). Exhausting the budget retains
unproven literals. See [MINIMIZATION.md](MINIMIZATION.md) for validation and
before/after measurements.

The default recursive minimizer now uses the same inspection budget, checks
deadlines inside reason traversal, and caches proven subtrees within each
literal's redundancy check. Cached heights preserve its existing depth limit.
Depth alone could not bound work on shared implication graphs. See
[MINIMIZATION_LIMITS.md](MINIMIZATION_LIMITS.md)
for the regression and before/after evidence.

Opt-in `--equiv` substitutes signed binary SCC equivalences after probing. Its
separate `--equiv-budget` defaults to 1000000 inspections; zero disables the pass.
It supports proof logging and model reconstruction, and skips assumption calls.
Measured runs showed no solved-count gain and increased memory use, so it remains
experimental. See [EQUIVALENCE.md](EQUIVALENCE.md).

Opt-in `--protect-used` grants one reduction reprieve to learned clauses with
LBD at most six after use in conflict analysis. Reuse renews protection; otherwise
it expires. The development sample gained one verified SAT solve, while fresh
inputs showed no solved-count change. See [USED_CLAUSES.md](USED_CLAUSES.md) for
tests, measurements and the limits of this experimental policy.

Opt-in `--dynamic-lbd` lowers learned-clause quality scores when later conflict
analysis uses fewer decision levels. This changes retention during reduction.
The stress sample showed mixed timing improvements and regressions, with no
competition solved-count gain. See [DYNAMIC_LBD.md](DYNAMIC_LBD.md) for tests,
measurements and the comparison with Glucose.

## C API

Include `include/solver.h` and link the core objects without `main.o`.
Use `solver_model_value` to read assignments; invalid variable indices return
UNDEF. Internal structure layout changed with the compact assignment array:
recompile embedding code and replace direct `vars[v].value` accesses with the
accessor. The C API does not promise a stable binary layout.
`solver_new_with_opts` returns NULL for invalid numeric options; `clause_decay`
must be finite and in (0,1].
`solver_solve_with_assumptions` accepts repeated or contradictory assumptions.
Subsequent solves, new variables, or added clauses rebuild working state from the
original input. This restores eliminated clauses and avoids stale assumption
results, but does not retain learned clauses across calls. Allocation failures or
invalid API literals set `Solver.error`; no SAT/UNSAT answer is returned on error.

An assumption solve with proof output configured returns UNKNOWN: the current
API does not expose conditional proofs. For a certificate of the augmented
formula, explicitly add the assumptions as unit clauses to that formula.
The solver still uses process-wide diagnostic flags and a SIGUSR1 progress
handler; concurrent embedding is not yet a supported API contract.

## Measure performance

```sh
python3 tests/generate_benchmarks.py /tmp/bsat-development --split development
python3 tests/generate_benchmarks.py /tmp/bsat-heldout --split heldout
python3 tests/benchmark.py --checker /path/to/drat-trim \
  --solver 'bsat=bin/bsat --proof {proof} {input}' \
  --solver 'kissat=/path/to/kissat {input} {proof}' \
  --solver 'cadical=/path/to/cadical {input} {proof}' \
  --timeout 30 --repeats 3 --split heldout --output results.json /tmp/bsat-heldout
```

Runs are serial within the harness and order is seeded. Results include executable
and input hashes, commands, platform, wall/CPU time, peak RSS, solver counters,
verified solved counts and PAR-2. Certificate checking is outside solver timing;
unverified results receive the timeout penalty. Keep development and held-out
families separate and avoid other workloads during timing. Synthetic cases and
millisecond process runs establish smoke-test coverage, not industrial speedups.

For an additional-instance screen, `tests/select_corpus.py` can exclude filenames
and content hashes already recorded in a JSON results directory and select one
small input per family. Pass its output to `tests/benchmark.py --manifest FILE`;
the runner verifies input hashes before starting and records the manifest hash.
See [DEFAULT_BRANCHING.md](DEFAULT_BRANCHING.md) for commands, tests, sampling
limitations, and the additional-input comparison supporting the heap default.

An [LBD generation-mark experiment](LBD_GENERATION_EXPERIMENT.md) removed the
per-clause mark-clearing scan, but repeated competition-input measurements did
not justify its extra storage. The tested patch and results are preserved; the
production LBD implementation is unchanged.

A [glue-reason activity reward](GLUE_REASON_BUMP_EXPERIMENT.md) improved one
Hamiltonian search but added no solve in the targeted or longer multiplier
comparisons. The fixed-threshold prototype was rejected; its tested patch and
measurements are preserved.

The follow-up [relative-quality reason reward](QUALITY_REASON_BUMP_EXPERIMENT.md)
compared reason LBD with the final minimized learned clause. It lost both
battleship solves in the repeated reference screen and was also rejected; the
implementation, tests and pinned-Kissat comparison are preserved.

BVE now uses [linear signed-mark resolution](LINEAR_ELIMINATION.md) for costing
and constructing resolvents, with interruption-safe scratch cleanup and staging.
Long overlapping-clause measurements show substantial kernel speedups; measured
competition solved counts are unchanged. BVE remains opt-in with `--elim`.

[Reconstruction staging](ELIMINATION_STAGING.md) now charges copy work, checks
deadlines while staging pure clauses, and transfers the completed buffer into
the elimination stack. This removes a duplicate allocation/copy while preserving
the public copying API used by BCE and equivalence.
