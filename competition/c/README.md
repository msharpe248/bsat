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

DIMACS requires one `p cnf` header, in-range variables, an exact clause count and
zero-terminated clauses. Clauses may span lines, and multiple clauses may share
one line. Duplicates and tautologies are normalized. Numeric overflow and
unfinished clauses are errors. Empty formulas are SAT; empty clauses are UNSAT.

## Search controls

Defaults use VSIDS, phase saving, deterministic random diversification,
LBD-based EMA restarts, bounded failed-literal probing, clause minimization,
bounded subsumption, learned-clause reduction, and circular watch scanning.
Binary propagation retains compact implicit watches; every original clause also
has an arena record for preprocessing and bookkeeping.

Best-phase targets append only new literals while the saved trail prefix remains
valid. Backtracking into that prefix triggers a full rebuild at the next record.
This preserves target choices and removes quadratic copying on uninterrupted
descents. See [TARGET_SAVING.md](TARGET_SAVING.md) for tests and measurements.

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
