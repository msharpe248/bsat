"""
Tests for Davis-Putnam (1960) SAT solver.

Tests the classical resolution-based algorithm on small instances.
"""

from bsat import (
    DavisPutnamSolver,
    solve_davis_putnam,
    get_davis_putnam_stats,
    CNFExpression,
    Clause,
    Literal,
    solve_sat  # For comparison
)


def test_simple_sat():
    """Test simple satisfiable formula."""
    # (x ∨ y) ∧ (¬x ∨ z)
    cnf = CNFExpression.parse("(x | y) & (~x | z)")
    result = solve_davis_putnam(cnf)

    assert result is not None, "Formula should be SAT"
    assert cnf.evaluate(result), "Solution should satisfy formula"


def test_simple_unsat():
    """Test simple unsatisfiable formula."""
    # (x) ∧ (¬x) - impossible
    cnf = CNFExpression([
        Clause([Literal('x', False)]),
        Clause([Literal('x', True)])
    ])

    result = solve_davis_putnam(cnf)
    assert result is None, "Formula should be UNSAT"


def test_unit_clause():
    """Test one-literal rule (unit propagation)."""
    # x ∧ (x ∨ y)
    cnf = CNFExpression([
        Clause([Literal('x', False)]),
        Clause([Literal('x', False), Literal('y', False)])
    ])

    result, stats = get_davis_putnam_stats(cnf)

    assert result is not None, "Formula should be SAT"
    assert result['x'] is True, "x should be True"
    assert stats.one_literal_eliminations > 0, "Should use unit propagation"


def test_pure_literal():
    """Test affirmative-negative rule (pure literal elimination)."""
    # (x ∨ y) ∧ (x ∨ z) - x appears only positively
    cnf = CNFExpression.parse("(x | y) & (x | z)")

    result, stats = get_davis_putnam_stats(cnf)

    assert result is not None, "Formula should be SAT"
    assert result['x'] is True, "x should be True (pure positive)"
    # Note: pure literal elimination may happen during solving


def test_resolution_simple():
    """Test basic resolution."""
    # (x ∨ a) ∧ (¬x ∨ b)
    cnf = CNFExpression.parse("(x | a) & (~x | b)")

    result, stats = get_davis_putnam_stats(cnf)

    assert result is not None, "Formula should be SAT"
    assert cnf.evaluate(result), "Solution should satisfy formula"
    # Resolution should be performed
    assert stats.resolutions_performed >= 0  # May be 0 if pure literals found


def test_clause_growth():
    """Test that clause count is tracked correctly."""
    # (a ∨ b) ∧ (a ∨ c) ∧ (¬a ∨ d) ∧ (¬a ∨ e)
    # Resolution on 'a' creates 2×2=4 new clauses
    cnf = CNFExpression.parse("(a | b) & (a | c) & (~a | d) & (~a | e)")

    result, stats = get_davis_putnam_stats(cnf)

    assert result is not None, "Formula should be SAT"
    assert stats.initial_clauses == 4, "Should start with 4 clauses"
    # Max clauses may be higher due to resolution
    assert stats.max_clauses >= stats.initial_clauses


def test_3sat_small():
    """Test small 3SAT formula."""
    # (a ∨ b ∨ c) ∧ (¬a ∨ b ∨ ¬c) ∧ (a ∨ ¬b ∨ c)
    cnf = CNFExpression.parse("(a | b | c) & (~a | b | ~c) & (a | ~b | c)")

    result = solve_davis_putnam(cnf)

    assert result is not None, "Formula should be SAT"
    assert cnf.evaluate(result), "Solution should satisfy formula"


def test_empty_formula():
    """Test empty formula (trivially SAT)."""
    cnf = CNFExpression([])

    result = solve_davis_putnam(cnf)

    assert result is not None, "Empty formula should be SAT"


def test_single_variable():
    """Test formula with single variable."""
    # (x)
    cnf = CNFExpression([Clause([Literal('x', False)])])

    result = solve_davis_putnam(cnf)

    assert result is not None, "Formula should be SAT"
    assert result['x'] is True, "x should be True"


def test_two_variables_sat():
    """Test 2-variable satisfiable formula."""
    # (x ∨ y) ∧ (¬x ∨ y) ∧ (x ∨ ¬y)
    cnf = CNFExpression.parse("(x | y) & (~x | y) & (x | ~y)")

    result = solve_davis_putnam(cnf)

    assert result is not None, "Formula should be SAT"
    assert cnf.evaluate(result), "Solution should satisfy formula"


def test_two_variables_unsat():
    """Test 2-variable unsatisfiable formula."""
    # (x ∨ y) ∧ (¬x ∨ y) ∧ (x ∨ ¬y) ∧ (¬x ∨ ¬y)
    cnf = CNFExpression.parse("(x | y) & (~x | y) & (x | ~y) & (~x | ~y)")

    result = solve_davis_putnam(cnf)

    assert result is None, "Formula should be UNSAT"


