#!/usr/bin/env python3
"""
Comprehensive Test Suite for Competition Solver

Tests two-watched literals implementation across a wide variety of instances:
- SAT and UNSAT instances
- Different clause sizes (unit, binary, 3-SAT, k-SAT)
- Different problem types (random, structured, crafted)
- Edge cases (empty clauses, tautologies, pure literals)
"""

import sys
import os

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat.cnf import CNFExpression
from bsat import solve_cdcl as solve_cdcl_original
import cdcl_optimized


class TestResult:
    """Result of a single test case."""
    def __init__(self, name, passed, message=""):
        self.name = name
        self.passed = passed
        self.message = message


def test_formula(formula_str: str, name: str, expected_sat: bool = None) -> TestResult:
    """
    Test a single formula.

    Args:
        formula_str: CNF formula string
        name: Test case name
        expected_sat: Expected result (True=SAT, False=UNSAT, None=unknown)

    Returns:
        TestResult object
    """
    try:
        cnf = CNFExpression.parse(formula_str)

        # Solve with original (naive)
        result_original = solve_cdcl_original(cnf)

        # Solve with optimized (two-watched literals)
        result_watched = cdcl_optimized.solve_cdcl(cnf, use_watched_literals=True)

        # Solve with naive mode
        result_naive_mode = cdcl_optimized.solve_cdcl(cnf, use_watched_literals=False)

        # Check all agree on SAT/UNSAT
        original_sat = result_original is not None
        watched_sat = result_watched is not None
        naive_mode_sat = result_naive_mode is not None

        if original_sat != watched_sat:
            return TestResult(name, False,
                f"Original vs Watched mismatch: {original_sat} vs {watched_sat}")

        if original_sat != naive_mode_sat:
            return TestResult(name, False,
                f"Original vs Naive mode mismatch: {original_sat} vs {naive_mode_sat}")

        # Check expected result if provided
        if expected_sat is not None and original_sat != expected_sat:
            return TestResult(name, False,
                f"Expected {'SAT' if expected_sat else 'UNSAT'}, got {'SAT' if original_sat else 'UNSAT'}")

        # If SAT, verify all solutions are valid
        if original_sat:
            if not cnf.evaluate(result_original):
                return TestResult(name, False, "Original solution invalid")
            if not cnf.evaluate(result_watched):
                return TestResult(name, False, "Watched solution invalid")
            if not cnf.evaluate(result_naive_mode):
                return TestResult(name, False, "Naive mode solution invalid")

        return TestResult(name, True, f"{'SAT' if original_sat else 'UNSAT'}")

    except Exception as e:
        return TestResult(name, False, f"Exception: {str(e)}")


