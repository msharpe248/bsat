# BSAT C solver

BSAT is a CDCL solver under active development. The September 2026 hardening
work fixes known wrong-answer and memory-management defects and adds independent
certificate validation. Passing these tests does not establish production
readiness for arbitrary workloads. Start with
[Production readiness](PRODUCTION_READINESS.md), the authoritative capability
matrix, evidence limits and release gates. The current batch is recorded in
[ASSUMPTION_SEARCH_MILESTONES.md](ASSUMPTION_SEARCH_MILESTONES.md).
[HARDENING.md](HARDENING.md) and earlier milestone reports preserve historical
changes and measurements. Reproducible checks are indexed in
[tests/FEATURE_COVERAGE.md](tests/FEATURE_COVERAGE.md).

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
logging covers search, minimization, probing, gate congruence, equivalence substitution, bounded factoring, BVE, BCE
deletion and vivification;
UNSAT certificates end with an empty clause. Text and binary DRAT are supported.
The search algorithm is the same with proof logging enabled or disabled.
Text proofs use chunked decimal encoding. See [CHUNKED_TEXT_PROOFS.md](CHUNKED_TEXT_PROOFS.md)
for byte/error tests, measured text-proof CPU savings and the larger-input
comparison with Kissat.

DIMACS input uses a fixed 64 KiB reader to reduce per-byte stdio overhead.
Embedded NULs in tokens are rejected and stream failures report file errors.
See [BUFFERED_INPUT.md](BUFFERED_INPUT.md) for boundary tests and parse-only
and end-to-end measurements.

DIMACS requires one `p cnf` header, in-range variables, an exact clause count and
zero-terminated clauses. Clauses may span lines, and multiple clauses may share
one line. Duplicates and tautologies are normalized. Numeric overflow and
unfinished clauses are errors. Empty formulas are SAT; empty clauses are UNSAT.

## Search controls

Use `make MODE=diagnostic` and `bin/bsat_diagnostic --accounting` for detailed
watch/scan and learned-reason counters. These counters are compiled out of ordinary
builds. See [PLANNING_PROPAGATION.md](PLANNING_PROPAGATION.md) for the corrected
blocker skip-rate denominator and measured planning bottlenecks. The combined
phase/factoring holdout rejected a global policy change; see
[COMBINED_HOLDOUT.md](COMBINED_HOLDOUT.md) and [PHASE_REFRESH.md](PHASE_REFRESH.md).

Independent long API histories and an unmodified upstream IPASIR application are
covered in [INDEPENDENT_INTEGRATION.md](INDEPENDENT_INTEGRATION.md). For measured
checkpoint/export latency, core-capacity growth and the cost of discarding learning,
see [SERVICE_SESSION_TAILS.md](SERVICE_SESSION_TAILS.md).

Experimental `--reduce-increment 1000` grows the gap between learned-clause
reductions, starting at `--reduce-interval` (2,000 by default). Zero increment
keeps the existing fixed schedule. See [REDUCTION_GROWTH.md](REDUCTION_GROWTH.md)
for correctness checks and the measured speed/memory tradeoff.

Experimental `--chrono` keeps lower-level implications when backtracking past
higher-level decisions. With congruence and alternating VMTF it substantially
reduced decisions on hardware model checking and recovered development SAT
solves. It is disabled by default. See
[LOGICAL_CHRONOLOGICAL_BACKTRACKING.md](LOGICAL_CHRONOLOGICAL_BACKTRACKING.md)
for the trail invariants, independent checks and measured limits.

Successful equivalence rebuilding now preserves the scratch state used for
learned-clause LBD scores. Earlier affected runs incorrectly scored all learned
clauses as glue. See [EQUIV_LBD_STATE.md](EQUIV_LBD_STATE.md) for regression
coverage and the mixed speed, solve-count and memory results after correction.

Experimental `--congruence --equiv --equiv-budget 100000000` extracts AND,
XOR and ITE gates, proves aliases with RUP, then substitutes equivalent
variables. It improves a large hardware-model-checking development case but
adds preprocessing and memory costs. It is disabled by default. See
[GATE_CONGRUENCE.md](GATE_CONGRUENCE.md) for checks, measurements and limitations.

Experimental `--portfolio 2` gives focused search two CPU seconds, then starts
fresh alternating search if that slice expires. `--time`, conflict and decision
limits cover both attempts; the second attempt resets its proof to the original
input. It combines complementary development solves at the cost of rebuilding,
extra memory and slower answers on some inputs. It does not change the default.
See [PORTFOLIO_SEARCH.md](PORTFOLIO_SEARCH.md) for the API contract, independent
checks and benchmark evidence.

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
with focused LBD restarts, starting with 1,000 focused conflicts.
[Initial focused interval measurements](FOCUSED_START_EXPERIMENT.md) show repeated
gains on `ktf` and belpyramid, a small absolute Hamiltonian slowdown, and unchanged
solved counts. Alternating mode is experimental and does not implement every
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

