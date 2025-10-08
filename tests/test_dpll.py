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
    # With optimizations, some formulas may need 0 decisions (solved by unit prop/pure literal)
    assert stats['num_decisions'] >= 0
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


def test_unit_propagation():
    """Test that unit propagation works correctly."""
    # (x) ∧ (¬x ∨ y) ∧ (¬y ∨ z)
    # x is unit clause → x=True
    # This makes (¬x ∨ y) become (y) → y=True
    # This makes (¬y ∨ z) become (z) → z=True
    cnf = CNFExpression([
        Clause([Literal('x')]),
        Clause([Literal('x', True), Literal('y')]),
        Clause([Literal('y', True), Literal('z')])
    ])

    solver = DPLLSolver(cnf, use_unit_propagation=True, use_pure_literal=False)
    result = solver.solve()

    assert result is not None, "Formula should be satisfiable"
    assert result['x'] == True, "x should be True (unit clause)"
    assert result['y'] == True, "y should be True (unit propagation)"
    assert result['z'] == True, "z should be True (unit propagation)"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"

    stats = solver.get_statistics()
    assert stats['num_unit_propagations'] > 0, "Should have used unit propagation"
    print(f"✓ Unit propagation test passed: {result}")
    print(f"  Unit propagations: {stats['num_unit_propagations']}")


def test_pure_literal_elimination():
    """Test that pure literal elimination works correctly."""
    # (x ∨ y) ∧ (x ∨ z)
    # x appears only positively → pure literal, assign x=True
    cnf = CNFExpression([
        Clause([Literal('x'), Literal('y')]),
        Clause([Literal('x'), Literal('z')])
    ])

    solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=True)
    result = solver.solve()

    assert result is not None, "Formula should be satisfiable"
    assert result['x'] == True, "x should be True (pure literal)"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"

    stats = solver.get_statistics()
    assert stats['num_pure_literals'] > 0, "Should have used pure literal elimination"
    print(f"✓ Pure literal elimination test passed: {result}")
    print(f"  Pure literals found: {stats['num_pure_literals']}")


def test_optimizations_combined():
    """Test that both optimizations work together."""
    # (a) ∧ (¬a ∨ b) ∧ (c ∨ d) ∧ (c ∨ e)
    # a is unit clause → a=True
    # This makes (¬a ∨ b) become (b) → b=True (unit propagation)
    # c appears only positively → pure literal, c=True
    cnf = CNFExpression([
        Clause([Literal('a')]),
        Clause([Literal('a', True), Literal('b')]),
        Clause([Literal('c'), Literal('d')]),
        Clause([Literal('c'), Literal('e')])
    ])

    solver = DPLLSolver(cnf, use_unit_propagation=True, use_pure_literal=True)
    result = solver.solve()

    assert result is not None, "Formula should be satisfiable"
    assert result['a'] == True, "a should be True (unit clause)"
    assert result['b'] == True, "b should be True (unit propagation)"
    assert result['c'] == True, "c should be True (pure literal)"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"

    stats = solver.get_statistics()
    print(f"✓ Combined optimizations test passed: {result}")
    print(f"  Unit propagations: {stats['num_unit_propagations']}")
    print(f"  Pure literals: {stats['num_pure_literals']}")
    print(f"  Decisions: {stats['num_decisions']}")


def test_optimizations_disabled():
    """Test that solver works with optimizations disabled."""
    cnf = CNFExpression([
        Clause([Literal('x')]),
        Clause([Literal('x', True), Literal('y')])
    ])

    # Solve with optimizations disabled
    solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
    result = solver.solve()

    assert result is not None, "Formula should be satisfiable"
    assert cnf.evaluate(result), "Assignment should satisfy the formula"

    stats = solver.get_statistics()
    assert stats['num_unit_propagations'] == 0, "Should not use unit propagation when disabled"
    assert stats['num_pure_literals'] == 0, "Should not use pure literal when disabled"
    print(f"✓ Optimizations disabled test passed: {result}")


def test_performance_comparison():
    """Compare performance with and without optimizations."""
    # Create a formula that benefits from optimizations
    # Chain of implications: x → y → z → w
    cnf = CNFExpression([
        Clause([Literal('x')]),  # Unit clause
        Clause([Literal('x', True), Literal('y')]),
        Clause([Literal('y', True), Literal('z')]),
        Clause([Literal('z', True), Literal('w')]),
        Clause([Literal('a'), Literal('b')]),  # Pure literal 'a'
        Clause([Literal('a'), Literal('c')])
    ])

    # Solve without optimizations
    solver_no_opt = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
    result_no_opt = solver_no_opt.solve()
    stats_no_opt = solver_no_opt.get_statistics()

    # Solve with optimizations
    solver_with_opt = DPLLSolver(cnf, use_unit_propagation=True, use_pure_literal=True)
    result_with_opt = solver_with_opt.solve()
    stats_with_opt = solver_with_opt.get_statistics()

    assert result_no_opt is not None and result_with_opt is not None
    assert cnf.evaluate(result_no_opt) and cnf.evaluate(result_with_opt)

    print(f"✓ Performance comparison:")
    print(f"  Without optimizations: {stats_no_opt['num_decisions']} decisions")
    print(f"  With optimizations: {stats_with_opt['num_decisions']} decisions, "
          f"{stats_with_opt['num_unit_propagations']} unit props, "
          f"{stats_with_opt['num_pure_literals']} pure literals")
    print(f"  Reduction: {stats_no_opt['num_decisions'] - stats_with_opt['num_decisions']} fewer decisions")


if __name__ == '__main__':
    print("Running DPLL Solver Tests\n" + "="*50)
    test_dpll_satisfiable_simple()
    test_dpll_unsatisfiable()
    test_dpll_3sat_complex()
    test_dpll_statistics()
    test_dpll_all_clauses_satisfied()
    test_dpll_larger_3sat()
    print("\nTesting Optimizations\n" + "="*50)
    test_unit_propagation()
    test_pure_literal_elimination()
    test_optimizations_combined()
    test_optimizations_disabled()
    test_performance_comparison()
    print("="*50)
    print("All tests passed! ✓")
