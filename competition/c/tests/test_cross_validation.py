#!/usr/bin/env python3
"""
BSAT Cross-Validation Tests

Tests that C and Python solvers produce identical results on all common fixtures.

This integration test suite:
1. Runs both C and Python solvers on all shared fixtures
2. Verifies SAT/UNSAT agreement
3. Validates solutions actually satisfy the CNF
4. Reports any discrepancies between solvers

Exit codes:
  0 - All tests passed, C and Python agree perfectly
  1 - Tests failed, solvers disagree or solutions invalid
"""

import sys
import os
import json
import subprocess
from pathlib import Path

# Add src to path for Python solver
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../python'))

from bsat.dimacs import read_dimacs_file, parse_dimacs_solution
from bsat.cnf import CNFExpression
import cdcl_optimized


# Paths
TESTS_DIR = Path(__file__).parent.parent.parent / "tests"
FIXTURES_DIR = TESTS_DIR / "fixtures"
EXPECTED_RESULTS_FILE = TESTS_DIR / "expected_results.json"
C_SOLVER = Path(__file__).parent.parent / "bin" / "bsat"


def load_expected_results():
    """Load expected SAT/UNSAT results for all fixtures."""
    with open(EXPECTED_RESULTS_FILE, 'r') as f:
        return json.load(f)


def solve_with_c(fixture_path):
    """
    Solve using C solver.

    Returns:
        (result, solution) where:
        - result: "SAT", "UNSAT", or "UNKNOWN"
        - solution: dict mapping var names to bool (if SAT), else None
    """
    try:
        # Run C solver
        result = subprocess.run(
            [str(C_SOLVER), str(fixture_path)],
            capture_output=True,
            text=True,
            timeout=5
        )

        # Parse output
        for line in result.stdout.splitlines():
            if line.startswith('s '):
                status = line[2:].strip()
                if status == 'SATISFIABLE':
                    # Find solution line
                    for sol_line in result.stdout.splitlines():
                        if sol_line.startswith('v '):
                            # Parse DIMACS solution: "v 1 -2 3 0"
                            solution_str = sol_line[2:].strip()
                            literals = [int(x) for x in solution_str.split() if x != '0']

                            # Convert to dict: {x1: True, x2: False, x3: True}
                            solution = {}
                            for lit in literals:
                                var_num = abs(lit)
                                var_name = f'x{var_num}'
                                solution[var_name] = (lit > 0)

                            return "SAT", solution
                    return "SAT", {}  # SAT but no solution found
                elif status == 'UNSATISFIABLE':
                    return "UNSAT", None
                else:
                    return "UNKNOWN", None

        return "UNKNOWN", None

    except subprocess.TimeoutExpired:
        return "UNKNOWN", None
    except Exception as e:
        print(f"Error running C solver: {e}", file=sys.stderr)
        return "UNKNOWN", None


def solve_with_python(fixture_path):
    """
    Solve using Python solver.

    Returns:
        (result, solution) where:
        - result: "SAT", "UNSAT", or "UNKNOWN"
        - solution: dict mapping var names to bool (if SAT), else None
    """
    try:
        cnf = read_dimacs_file(str(fixture_path))
        solution = cdcl_optimized.solve_cdcl(cnf)

        if solution is None:
            return "UNSAT", None
        else:
            return "SAT", solution

    except Exception as e:
        print(f"Error running Python solver: {e}", file=sys.stderr)
        return "UNKNOWN", None


def validate_solution(fixture_path, solution):
    """
    Validate that a solution actually satisfies the CNF.

    Returns:
        True if valid, False otherwise
    """
    try:
        cnf = read_dimacs_file(str(fixture_path))
        return cnf.evaluate(solution)
    except Exception as e:
        print(f"Error validating solution: {e}", file=sys.stderr)
        return False


def test_fixture(fixture_rel_path, expected_result):
    """
    Test a single fixture with both C and Python solvers.

    Args:
        fixture_rel_path: Relative path from tests/ directory
        expected_result: "SAT" or "UNSAT"

    Returns:
        (passed, message) tuple
    """
    fixture_path = TESTS_DIR / fixture_rel_path

    # Solve with both solvers
    c_result, c_solution = solve_with_c(fixture_path)
    py_result, py_solution = solve_with_python(fixture_path)

    # Check agreement
    if c_result != py_result:
        return False, f"❌ DISAGREE: C={c_result}, Python={py_result}"

    # Check against expected result
    if c_result != expected_result:
        return False, f"❌ WRONG: Expected {expected_result}, got {c_result}"

    # For SAT, validate both solutions
    if c_result == "SAT":
        if c_solution is None or py_solution is None:
            return False, "❌ SAT but no solution provided"

        c_valid = validate_solution(fixture_path, c_solution)
        py_valid = validate_solution(fixture_path, py_solution)

        if not c_valid:
            return False, "❌ C solution invalid"
        if not py_valid:
            return False, "❌ Python solution invalid"

        return True, f"✅ PASS (SAT, both solutions valid)"

    elif c_result == "UNSAT":
        return True, "✅ PASS (UNSAT)"

    else:
        return False, f"❌ UNKNOWN result from both solvers"


def run_all_tests():
    """
    Run cross-validation tests on all fixtures.

    Returns:
        Exit code (0 = success, 1 = failure)
    """
    print("=" * 70)
    print("BSAT Cross-Validation Tests (C vs Python)")
    print("=" * 70)
    print()

    # Check C solver exists
    if not C_SOLVER.exists():
        print(f"❌ C solver not found at {C_SOLVER}")
        print("Run 'make' to build the C solver first.")
        return 1

    # Load expected results
    expected = load_expected_results()

    # Track results
    total = 0
    passed = 0
    failed = 0
    failed_tests = []

    # Test each fixture
    for fixture_rel_path, info in sorted(expected.items()):
        total += 1
        expected_result = info["result"]
        description = info["description"]

        # Test fixture
        test_passed, message = test_fixture(fixture_rel_path, expected_result)

        # Display result
        fixture_name = Path(fixture_rel_path).name
        print(f"{fixture_name:30s} {message}")

        if test_passed:
            passed += 1
        else:
            failed += 1
            failed_tests.append(fixture_name)

    # Summary
    print()
    print("=" * 70)
    print("Summary")
    print("=" * 70)
    print(f"Total:  {total}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")

    if failed > 0:
        print()
        print("Failed tests:")
        for name in failed_tests:
            print(f"  - {name}")
        print()
        print("❌ Cross-validation FAILED")
        return 1
    else:
        print()
        print("✅ All tests passed! C and Python solvers agree perfectly.")
        return 0


if __name__ == '__main__':
    sys.exit(run_all_tests())
