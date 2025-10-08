"""Tests for the DPLL SAT solver."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from bsat.cnf import CNFExpression, Clause, Literal
from bsat.dpll import DPLLSolver, solve_sat


def test_dpll_satisfiable_simple():
    """Test DPLL on a simple satisfiable 3SAT formula."""
    # (x ∨ y ∨ z) ∧ (¬x ∨ y ∨ z)
    cnf = CNFExpression([
        Clause([Literal('x'), Literal('y'), Literal('z')]),
        Clause([Literal('x', True), Literal('y'), Literal('z')])
    ])

    result = solve_sat(cnf)
    assert result is not None, "Formula should be satisfiable"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"
    print(f"✓ Simple satisfiable test passed: {result}")


def test_dpll_unsatisfiable():
    """Test DPLL on an unsatisfiable formula."""
    # (x) ∧ (¬x)
    cnf = CNFExpression([
        Clause([Literal('x')]),
        Clause([Literal('x', True)])
    ])

    result = solve_sat(cnf)
    assert result is None, "Formula should be unsatisfiable"
    print("✓ Unsatisfiable test passed")


def test_dpll_3sat_complex():
    """Test DPLL on a more complex 3SAT formula."""
    # (x ∨ y ∨ z) ∧ (¬x ∨ ¬y ∨ z) ∧ (x ∨ ¬y ∨ ¬z) ∧ (¬x ∨ y ∨ ¬z)
    cnf = CNFExpression([
        Clause([Literal('x'), Literal('y'), Literal('z')]),
        Clause([Literal('x', True), Literal('y', True), Literal('z')]),
        Clause([Literal('x'), Literal('y', True), Literal('z', True)]),
        Clause([Literal('x', True), Literal('y'), Literal('z', True)])
    ])

    result = solve_sat(cnf)
    assert result is not None, "Formula should be satisfiable"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"
    print(f"✓ Complex 3SAT test passed: {result}")


def test_dpll_statistics():
    """Test that solver statistics are tracked."""
    cnf = CNFExpression([
        Clause([Literal('x'), Literal('y')]),
        Clause([Literal('x', True), Literal('z')])
    ])

    solver = DPLLSolver(cnf)
    result = solver.solve()

    stats = solver.get_statistics()
    assert stats['num_variables'] == 3
    assert stats['num_clauses'] == 2
    assert stats['num_decisions'] > 0
    print(f"✓ Statistics test passed: {stats}")


def test_dpll_all_clauses_satisfied():
    """Test DPLL recognizes when all clauses are satisfied."""
    # (x ∨ y)
    cnf = CNFExpression([
        Clause([Literal('x'), Literal('y')])
    ])

    result = solve_sat(cnf)
    assert result is not None, "Formula should be satisfiable"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"
    print(f"✓ All clauses satisfied test passed: {result}")


def test_dpll_larger_3sat():
    """Test DPLL on a larger 3SAT instance."""
    # Create a random-looking but satisfiable 3SAT with 5 variables and 8 clauses
    cnf = CNFExpression([
        Clause([Literal('a'), Literal('b'), Literal('c')]),
        Clause([Literal('a', True), Literal('b'), Literal('d')]),
        Clause([Literal('b', True), Literal('c', True), Literal('e')]),
        Clause([Literal('a'), Literal('c', True), Literal('e', True)]),
        Clause([Literal('b'), Literal('d', True), Literal('e')]),
        Clause([Literal('a', True), Literal('c'), Literal('d', True)]),
        Clause([Literal('c'), Literal('d'), Literal('e', True)]),
        Clause([Literal('a', True), Literal('b', True), Literal('e')])
    ])

    solver = DPLLSolver(cnf)
    result = solver.solve()

    assert result is not None, "Formula should be satisfiable"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"

    stats = solver.get_statistics()
    print(f"✓ Larger 3SAT test passed: {result}")
    print(f"  Statistics: {stats}")


if __name__ == '__main__':
    print("Running DPLL Solver Tests\n" + "="*50)
    test_dpll_satisfiable_simple()
    test_dpll_unsatisfiable()
    test_dpll_3sat_complex()
    test_dpll_statistics()
    test_dpll_all_clauses_satisfied()
    test_dpll_larger_3sat()
    print("="*50)
    print("All tests passed! ✓")