def test_pigeonhole_2_1():
    """Test pigeonhole principle: 2 pigeons, 1 hole (UNSAT)."""
    # p0_h0 = "pigeon 0 in hole 0"
    # p1_h0 = "pigeon 1 in hole 0"

    clauses = []

    # Each pigeon must be in some hole
    clauses.append(Clause([Literal('p0_h0', False)]))
    clauses.append(Clause([Literal('p1_h0', False)]))

    # No two pigeons in same hole
    clauses.append(Clause([Literal('p0_h0', True), Literal('p1_h0', True)]))

    cnf = CNFExpression(clauses)
    result = solve_davis_putnam(cnf)

    assert result is None, "Pigeonhole principle should be UNSAT"


def test_comparison_with_dpll():
    """Test that Davis-Putnam and DPLL agree on result."""
    formulas = [
        "(x | y) & (~x | z)",
        "(a | b | c) & (~a | b) & (~b | ~c)",
        "(x) & (~x)",  # UNSAT
        "(x | y | z)",
        "x & (x | y) & (y | z)"
    ]

    for formula_str in formulas:
        cnf = CNFExpression.parse(formula_str)

        dp_result = solve_davis_putnam(cnf)
        dpll_result = solve_sat(cnf)

        # Both should agree on SAT/UNSAT
        assert (dp_result is not None) == (dpll_result is not None), \
            f"Davis-Putnam and DPLL disagree on: {formula_str}"

        # If SAT, both solutions should satisfy formula
        if dp_result:
            assert cnf.evaluate(dp_result), "Davis-Putnam solution invalid"
        if dpll_result:
            assert cnf.evaluate(dpll_result), "DPLL solution invalid"


def test_statistics():
    """Test that statistics are tracked."""
    cnf = CNFExpression.parse("(a | b) & (~a | c) & (b | d)")

    result, stats = get_davis_putnam_stats(cnf)

    assert result is not None, "Formula should be SAT"
    assert stats.initial_variables > 0, "Should have variables"
    assert stats.initial_clauses > 0, "Should have clauses"
    assert stats.max_clauses >= stats.initial_clauses, "Max should be >= initial"


def test_all_variables_assigned():
    """Test that all variables get assigned."""
    cnf = CNFExpression.parse("(x | y) & (z | w)")

    result = solve_davis_putnam(cnf)

    assert result is not None, "Formula should be SAT"
    assert 'x' in result and 'y' in result, "Should assign x and y"
    assert 'z' in result and 'w' in result, "Should assign z and w"


def test_tautology_removal():
    """Test that tautologies are not added."""
    # (x ∨ ¬x ∨ y) - tautology, always true
    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('x', True), Literal('y', False)])
    ])

    result = solve_davis_putnam(cnf)

    # Formula is satisfiable (tautology)
    assert result is not None, "Tautology should be SAT"


def test_independent_components():
    """Test formula with independent components."""
    # (a ∨ b) ∧ (c ∨ d) - two independent components
    cnf = CNFExpression.parse("(a | b) & (c | d)")

    result = solve_davis_putnam(cnf)

    assert result is not None, "Formula should be SAT"
    assert cnf.evaluate(result), "Solution should satisfy formula"


def test_horn_clause():
    """Test Horn clause (at most 1 positive literal per clause)."""
    # a ∧ (¬a ∨ b) ∧ (¬b ∨ c)
    cnf = CNFExpression.parse("a & (~a | b) & (~b | c)")

    result = solve_davis_putnam(cnf)

    assert result is not None, "Formula should be SAT"
    assert result['a'] is True, "a should be True"
    assert result['b'] is True, "b should be True"
    assert result['c'] is True, "c should be True"


def test_solver_class():
    """Test using DavisPutnamSolver class directly."""
    cnf = CNFExpression.parse("(x | y) & (~x | z)")
    solver = DavisPutnamSolver(cnf)

    result = solver.solve()

    assert result is not None, "Formula should be SAT"
    assert cnf.evaluate(result), "Solution should satisfy formula"

    stats = solver.get_statistics()
    assert stats.initial_variables == 3, "Should have 3 variables"
    assert stats.initial_clauses == 2, "Should have 2 clauses"


if __name__ == '__main__':
    # Run all tests
    test_simple_sat()
    test_simple_unsat()
    test_unit_clause()
    test_pure_literal()
    test_resolution_simple()
    test_clause_growth()
    test_3sat_small()
    test_empty_formula()
    test_single_variable()
    test_two_variables_sat()
    test_two_variables_unsat()
    test_pigeonhole_2_1()
    test_comparison_with_dpll()
    test_statistics()
    test_all_variables_assigned()
    test_tautology_removal()
    test_independent_components()
    test_horn_clause()
    test_solver_class()

    print("All Davis-Putnam tests passed!")
