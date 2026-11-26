"""
Tests for formula simplification: probing, BCE, BVE and vivification.
"""

import itertools
import random
import unittest

from bsat import (
    CNFExpression, Clause, Literal,
    Simplifier, simplify_cnf, solve_with_simplification,
    solve_sat, solve_cdcl,
)


def random_cnf(rng, max_vars=9):
    n = rng.randint(1, max_vars)
    variables = [f"v{i}" for i in range(n)]
    return CNFExpression([
        Clause([Literal(rng.choice(variables), rng.random() < 0.5)
                for _ in range(rng.randint(1, 4))])
        for _ in range(rng.randint(0, int(n * rng.choice([1.5, 3, 4.3]))))
    ])


def all_models(cnf, variables):
    """Every satisfying assignment of cnf over the given variables."""
    return {
        bits for bits in itertools.product([False, True], repeat=len(variables))
        if cnf.evaluate(dict(zip(variables, bits)))
    }


def clause_sets(cnf):
    return {frozenset((l.variable, l.negated) for l in c.literals) for c in cnf.clauses}


class TestFailedLiteralProbing(unittest.TestCase):

    def test_failed_literal_is_fixed(self):
        # Assuming a leads to both b and ¬b, so ¬a must hold
        cnf = CNFExpression.parse("(~a | b) & (~a | ~b) & (a | c | d)")
        simplifier = Simplifier(cnf)
        self.assertGreaterEqual(simplifier.probe_failed_literals(), 1)
        result = simplifier.result()
        self.assertNotIn('a', result.cnf.get_variables())
        model = result.extend_model(solve_sat(result.cnf))
        self.assertFalse(model['a'])
        self.assertTrue(cnf.evaluate(model))

    def test_both_polarities_fail(self):
        cnf = CNFExpression.parse("(~a | b) & (~a | ~b) & (a | c) & (a | ~c)")
        simplifier = Simplifier(cnf)
        simplifier.probe_failed_literals()
        self.assertTrue(simplifier.result().is_unsat)

    def test_nothing_to_fix(self):
        cnf = CNFExpression.parse("(a | b | c) & (~a | ~b | ~c)")
        simplifier = Simplifier(cnf)
        self.assertEqual(simplifier.probe_failed_literals(), 0)
        self.assertEqual(clause_sets(simplifier.result().cnf), clause_sets(cnf))


class TestBlockedClauseElimination(unittest.TestCase):

    def test_blocked_clause_is_removed(self):
        # (a | b) is blocked on a: its only resolution partner on a is
        # (~a | ~b | c), and the resolvent (b | ~b | c) is a tautology
        cnf = CNFExpression.parse("(a | b) & (~a | ~b | c) & (b | c) & (~b | ~c)")
        simplifier = Simplifier(cnf)
        self.assertGreaterEqual(simplifier.eliminate_blocked_clauses(), 1)

    def test_reconstruction_flips_blocking_literal(self):
        cnf = CNFExpression.parse("(a | b) & (~a | ~b)")
        simplifier = Simplifier(cnf)
        simplifier.eliminate_blocked_clauses()
        result = simplifier.result()
        self.assertEqual(len(result.cnf.clauses), 0)
        # The empty formula is satisfied by anything, including all-False,
        # which violates (a | b); reconstruction must repair it
        model = result.extend_model({})
        self.assertTrue(cnf.evaluate(model))

    def test_unsat_formula_keeps_a_core(self):
        cnf = CNFExpression.parse("(a | b) & (~a | b) & (a | ~b) & (~a | ~b)")
        simplifier = Simplifier(cnf)
        self.assertEqual(simplifier.eliminate_blocked_clauses(), 0)


class TestVariableElimination(unittest.TestCase):

    def test_variable_is_resolved_away(self):
        cnf = CNFExpression.parse("(a | x) & (b | x) & (~x | c)")
        simplifier = Simplifier(cnf)
        self.assertGreaterEqual(simplifier.eliminate_variables(), 1)
        self.assertNotIn('x', simplifier.result().cnf.get_variables())

    def test_resolvents_replace_clauses(self):
        # Eliminating only x (the others occur too often for the limit)
        cnf = CNFExpression.parse("(a | x) & (~x | b)")
        simplifier = Simplifier(cnf)
        positive = [c for c in simplifier.clauses if simplifier.names.index('x') + 1 in c]
        self.assertEqual(len(positive), 1)
        simplifier.eliminate_variables()
        result = simplifier.result()
        model = result.extend_model(solve_sat(result.cnf) or {})
        self.assertTrue(cnf.evaluate(model))

    def test_growth_bound_is_respected(self):
        # x occurs 3 times in each polarity with disjoint side literals:
        # 9 resolvents would replace 6 clauses, so x must be kept
        clauses = [Clause([Literal('x'), Literal(f"p{i}"), Literal(f"q{i}")]) for i in range(3)]
        clauses += [Clause([Literal('x', True), Literal(f"r{i}"), Literal(f"s{i}")])
                    for i in range(3)]
        # Give every side variable both polarities many times, so that x is
        # the only variable whose elimination is even close to the bound
        sides = [f"{prefix}{i}" for prefix in "pqrs" for i in range(3)]
        for first in sides:
            for second in sides:
                if first < second:
                    clauses.append(Clause([Literal(first, True), Literal(second, True)]))
                    clauses.append(Clause([Literal(first), Literal(second)]))
        cnf = CNFExpression(clauses)

        def x_survives(max_growth):
            simplifier = Simplifier(cnf)
            simplifier.eliminate_variables(max_occurrences=3, max_growth=max_growth)
            return 'x' in simplifier.result().cnf.get_variables()

        self.assertTrue(x_survives(max_growth=0))       # 9 > 6
        self.assertTrue(x_survives(max_growth=2))       # 9 > 8
        self.assertFalse(x_survives(max_growth=3))      # 9 <= 9

    def test_empty_resolvent_means_unsat(self):
        cnf = CNFExpression.parse("x & ~x")
        simplifier = Simplifier(cnf)
        simplifier.eliminate_variables()
        self.assertTrue(simplifier.result().is_unsat)

    def test_never_increases_clause_count(self):
        rng = random.Random(13)
        for _ in range(300):
            cnf = random_cnf(rng)
            simplifier = Simplifier(cnf)
            before = len(simplifier.clauses)
            simplifier.eliminate_variables(max_growth=0)
            self.assertLessEqual(len(simplifier.clauses), before)


