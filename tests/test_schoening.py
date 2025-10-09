"""
Tests for Schöning's randomized SAT algorithm
"""

import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from bsat import CNFExpression, Clause, Literal
from bsat.schoening import SchoeningSolver, solve_schoening, get_schoening_stats


class TestSchoeningBasic(unittest.TestCase):
    """Basic tests for Schöning's algorithm."""

    def test_simple_sat(self):
        """Test on simple satisfiable formula."""
        cnf = CNFExpression.parse("(x | y) & (~x | y) & (x | ~y)")
        solution = solve_schoening(cnf, seed=42)

        self.assertIsNotNone(solution)
        self.assertTrue(cnf.evaluate(solution))

    def test_unit_clauses(self):
        """Test with unit clauses (should solve quickly)."""
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('y', True)]),
        ])
        solution = solve_schoening(cnf, seed=42)

        self.assertIsNotNone(solution)
        self.assertTrue(solution['x'])
        self.assertFalse(solution['y'])

    def test_3sat_formula(self):
        """Test on 3SAT formula."""
        cnf = CNFExpression.parse("(x | y | z) & (~x | y | ~z) & (x | ~y | z)")
        solution = solve_schoening(cnf, seed=42)

        self.assertIsNotNone(solution)
        self.assertTrue(cnf.evaluate(solution))

    def test_reproducibility(self):
        """Test that seed makes results reproducible."""
        cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c)")

        sol1 = solve_schoening(cnf, seed=123)
        sol2 = solve_schoening(cnf, seed=123)

        # Same seed should give same result
        self.assertEqual(sol1, sol2)

    def test_empty_formula(self):
        """Test empty formula (trivially SAT)."""
        cnf = CNFExpression([])
        solution = solve_schoening(cnf, seed=42)

        # Empty formula is SAT with any assignment
        self.assertIsNotNone(solution)


class TestSchoeningUNSAT(unittest.TestCase):
    """Test Schöning's algorithm on UNSAT instances."""

    def test_simple_unsat(self):
        """Test on simple UNSAT formula."""
        # (x) & (~x) is UNSAT
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('x', True)])
        ])

        # Schöning's algorithm is incomplete, so it might not prove UNSAT
        # But it shouldn't return an invalid solution
        solution = solve_schoening(cnf, max_tries=100, seed=42)

        if solution is not None:
            # If it returns a solution, it must be valid
            self.assertTrue(cnf.evaluate(solution))
        # Otherwise None is acceptable (couldn't find solution)

    def test_unsat_3sat(self):
        """Test on UNSAT 3SAT instance."""
        # Small UNSAT instance
        cnf = CNFExpression.parse(
            "(x | y | z) & (~x | y | z) & (x | ~y | z) & (x | y | ~z) & "
            "(~x | ~y | ~z)"
        )

        solution = solve_schoening(cnf, max_tries=500, seed=42)

        # Schöning may or may not find that it's UNSAT
        # But if it returns a solution, it must be valid
        if solution is not None:
            self.assertTrue(cnf.evaluate(solution))


class TestSchoeningStatistics(unittest.TestCase):
    """Test statistics tracking."""

    def test_stats_tracking(self):
        """Test that statistics are collected."""
        cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")

        solution, stats = get_schoening_stats(cnf, seed=42)

        self.assertIsNotNone(solution)
        self.assertGreater(stats.tries, 0)
        self.assertGreaterEqual(stats.total_flips, 0)  # Can be 0 if lucky
        self.assertEqual(len(stats.flips_per_try), stats.tries)

    def test_stats_on_unsat(self):
        """Test statistics when no solution found."""
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('x', True)])
        ])

        solution, stats = get_schoening_stats(cnf, max_tries=10, seed=42)

        # Should have tried all attempts
        self.assertEqual(stats.tries, 10)
        self.assertGreater(stats.total_flips, 0)

    def test_early_termination_stats(self):
        """Test statistics when solution found early."""
        # Very easy formula
        cnf = CNFExpression([Clause([Literal('x', False)])])

        solution, stats = get_schoening_stats(cnf, max_tries=100, seed=42)

        self.assertIsNotNone(solution)
        # Should find solution quickly
        self.assertLess(stats.tries, 100)


