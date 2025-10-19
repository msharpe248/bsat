#!/usr/bin/env python3
"""
Test Correctness: Optimized CDCL vs Original CDCL

Validates that the two-watched literal optimization produces
identical results to the original naive implementation.

CRITICAL: Before benchmarking speedup, we must verify correctness!
"""

import sys
import os

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat.cnf import CNFExpression
from bsat import solve_cdcl as solve_cdcl_original
import cdcl_optimized


def test_formula(formula_str: str, name: str):
    """Test a single formula with both solvers."""
    print(f"\nTesting: {name}")
    print(f"Formula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with original (naive)
    result_original = solve_cdcl_original(cnf)

    # Solve with optimized (two-watched literals enabled)
    result_watched = cdcl_optimized.solve_cdcl(cnf, use_watched_literals=True)

    # Solve with optimized but two-watched disabled (for comparison)
    result_naive_mode = cdcl_optimized.solve_cdcl(cnf, use_watched_literals=False)

    # Verify all three give same SAT/UNSAT result
    original_sat = result_original is not None
    watched_sat = result_watched is not None
    naive_mode_sat = result_naive_mode is not None

    if original_sat != watched_sat or original_sat != naive_mode_sat:
        print(f"  ❌ MISMATCH!")
        print(f"     Original: {'SAT' if original_sat else 'UNSAT'}")
        print(f"     Watched:  {'SAT' if watched_sat else 'UNSAT'}")
        print(f"     Naive mode: {'SAT' if naive_mode_sat else 'UNSAT'}")
        return False

    # If SAT, verify both solutions actually satisfy the formula
    if original_sat:
        if not cnf.evaluate(result_original):
            print(f"  ❌ Original solution invalid!")
            return False
        if not cnf.evaluate(result_watched):
            print(f"  ❌ Watched solution invalid!")
            return False
        if not cnf.evaluate(result_naive_mode):
            print(f"  ❌ Naive mode solution invalid!")
            return False

    print(f"  ✅ {'SAT' if original_sat else 'UNSAT'} - All solvers agree!")
    return True


def run_tests():
    """Run comprehensive correctness tests."""
    print("=" * 70)
    print("CORRECTNESS TEST: Optimized CDCL with Two-Watched Literals")
    print("=" * 70)

    test_cases = [
        # Basic SAT instances
        ("(x)", "Single variable"),
        ("(x | y)", "Single clause, 2 literals"),
        ("(x | y) & (~x | y)", "Simple SAT"),
        ("(a | b) & (~a | c) & (b | ~c)", "3 variables, 3 clauses SAT"),

        # UNSAT instances
        ("(x) & (~x)", "Trivial UNSAT"),
        ("(x | y) & (~x) & (~y)", "Small UNSAT"),
        ("(a | b) & (~a | b) & (a | ~b) & (~a | ~b)", "Classic UNSAT"),

        # Unit propagation
        ("(x) & (x | y) & (~y | z)", "Unit propagation chain"),
        ("(a) & (a | b) & (a | ~b | c)", "Forced assignments"),

        # Harder instances
        ("(a | b | c) & (~a | b | c) & (a | ~b | c) & (a | b | ~c) & (~a | ~b | c) & (~a | b | ~c) & (a | ~b | ~c)", "7-clause"),
        ("(p | q | r) & (~p | q | r) & (p | ~q | r) & (p | q | ~r) & (~p | ~q | r) & (~p | q | ~r) & (p | ~q | ~r) & (~p | ~q | ~r)", "8-clause UNSAT"),

        # From BSAT tests
        ("(x1 | x2 | x3) & (~x1 | x4) & (~x2 | x4) & (~x3 | x4) & (~x4)", "Forced UNSAT"),
        ("(a | b | c) & (~a | d) & (~b | d) & (~c | d)", "Forced SAT"),
    ]

    passed = 0
    failed = 0

    for formula, name in test_cases:
        if test_formula(formula, name):
            passed += 1
        else:
            failed += 1

    print("\n" + "=" * 70)
    print(f"RESULTS: {passed} passed, {failed} failed")
    print("=" * 70)

    if failed == 0:
        print("✅ All tests passed! Two-watched literals implementation is CORRECT.")
        return True
    else:
        print(f"❌ {failed} test(s) failed! Implementation has bugs.")
        return False


if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)