class TestVivification(unittest.TestCase):

    def test_clause_is_shortened(self):
        # ¬a implies b, so in (a | b | c) the literal c is unnecessary
        cnf = CNFExpression.parse("(a | b) & (a | b | c)")
        simplifier = Simplifier(cnf)
        self.assertGreaterEqual(simplifier.vivify(), 1)
        self.assertEqual(clause_sets(simplifier.result().cnf),
                         clause_sets(CNFExpression.parse("(a | b)")))

    def test_redundant_literal_is_dropped(self):
        # ¬a implies ¬b, so b can be removed from (a | b | c)
        cnf = CNFExpression.parse("(a | ~b) & (a | b | c)")
        simplifier = Simplifier(cnf)
        simplifier.vivify()
        self.assertIn(frozenset({('a', False), ('c', False)}),
                      clause_sets(simplifier.result().cnf))

    def test_preserves_all_models(self):
        rng = random.Random(17)
        shortened = 0
        for _ in range(500):
            cnf = random_cnf(rng, max_vars=8)
            variables = sorted(cnf.get_variables())
            simplifier = Simplifier(cnf)
            shortened += simplifier.vivify()
            result = simplifier.result()
            self.assertEqual(all_models(cnf, variables), all_models(result.cnf, variables), str(cnf))
        self.assertGreater(shortened, 50)


class TestReconstruction(unittest.TestCase):
    """Any sequence of techniques must be undone correctly."""

    TECHNIQUES = ['probe_failed_literals', 'eliminate_blocked_clauses',
                  'eliminate_variables', 'vivify']

    def test_every_reduced_model_extends_to_original(self):
        rng = random.Random(7)
        extended = 0
        for _ in range(1500):
            cnf = random_cnf(rng)
            reference = solve_sat(cnf)

            simplifier = Simplifier(cnf)
            for _ in range(rng.randint(1, 5)):
                technique = rng.choice(self.TECHNIQUES)
                if technique == 'eliminate_variables':
                    simplifier.eliminate_variables(max_occurrences=rng.choice([2, 10]),
                                                   max_growth=rng.choice([0, 0, 3]))
                else:
                    getattr(simplifier, technique)()
            result = simplifier.result()

            # Simplification preserves satisfiability
            self.assertEqual(solve_sat(result.cnf) is None, reference is None, str(cnf))
            if result.is_unsat:
                self.assertIsNone(reference)
                continue

            # Every model of the simplified formula, not just one, must extend
            reduced_vars = sorted(result.cnf.get_variables())
            for bits in all_models(result.cnf, reduced_vars):
                full = result.extend_model(dict(zip(reduced_vars, bits)))
                self.assertTrue(cnf.evaluate(full), str(cnf))
                self.assertEqual(set(full), set(cnf.get_variables()))
                extended += 1
        self.assertGreater(extended, 1000)


class TestConvenienceFunctions(unittest.TestCase):

    def test_simplify_cnf_reports_statistics(self):
        cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")
        result = simplify_cnf(cnf)
        self.assertEqual(result.stats.original_clauses, 4)
        self.assertEqual(result.stats.original_vars, 4)
        self.assertLess(result.stats.final_clauses, 4)

    def test_techniques_can_be_disabled(self):
        cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")
        result = simplify_cnf(cnf, probing=False, blocked_clauses=False,
                              variable_elimination=False, vivification=False)
        self.assertEqual(clause_sets(result.cnf), clause_sets(cnf))

    def test_input_is_not_modified(self):
        cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")
        before = str(cnf)
        simplify_cnf(cnf)
        self.assertEqual(str(cnf), before)

    def test_solve_with_simplification(self):
        rng = random.Random(19)
        for _ in range(500):
            cnf = random_cnf(rng, max_vars=12)
            reference = solve_sat(cnf)
            result = solve_with_simplification(cnf)
            self.assertEqual(result is None, reference is None, str(cnf))
            if result is not None:
                self.assertTrue(cnf.evaluate(result), str(cnf))

    def test_custom_solver(self):
        cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (~b | c) & (~c | ~a)")
        result = solve_with_simplification(cnf, solver=solve_cdcl)
        self.assertTrue(cnf.evaluate(result))

    def test_unsat(self):
        cnf = CNFExpression.parse("(a | b) & (~a | b) & (a | ~b) & (~a | ~b)")
        self.assertIsNone(solve_with_simplification(cnf))
        self.assertTrue(simplify_cnf(cnf).is_unsat or solve_sat(simplify_cnf(cnf).cnf) is None)


if __name__ == '__main__':
    unittest.main()
