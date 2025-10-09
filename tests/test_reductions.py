"""
Tests for SAT formula reductions, particularly k-SAT to 3-SAT.
"""

import unittest
from bsat import (
    CNFExpression, Clause, Literal,
    reduce_to_3sat, extract_original_solution, solve_with_reduction,
    is_3sat, get_max_clause_size, solve_sat
)


class TestReductions(unittest.TestCase):
    """Test cases for SAT formula reductions."""

    def test_is_3sat_true(self):
        """Test that 3-SAT formulas are correctly identified."""
        # All clauses have ≤ 3 literals
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
            Clause([Literal('a', False), Literal('b', False)]),
            Clause([Literal('c', False)])
        ])
        self.assertTrue(is_3sat(cnf))

    def test_is_3sat_false(self):
        """Test that non-3-SAT formulas are correctly identified."""
        # Has a 4-literal clause
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
            Clause([Literal('a', False), Literal('b', False), Literal('c', False), Literal('d', False)])
        ])
        self.assertFalse(is_3sat(cnf))

    def test_get_max_clause_size(self):
        """Test getting maximum clause size."""
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False)]),  # 2 literals
            Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),  # 3 literals
            Clause([Literal('p', False)])  # 1 literal
        ])
        self.assertEqual(get_max_clause_size(cnf), 3)

    def test_get_max_clause_size_empty(self):
        """Test maximum clause size for empty formula."""
        cnf = CNFExpression([])
        self.assertEqual(get_max_clause_size(cnf), 0)

    def test_reduce_4sat_to_3sat(self):
        """Test reducing a 4-SAT clause to 3-SAT."""
        # (a ∨ b ∨ c ∨ d)
        cnf = CNFExpression([
            Clause([
                Literal('a', False),
                Literal('b', False),
                Literal('c', False),
                Literal('d', False)
            ])
        ])

        reduced, aux_map, stats = reduce_to_3sat(cnf)

        # Should create 2 clauses and 1 auxiliary variable
        self.assertEqual(len(reduced.clauses), 2)
        self.assertEqual(len(aux_map), 1)
        self.assertEqual(stats.auxiliary_variables, 1)
        self.assertEqual(stats.clauses_expanded, 1)

        # All clauses should have exactly 3 literals
        for clause in reduced.clauses:
            self.assertEqual(len(clause.literals), 3)

        # Check it's now 3-SAT
        self.assertTrue(is_3sat(reduced))

    def test_reduce_5sat_to_3sat(self):
        """Test reducing a 5-SAT clause to 3-SAT."""
        # (a ∨ b ∨ c ∨ d ∨ e)
        cnf = CNFExpression([
            Clause([
                Literal('a', False),
                Literal('b', False),
                Literal('c', False),
                Literal('d', False),
                Literal('e', False)
            ])
        ])

        reduced, aux_map, stats = reduce_to_3sat(cnf)

        # Should create 3 clauses and 2 auxiliary variables
        self.assertEqual(len(reduced.clauses), 3)
        self.assertEqual(len(aux_map), 2)
        self.assertEqual(stats.auxiliary_variables, 2)

        # All clauses should have exactly 3 literals
        for clause in reduced.clauses:
            self.assertEqual(len(clause.literals), 3)

        self.assertTrue(is_3sat(reduced))

    def test_reduce_preserves_small_clauses(self):
        """Test that clauses with ≤ 3 literals are preserved."""
        cnf = CNFExpression([
            Clause([Literal('x', False)]),  # 1 literal
            Clause([Literal('y', False), Literal('z', False)]),  # 2 literals
            Clause([Literal('a', False), Literal('b', False), Literal('c', False)])  # 3 literals
        ])

        reduced, aux_map, stats = reduce_to_3sat(cnf)

        # Should be unchanged
        self.assertEqual(len(reduced.clauses), 3)
        self.assertEqual(len(aux_map), 0)
        self.assertEqual(stats.auxiliary_variables, 0)
        self.assertEqual(stats.clauses_expanded, 0)

        # Clauses should be identical
        for orig, red in zip(cnf.clauses, reduced.clauses):
            self.assertEqual(len(orig.literals), len(red.literals))

    def test_reduce_mixed_formula(self):
        """Test reducing a formula with mixed clause sizes."""
        cnf = CNFExpression([
            Clause([Literal('x', False), Literal('y', False)]),  # 2-SAT
            Clause([Literal('a', False), Literal('b', False), Literal('c', False), Literal('d', False)]),  # 4-SAT
            Clause([Literal('p', False), Literal('q', False), Literal('r', False)])  # 3-SAT
        ])

        reduced, aux_map, stats = reduce_to_3sat(cnf)

        # Original: 3 clauses
        # Only the 4-SAT clause is expanded into 2 clauses
        # Total: 2 + 2 + 1 = 4 clauses
        self.assertEqual(len(reduced.clauses), 4)
        self.assertEqual(stats.clauses_expanded, 1)
        self.assertEqual(stats.auxiliary_variables, 1)

        # Should be 3-SAT now
        self.assertTrue(is_3sat(reduced))

    def test_reduction_equivalence_4sat(self):
        """Test that reduced formula is satisfiable iff original is."""
        # (a ∨ b ∨ c ∨ d)
        cnf = CNFExpression([
            Clause([
                Literal('a', False),
                Literal('b', False),
                Literal('c', False),
                Literal('d', False)
            ])
        ])

        reduced, aux_map, _ = reduce_to_3sat(cnf)

        # Original should be SAT (set any variable to True)
        orig_solution = solve_sat(cnf)
        self.assertIsNotNone(orig_solution)

        # Reduced should also be SAT
        reduced_solution = solve_sat(reduced)
        self.assertIsNotNone(reduced_solution)

        # Extract original variables
        extracted = extract_original_solution(reduced_solution, aux_map)

        # Should satisfy original formula
        self.assertTrue(cnf.evaluate(extracted))

    def test_reduction_equivalence_unsat(self):
        """Test that UNSAT formulas remain UNSAT after reduction."""
        # (a ∨ b ∨ c ∨ d) ∧ (¬a) ∧ (¬b) ∧ (¬c) ∧ (¬d)
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False), Literal('c', False), Literal('d', False)]),
            Clause([Literal('a', True)]),
            Clause([Literal('b', True)]),
            Clause([Literal('c', True)]),
            Clause([Literal('d', True)])
        ])

        reduced, aux_map, _ = reduce_to_3sat(cnf)

        # Original should be UNSAT
        orig_solution = solve_sat(cnf)
        self.assertIsNone(orig_solution)

        # Reduced should also be UNSAT
        reduced_solution = solve_sat(reduced)
        self.assertIsNone(reduced_solution)

    def test_extract_original_solution(self):
        """Test extracting original variables from solution."""
        solution = {
            'a': True,
            'b': False,
            'c': True,
            '_aux0': True,
            '_aux1': False
        }

        aux_map = {
            '_aux0': 'Auxiliary 1',
            '_aux1': 'Auxiliary 2'
        }

        extracted = extract_original_solution(solution, aux_map)

        self.assertEqual(extracted, {'a': True, 'b': False, 'c': True})
        self.assertNotIn('_aux0', extracted)
        self.assertNotIn('_aux1', extracted)

    def test_extract_original_solution_none(self):
        """Test extracting from None solution."""
        result = extract_original_solution(None, {})
        self.assertIsNone(result)

    def test_solve_with_reduction_4sat(self):
        """Test the convenience function solve_with_reduction."""
        # (a ∨ b ∨ c ∨ d)
        cnf = CNFExpression([
            Clause([
                Literal('a', False),
                Literal('b', False),
                Literal('c', False),
                Literal('d', False)
            ])
        ])

        solution, stats = solve_with_reduction(cnf)

        # Should find a solution
        self.assertIsNotNone(solution)

        # Solution should only contain original variables
        self.assertIn('a', solution)
        self.assertIn('b', solution)
        self.assertIn('c', solution)
        self.assertIn('d', solution)

        # Should not contain auxiliary variables
        for var in solution:
            self.assertFalse(var.startswith('_aux'))

        # Should satisfy original formula
        self.assertTrue(cnf.evaluate(solution))

        # Check stats
        self.assertEqual(stats.original_clauses, 1)
        self.assertEqual(stats.auxiliary_variables, 1)

    def test_solve_with_reduction_5sat(self):
        """Test solving 5-SAT with reduction."""
        # (a ∨ b ∨ c ∨ d ∨ e) ∧ (¬a ∨ ¬b ∨ ¬c ∨ ¬d ∨ ¬e)
        cnf = CNFExpression([
            Clause([
                Literal('a', False),
                Literal('b', False),
                Literal('c', False),
                Literal('d', False),
                Literal('e', False)
            ]),
            Clause([
                Literal('a', True),
                Literal('b', True),
                Literal('c', True),
                Literal('d', True),
                Literal('e', True)
            ])
        ])

        solution, stats = solve_with_reduction(cnf)

        # Should find a solution
        self.assertIsNotNone(solution)

        # Should satisfy original
        self.assertTrue(cnf.evaluate(solution))

        # Check stats
        self.assertEqual(stats.original_clauses, 2)
        self.assertEqual(stats.clauses_expanded, 2)  # Both clauses expanded
        self.assertEqual(stats.auxiliary_variables, 4)  # 2 per clause

    def test_large_clause_reduction(self):
        """Test reducing a very large clause."""
        # Create a 10-literal clause
        literals = [Literal(f'x{i}', False) for i in range(10)]
        cnf = CNFExpression([Clause(literals)])

        reduced, aux_map, stats = reduce_to_3sat(cnf)

        # 10 literals → need 7 auxiliary variables → 8 clauses
        self.assertEqual(stats.auxiliary_variables, 7)
        self.assertEqual(len(reduced.clauses), 8)

        # All should be 3-SAT
        self.assertTrue(is_3sat(reduced))
        for clause in reduced.clauses:
            self.assertEqual(len(clause.literals), 3)

    def test_custom_var_prefix(self):
        """Test using a custom auxiliary variable prefix."""
        cnf = CNFExpression([
            Clause([
                Literal('a', False),
                Literal('b', False),
                Literal('c', False),
                Literal('d', False)
            ])
        ])

        reduced, aux_map, _ = reduce_to_3sat(cnf, var_prefix="helper")

        # Auxiliary variables should use custom prefix
        for aux_var in aux_map.keys():
            self.assertTrue(aux_var.startswith("helper"))

    def test_reduction_stats(self):
        """Test that reduction statistics are correct."""
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False)]),
            Clause([Literal('x', False), Literal('y', False), Literal('z', False), Literal('w', False)])
        ])

        _, _, stats = reduce_to_3sat(cnf)

        self.assertEqual(stats.original_clauses, 2)
        self.assertEqual(stats.original_variables, 6)  # a, b, x, y, z, w
        self.assertEqual(stats.reduced_clauses, 3)  # 1 unchanged + 2 from expansion
        self.assertEqual(stats.auxiliary_variables, 1)
        self.assertEqual(stats.clauses_expanded, 1)
        self.assertEqual(stats.max_clause_size_original, 4)
        self.assertEqual(stats.max_clause_size_reduced, 3)

    def test_reduction_preserves_negations(self):
        """Test that negated literals are preserved correctly."""
        cnf = CNFExpression([
            Clause([
                Literal('a', True),   # ¬a
                Literal('b', False),  # b
                Literal('c', True),   # ¬c
                Literal('d', False)   # d
            ])
        ])

        reduced, aux_map, _ = reduce_to_3sat(cnf)

        # Should create (¬a ∨ b ∨ x) ∧ (¬x ∨ ¬c ∨ d)
        self.assertEqual(len(reduced.clauses), 2)

        # Check first clause has ¬a and b
        first_clause = reduced.clauses[0]
        var_names = [lit.variable for lit in first_clause.literals]
        self.assertIn('a', var_names)
        self.assertIn('b', var_names)

        # Find the literal for 'a' and check it's negated
        a_lit = next(lit for lit in first_clause.literals if lit.variable == 'a')
        self.assertTrue(a_lit.negated)

        # Find the literal for 'b' and check it's positive
        b_lit = next(lit for lit in first_clause.literals if lit.variable == 'b')
        self.assertFalse(b_lit.negated)


def run_tests():
    """Run all reduction tests."""
    suite = unittest.TestLoader().loadTestsFromTestCase(TestReductions)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return result.wasSuccessful()


if __name__ == '__main__':
    import sys
    success = run_tests()
    sys.exit(0 if success else 1)
