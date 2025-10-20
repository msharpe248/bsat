# Competition Solver Test Suite

Tests for the optimized CDCL solver with two-watched literals.

## Test Files

### Core Test Suite
- **test_simple.py**: Quick smoke test (2 basic cases)
- **test_correctness.py**: Full correctness validation vs original solver
- **test_comprehensive.py**: 40+ test cases across all problem types

### Inprocessing Tests
- **test_inprocessing.py**: Basic inprocessing functionality tests
- **test_inprocessing_soundness.py**: Comprehensive soundness tests for subsumption, self-subsumption, and variable elimination

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

# Inprocessing tests
python test_inprocessing.py
python test_inprocessing_soundness.py

# Benchmark inprocessing (slow)
python benchmark_inprocessing.py
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

✅ All core tests pass after soundness bug fix (October 20, 2025)
✅ Inprocessing algorithms correct but disabled by default (too slow in Python)
✅ Solver ready for Week 2-3: Adaptive restarts implementation