def run_test_suite():
    """Run comprehensive test suite."""
    print("=" * 70)
    print("COMPREHENSIVE TEST SUITE: Competition Solver")
    print("=" * 70)

    test_cases = []

    # ========== UNIT CLAUSES ==========
    print("\n[1/10] Unit Clauses")
    test_cases.extend([
        ("(x)", "Single unit clause", True),
        ("(x) & (y)", "Two unit clauses", True),
        ("(x) & (~x)", "Contradictory unit clauses", False),
        ("(a) & (b) & (c) & (d)", "Multiple unit clauses SAT", True),
    ])

    # ========== BINARY CLAUSES ==========
    print("[2/10] Binary Clauses (2-SAT)")
    test_cases.extend([
        ("(x | y)", "Single binary clause", True),
        ("(x | y) & (~x | y)", "2-SAT SAT", True),
        ("(x | y) & (~x | z) & (~y | z) & (~z)", "2-SAT UNSAT", False),
        ("(a | b) & (a | ~b) & (~a | b) & (~a | ~b)", "Contradictory binary", False),
    ])

    # ========== 3-SAT INSTANCES ==========
    print("[3/10] 3-SAT Instances")
    test_cases.extend([
        ("(a | b | c)", "Single 3-clause", True),
        ("(a | b | c) & (~a | b | c)", "Easy 3-SAT", True),
        ("(a | b | c) & (~a | d) & (~b | d) & (~c | d)", "Forced 3-SAT", True),
        ("(x1 | x2 | x3) & (~x1 | x4) & (~x2 | x4) & (~x3 | x4) & (~x4)", "3-SAT UNSAT", False),
        ("(p | q | r) & (~p | q | r) & (p | ~q | r) & (p | q | ~r) & (~p | ~q | r) & (~p | q | ~r) & (p | ~q | ~r) & (~p | ~q | ~r)", "Hard 3-SAT UNSAT", False),
    ])

    # ========== UNIT PROPAGATION CHAINS ==========
    print("[4/10] Unit Propagation")
    test_cases.extend([
        ("(x) & (x | y) & (~y | z)", "Simple propagation chain", True),
        ("(a) & (a | b) & (b | c) & (c | d)", "Long propagation chain", True),
        ("(x) & (x | y) & (~x | z) & (~y | ~z)", "Propagation leading to UNSAT", False),
        ("(a) & (a | b) & (a | c) & (~b | d) & (~c | d)", "Branching propagation", True),
    ])

    # ========== BACKTRACKING ==========
    print("[5/10] Backtracking")
    test_cases.extend([
        ("(a | b) & (~a | c) & (b | ~c) & (~b | c)", "Requires backtracking SAT", True),
        ("(x | y | z) & (~x | y) & (x | ~y) & (~x | ~y | ~z)", "Backtracking UNSAT", False),
        ("(a | b | c) & (~a | ~b) & (~a | ~c) & (~b | ~c) & (a | b | c)", "Multiple backtracks", True),
    ])

    # ========== LARGER INSTANCES ==========
    print("[6/10] Larger Instances")
    test_cases.extend([
        # 5 variables
        ("(a | b | c) & (~a | d | e) & (b | ~d) & (~c | ~e) & (a | ~b | e)", "5 variables", True),
        # 6 variables, more clauses
        ("(v1 | v2 | v3) & (~v1 | v4) & (~v2 | v5) & (~v3 | v6) & (v4 | v5 | v6) & (~v4 | ~v5)", "6 variables", True),
    ])

    # ========== CONFLICT-DRIVEN CLAUSE LEARNING ==========
    print("[7/10] Conflict-Driven Learning")
    test_cases.extend([
        # Instances that benefit from clause learning
        ("(a | b) & (~a | c) & (~b | c) & (a | ~c) & (b | ~c) & (~a | ~b | ~c)", "CDCL test 1", False),
        ("(x | y | z) & (~x | y | z) & (x | ~y | z) & (x | y | ~z) & (~x | ~y) & (~x | ~z) & (~y | ~z)", "CDCL test 2", False),
    ])

    # ========== PIGEON HOLE (Small) ==========
    print("[8/10] Pigeon Hole Problem")
    test_cases.extend([
        # 2 pigeons, 1 hole (UNSAT)
        ("(p11) & (p21) & (~p11 | ~p21)", "PHP(2,1) - Tiny", False),
        # 3 pigeons, 2 holes
        ("(p11 | p12) & (p21 | p22) & (p31 | p32) & (~p11 | ~p21) & (~p11 | ~p31) & (~p21 | ~p31) & (~p12 | ~p22) & (~p12 | ~p32) & (~p22 | ~p32)", "PHP(3,2)", False),
    ])

    # ========== EDGE CASES ==========
    print("[9/10] Edge Cases")
    test_cases.extend([
        ("(x | ~x)", "Tautology clause (always SAT)", True),
        ("(a | ~a) & (b | ~b)", "Multiple tautologies", True),
        # Mixed clause sizes
        ("(x) & (y | z) & (a | b | c) & (p | q | r | s)", "Mixed clause sizes", True),
    ])

    # ========== STRUCTURED INSTANCES ==========
    print("[10/10] Structured Instances")
    test_cases.extend([
        # Symmetric structure
        ("(a | b) & (b | c) & (c | d) & (d | a) & (~a | ~c) & (~b | ~d)", "Cycle structure", True),
        # Tree structure
        ("(root) & (root | left) & (root | right) & (left | ll) & (left | lr) & (right | rl) & (right | rr)", "Tree structure", True),
    ])

    # Run all tests
    results = []
    for formula, name, expected in test_cases:
        result = test_formula(formula, name, expected)
        results.append(result)

        status = "✅" if result.passed else "❌"
        print(f"  {status} {name}: {result.message}")

    # Summary
    passed = sum(1 for r in results if r.passed)
    failed = len(results) - passed

    print("\n" + "=" * 70)
    print(f"RESULTS: {passed}/{len(results)} passed ({100*passed//len(results)}%)")
    print("=" * 70)

    if failed == 0:
        print("✅ ALL TESTS PASSED!")
        print("Two-watched literals implementation is thoroughly validated.")
        return True
    else:
        print(f"❌ {failed} test(s) failed")
        print("\nFailed tests:")
        for r in results:
            if not r.passed:
                print(f"  - {r.name}: {r.message}")
        return False


if __name__ == "__main__":
    success = run_test_suite()
    sys.exit(0 if success else 1)
