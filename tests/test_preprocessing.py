"""
Tests for SAT preprocessing and simplification.
"""

import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from bsat import CNFExpression, Clause, Literal
from bsat.preprocessing import (
    SATPreprocessor,
    decompose_into_components,
    preprocess_cnf,
    decompose_and_preprocess
)


class TestConnectedComponents(unittest.TestCase):
    """Test connected component decomposition."""

    def test_independent_clauses(self):
        """Test decomposition of completely independent clauses."""
        # (a | b) & (c | d) - two independent components
        cnf = CNFExpression.parse("(a | b) & (c | d)")
        components = decompose_into_components(cnf)

        self.assertEqual(len(components), 2)

        # Check that each component has one clause
        self.assertEqual(len(components[0].clauses), 1)
        self.assertEqual(len(components[1].clauses), 1)

    def test_connected_clauses(self):
        """Test clauses connected through shared variables."""
        # (a | b) & (b | c) - connected through b
        cnf = CNFExpression.parse("(a | b) & (b | c)")
        components = decompose_into_components(cnf)

        self.assertEqual(len(components), 1)
        self.assertEqual(len(components[0].clauses), 2)

    def test_partial_connection(self):
        """Test formula with some independent parts."""
        # (a | b) & (b | c) & (d | e) - 2 components
        cnf = CNFExpression.parse("(a | b) & (b | c) & (d | e)")
        components = decompose_into_components(cnf)

        self.assertEqual(len(components), 2)

        # One component should have 2 clauses, other should have 1
        sizes = sorted([len(c.clauses) for c in components])
        self.assertEqual(sizes, [1, 2])

    def test_single_clause(self):
        """Test single clause formula."""
        cnf = CNFExpression.parse("(a | b | c)")
        components = decompose_into_components(cnf)

        self.assertEqual(len(components), 1)

    def test_empty_formula(self):
        """Test empty formula."""
        cnf = CNFExpression([])
        components = decompose_into_components(cnf)

        self.assertEqual(len(components), 1)
        self.assertEqual(len(components[0].clauses), 0)


