# Common Test Suite for BSAT Solvers

This directory contains shared test fixtures and expected results used by both the C and Python competition solvers.

## Philosophy

**Language-Independent Testing**: These test cases validate SAT solver behavior regardless of implementation language. Both C and Python solvers should produce identical results (SAT/UNSAT) on all fixtures.

**Fast Unit Tests**: All fixtures in `fixtures/unit/` are designed to solve in <1ms each, enabling rapid test-driven development.

## Directory Structure

```
tests/
├── fixtures/
│   ├── unit/          # Minimal test cases (<1ms solve time)
│   └── regression/    # Edge cases found during development
├── expected_results.json  # Expected SAT/UNSAT for each fixture
└── README.md          # This file
```

## Test Fixtures

### Unit Tests (`fixtures/unit/`)

Fast, minimal test cases covering core functionality:

| File | Description | Expected Result |
|------|-------------|-----------------|
| `trivial_sat.cnf` | Single positive literal | SAT |
| `trivial_unsat.cnf` | Contradiction: (x) ∧ (¬x) | UNSAT |
| `empty.cnf` | Empty CNF (vacuously true) | SAT |
| `unit_propagation.cnf` | Unit propagation chain | UNSAT |
| `simple_sat_3.cnf` | 3-variable SAT instance | SAT |
| `simple_unsat_3.cnf` | 3-variable UNSAT (pigeonhole) | UNSAT |
| `pure_literal.cnf` | Contains pure literal | SAT |
| `horn_sat.cnf` | Horn formula, satisfiable | SAT |
| `horn_unsat.cnf` | Horn formula, unsatisfiable | UNSAT |

### Regression Tests (`fixtures/regression/`)

Edge cases discovered during development:

| File | Description | Expected Result |
|------|-------------|-----------------|
| `backtrack_level_zero.cnf` | Backtrack to decision level 0 | UNSAT |

## Expected Results Format

The `expected_results.json` file maps each fixture to its expected outcome:

```json
{
  "fixtures/unit/trivial_sat.cnf": {
    "result": "SAT",
    "description": "Single positive literal"
  },
  "fixtures/unit/trivial_unsat.cnf": {
    "result": "UNSAT",
    "description": "Contradiction: (x) AND (~x)"
  },
  ...
}
```

## Adding New Fixtures

1. **Create the CNF file** in the appropriate subdirectory:
   - `fixtures/unit/` for fast unit tests
   - `fixtures/regression/` for bug regression tests

2. **Add expected result** to `expected_results.json`:
   ```json
   "fixtures/unit/my_test.cnf": {
     "result": "SAT",  # or "UNSAT"
     "description": "Brief description of what this tests"
   }
   ```

3. **Include comments** in the CNF file explaining:
   - What behavior it tests
   - Why the expected result is SAT/UNSAT
   - Any special properties (pure literals, Horn, etc.)

## Using Common Fixtures

### C Solver

```c
// tests/test_dimacs.c
Solver* s = solver_new();
DimacsError err = dimacs_parse_file(s, "../../tests/fixtures/unit/trivial_sat.cnf");
assert(err == DIMACS_OK);
lbool result = solver_solve(s);
assert(result == TRUE);  // SAT
```

### Python Solver

```python
# tests/test_common_fixtures.py
from bsat.dimacs import read_dimacs_file
from competition.python.competition_solver import solve_dimacs

cnf = read_dimacs_file("../tests/fixtures/unit/trivial_sat.cnf")
result = solve_dimacs(cnf)
assert result is not None  # SAT
```

## Cross-Validation

The integration test suite (`competition/c/tests/integration_test.py`) automatically:
1. Runs both C and Python solvers on all fixtures
2. Verifies SAT/UNSAT matches expected results
3. For SAT results: validates solutions actually satisfy the CNF
4. Reports any discrepancies

## Design Principles

1. **Minimal**: Each fixture tests exactly one concept
2. **Fast**: Unit tests complete in <1ms (total suite <100ms)
3. **Documented**: Comments explain the test case
4. **Verifiable**: Solutions can be checked by evaluation
5. **Portable**: Pure DIMACS format, no language-specific features

## Statistics

- **Unit fixtures**: 9 test cases
- **Regression fixtures**: 1 test case
- **Total**: 10 fixtures
- **Coverage**: SAT/UNSAT, unit propagation, pure literals, Horn formulas, edge cases

Last updated: 2025-10-21
