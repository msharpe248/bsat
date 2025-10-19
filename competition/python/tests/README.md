# Competition Solver Test Suite

Tests for the optimized CDCL solver with two-watched literals.

## Test Files

- **test_simple.py**: Quick smoke test (2 basic cases)
- **test_correctness.py**: Full correctness validation vs original solver
- **test_comprehensive.py**: 40+ test cases across all problem types

## Running Tests

```bash
# Quick smoke test
python test_simple.py

# Full test suite
python test_comprehensive.py

# Correctness validation
python test_correctness.py
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

## Status

Basic tests pass (unit clauses, simple SAT/UNSAT). Full suite has some edge cases to debug.