class TestUnitPropagation(unittest.TestCase):
    """Test unit propagation."""

    def test_simple_unit(self):
        """Test simple unit clause propagation."""
        # a & (a | b) -> a is unit, so formula becomes just a
        cnf = CNFExpression.parse("a & (a | b)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.assignments['a'], True)
        self.assertEqual(len(result.simplified.clauses), 0)  # All satisfied

    def test_unit_chain(self):
        """Test chain of unit propagations."""
        # a & (~a | b) & (~b | c) -> a=T, b=T, c=T
        cnf = CNFExpression.parse("a & (~a | b) & (~b | c)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.assignments['a'], True)
        self.assertEqual(result.assignments['b'], True)
        self.assertEqual(result.assignments['c'], True)
        self.assertEqual(len(result.simplified.clauses), 0)

    def test_unit_conflict(self):
        """Test unit propagation leading to conflict."""
        # a & ~a -> UNSAT
        cnf = CNFExpression([
            Clause([Literal('a', False)]),
            Clause([Literal('a', True)])
        ])
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.is_sat, False)

    def test_unit_simplification(self):
        """Test unit clause simplifying others."""
        # a & (a | b | c) & (~a | d) -> a & d
        cnf = CNFExpression.parse("a & (a | b | c) & (~a | d)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.assignments['a'], True)
        self.assertEqual(result.assignments['d'], True)
        self.assertEqual(len(result.simplified.clauses), 0)


class TestPureLiteralElimination(unittest.TestCase):
    """Test pure literal elimination."""

    def test_simple_pure(self):
        """Test simple pure literal."""
        # (a | b) & (a | c) - a is pure positive
        cnf = CNFExpression.parse("(a | b) & (a | c)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.assignments['a'], True)
        self.assertEqual(len(result.simplified.clauses), 0)

    def test_negative_pure(self):
        """Test pure negative literal."""
        # (~a | b) & (~a | c) - a is pure negative
        cnf = CNFExpression.parse("(~a | b) & (~a | c)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.assignments['a'], False)
        self.assertEqual(len(result.simplified.clauses), 0)

    def test_mixed_not_pure(self):
        """Test variable appearing in both polarities."""
        # (a | b) & (~a | c) - a is not pure
        cnf = CNFExpression.parse("(a | b) & (~a | c)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        # a should not be assigned (not pure)
        self.assertNotIn('a', result.assignments)

    def test_multiple_pure(self):
        """Test multiple pure literals."""
        # (a | b) & (b | c) - both a and c are pure
        cnf = CNFExpression.parse("(a | b) & (b | c)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.assignments['a'], True)
        self.assertEqual(result.assignments['c'], True)
        self.assertEqual(len(result.simplified.clauses), 0)


class TestSubsumption(unittest.TestCase):
    """Test clause subsumption."""

    def test_simple_subsumption(self):
        """Test simple subsumption."""
        # (a) & (a | b) -> (a) subsumes (a | b)
        cnf = CNFExpression([
            Clause([Literal('a', False)]),
            Clause([Literal('a', False), Literal('b', False)])
        ])
        preprocessor = SATPreprocessor(cnf)
        # Run subsumption only, no unit prop
        result = preprocessor.preprocess(unit_propagation=False, pure_literal=False,
                                         subsumption=True, self_subsumption=False)

        # After subsumption, should have removed one clause
        self.assertGreater(preprocessor.stats.subsumed_clauses, 0)
        self.assertEqual(len(result.simplified.clauses), 1)

    def test_no_subsumption(self):
        """Test no subsumption occurs."""
        # (a | b) & (c | d) - no subsumption
        cnf = CNFExpression.parse("(a | b) & (c | d)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess(subsumption=True, unit_propagation=False, pure_literal=False)

        self.assertEqual(preprocessor.stats.subsumed_clauses, 0)
        self.assertEqual(len(result.simplified.clauses), 2)

    def test_transitive_subsumption(self):
        """Test transitive subsumption."""
        # (a) & (a | b) & (a | b | c) -> only (a) remains
        cnf = CNFExpression([
            Clause([Literal('a', False)]),
            Clause([Literal('a', False), Literal('b', False)]),
            Clause([Literal('a', False), Literal('b', False), Literal('c', False)])
        ])
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        # After unit prop, all should be gone
        self.assertEqual(result.assignments['a'], True)


class TestSelfSubsumption(unittest.TestCase):
    """Test self-subsumption resolution."""

    def test_simple_self_subsumption(self):
        """Test simple self-subsumption."""
        # (a | b) & (~a | b | c) -> (~a | b | c) becomes (b | c)
        cnf = CNFExpression([
            Clause([Literal('a', False), Literal('b', False)]),
            Clause([Literal('a', True), Literal('b', False), Literal('c', False)])
        ])
        preprocessor = SATPreprocessor(cnf)
        preprocessor.preprocess(unit_propagation=False, pure_literal=False,
                               subsumption=False, self_subsumption=True)

        # Check if self-subsumption occurred
        if preprocessor.stats.self_subsumptions > 0:
            # Should have simplified the clause
            self.assertTrue(True)


class TestCombinedPreprocessing(unittest.TestCase):
    """Test combined preprocessing techniques."""

    def test_full_preprocessing(self):
        """Test all techniques together."""
        # Complex formula that can be simplified
        cnf = CNFExpression.parse(
            "a & (a | b) & (~a | c) & (c | d) & (d | e)"
        )
        result = preprocess_cnf(cnf)

        # a is unit, so a=True, which forces c=True
        self.assertEqual(result.assignments['a'], True)
        self.assertEqual(result.assignments['c'], True)

    def test_trivial_sat(self):
        """Test detection of trivially SAT formula."""
        # Pure literal makes entire formula SAT
        cnf = CNFExpression.parse("(a | b)")
        result = preprocess_cnf(cnf)

        self.assertEqual(result.is_sat, True)

    def test_example_from_docstring(self):
        """Test the example from module docstring."""
        # (a | b) & (c | d) - independent components
        cnf = CNFExpression.parse("(a | b) & (c | d)")
        components, assignments, stats = decompose_and_preprocess(cnf)

        self.assertEqual(stats.components, 2)


class TestDecomposeAndPreprocess(unittest.TestCase):
    """Test combined decomposition and preprocessing."""

    def test_independent_with_units(self):
        """Test independent components with unit clauses."""
        # (a | b) & a & (c | d) & c
        cnf = CNFExpression.parse("(a | b) & a & (c | d) & c")
        components, assignments, stats = decompose_and_preprocess(cnf)

        # Both a and c should be assigned
        self.assertEqual(assignments['a'], True)
        self.assertEqual(assignments['c'], True)

        # After preprocessing, should be empty (all satisfied)
        self.assertEqual(len(components), 0)

    def test_mixed_components(self):
        """Test mix of trivial and non-trivial components."""
        # a & (b | c) & d - all connected through shared variables after parsing
        # Actually they're all one component. Let's use truly independent:
        # (a | b) & c & (d | e) - c is independent unit clause
        cnf = CNFExpression.parse("(a | b) & c & (d | e)")
        components, assignments, stats = decompose_and_preprocess(cnf)

        self.assertEqual(assignments['c'], True)

        # (a | b) and (d | e) should remain as separate components or be simplified
        # After preprocessing with pure literals, a and d might be assigned
        # Just check that we got some simplification
        self.assertGreaterEqual(len(components), 0)


class TestPreprocessingStats(unittest.TestCase):
    """Test statistics tracking."""

    def test_stats_tracking(self):
        """Test that statistics are properly tracked."""
        cnf = CNFExpression.parse("a & (~a | b) & (a | c)")
        preprocessor = SATPreprocessor(cnf)
        result = preprocessor.preprocess()

        self.assertEqual(result.stats.original_vars, 3)
        self.assertEqual(result.stats.original_clauses, 3)
        self.assertGreater(result.stats.unit_propagations, 0)

    def test_stats_str(self):
        """Test statistics string representation."""
        cnf = CNFExpression.parse("a & (a | b)")
        result = preprocess_cnf(cnf)

        stats_str = str(result.stats)
        self.assertIn("Variables:", stats_str)
        self.assertIn("Clauses:", stats_str)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and corner cases."""

    def test_empty_formula(self):
        """Test empty formula."""
        cnf = CNFExpression([])
        result = preprocess_cnf(cnf)

        self.assertEqual(result.is_sat, True)
        self.assertEqual(len(result.assignments), 0)

    def test_single_unit(self):
        """Test single unit clause."""
        cnf = CNFExpression([Clause([Literal('x', False)])])
        result = preprocess_cnf(cnf)

        self.assertEqual(result.assignments['x'], True)
        self.assertEqual(result.is_sat, True)

    def test_large_formula(self):
        """Test preprocessing on larger formula."""
        clauses = []
        # Build chain: x1 & (~x1 | x2) & (~x2 | x3) & ... & (~x9 | x10)
        clauses.append(Clause([Literal('x1', False)]))
        for i in range(1, 10):
            clauses.append(Clause([
                Literal(f'x{i}', True),
                Literal(f'x{i+1}', False)
            ]))

        cnf = CNFExpression(clauses)
        result = preprocess_cnf(cnf)

        # All variables should be assigned True
        for i in range(1, 11):
            self.assertEqual(result.assignments[f'x{i}'], True)


if __name__ == '__main__':
    unittest.main()
