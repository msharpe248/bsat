"""
Tests for CDCL SAT solver.
"""

import unittest
from bsat import (
    CNFExpression, Clause, Literal,
    CDCLSolver, solve_cdcl, get_cdcl_stats
)


class TestCDCL(unittest.TestCase):
    """Test cases for CDCL solver."""

    def test_simple_sat(self):
        """Test simple satisfiable formula."""
        # (x)
        cnf = CNFExpression([
            Clause([Literal('x', False)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'], True)
        self.assertTrue(cnf.evaluate(result))

    def test_simple_unsat(self):
        """Test simple unsatisfiable formula."""
        # (x) ∧ (¬x)
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('x', True)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNone(result)

    def test_2sat_formula(self):
        """Test 2-SAT formula."""
        # (x ∨ y) ∧ (¬x ∨ z) ∧ (¬y ∨ ¬z)
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)]),
            Clause([Literal('y', True), Literal('z', True)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_3sat_formula(self):
        """Test 3-SAT formula."""
        # (x ∨ y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ z)
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
            Clause([Literal('x', True), Literal('y', False), Literal('z', True)]),
            Clause([Literal('x', False), Literal('y', True), Literal('z', False)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_unit_propagation(self):
        """Test that unit propagation works."""
        # (x) ∧ (¬x ∨ y) ∧ (¬y ∨ z)
        # Should propagate: x=T → y=T → z=T
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('x', True), Literal('y', False)]),
            Clause([Literal('y', True), Literal('z', False)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'], True)
        self.assertEqual(result['y'], True)
        self.assertEqual(result['z'], True)

    def test_unsatisfiable_simple(self):
        """Test simple unsatisfiable formula."""
        # x ∧ (¬x ∨ y) ∧ ¬y
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('x', True), Literal('y', False)]),
            Clause([Literal('y', True)])
        ])
        result = solve_cdcl(cnf, max_conflicts=100)
        self.assertIsNone(result)

    def test_empty_formula(self):
        """Test empty formula (trivially SAT)."""
        cnf = CNFExpression([])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result, {})

    def test_empty_clause(self):
        """Test formula with empty clause (immediately UNSAT)."""
        cnf = CNFExpression([
            Clause([])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNone(result)

    def test_large_satisfiable(self):
        """Test larger satisfiable formula."""
        # Chain of implications
        clauses = []
        for i in range(10):
            clauses.append(Clause([Literal(f'x{i}', True), Literal(f'x{i+1}', False)]))
        clauses.append(Clause([Literal('x0', False)]))

        cnf = CNFExpression(clauses)
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_pigeonhole_principle(self):
        """Test pigeonhole principle (should be UNSAT)."""
        # 2 pigeons, 1 hole - unsatisfiable (simpler version)
        # p_i means pigeon i in the hole
        cnf = CNFExpression([
            # Each pigeon must be in the hole
            Clause([Literal('p_0', False)]),
            Clause([Literal('p_1', False)]),
            # No two pigeons in same hole
            Clause([Literal('p_0', True), Literal('p_1', True)])
        ])
        result = solve_cdcl(cnf, max_conflicts=100)
        self.assertIsNone(result)

    def test_statistics(self):
        """Test that statistics are collected."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)]),
            Clause([Literal('y', True), Literal('z', True)])
        ])
        result, stats = get_cdcl_stats(cnf)
        self.assertIsNotNone(result)
        self.assertGreater(stats.decisions, 0)
        self.assertGreaterEqual(stats.propagations, 0)

    def test_clause_learning(self):
        """Test that clause learning happens on conflicts."""
        # This formula should trigger conflicts and learning
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False)]),
            Clause([Literal('a', True), Literal('c', False)]),
            Clause([Literal('b', True), Literal('c', True)]),
            Clause([Literal('c', False), Literal('d', False)]),
            Clause([Literal('d', True)])
        ])
        result, stats = get_cdcl_stats(cnf)
        self.assertIsNotNone(result)
        # Should have learned at least one clause
        self.assertGreaterEqual(stats.learned_clauses, 0)

    def test_vsids_heuristic(self):
        """Test VSIDS heuristic with different decay factors."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)]),
            Clause([Literal('y', True), Literal('z', True)])
        ])

        # Try different VSIDS decay factors
        for decay in [0.95]:
            result = solve_cdcl(cnf, vsids_decay=decay, max_conflicts=100)
            self.assertIsNotNone(result)
            self.assertTrue(cnf.evaluate(result))

    def test_mixed_clause_sizes(self):
        """Test formula with mixed clause sizes."""
        cnf = CNFExpression([
            Clause([Literal('a', False)]),  # Unit clause
            Clause([Literal('b', False), Literal('c', False)]),  # Binary
            Clause([Literal('d', False), Literal('e', False), Literal('f', False)]),  # Ternary
            Clause([Literal('a', True), Literal('b', True), Literal('d', True)]),
            Clause([Literal('c', True), Literal('e', True)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_all_positive_literals(self):
        """Test formula with all positive literals."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('y', False), Literal('z', False)]),
            Clause([Literal('z', False), Literal('w', False)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_all_negative_literals(self):
        """Test formula with all negative literals."""
        cnf = CNFExpression([
            Clause([Literal('x', True), Literal('y', True)]),
            Clause([Literal('y', True), Literal('z', True)]),
            Clause([Literal('z', True), Literal('w', True)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_single_variable_multiple_clauses(self):
        """Test formula with single variable in multiple clauses."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', False), Literal('z', False)]),
            Clause([Literal('x', True), Literal('y', True), Literal('z', True)])
        ])
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_solver_class(self):
        """Test using CDCLSolver class directly."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)])
        ])
        solver = CDCLSolver(cnf)
        result = solver.solve()
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

        stats = solver.get_stats()
        self.assertIsNotNone(stats)

    def test_backtracking(self):
        """Test that backtracking works correctly."""
        # Formula that requires backtracking and is SAT
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False)]),
            Clause([Literal('a', True), Literal('c', False)]),
            Clause([Literal('b', True), Literal('c', False)])
        ])
        result, stats = get_cdcl_stats(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))
        # Should have done some decisions
        self.assertGreater(stats.decisions, 0)

    def test_parse_and_solve(self):
        """Test parsing and solving."""
        formula = "(x | y) & (~x | z) & (~y | ~z)"
        cnf = CNFExpression.parse(formula)
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))


def run_tests():
    """Run all CDCL tests."""
    suite = unittest.TestLoader().loadTestsFromTestCase(TestCDCL)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return result.wasSuccessful()


if __name__ == '__main__':
    import sys
    success = run_tests()
    sys.exit(0 if success else 1)
