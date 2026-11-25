# BSAT Competition Solver Testing Guide

How to run the tests shared by the C and Python BSAT competition solvers: the
common fixtures, the Python fixture tests, and the C-vs-Python cross-validation.

This guide covers the shared-fixture layer only. The C solver has a much larger
test and verification suite of its own (unit tests, `validate.py`, sanitizer,
fault-injection, fuzzing, and proof-checking targets); see
[c/README.md](c/README.md#build-and-verify) and
[c/tests/FEATURE_COVERAGE.md](c/tests/FEATURE_COVERAGE.md). The Python
prototype's own scripts are described in
[python/tests/README.md](python/tests/README.md).

## Quick Start

```bash
# C unit tests (every tests/test_*.c program; includes the fixture tests)
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
│   │   ├── test_dimacs.c          # DIMACS I/O + shared fixture tests (12 tests)
│   │   ├── test_solver.c          # Core solver API tests (11 tests)
│   │   ├── test_*.c               # ~75 further C test programs (see c/tests/FEATURE_COVERAGE.md)
│   │   ├── test_cross_validation.py  # C vs Python validation
│   │   └── run_all.sh             # Wrapper that runs `make test`
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
- **Fast**: Each fixture solves in well under a millisecond
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

### 2. C Unit Tests

**Location**: `competition/c/tests/`

`make test` builds and runs every `tests/test_*.c` program (77 at the time of
writing; about 5 seconds for a release build). Two of them exercise the shared
fixtures and the basic solver API and are described here; the rest cover
individual C solver features and are indexed in
[c/tests/FEATURE_COVERAGE.md](c/tests/FEATURE_COVERAGE.md).

#### 2.1 DIMACS I/O Tests (`test_dimacs.c` - 12 tests)

Tests DIMACS parsing and file I/O:
- Parse and solve the common unit fixtures (all except `pure_literal.cnf`)
- Handle comments and empty lines
- Parse unit clauses correctly
- Reject malformed input
- Validate SAT/UNSAT results

**Run** (the fixture paths are relative to `competition/c`, so run from there):
```bash
cd competition/c
make test
# OR, after `make tests`
bin/tests/release/test_dimacs
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
# OR, after `make tests`
bin/tests/release/test_solver
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

**Output** (abridged):
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
- Compiles each `tests/test_*.c` into `bin/tests/<MODE>/` (`MODE=release` by default; `MODE=debug` adds ASan/UBSan)
- Links each test against every solver object except `main.o`
- Runs each test binary sequentially and stops at the first failure

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

3. **Cross-validation picks it up automatically** (it iterates over
   `expected_results.json`). The Python fixture tests and `test_dimacs.c` name
   each fixture explicitly, so add a test method/function there as shown below.

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

**C Tests on the shared layer**: 23 tests
- DIMACS I/O and fixtures (`test_dimacs.c`): 12 tests
- Core solver API (`test_solver.c`): 11 tests
- (`make test` runs these plus all other C test programs)

**Python Tests**: 10 tests
- All common fixtures

**Cross-Validation**: 10 test cases
- Validates C-Python agreement

### Performance

**C Unit Tests** (`make test`, all programs, release build): about 5 seconds
- `test_dimacs` and `test_solver` themselves finish in a few milliseconds

**Python Tests**: ~1-10ms total (plus interpreter start-up)

**Cross-Validation**: well under a second
- Runs both solvers on 10 fixtures
- Includes solution validation

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

`test_common_fixtures.py` and `test_cross_validation.py` add the repository's
`src/` directory to `sys.path` themselves, so no installation is required as
long as the repository layout is intact. If you run solver code from elsewhere,
install the package from the repository root:
```bash
pip install -e .
```

### Cross-Validation Fails

1. Ensure C solver is built: `cd competition/c && make`
2. Check Python solver works: `cd ../python && python test_common_fixtures.py`
3. Re-run cross-validation; each fixture line reports which solver disagreed or returned an invalid model

### Tests Pass But Solver Fails on Real Instances

- Common fixtures are minimal test cases
- Use `dataset/simple_tests/` and `dataset/medium_tests/` (repository root) for more comprehensive testing
- For the C solver, run `python3 tests/validate.py --solver bin/bsat` and the
  wide-corpus targets described in [c/README.md](c/README.md)

## Design Principles

1. **Language Independence**: Common fixtures work with any SAT solver
2. **Fast Execution**: The fixture tests complete in well under a second
3. **Basic Coverage**: SAT/UNSAT, propagation, and edge cases; feature-level coverage lives in the C test suite
4. **Automatic Validation**: Solutions are verified, not just accepted
5. **Easy to Extend**: Adding new tests is straightforward
6. **Build Integration**: Tests run via `make test`
7. **Cross-Platform**: C11/POSIX and Python 3

## Related Documentation

- [tests/README.md](tests/README.md) - Common fixture documentation
- [c/README.md](c/README.md) - C solver documentation
- [c/tests/FEATURE_COVERAGE.md](c/tests/FEATURE_COVERAGE.md) - C test coverage index
- [python/README.md](python/README.md) - Python solver documentation
- [FEATURE_TESTING.md](FEATURE_TESTING.md) - Choosing instances that exercise specific features
- [docs/](docs/README.md) - Algorithm documentation

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

Last updated: 2026-11-25
