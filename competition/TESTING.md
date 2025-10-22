# BSAT Competition Solver Testing Guide

Comprehensive testing infrastructure for both C and Python BSAT competition solvers.

## Quick Start

```bash
# C Unit Tests (22 tests)
cd competition/c
make test

# Python Tests (10 tests)
cd competition/python
python test_common_fixtures.py

# Cross-Validation (C vs Python, 10 fixtures)
cd competition/c/tests
python test_cross_validation.py
```

## Test Organization

```
competition/
├── tests/                          # Shared test infrastructure
│   ├── fixtures/
│   │   ├── unit/                   # Fast unit tests (<1ms each)
│   │   │   ├── trivial_sat.cnf
│   │   │   ├── trivial_unsat.cnf
│   │   │   ├── empty.cnf
│   │   │   ├── unit_propagation.cnf
│   │   │   ├── simple_sat_3.cnf
│   │   │   ├── simple_unsat_3.cnf
│   │   │   ├── pure_literal.cnf
│   │   │   ├── horn_sat.cnf
│   │   │   └── horn_unsat.cnf
│   │   └── regression/             # Edge case regression tests
│   │       └── backtrack_level_zero.cnf
│   ├── expected_results.json       # Expected SAT/UNSAT for each fixture
│   └── README.md                   # Fixture documentation
│
├── c/
│   ├── tests/
│   │   ├── test_dimacs.c          # DIMACS I/O tests (11 tests)
│   │   ├── test_solver.c          # Core solver tests (11 tests)
│   │   ├── test_cross_validation.py  # C vs Python validation
│   │   └── run_all.sh             # Test runner script
│   └── Makefile                    # Build system with test targets
│
└── python/
    └── test_common_fixtures.py     # Python solver tests (10 tests)
```

## Test Suites

### 1. Common Test Fixtures (10 fixtures)

**Location**: `competition/tests/fixtures/`

Language-independent test cases used by both C and Python solvers.

**Philosophy**:
- **Minimal**: Each fixture tests exactly one concept
- **Fast**: All unit tests complete in <1ms (total suite <10ms)
- **Documented**: Comments explain expected behavior
- **Verifiable**: Solutions can be validated by CNF evaluation

**Coverage**:
- SAT/UNSAT detection
- Empty formulas
- Unit propagation
- Contradiction detection
- Pure literal elimination
- Horn formulas
- Regression edge cases

**Expected Results**: All fixtures have expected SAT/UNSAT results in `tests/expected_results.json`

### 2. C Unit Tests (22 tests)

**Location**: `competition/c/tests/`

Tests for C solver implementation.

#### 2.1 DIMACS I/O Tests (`test_dimacs.c` - 11 tests)

Tests DIMACS parsing, writing, and file I/O:
- Parse all common fixtures
- Handle comments and empty lines
- Parse unit clauses correctly
- Validate SAT/UNSAT results

**Run**:
```bash
cd competition/c
make test
# OR
bin/test_test_dimacs
```

#### 2.2 Core Solver Tests (`test_solver.c` - 11 tests)

Tests core solver functionality:
- Solver creation/destruction
- Variable and clause management
- Empty formulas (SAT)
- Unit clauses
- Contradiction detection
- Simple SAT instances
- Statistics tracking
- Conflict limits
- Solving with assumptions
- Multiple solve calls

**Run**:
```bash
cd competition/c
make test
# OR
bin/test_test_solver
```

### 3. Python Tests (10 tests)

**Location**: `competition/python/test_common_fixtures.py`

Tests Python solver against all common fixtures:
- Validates correct SAT/UNSAT determination
- Verifies solutions actually satisfy CNF
- Ensures all fixtures pass

**Run**:
```bash
cd competition/python
python test_common_fixtures.py
```

**Output**:
```
test_trivial_sat ... ok
test_trivial_unsat ... ok
test_empty ... ok
...
----------------------------------------------------------------------
Ran 10 tests in 0.001s

OK
```

### 4. Cross-Validation Tests (10 fixtures)

**Location**: `competition/c/tests/test_cross_validation.py`

Integration tests that verify C and Python solvers produce identical results:
1. Runs both solvers on all fixtures
2. Verifies SAT/UNSAT agreement
3. Validates both solutions are correct
4. Reports any discrepancies

**Run**:
```bash
cd competition/c/tests
python test_cross_validation.py
```

**Output**:
```
======================================================================
BSAT Cross-Validation Tests (C vs Python)
======================================================================

trivial_sat.cnf                ✅ PASS (SAT, both solutions valid)
trivial_unsat.cnf              ✅ PASS (UNSAT)
...
======================================================================
Summary
======================================================================
Total:  10
Passed: 10
Failed: 0

✅ All tests passed! C and Python solvers agree perfectly.
```