For production embedding, include `include/bsat.h` and build with `make shared`.
The opaque ABI-v1 interface supports independent solver instances, copied DIMACS
clause/assumption arrays, model/core queries, per-query limits/statistics and
cooperative cancellation. The installed `ipasir.h` supplies the IPASIR adapter;
see [IPASIR.md](IPASIR.md). `make embedding-test soak-test` exercises a dynamically linked
client and larger stateful histories. See [EMBEDDING.md](EMBEDDING.md) and
[API_CONTRACT.md](API_CONTRACT.md) for ownership, error and concurrency rules.

Calls on one handle must be serialized; distinct handles may solve concurrently.
The library does not install signal handlers. CPU limits use solving-thread CPU
time. The CLI alone handles SIGUSR1 progress requests and environment diagnostics.

Opt-in `BSAT_REUSE_LEARNTS` (internal `SolverOpts.reuse_learnts`) retains learned
clauses across compatible calls, including conditional UNSAT. Entailed congruence
additions are compatible; reconstruction, destructive preprocessing, ordinary
proof streams and interruption use conservative rebuild fallbacks. See
[INCREMENTAL_REUSE.md](INCREMENTAL_REUSE.md).
`solver.h` remains an internal development interface with no stable structure ABI.
Allocation/argument failures poison a handle and cannot yield a conclusive answer.

`BSAT_CERTIFICATES` and `bsat_export_query` export exact query CNFs and proof
prefixes from the actual retained session without re-solving. See
[RETAINED_CERTIFICATES.md](RETAINED_CERTIFICATES.md). The internal ordinary
assumptions-plus-proof-stream call still returns UNKNOWN; the CLI-oriented
[certify_query.py](tests/certify_query.py) fresh-solve workflow remains available.
Growing counter/configuration histories above 4,000 variables and their independent
oracles/certificates are documented in [APPLICATION_HISTORIES.md](APPLICATION_HISTORIES.md).

Opt-in `--factor` performs proof-producing binary/ternary residual factoring.
It closes the measured reg-n development gap but regresses another workload;
see [FACTORING.md](FACTORING.md) for scope, correctness checks and timings.

The latest longer evaluation is in [INDUSTRIAL_LONG.md](INDUSTRIAL_LONG.md):
72 audited trials, 12 fresh inputs across 11 families, and 60-second solve limits.
It exposes remaining speed gaps against Kissat; every conclusive answer is
independently checked. [FULL_SEARCH_MEMORY.md](FULL_SEARCH_MEMORY.md) separates
persistent retained-capacity savings from variable peak RSS.

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
verified solved counts and PAR-2. Certificate checking is outside solver timing; validation and combined end-to-end
time are reported separately. Unverified results receive the timeout penalty. Keep development and held-out
families separate and avoid other workloads during timing. Synthetic cases and
millisecond process runs establish smoke-test coverage, not industrial speedups.

Use `--retain-unverified /path/to/artifacts` to keep unchecked answer evidence
after a failed or timed-out check. Inputs, proof bytes, solver/checker output
and hashed metadata are saved in unique directories. Retention does not mark
an answer verified or remove its penalty. Checker acceptance requires an exact
`s VERIFIED` line and a recognized exit status, including drat-trim's
trivial-UNSAT exit-code-1 case. See
[CERTIFICATE_ARTIFACTS.md](CERTIFICATE_ARTIFACTS.md).

For an additional-instance screen, `tests/select_corpus.py` can exclude filenames
and content hashes already recorded in a JSON results directory and select one
input per family, smallest first within optional inclusive `--min-bytes` and
`--max-bytes` limits (zero maximum means unlimited). Pass its output to
`tests/benchmark.py --manifest FILE`;
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

[Compact reconstruction records](COMPACT_ELIMINATION_RECORDS.md) store a default
pivot and the cheaper parent polarity. Pure-variable records need only two
words, and overlapping-clause examples use roughly half the previous payload.
Independent model/proof checks pass; competition solved counts are unchanged,
and BVE remains opt-in.

A [long-clause prefetch experiment](WATCH_PREFETCH_EXPERIMENT.md) preserved search
counters but measured 1.36% higher aggregate CPU time on 26 development inputs.
The hint was rejected; 459 mixed-watch regression cases and the measured patch
are retained.

A [deep-trail restart-blocking experiment](RESTART_BLOCKING_EXPERIMENT.md) tested
EMA and sliding-window variants against the existing defaults and pinned Kissat.
It added no solves and repeatedly slowed battleship with EMA; the prototype was
rejected, with its implementation, tests and measurements preserved.

[Compact variable metadata](COMPACT_VARIABLES.md) keeps LRB-only timestamps in
an optional array and removes an unused field. Default variable records shrink
from 40 to 32 bytes on arm64, with matching measured search counters and lower
peak RSS on large allocation cases. Broad fixed-work CPU results are essentially
flat.

