"""Tests for the Horn-SAT solver."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from bsat.cnf import CNFExpression, Clause, Literal
from bsat.hornsat import HornSATSolver, is_horn_formula, solve_horn_sat


def test_is_horn_formula():
    """Test Horn formula detection."""
    # Valid Horn formulas
    horn1 = CNFExpression.parse("(x | ~y | ~z)")  # 1 positive
    horn2 = CNFExpression.parse("(~x | ~y | ~z)")  # 0 positive
    horn3 = CNFExpression.parse("x")  # 1 positive (unit)
    horn4 = CNFExpression.parse("(x | ~y) & (~a | ~b)")

    assert is_horn_formula(horn1), "Should be Horn (1 positive)"
    assert is_horn_formula(horn2), "Should be Horn (0 positive)"
    assert is_horn_formula(horn3), "Should be Horn (unit clause)"
    assert is_horn_formula(horn4), "Should be Horn (mixed)"

    # Not Horn formulas
    not_horn1 = CNFExpression.parse("(x | y | ~z)")  # 2 positive
    not_horn2 = CNFExpression.parse("(x | y)")  # 2 positive

    assert not is_horn_formula(not_horn1), "Should not be Horn (2 positive)"
    assert not is_horn_formula(not_horn2), "Should not be Horn (2 positive)"

    print("✓ Horn formula detection test passed")


def test_horn_sat_satisfiable():
    """Test Horn-SAT on satisfiable formula."""
    # (x) ∧ (¬x ∨ y) ∧ (¬y ∨ z)
    # x=T → y=T → z=T
    cnf = CNFExpression([
        Clause([Literal('x')]),
        Clause([Literal('x', True), Literal('y')]),
        Clause([Literal('y', True), Literal('z')])
    ])

    result = solve_horn_sat(cnf)
    assert result is not None, "Formula should be satisfiable"
    assert result['x'] == True
    assert result['y'] == True
    assert result['z'] == True
    assert cnf.evaluate(result), "Assignment should satisfy formula"

    print(f"✓ Horn-SAT satisfiable test passed: {result}")


def test_horn_sat_unsatisfiable():
    """Test Horn-SAT on unsatisfiable formula."""
    # (x) ∧ (¬x)
    cnf = CNFExpression([
        Clause([Literal('x')]),
        Clause([Literal('x', True)])
    ])

    result = solve_horn_sat(cnf)
    assert result is None, "Formula should be unsatisfiable"

    print("✓ Horn-SAT unsatisfiable test passed")


def test_horn_sat_all_negative():
    """Test Horn-SAT with all negative clauses."""
    # (¬x ∨ ¬y) ∧ (¬y ∨ ¬z) ∧ (¬x ∨ ¬z)
    # All variables can be False
    cnf = CNFExpression([
        Clause([Literal('x', True), Literal('y', True)]),
        Clause([Literal('y', True), Literal('z', True)]),
        Clause([Literal('x', True), Literal('z', True)])
    ])

    result = solve_horn_sat(cnf)
    assert result is not None, "Formula should be satisfiable"
    # All variables should be False (default assignment works)
    assert result['x'] == False
    assert result['y'] == False
    assert result['z'] == False
    assert cnf.evaluate(result), "Assignment should satisfy formula"

    print(f"✓ All negative clauses test passed: {result}")


def test_horn_sat_chain():
    """Test Horn-SAT with implication chain."""
    # (a) ∧ (¬a ∨ b) ∧ (¬b ∨ c) ∧ (¬c ∨ d)
    # a=T → b=T → c=T → d=T
    cnf = CNFExpression([
        Clause([Literal('a')]),
        Clause([Literal('a', True), Literal('b')]),
        Clause([Literal('b', True), Literal('c')]),
        Clause([Literal('c', True), Literal('d')])
    ])

    solver = HornSATSolver(cnf)
    result = solver.solve()

    assert result is not None, "Formula should be satisfiable"
    assert result['a'] == True
    assert result['b'] == True
    assert result['c'] == True
    assert result['d'] == True
    assert cnf.evaluate(result), "Assignment should satisfy formula"

    stats = solver.get_statistics()
    assert stats['num_unit_propagations'] == 4, "Should propagate 4 times"

    print(f"✓ Implication chain test passed: {result}")
    print(f"  Unit propagations: {stats['num_unit_propagations']}")


def test_horn_sat_logic_program():
    """Test Horn-SAT representing a logic program."""
    # Prolog-like rules:
    # likes_pizza(john).
    # likes_italian(X) :- likes_pizza(X).
    # happy(X) :- likes_italian(X).
    #
    # As Horn clauses:
    # (likes_pizza_john)
    # (¬likes_pizza_john ∨ likes_italian_john)
    # (¬likes_italian_john ∨ happy_john)

    cnf = CNFExpression([
        Clause([Literal('likes_pizza_john')]),
        Clause([Literal('likes_pizza_john', True), Literal('likes_italian_john')]),
        Clause([Literal('likes_italian_john', True), Literal('happy_john')])
    ])

    result = solve_horn_sat(cnf)

    assert result is not None, "Formula should be satisfiable"
    assert result['likes_pizza_john'] == True
    assert result['likes_italian_john'] == True
    assert result['happy_john'] == True
    assert cnf.evaluate(result), "Assignment should satisfy formula"

    print(f"✓ Logic program test passed: {result}")


def test_horn_sat_mixed():
    """Test Horn-SAT with mix of positive and negative clauses."""
    # (a) ∧ (¬a ∨ ¬b) ∧ (¬a ∨ ¬c) ∧ (b ∨ ¬d)
    # a=T → b=F, c=F, and (b ∨ ¬d) = (F ∨ ¬d) = (¬d) so d=F
    cnf = CNFExpression([
        Clause([Literal('a')]),
        Clause([Literal('a', True), Literal('b', True)]),
        Clause([Literal('a', True), Literal('c', True)]),
        Clause([Literal('b'), Literal('d', True)])
    ])

    result = solve_horn_sat(cnf)

    assert result is not None, "Formula should be satisfiable"
    assert result['a'] == True, "a should be True (unit clause)"
    assert cnf.evaluate(result), "Assignment should satisfy formula"

    print(f"✓ Mixed clauses test passed: {result}")


def test_non_horn_rejection():
    """Test that non-Horn formulas are rejected."""
    # (x | y) - has 2 positive literals
    cnf = CNFExpression([
        Clause([Literal('x'), Literal('y')])
    ])

    try:
        solver = HornSATSolver(cnf)
        assert False, "Should have raised ValueError for non-Horn formula"
    except ValueError as e:
        assert "not Horn-SAT" in str(e)
        print("✓ Non-Horn rejection test passed")


def test_horn_sat_statistics():
    """Test that solver statistics are tracked."""
    cnf = CNFExpression([
        Clause([Literal('x')]),
        Clause([Literal('x', True), Literal('y')]),
        Clause([Literal('y', True), Literal('z')])
    ])

    solver = HornSATSolver(cnf)
    result = solver.solve()

    stats = solver.get_statistics()
    assert stats['num_variables'] == 3
    assert stats['num_clauses'] == 3
    assert stats['num_unit_propagations'] > 0

    print(f"✓ Statistics test passed: {stats}")


def test_horn_sat_empty_formula():
    """Test Horn-SAT on empty formula."""
    cnf = CNFExpression([])

    result = solve_horn_sat(cnf)
    assert result is not None, "Empty formula should be satisfiable"
    assert cnf.evaluate(result), "Assignment should satisfy formula"

    print("✓ Empty formula test passed")


def test_horn_sat_single_variable():
    """Test Horn-SAT with single variable formulas."""
    # Just (x)
    cnf1 = CNFExpression([Clause([Literal('x')])])
    result1 = solve_horn_sat(cnf1)
    assert result1 is not None
    assert result1['x'] == True

    # Just (¬x)
    cnf2 = CNFExpression([Clause([Literal('x', True)])])
    result2 = solve_horn_sat(cnf2)
    assert result2 is not None
    assert result2['x'] == False

    print("✓ Single variable test passed")


if __name__ == '__main__':
    print("Running Horn-SAT Solver Tests\n" + "="*50)
    test_is_horn_formula()
    test_horn_sat_satisfiable()
    test_horn_sat_unsatisfiable()
    test_horn_sat_all_negative()
    test_horn_sat_chain()
    test_horn_sat_logic_program()
    test_horn_sat_mixed()
    test_non_horn_rejection()
    test_horn_sat_statistics()
    test_horn_sat_empty_formula()
    test_horn_sat_single_variable()
    print("="*50)
    print("All tests passed! ✓")
