#!/usr/bin/env python3
"""
BSAT Python Competition Solver - Common Fixture Tests

Tests the Python competition solver against shared test fixtures
to ensure correctness and parity with C solver.

This test suite validates:
- Correct SAT/UNSAT determination
- Solution validity for SAT instances
- Proper handling of edge cases
"""

import sys
import os
import json
import unittest

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.dimacs import read_dimacs_file
from bsat.cnf import CNFExpression
import cdcl_optimized


# Path to shared test infrastructure
TESTS_DIR = os.path.join(os.path.dirname(__file__), '../tests')
FIXTURES_DIR = os.path.join(TESTS_DIR, 'fixtures')
EXPECTED_RESULTS_FILE = os.path.join(TESTS_DIR, 'expected_results.json')


def load_expected_results():
    """Load expected SAT/UNSAT results for all fixtures."""
    with open(EXPECTED_RESULTS_FILE, 'r') as f:
        return json.load(f)


def solve_fixture(fixture_path):
    """
    Solve a fixture file using the competition solver.

    Returns:
        (result, solution) where:
        - result: True (SAT), False (UNSAT), or None (UNKNOWN)
        - solution: dict mapping var names to bool (if SAT), else None
    """
    cnf = read_dimacs_file(fixture_path)
    solution = cdcl_optimized.solve_cdcl(cnf)

    if solution is None:
        return False, None
    else:
        return True, solution


class TestCommonFixtures(unittest.TestCase):
    """Test Python solver against shared fixtures."""

    @classmethod
    def setUpClass(cls):
        """Load expected results once for all tests."""
        cls.expected = load_expected_results()

    def _test_fixture(self, fixture_name, expected_result):
        """
        Test a single fixture.

        Args:
            fixture_name: Relative path from tests/ directory
            expected_result: "SAT" or "UNSAT"
        """
        fixture_path = os.path.join(TESTS_DIR, fixture_name)

        # Solve the fixture
        is_sat, solution = solve_fixture(fixture_path)

        # Check SAT/UNSAT matches expectation
        if expected_result == "SAT":
            self.assertTrue(is_sat, f"{fixture_name} should be SAT")
            self.assertIsNotNone(solution, f"{fixture_name} should have solution")

            # Validate solution actually satisfies CNF
            cnf = read_dimacs_file(fixture_path)
            is_valid = cnf.evaluate(solution)
            self.assertTrue(is_valid, f"{fixture_name} solution should be valid")

        elif expected_result == "UNSAT":
            self.assertFalse(is_sat, f"{fixture_name} should be UNSAT")
            self.assertIsNone(solution, f"{fixture_name} should have no solution")
        else:
            self.fail(f"Invalid expected result: {expected_result}")

    # ========================================
    # Unit Tests
    # ========================================

    def test_trivial_sat(self):
        """Single positive literal."""
        self._test_fixture(
            "fixtures/unit/trivial_sat.cnf",
            self.expected["fixtures/unit/trivial_sat.cnf"]["result"]
        )

    def test_trivial_unsat(self):
        """Contradiction: (x) ∧ (¬x)."""
        self._test_fixture(
            "fixtures/unit/trivial_unsat.cnf",
            self.expected["fixtures/unit/trivial_unsat.cnf"]["result"]
        )

    def test_empty(self):
        """Empty CNF (vacuously true)."""
        self._test_fixture(
            "fixtures/unit/empty.cnf",
            self.expected["fixtures/unit/empty.cnf"]["result"]
        )

    def test_unit_propagation(self):
        """Unit propagation chain leading to conflict."""
        self._test_fixture(
            "fixtures/unit/unit_propagation.cnf",
            self.expected["fixtures/unit/unit_propagation.cnf"]["result"]
        )

    def test_simple_sat_3(self):
        """Simple 3-variable SAT instance."""
        self._test_fixture(
            "fixtures/unit/simple_sat_3.cnf",
            self.expected["fixtures/unit/simple_sat_3.cnf"]["result"]
        )

    def test_simple_unsat_3(self):
        """Simple 3-variable UNSAT (pigeonhole)."""
        self._test_fixture(
            "fixtures/unit/simple_unsat_3.cnf",
            self.expected["fixtures/unit/simple_unsat_3.cnf"]["result"]
        )

    def test_pure_literal(self):
        """Contains pure literal."""
        self._test_fixture(
            "fixtures/unit/pure_literal.cnf",
            self.expected["fixtures/unit/pure_literal.cnf"]["result"]
        )

    def test_horn_sat(self):
        """Horn formula, satisfiable."""
        self._test_fixture(
            "fixtures/unit/horn_sat.cnf",
            self.expected["fixtures/unit/horn_sat.cnf"]["result"]
        )

    def test_horn_unsat(self):
        """Horn formula, unsatisfiable."""
        self._test_fixture(
            "fixtures/unit/horn_unsat.cnf",
            self.expected["fixtures/unit/horn_unsat.cnf"]["result"]
        )

    # ========================================
    # Regression Tests
    # ========================================

    def test_backtrack_level_zero(self):
        """Regression: backtrack to decision level 0."""
        self._test_fixture(
            "fixtures/regression/backtrack_level_zero.cnf",
            self.expected["fixtures/regression/backtrack_level_zero.cnf"]["result"]
        )


def run_tests():
    """Run all tests and return exit code."""
    # Create test suite
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(TestCommonFixtures)

    # Run tests with verbose output
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # Return 0 if all passed, 1 otherwise
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())