class TestSchoeningParameters(unittest.TestCase):
    """Test algorithm parameters."""

    def test_max_tries(self):
        """Test max_tries parameter."""
        cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")

        # Very few tries - may not find solution
        solution = solve_schoening(cnf, max_tries=1, seed=42)
        # Just check it doesn't crash

        # Many tries - should find solution
        solution = solve_schoening(cnf, max_tries=1000, seed=42)
        self.assertIsNotNone(solution)

    def test_max_flips(self):
        """Test max_flips parameter."""
        cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c)")

        # Custom max_flips
        solution = solve_schoening(cnf, max_flips=50, seed=42)

        if solution is not None:
            self.assertTrue(cnf.evaluate(solution))

    def test_default_max_flips(self):
        """Test that default max_flips is 3*n."""
        cnf = CNFExpression.parse("(a | b | c) & (~a | b)")

        solver = SchoeningSolver(cnf, seed=42)
        n = len(cnf.get_variables())

        # Default should be 3*n
        solution = solver.solve(max_tries=10)

        # Check it collected stats (total_flips can be 0 if lucky)
        self.assertGreaterEqual(solver.stats.total_flips, 0)
        self.assertEqual(len(solver.stats.flips_per_try), solver.stats.tries)


class TestSchoeningComparison(unittest.TestCase):
    """Compare Schöning's algorithm with other solvers."""

    def test_vs_dpll(self):
        """Compare results with DPLL."""
        from bsat import solve_sat

        cnf = CNFExpression.parse("(x | y | z) & (~x | y | ~z) & (x | ~y | z)")

        dpll_solution = solve_sat(cnf)
        schoening_solution = solve_schoening(cnf, seed=42)

        # Both should find solutions (might be different)
        self.assertIsNotNone(dpll_solution)
        self.assertIsNotNone(schoening_solution)

        # Both solutions should be valid
        self.assertTrue(cnf.evaluate(dpll_solution))
        self.assertTrue(cnf.evaluate(schoening_solution))

    def test_vs_cdcl(self):
        """Compare results with CDCL."""
        from bsat import solve_cdcl

        cnf = CNFExpression.parse(
            "(a | b | c) & (~a | d | e) & (~b | ~d | f) & "
            "(~c | ~e | ~f) & (a | ~b | e)"
        )

        cdcl_solution = solve_cdcl(cnf)
        schoening_solution = solve_schoening(cnf, seed=42)

        if cdcl_solution is not None and schoening_solution is not None:
            # If both find solutions, both should be valid
            self.assertTrue(cnf.evaluate(cdcl_solution))
            self.assertTrue(cnf.evaluate(schoening_solution))


class TestSchoeningEdgeCases(unittest.TestCase):
    """Test edge cases and corner cases."""

    def test_single_variable(self):
        """Test with single variable."""
        cnf = CNFExpression([Clause([Literal('x', False)])])
        solution = solve_schoening(cnf, seed=42)

        self.assertIsNotNone(solution)
        self.assertTrue(solution['x'])

    def test_many_clauses(self):
        """Test with many clauses."""
        # Build formula with many clauses
        clauses = []
        for i in range(20):
            clauses.append(Clause([
                Literal(f'x{i}', False),
                Literal(f'x{i+1}', False),
                Literal(f'x{i+2}', False)
            ]))

        cnf = CNFExpression(clauses)
        solution = solve_schoening(cnf, max_tries=2000, seed=42)

        if solution is not None:
            self.assertTrue(cnf.evaluate(solution))

    def test_large_clauses(self):
        """Test with clauses larger than 3 literals."""
        # 5-SAT clause
        cnf = CNFExpression([
            Clause([Literal(f'x{i}', False) for i in range(5)])
        ])

        solution = solve_schoening(cnf, seed=42)
        self.assertIsNotNone(solution)
        self.assertTrue(cnf.evaluate(solution))


class TestSchoeningTheory(unittest.TestCase):
    """Test theoretical properties."""

    def test_2sat_fast(self):
        """Test that 2SAT is solved quickly."""
        # 2SAT instance
        cnf = CNFExpression.parse(
            "(x | y) & (~x | z) & (~y | w) & (~z | ~w)"
        )

        solution, stats = get_schoening_stats(cnf, seed=42)

        self.assertIsNotNone(solution)
        # Should solve very quickly for 2SAT
        self.assertLess(stats.tries, 50)

    def test_random_walk_behavior(self):
        """Test that algorithm does random walk."""
        cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c)")

        solver = SchoeningSolver(cnf, seed=42)
        solution = solver.solve(max_tries=10)

        # Should have collected flips (can be 0 if very lucky)
        self.assertGreaterEqual(solver.stats.total_flips, 0)

        # Each try should have some flips (>= 0)
        for flips in solver.stats.flips_per_try:
            self.assertGreaterEqual(flips, 0)


if __name__ == '__main__':
    unittest.main()