[Garbage collection forwarding](FORWARDING_GC.md) reuses old clause headers after
successful copying, removing the arena-sized temporary relocation map. Tests
preserve headers, watches, reasons and occurrences; measured search counters and
solved counts are unchanged. Collection CPU and peak RSS vary by workload.

[Bounded GC copying](GC_COPY_BUDGET.md) checks deadlines before allocation and
commit, charges header/copy work, and supports rollback at copy-chunk boundaries.
The committed relocation phase remains synchronous. Tests reproduce the old
ignored-budget behavior; measured aggregate CPU cost is about 0.9% on the fixed
work development screen, with unchanged solved counts.

[Larger fresh-input evaluation](LARGE_FRESH_EVALUATION.md) uses eight previously
unrecorded inputs with roughly 240,000–373,000 clauses and a 30-second wall limit. BSAT
verifies 1/8 answers versus Kissat 3/8, identifying two further search targets.
A five-repeat check confirms BSAT's fast result on the shared multiplier case;
this does not establish overall parity.

[Retention cutoff evaluation](RETENTION_CUTOFF_EVALUATION.md) found no solved-case
gain on the larger hardware-verification target after repeated 30-CPU-second
runs. Defaults remain unchanged. A new C regression covers 51,840 ranking,
keep-fraction, cutoff and relocation cases, with three mutation checks.

[Linear blocked-clause checks](LINEAR_BCE.md) reuse the bounded resolution-pair
routine instead of quadratic literal comparisons. Long overlapping-clause tests
run up to 6.45 times faster in process CPU and complete more elimination within
a fixed work budget. Larger-input solved counts are unchanged; BCE remains opt-in.

[A local scan-work counter experiment](SCAN_WORK_EXPERIMENT.md) preserved all
recorded search counters but measured about 1.3% more aggregate CPU on the fixed
work screen, so the runtime change was rejected. Retained tests cover exact
long-scan accounting and interruption replay at polling boundaries.

[Rephasing policy experiments](REPHASE_POLICY_EXPERIMENT.md) rejected disabling,
delaying or refreshing the historical target policy on the tested screen.
Retained checks cover partial-target application and rapid/disabled rephasing
with independent model and proof validation.

[Compact target phases](COMPACT_TARGET_PHASES.md) store the three target truth
values in one byte rather than four. The million-variable allocation case uses
about 4.3% less peak RSS; fixed-work search counters and solved counts match.
Measured aggregate CPU is about 0.7% higher, so this is retained as a memory
improvement rather than a demonstrated search speedup.

A [recursive binary minimization experiment](RECURSIVE_BINARY_MINIMIZATION_EXPERIMENT.md)
was rejected after repeated battleship and Hamiltonian regressions without added
solves. The default remains conservative; 160 signed graph/budget correctness
cases are retained across both existing minimizers.

An [incremental prefix vivification experiment](PREFIX_VIVIFICATION_EXPERIMENT.md)
reduced long-clause kernel CPU but weakened clauses and repeatedly regressed
battleship without new solves. The existing vivifier is retained, together with
4,688 independent truth-table/RUP regression cases and a diagnostic that reports
both cost and residual clause size.

[Reusing assumptions during full deletion vivification](VIVIFICATION_REUSE_EXPERIMENT.md)
kept the generated strengthening results and reduced kernel CPU, but lost two
competition solves. That replacement is also rejected. The retained suite now
adds exact deletion-oracle comparisons and long-scan interruption/retry checks.

A [rolling-omission trial setup experiment](ROLLING_OMISSION_EXPERIMENT.md)
preserved proof traces and measured search counters, but showed mixed kernel
results and no convincing CPU gain. The runtime change is rejected; 2,064 signed
partial-progress cutoff cases and an irreducible-clause benchmark are retained.

A [shared blocker-dispatch experiment](BLOCKER_DISPATCH_EXPERIMENT.md) found no
useful overall CPU gain and was rejected. The retained 10,368-case regression
checks mixed binary/long-watch truth, conflict order, reasons and phase saving.

A [dynamic BVE scheduling experiment](ELIMINATION_SCHEDULE.md) eliminated more
variables on two larger inputs but added no solves and regressed on belpyramid,
even with a larger budget. Numeric-order elimination remains in place. New
statistics report eliminated variables, resolvents and removed clauses; retained
tests check 512 complete BVE formulas and 1,539 cutoff/resume cases.

[Staged-resolvent redundancy experiments](MINIMAL_RESOLVENTS.md) tested exact
deduplication and removal of implied supersets. Neither added solves; the stronger
variant's large-budget gain came with an ordinary-budget regression, so both were
rejected. Retained tests cover 2,048 signed-parent projections and all 84 staging
cutoffs without requiring either experimental policy.