## Build Integration

### C Makefile Targets

```bash
# Build all test binaries
make tests

# Run all tests
make test

# Clean and rebuild
make clean
make test
```

The Makefile automatically:
- Compiles test sources with all dependencies
- Links against solver core (arena, dimacs, solver, watch)
- Runs each test binary sequentially
- Reports pass/fail for each test

## Test Development

### Adding New Common Fixtures

1. **Create CNF file** in `tests/fixtures/unit/` or `tests/fixtures/regression/`:
   ```dimacs
   c Description of what this tests
   c Expected: SATISFIABLE or UNSATISFIABLE
   p cnf <nvars> <nclauses>
   <clause1>
   <clause2>
   ...
   ```

2. **Add expected result** to `tests/expected_results.json`:
   ```json
   "fixtures/unit/my_test.cnf": {
     "result": "SAT",
     "description": "Brief description"
   }
   ```

3. **Tests automatically pick it up** - no code changes needed!

### Adding C Unit Tests

Edit `competition/c/tests/test_dimacs.c` or `test_solver.c`:

```c
void test_my_feature() {
    TEST("My feature description");

    Solver* s = solver_new();
    // ... test code ...

    if (/* failure condition */) {
        FAIL("Error message");
    }

    solver_free(s);
    PASS();
}

// Add to main():
int main() {
    // ...
    test_my_feature();
    // ...
}
```

Rebuild and run: `make test`

### Adding Python Tests

Edit `competition/python/test_common_fixtures.py`:

```python
def test_my_feature(self):
    """Description of test."""
    self._test_fixture(
        "fixtures/unit/my_test.cnf",
        self.expected["fixtures/unit/my_test.cnf"]["result"]
    )
```

Run: `python test_common_fixtures.py`

## Test Statistics

### Coverage

**Common Fixtures**: 10 test cases
- Unit tests: 9 cases
- Regression tests: 1 case

**C Tests**: 22 unit tests
- DIMACS I/O: 11 tests
- Core solver: 11 tests

**Python Tests**: 10 tests
- All common fixtures

**Cross-Validation**: 10 test cases
- Validates C-Python agreement

**Total**: 42 distinct test cases

### Performance

**C Unit Tests**: ~100ms total
- Individual tests: <10ms each
- Suitable for rapid iteration

**Python Tests**: ~1-10ms total
- Individual tests: <1ms each
- Very fast feedback

**Cross-Validation**: ~50-100ms
- Runs both solvers on 10 fixtures
- Includes solution validation

**Total Test Suite**: <500ms
- Fast enough for pre-commit hooks
- Suitable for CI/CD integration

## Continuous Integration

Recommended CI workflow:

```yaml
test:
  script:
    # C tests
    - cd competition/c
    - make test

    # Python tests
    - cd ../python
    - python test_common_fixtures.py

    # Cross-validation
    - cd ../c/tests
    - python test_cross_validation.py
```

## Troubleshooting

### C Tests Fail to Build

```bash
cd competition/c
make clean
make tests
```

### Python Import Errors

Ensure `bsat` package is installed:
```bash
cd ../..  # bsat root
pip install -e .
```

### Cross-Validation Fails

1. Ensure C solver is built: `cd competition/c && make`
2. Check Python solver works: `cd ../python && python test_common_fixtures.py`
3. Run cross-validation with verbose output

### Tests Pass But Solver Fails on Real Instances

- Common fixtures are minimal test cases
- Use `dataset/simple_tests/` for more comprehensive testing
- Run benchmarks for performance validation

## Design Principles

1. **Language Independence**: Common fixtures work with any SAT solver
2. **Fast Execution**: All tests complete in <500ms
3. **Comprehensive Coverage**: Tests all major solver features
4. **Automatic Validation**: Solutions are verified, not just accepted
5. **Easy to Extend**: Adding new tests is straightforward
6. **Build Integration**: Tests run via `make test`
7. **Cross-Platform**: Pure C11 and Python 3, no platform-specific code

## Related Documentation

- `tests/README.md` - Common fixture documentation
- `c/README.md` - C solver documentation
- `python/README.md` - Python solver documentation (if exists)
- `../docs/` - Algorithm documentation

## Maintenance

**When to Run Tests**:
- Before committing changes
- After modifying solver core
- After adding new features
- Before releases

**When to Add Tests**:
- Discovered a bug → Add regression test
- New feature → Add unit test
- Edge case found → Add to common fixtures

**When to Update Fixtures**:
- Found incorrect expected result
- Need more coverage in specific area
- Regression test for production bug

Last updated: 2025-10-21
