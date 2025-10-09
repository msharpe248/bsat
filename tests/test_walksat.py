"""
Tests for WalkSAT solver.

WalkSAT is an incomplete randomized local search algorithm. Unlike complete
solvers, it may not find a solution even if one exists, and cannot prove UNSAT.
However, it's often very fast for satisfiable instances.
"""

import unittest
from bsat import CNFExpression, Literal, Clause, solve_walksat, get_walksat_stats, WalkSATSolver


class TestWalkSAT(unittest.TestCase):
    """Test cases for WalkSAT solver."""

    def test_empty_formula(self):
        """Empty formula should be SAT with empty assignment."""
        cnf = CNFExpression([])
        result = solve_walksat(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertEqual(result, {})

    def test_single_literal_positive(self):
        """Single positive literal: x"""
        cnf = CNFExpression([
            Clause([Literal('x', False)])
        ])
        result = solve_walksat(cnf, seed=42)
        self.assertIsNotNone(result)
        # Should find x=True
        self.assertTrue(result['x'])

    def test_single_literal_negative(self):
        """Single negative literal: ¬x"""
        cnf = CNFExpression([
            Clause([Literal('x', True)])
        ])
        result = solve_walksat(cnf, seed=42)
        self.assertIsNotNone(result)
        # Should find x=False
        self.assertFalse(result['x'])

    def test_simple_satisfiable(self):
        """Simple satisfiable formula."""
        # (x ∨ y) ∧ (¬x ∨ z) ∧ (¬y ∨ ¬z)
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)]),
            Clause([Literal('y', True), Literal('z', True)])
        ])
        result = solve_walksat(cnf, seed=42)
        self.assertIsNotNone(result)
        # Verify solution
        self.assertTrue(cnf.evaluate(result))

    def test_3sat_satisfiable(self):
        """3-SAT satisfiable instance."""
        # (x ∨ y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ z)
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
            Clause([Literal('x', True), Literal('y', False), Literal('z', True)]),
            Clause([Literal('x', False), Literal('y', True), Literal('z', False)])
        ])
        result = solve_walksat(cnf, max_flips=1000, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_unsatisfiable_simple(self):
        """Simple UNSAT formula - WalkSAT won't find solution."""
        # x ∧ ¬x
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('x', True)])
        ])
        # With limited flips, should not find solution
        result = solve_walksat(cnf, max_flips=100, max_tries=5, seed=42)
        # WalkSAT is incomplete, so None doesn't prove UNSAT
        # We just check it returns None (no solution found)
        self.assertIsNone(result)

    def test_noise_parameter_zero(self):
        """Test with noise=0 (pure greedy)."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)])
        ])
        result = solve_walksat(cnf, noise=0.0, max_flips=1000, seed=42)
        if result:  # Greedy might get stuck, so solution not guaranteed
            self.assertTrue(cnf.evaluate(result))

    def test_noise_parameter_one(self):
        """Test with noise=1.0 (pure random walk)."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)])
        ])
        result = solve_walksat(cnf, noise=1.0, max_flips=5000, seed=42)
        if result:  # Random walk is inefficient
            self.assertTrue(cnf.evaluate(result))

    def test_noise_parameter_optimal(self):
        """Test with typical optimal noise (0.5)."""
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),
            Clause([Literal('a', True), Literal('b', False), Literal('c', True)]),
            Clause([Literal('a', False), Literal('b', True), Literal('c', False)]),
            Clause([Literal('a', True), Literal('b', True), Literal('c', True)])
        ])
        result = solve_walksat(cnf, noise=0.5, max_flips=1000, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_max_flips_limit(self):
        """Test that solver respects max_flips limit."""
        # Create a harder instance
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),
            Clause([Literal('a', True), Literal('b', False), Literal('c', True)]),
            Clause([Literal('a', False), Literal('b', True), Literal('d', False)]),
            Clause([Literal('b', True), Literal('c', True), Literal('d', True)]),
            Clause([Literal('a', True), Literal('c', False), Literal('d', False)])
        ])
        # Very low max_flips might not find solution
        result = solve_walksat(cnf, max_flips=5, max_tries=1, seed=42)
        # Just verify it doesn't crash and terminates

    def test_max_tries_restarts(self):
        """Test multiple random restarts."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)]),
            Clause([Literal('y', True), Literal('z', True)])
        ])
        # Use multiple tries with fewer flips each
        result = solve_walksat(cnf, max_flips=100, max_tries=10, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_reproducibility_with_seed(self):
        """Same seed should give same result."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
            Clause([Literal('x', True), Literal('y', False), Literal('z', True)]),
            Clause([Literal('x', False), Literal('y', True), Literal('z', False)])
        ])

        result1 = solve_walksat(cnf, seed=12345)
        result2 = solve_walksat(cnf, seed=12345)

        # Should get same result with same seed
        self.assertEqual(result1, result2)

    def test_different_seeds_may_differ(self):
        """Different seeds may give different results."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
            Clause([Literal('x', True), Literal('y', False), Literal('z', True)])
        ])

        result1 = solve_walksat(cnf, seed=111, max_tries=1)
        result2 = solve_walksat(cnf, seed=222, max_tries=1)

        # May or may not be different, but both should be valid if not None
        if result1:
            self.assertTrue(cnf.evaluate(result1))
        if result2:
            self.assertTrue(cnf.evaluate(result2))

    def test_statistics(self):
        """Test statistics collection."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('x', True), Literal('z', False)])
        ])

        stats_result = get_walksat_stats(cnf, max_flips=1000, seed=42)

        self.assertIn('solution', stats_result)
        self.assertIn('found', stats_result)
        self.assertIn('stats', stats_result)

        if stats_result['found']:
            self.assertIsNotNone(stats_result['solution'])
            self.assertTrue(cnf.evaluate(stats_result['solution']))

        # Check stats
        self.assertIn('total_flips', stats_result['stats'])
        self.assertIn('total_tries', stats_result['stats'])
        self.assertIn('num_variables', stats_result['stats'])
        self.assertIn('num_clauses', stats_result['stats'])

        self.assertEqual(stats_result['stats']['num_variables'], 3)
        self.assertEqual(stats_result['stats']['num_clauses'], 2)
        self.assertGreater(stats_result['stats']['total_flips'], 0)

    def test_larger_satisfiable_instance(self):
        """Test on a larger satisfiable instance."""
        # Create a larger random-ish 3-SAT instance that's satisfiable
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),
            Clause([Literal('b', True), Literal('c', False), Literal('d', False)]),
            Clause([Literal('a', True), Literal('c', True), Literal('e', False)]),
            Clause([Literal('d', True), Literal('e', True), Literal('f', False)]),
            Clause([Literal('a', False), Literal('d', False), Literal('f', False)]),
            Clause([Literal('b', False), Literal('e', True), Literal('f', True)])
        ])

        result = solve_walksat(cnf, max_flips=5000, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_solver_class_direct(self):
        """Test using WalkSATSolver class directly."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('y', True), Literal('z', False)])
        ])

        solver = WalkSATSolver(cnf, noise=0.5, max_flips=1000, seed=42)
        result = solver.solve()

        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))
        self.assertGreater(solver.stats['total_flips'], 0)

    def test_all_positive_clauses(self):
        """Formula with all positive literals."""
        # (x ∨ y ∨ z) ∧ (a ∨ b ∨ c)
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
            Clause([Literal('a', False), Literal('b', False), Literal('c', False)])
        ])
        result = solve_walksat(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_horn_clause_example(self):
        """WalkSAT can solve Horn clauses (though Horn-SAT solver is better)."""
        # x ∧ (¬x ∨ y) ∧ (¬y ∨ z)
        cnf = CNFExpression([
            Clause([Literal('x', False)]),
            Clause([Literal('x', True), Literal('y', False)]),
            Clause([Literal('y', True), Literal('z', False)])
        ])
        result = solve_walksat(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))
        # Should derive x=T, y=T, z=T
        self.assertTrue(result['x'])
        self.assertTrue(result['y'])
        self.assertTrue(result['z'])

    def test_verify_solution_validity(self):
        """Verify that all returned solutions are actually valid."""
        test_formulas = [
            CNFExpression([
                Clause([Literal('x', False), Literal('y', False)]),
                Clause([Literal('x', True), Literal('z', False)])
            ]),
            CNFExpression([
                Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),
                Clause([Literal('a', True), Literal('b', True), Literal('c', True)])
            ]),
            CNFExpression([
                Clause([Literal('p', False), Literal('q', True)]),
                Clause([Literal('q', False), Literal('r', False)]),
                Clause([Literal('r', True), Literal('p', True)])
            ])
        ]

        for cnf in test_formulas:
            result = solve_walksat(cnf, max_flips=2000, seed=42)
            if result is not None:
                # Every non-None result must be a valid solution
                self.assertTrue(cnf.evaluate(result),
                              f"Invalid solution for formula: {cnf}")


def run_tests():
    """Run all WalkSAT tests."""
    suite = unittest.TestLoader().loadTestsFromTestCase(TestWalkSAT)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return result.wasSuccessful()


if __name__ == '__main__':
    import sys
    success = run_tests()
    sys.exit(0 if success else 1)
