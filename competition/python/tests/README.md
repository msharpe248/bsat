# Competition Solver Test Suite

Tests for the Python prototype CDCL solver (`../cdcl_optimized.py`) with two-watched literals.

Results below were last checked on 2026-11-25. The shared fixture tests used by
both solvers live one level up (`../test_common_fixtures.py`, see
[`../../TESTING.md`](../../TESTING.md)).

## Test Files

### Core Test Suite
- **test_simple.py**: Quick smoke test (2 basic cases) — passes
- **test_correctness.py**: Correctness validation vs original solver (13 formulas) — passes
- **test_comprehensive.py**: 31 test cases across all problem types — 28 pass. The 3 reported failures ("Propagation leading to UNSAT", "Backtracking UNSAT", "Cycle structure") are wrong expected values in the test script: all three solver configurations agree with each other, and the formulas are respectively SAT, SAT, and UNSAT
- **test_competition_solver.py**: Runs `../competition_solver.py` on a few `dataset/` instances — passes
- **test_soundness.py**: Compares watched-literal mode against the solver's legacy naive propagation mode (`use_watched_literals=False`) on 10 `dataset/` instances — currently 6/10. In all 4 mismatches the default watched-literal mode returns a verified satisfying assignment while the naive mode reports UNSAT after a handful of conflicts, i.e. the naive mode is the unsound one. Do not rely on `use_watched_literals=False`

### Inprocessing Tests
- **test_inprocessing.py**: Basic inprocessing functionality tests
- **test_inprocessing_soundness.py**: Comprehensive soundness tests for subsumption, self-subsumption, and variable elimination

These two scripts, the debugging scripts, and `benchmark_inprocessing.py` still
carry import and `dataset/` paths from before they were moved into `tests/`.
Run them with `PYTHONPATH=..:../../../src` (see below); their parts that load
`dataset/` files are skipped or report "File not found".

### Debugging Scripts (Historical)
- **debug_simple.py**: Debug script for soundness bug investigation
- **trace_bug.py**: Detailed trace of initial unit clause propagation bug
- **test_watch_key_bug.py**: Verification script for watch key bug analysis

### Benchmarks
- **benchmark_inprocessing.py**: Performance testing for inprocessing features (disabled by default due to O(n²) complexity in Python)

## Running Tests

```bash
# Quick smoke test
python test_simple.py

# Full test suite
python test_comprehensive.py

# Correctness validation
python test_correctness.py

# Command-line wrapper and naive-vs-watched comparison
python test_competition_solver.py
python test_soundness.py

# Inprocessing tests (need the import path set explicitly)
PYTHONPATH=..:../../../src python test_inprocessing.py
PYTHONPATH=..:../../../src python test_inprocessing_soundness.py

# Benchmark inprocessing (slow)
PYTHONPATH=..:../../../src python benchmark_inprocessing.py
```

## Test Coverage

- ✅ Unit clauses
- ✅ Binary clauses (2-SAT)
- ✅ 3-SAT instances
- ✅ Unit propagation chains
- ✅ Backtracking
- ✅ Conflict-driven learning
- ✅ Pigeon hole problems (small)
- ✅ Edge cases (tautologies, mixed sizes)
- ✅ Structured instances
- ✅ Inprocessing correctness (subsumption, self-subsumption, BVE)
- ✅ Initial unit clause propagation (soundness bug fixed)

## Status

- Default configuration (watched literals): `test_simple.py`, `test_correctness.py`, `test_competition_solver.py`, and the shared fixture tests pass; `test_comprehensive.py` passes except for its 3 incorrect expectations.
- Legacy naive mode (`use_watched_literals=False`): unsound on some instances (`test_soundness.py`, 6/10).
- Inprocessing algorithms are disabled by default (too slow in Python).
- The prototype is no longer under active development; current solver work is in [`../../c/`](../../c/README.md).
