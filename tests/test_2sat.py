#!/usr/bin/env python3
"""Test suite for 2SAT solver."""

from bsat import CNFExpression, TwoSATSolver, solve_2sat, is_2sat_satisfiable


def test_simple_satisfiable():
    """Test a simple satisfiable 2SAT instance."""
    print("Test 1: Simple satisfiable formula")
    print("-" * 60)

    # (x ∨ y) ∧ (¬x ∨ z) ∧ (¬y ∨ z)
    expr = CNFExpression.parse("(x | y) & (~x | z) & (~y | z)")
    print(f"Formula: {expr}")

    solution = solve_2sat(expr)
    print(f"Solution: {solution}")

    if solution:
        result = expr.evaluate(solution)
        print(f"Verification: {result}")
        assert result, "Solution should satisfy the formula"
        print("✓ PASSED\n")
    else:
        print("✗ FAILED: Expected satisfiable\n")


def test_simple_unsatisfiable():
    """Test a simple unsatisfiable 2SAT instance."""
    print("Test 2: Simple unsatisfiable formula")
    print("-" * 60)

    # (x ∨ y) ∧ (¬x ∨ y) ∧ (x ∨ ¬y) ∧ (¬x ∨ ¬y)
    expr = CNFExpression.parse("(x | y) & (~x | y) & (x | ~y) & (~x | ~y)")
    print(f"Formula: {expr}")

    solution = solve_2sat(expr)
    print(f"Solution: {solution}")

    if solution is None:
        print("✓ PASSED: Correctly identified as unsatisfiable\n")
    else:
        print("✗ FAILED: Should be unsatisfiable\n")


def test_implication_chain():
    """Test a formula with implication chains."""
    print("Test 3: Implication chain")
    print("-" * 60)

    # (x ∨ y) ∧ (¬y ∨ z) ∧ (¬z ∨ w) - implies x → y → z → w
    expr = CNFExpression.parse("(x | y) & (~y | z) & (~z | w)")
    print(f"Formula: {expr}")

    solution = solve_2sat(expr)
    print(f"Solution: {solution}")

    if solution:
        result = expr.evaluate(solution)
        print(f"Verification: {result}")
        assert result, "Solution should satisfy the formula"

        # If x is true, then w must be true due to implications
        if solution.get('x', False):
            print(f"x is True, so w should be True: w = {solution.get('w', False)}")

        print("✓ PASSED\n")
    else:
        print("✗ FAILED: Expected satisfiable\n")


def test_equivalence():
    """Test a formula expressing equivalence."""
    print("Test 4: Equivalence (x ↔ y)")
    print("-" * 60)

    # x ↔ y means (x → y) ∧ (y → x) which is (¬x ∨ y) ∧ (¬y ∨ x)
    expr = CNFExpression.parse("(~x | y) & (~y | x)")
    print(f"Formula: {expr} (encodes x ↔ y)")

    solution = solve_2sat(expr)
    print(f"Solution: {solution}")

    if solution:
        result = expr.evaluate(solution)
        print(f"Verification: {result}")
        assert result, "Solution should satisfy the formula"

        # x and y should have the same value
        x_val = solution.get('x', False)
        y_val = solution.get('y', False)
        print(f"x = {x_val}, y = {y_val}")
        assert x_val == y_val, "x and y should be equivalent"
        print("✓ PASSED\n")
    else:
        print("✗ FAILED: Expected satisfiable\n")


def test_contradiction():
    """Test a formula with direct contradiction."""
    print("Test 5: Direct contradiction")
    print("-" * 60)

    # (x ∨ x) ∧ (¬x ∨ ¬x) simplifies to x ∧ ¬x
    expr = CNFExpression.parse("(x | x) & (~x | ~x)")
    print(f"Formula: {expr}")

    is_sat = is_2sat_satisfiable(expr)
    solution = solve_2sat(expr)

    print(f"Is satisfiable: {is_sat}")
    print(f"Solution: {solution}")

    if solution is None and not is_sat:
        print("✓ PASSED: Correctly identified as unsatisfiable\n")
    else:
        print("✗ FAILED: Should be unsatisfiable\n")


def test_complex_satisfiable():
    """Test a more complex satisfiable instance."""
    print("Test 6: Complex satisfiable formula")
    print("-" * 60)

    # Multiple variables with various constraints
    expr = CNFExpression.parse(
        "(a | b) & (~a | c) & (~b | c) & (c | d) & (~c | ~d) & (d | e) & (~e | a)"
    )
    print(f"Formula: {expr}")

    solution = solve_2sat(expr)
    print(f"Solution: {solution}")

    if solution:
        result = expr.evaluate(solution)
        print(f"Verification: {result}")
        assert result, "Solution should satisfy the formula"
        print("✓ PASSED\n")
    else:
        print("✗ FAILED: Expected satisfiable\n")


def test_all_true_solution():
    """Test where all variables can be true."""
    print("Test 7: All variables true")
    print("-" * 60)

    # (x ∨ y) ∧ (y ∨ z) - satisfied by all true
    expr = CNFExpression.parse("(x | y) & (y | z)")
    print(f"Formula: {expr}")

    solution = solve_2sat(expr)
    print(f"Solution: {solution}")

    if solution:
        result = expr.evaluate(solution)
        print(f"Verification: {result}")
        assert result, "Solution should satisfy the formula"

        # Try all true assignment
        all_true = {var: True for var in expr.get_variables()}
        all_true_works = expr.evaluate(all_true)
        print(f"All true assignment works: {all_true_works}")
        print("✓ PASSED\n")
    else:
        print("✗ FAILED: Expected satisfiable\n")


def test_solver_class():
    """Test using the TwoSATSolver class directly."""
    print("Test 8: Using TwoSATSolver class")
    print("-" * 60)

    expr = CNFExpression.parse("(p | q) & (~p | r) & (~q | r)")
    print(f"Formula: {expr}")

    solver = TwoSATSolver(expr)

    print(f"Is satisfiable: {solver.is_satisfiable()}")

    solution = solver.solve()
    print(f"Solution: {solution}")

    if solution:
        result = expr.evaluate(solution)
        print(f"Verification: {result}")
        assert result, "Solution should satisfy the formula"
        print("✓ PASSED\n")
    else:
        print("✗ FAILED: Expected satisfiable\n")


def test_invalid_input():
    """Test that non-2SAT formulas are rejected."""
    print("Test 9: Invalid input (not 2SAT)")
    print("-" * 60)

    # This is 3SAT, not 2SAT
    expr = CNFExpression.parse("(x | y | z)")
    print(f"Formula: {expr} (3SAT, not 2SAT)")

    try:
        solver = TwoSATSolver(expr)
        print("✗ FAILED: Should have raised ValueError\n")
    except ValueError as e:
        print(f"✓ PASSED: Correctly raised ValueError: {e}\n")


def run_all_tests():
    """Run all tests."""
    print("=" * 60)
    print("2SAT Solver Test Suite")
    print("=" * 60)
    print()

    test_simple_satisfiable()
    test_simple_unsatisfiable()
    test_implication_chain()
    test_equivalence()
    test_contradiction()
    test_complex_satisfiable()
    test_all_true_solution()
    test_solver_class()
    test_invalid_input()

    print("=" * 60)
    print("All tests completed!")
    print("=" * 60)


if __name__ == "__main__":
    run_all_tests()
