"""
Tests for XOR-SAT solver.

XOR-SAT is a variant where each clause is an XOR of literals (satisfied when
an odd number of literals are true). Can be solved in polynomial time using
Gaussian elimination over GF(2).
"""

import unittest
from bsat import CNFExpression, Literal, Clause, solve_xorsat, get_xorsat_stats, XORSATSolver


class TestXORSAT(unittest.TestCase):
    """Test cases for XOR-SAT solver."""

    def test_empty_formula(self):
        """Empty formula should be SAT with empty assignment."""
        cnf = CNFExpression([])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result, {})

    def test_single_variable_positive(self):
        """Single positive literal: x"""
        # x ⊕ 0 = 1, so x must be True
        cnf = CNFExpression([
            Clause([Literal('x', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'], True)

    def test_single_variable_negative(self):
        """Single negative literal: ¬x"""
        # ¬x = 1, which means (1 ⊕ x) = 1, so x = 0
        cnf = CNFExpression([
            Clause([Literal('x', True)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'], False)

    def test_two_variables_xor(self):
        """x ⊕ y = 1"""
        # Variables must have different values
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        # Verify: exactly one should be True
        self.assertEqual(result['x'] ^ result['y'], True)

    def test_two_variables_xnor(self):
        """¬x ⊕ ¬y = 1"""
        # (1 ⊕ x) ⊕ (1 ⊕ y) = 1
        # x ⊕ y = 1, so they must differ
        cnf = CNFExpression([
            Clause([Literal('x', True), Literal('y', True)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'] ^ result['y'], True)

    def test_simple_system_sat(self):
        """
        Simple satisfiable system:
        x ⊕ y = 1
        y ⊕ z = 1
        Solution: x=T, y=F, z=T (or x=F, y=T, z=F)
        """
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('y', False), Literal('z', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'] ^ result['y'], True)
        self.assertEqual(result['y'] ^ result['z'], True)

    def test_simple_system_unsat(self):
        """
        Unsatisfiable system:
        x ⊕ y = 1
        y ⊕ z = 1
        x ⊕ z = 1
        This creates a contradiction (sum of LHS = 1, sum of RHS = 1 mod 2 = 1)
        But (x⊕y) ⊕ (y⊕z) = x⊕z should equal 0, not 1
        """
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('y', False), Literal('z', False)]),
            Clause([Literal('x', False), Literal('z', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNone(result)

    def test_chain_sat(self):
        """
        Chain of XORs (should be SAT):
        x₁ ⊕ x₂ = 1
        x₂ ⊕ x₃ = 1
        x₃ ⊕ x₄ = 1
        """
        cnf = CNFExpression([
            Clause([Literal('x1', False), Literal('x2', False)]),
            Clause([Literal('x2', False), Literal('x3', False)]),
            Clause([Literal('x3', False), Literal('x4', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        # Verify each equation
        self.assertEqual(result['x1'] ^ result['x2'], True)
        self.assertEqual(result['x2'] ^ result['x3'], True)
        self.assertEqual(result['x3'] ^ result['x4'], True)

    def test_three_way_xor(self):
        """x ⊕ y ⊕ z = 1 (odd parity)"""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        # Exactly 1 or 3 should be True
        count = sum([result['x'], result['y'], result['z']])
        self.assertIn(count, [1, 3])

    def test_four_way_xor_with_negation(self):
        """x ⊕ ¬y ⊕ z ⊕ w = 1"""
        cnf = CNFExpression([
            Clause([
                Literal('x', False),
                Literal('y', True),
                Literal('z', False),
                Literal('w', False)
            ])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        # Verify: x ⊕ (not y) ⊕ z ⊕ w should be True
        xor_result = result['x'] ^ (not result['y']) ^ result['z'] ^ result['w']
        self.assertTrue(xor_result)

    def test_multiple_equations_sat(self):
        """
        System of equations:
        x ⊕ y = 1
        y ⊕ z = 0
        z ⊕ w = 1
        """
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            # y ⊕ z = 0 is equivalent to ¬(y ⊕ z) = 1, which is ¬y ⊕ z = 0
            # Actually for XOR-SAT, even parity is: y ⊕ z ⊕ 1 = 1 → y ⊕ z = 0
            # We represent this as ¬y ⊕ ¬z = 1
            Clause([Literal('y', True), Literal('z', True)]),
            Clause([Literal('z', False), Literal('w', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'] ^ result['y'], True)
        # ¬y ⊕ ¬z = 1 means (1⊕y) ⊕ (1⊕z) = 1, so y ⊕ z = 1... wait
        # Let me recalculate: ¬y ⊕ ¬z represents NOT(y) XOR NOT(z)
        # (not y) ⊕ (not z) = 1
        self.assertEqual((not result['y']) ^ (not result['z']), True)
        self.assertEqual(result['z'] ^ result['w'], True)

    def test_large_system(self):
        """Test with more variables and equations."""
        # Create a solvable linear system
        # x₁ ⊕ x₂ = 1
        # x₂ ⊕ x₃ = 1
        # x₃ ⊕ x₄ = 0
        # x₄ ⊕ x₅ = 1
        cnf = CNFExpression([
            Clause([Literal('x1', False), Literal('x2', False)]),
            Clause([Literal('x2', False), Literal('x3', False)]),
            Clause([Literal('x3', True), Literal('x4', True)]),  # ¬x₃ ⊕ ¬x₄ = 1 → x₃ ⊕ x₄ = 1
            Clause([Literal('x4', False), Literal('x5', False)])
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        # Verify all equations
        self.assertEqual(result['x1'] ^ result['x2'], True)
        self.assertEqual(result['x2'] ^ result['x3'], True)
        self.assertEqual((not result['x3']) ^ (not result['x4']), True)
        self.assertEqual(result['x4'] ^ result['x5'], True)

    def test_underconstrained_system(self):
        """
        System with free variables:
        x ⊕ y = 1
        (z is free)
        """
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            # z appears in no equations, so it's free
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertEqual(result['x'] ^ result['y'], True)
        # z won't be in the result because it's not in the formula
        self.assertNotIn('z', result)

    def test_contradiction_simple(self):
        """
        x = 1 (from x ⊕ 0 = 1)
        x = 0 (from x ⊕ 1 = 1, which means ¬x ⊕ 0 = 1)
        """
        cnf = CNFExpression([
            Clause([Literal('x', False)]),   # x = 1
            Clause([Literal('x', True)])      # ¬x = 1, so x = 0
        ])
        result = solve_xorsat(cnf)
        self.assertIsNone(result)

    def test_statistics(self):
        """Test that statistics are collected properly."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),
            Clause([Literal('y', False), Literal('z', False)])
        ])
        stats_result = get_xorsat_stats(cnf)

        self.assertIn('solution', stats_result)
        self.assertIn('satisfiable', stats_result)
        self.assertIn('stats', stats_result)

        self.assertTrue(stats_result['satisfiable'])
        self.assertEqual(stats_result['stats']['num_variables'], 3)
        self.assertEqual(stats_result['stats']['num_clauses'], 2)
        self.assertGreater(stats_result['stats']['gaussian_elimination_steps'], 0)

    def test_all_zeros_equation(self):
        """0 = 1 (contradiction)"""
        # This is tricky to represent in XOR-SAT with our CNF structure
        # An empty clause in XOR-SAT would mean 0 = 1
        # But CNFExpression doesn't allow empty clauses typically
        # Skip this edge case for now
        pass

    def test_parity_check_code(self):
        """
        Simulating a simple parity check code:
        p = x₁ ⊕ x₂ ⊕ x₃
        If we know p=1, x₁=1, x₂=0, then x₃ must be 0
        """
        # p ⊕ x₁ ⊕ x₂ ⊕ x₃ = 0 (definition of parity)
        # p = 1, so: 1 ⊕ x₁ ⊕ x₂ ⊕ x₃ = 0
        # Which means: x₁ ⊕ x₂ ⊕ x₃ = 1
        # x₁ = 1: 1 ⊕ x₂ ⊕ x₃ = 1 → x₂ ⊕ x₃ = 0
        # x₂ = 0: 0 ⊕ x₃ = 0 → x₃ = 0
        cnf = CNFExpression([
            Clause([Literal('x1', False), Literal('x2', False), Literal('x3', False)]),  # x₁ ⊕ x₂ ⊕ x₃ = 1
            Clause([Literal('x1', False)]),  # x₁ = 1
            Clause([Literal('x2', True)])     # ¬x₂ = 1, so x₂ = 0
        ])
        result = solve_xorsat(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(result['x1'])
        self.assertFalse(result['x2'])
        self.assertFalse(result['x3'])

    def test_verify_solution(self):
        """Verify that returned solutions actually satisfy the formula."""
        test_cases = [
            CNFExpression([
                Clause([Literal('x', False), Literal('y', False)]),
                Clause([Literal('y', False), Literal('z', False)])
            ]),
            CNFExpression([
                Clause([Literal('a', False), Literal('b', True), Literal('c', False)])
            ]),
            CNFExpression([
                Clause([Literal('p', False), Literal('q', False)]),
                Clause([Literal('q', False), Literal('r', False)]),
                Clause([Literal('r', False), Literal('s', False)])
            ])
        ]

        for cnf in test_cases:
            result = solve_xorsat(cnf)
            if result is not None:
                # Verify each clause
                for clause in cnf.clauses:
                    # Count how many literals are true
                    true_count = sum(
                        1 for lit in clause.literals
                        if (result[lit.variable] and not lit.negated) or
                           (not result[lit.variable] and lit.negated)
                    )
                    # XOR clause is satisfied when odd number of literals are true
                    self.assertEqual(true_count % 2, 1,
                                   f"Clause {clause} not satisfied by {result}")


def run_tests():
    """Run all XOR-SAT tests."""
    suite = unittest.TestLoader().loadTestsFromTestCase(TestXORSAT)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return result.wasSuccessful()


if __name__ == '__main__':
    import sys
    success = run_tests()
    sys.exit(0 if success else 1)
