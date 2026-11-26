"""
Tests for the modern CDCL solver (watched literals, minimization, LBD, restarts).
"""

import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from bsat import (
    CNFExpression, Clause, Literal,
    ModernCDCLSolver, solve_modern_cdcl, get_modern_cdcl_stats,
    solve_sat, solve_cdcl,
)
from benchmarks import pigeon_hole, phase_transition_3sat


def random_cnf(rng, max_vars=14, max_len=4):
    n = rng.randint(1, max_vars)
    variables = [f"v{i}" for i in range(n)]
    return CNFExpression([
        Clause([Literal(rng.choice(variables), rng.random() < 0.5)
                for _ in range(rng.randint(1, max_len))])
        for _ in range(rng.randint(0, n * 5))
    ])


# Option sets chosen so that restarts and database reductions actually happen
# on small formulas
CONFIGURATIONS = [
    {},
    {"restart_strategy": "luby", "restart_base": 2},
    {"restart_strategy": "none"},
    {"minimize_learned": False},
    {"phase_saving": False, "initial_phase": True},
    {"reduce_interval": 5, "reduce_increment": 1, "glucose_window": 3},
    {"reduce_interval": 2, "reduce_increment": 0, "glue_limit": 0, "glucose_window": 2},
    {"random_phase_freq": 0.3, "seed": 3, "restart_strategy": "luby", "restart_base": 1},
]


class TestModernCDCLBasics(unittest.TestCase):
    """Basic behaviour and edge cases."""

    def test_simple_sat(self):
        cnf = CNFExpression.parse("(x | y) & (~x | z) & (~y | ~z)")
        result = solve_modern_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_simple_unsat(self):
        cnf = CNFExpression.parse("(x | y) & (~x | y) & (x | ~y) & (~x | ~y)")
        result, stats = get_modern_cdcl_stats(cnf)
        self.assertIsNone(result)
        self.assertEqual(stats.status, "UNSAT")

    def test_empty_formula(self):
        solver = ModernCDCLSolver(CNFExpression([]))
        self.assertEqual(solver.solve(), {})
        self.assertEqual(solver.stats.status, "SAT")

    def test_empty_clause(self):
        cnf = CNFExpression([Clause([Literal('x')]), Clause([])])
        self.assertIsNone(solve_modern_cdcl(cnf))

    def test_contradicting_units(self):
        cnf = CNFExpression.parse("x & ~x")
        self.assertIsNone(solve_modern_cdcl(cnf))

    def test_unit_propagation_only(self):
        cnf = CNFExpression.parse("a & (~a | b) & (~b | c) & (~c | d)")
        result, stats = get_modern_cdcl_stats(cnf)
        self.assertEqual(result, {'a': True, 'b': True, 'c': True, 'd': True})
        self.assertEqual(stats.decisions, 0)
        self.assertEqual(stats.conflicts, 0)

    def test_tautology_and_duplicate_literals(self):
        cnf = CNFExpression([
            Clause([Literal('x'), Literal('x', True), Literal('y')]),   # tautology
            Clause([Literal('y', True), Literal('y', True)]),           # ¬y twice
            Clause([Literal('z'), Literal('z'), Literal('y')]),
        ])
        result = solve_modern_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))
        self.assertEqual(set(result), {'x', 'y', 'z'})

    def test_all_variables_assigned(self):
        cnf = CNFExpression.parse("(a | b | c) & (d | e)")
        result = solve_modern_cdcl(cnf)
        self.assertEqual(set(result), {'a', 'b', 'c', 'd', 'e'})

    def test_conflict_limit(self):
        solver = ModernCDCLSolver(pigeon_hole(7))
        self.assertIsNone(solver.solve(max_conflicts=10))
        self.assertEqual(solver.stats.status, "UNKNOWN")
        self.assertEqual(solver.stats.conflicts, 10)

    def test_invalid_restart_strategy(self):
        with self.assertRaises(ValueError):
            ModernCDCLSolver(CNFExpression.parse("(a | b)"), restart_strategy="sometimes")

    def test_invalid_glucose_window(self):
        with self.assertRaises(ValueError):
            ModernCDCLSolver(CNFExpression.parse("(a | b)"), glucose_window=0)

    def test_deterministic(self):
        cnf = phase_transition_3sat(50, seed=4)
        first, stats1 = get_modern_cdcl_stats(cnf)
        second, stats2 = get_modern_cdcl_stats(cnf)
        self.assertEqual(first, second)
        self.assertEqual(stats1.conflicts, stats2.conflicts)
        self.assertEqual(stats1.decisions, stats2.decisions)


class TestModernCDCLCorrectness(unittest.TestCase):
    """Cross-checks against the other solvers in every configuration."""

    def test_matches_dpll_on_random_formulas(self):
        rng = random.Random(1)
        for _ in range(1500):
            cnf = random_cnf(rng)
            reference = solve_sat(cnf)
            for options in CONFIGURATIONS:
                solver = ModernCDCLSolver(cnf, **options)
                result = solver.solve()
                self.assertEqual(result is None, reference is None, f"{options} {cnf}")
                if result is not None:
                    self.assertTrue(cnf.evaluate(result), f"{options} {cnf}")
                    self.assertEqual(set(result), set(cnf.get_variables()))
                self.assertEqual(solver.stats.status,
                                 "UNSAT" if result is None else "SAT")

    def test_matches_cdcl_on_threshold_3sat(self):
        for seed in range(12):
            cnf = phase_transition_3sat(50, seed=seed)
            reference = solve_cdcl(cnf)
            for options in CONFIGURATIONS:
                result = solve_modern_cdcl(cnf, **options)
                self.assertEqual(result is None, reference is None, f"seed {seed} {options}")
                if result is not None:
                    self.assertTrue(cnf.evaluate(result))

    def test_pigeon_hole_unsat(self):
        for options in CONFIGURATIONS:
            self.assertIsNone(solve_modern_cdcl(pigeon_hole(6), **options), str(options))


class TestWatchedLiterals(unittest.TestCase):
    """The two-watched-literal invariant."""

    def test_every_clause_is_watched_twice(self):
        cnf = phase_transition_3sat(30, seed=2)
        solver = ModernCDCLSolver(cnf)
        solver.solve()

        watch_count = {}
        for lit, watchers in solver.watches.items():
            for _, clause in watchers:
                if clause.deleted:
                    continue
                self.assertIn(lit, clause.lits[:2], "watched literal must be in position 0 or 1")
                watch_count[id(clause)] = watch_count.get(id(clause), 0) + 1
        for clause in solver.clauses + solver.learned:
            self.assertEqual(watch_count.get(id(clause)), 2)

    def test_invariant_holds_in_final_model(self):
        # In a complete assignment a false watched literal is only allowed if
        # the other watched literal is true
        cnf = phase_transition_3sat(40, seed=4)
        solver = ModernCDCLSolver(cnf)
        if solver.solve() is None:
            self.skipTest("instance is UNSAT")
        for clause in solver.clauses + solver.learned:
            first, second = clause.lits[0], clause.lits[1]
            self.assertTrue(solver._val(first) == 1 or solver._val(second) == 1)

    def test_unwatched_literals_cost_nothing(self):
        # Twenty literals of a long clause become false, but none of them is
        # one of the two watched literals, so the clause is never visited
        variables = [f"v{i:02d}" for i in range(30)]
        long_clause = Clause([Literal(v) for v in variables])
        units = [Clause([Literal(v, True)]) for v in variables[5:25]]
        solver = ModernCDCLSolver(CNFExpression([long_clause] + units))
        self.assertIsNone(solver._propagate())
        self.assertEqual(len(solver.trail), 20)
        self.assertEqual(solver.stats.clause_visits, 0)


class TestClauseLearning(unittest.TestCase):
    """Learned clauses, minimization and LBD."""

    def test_learned_clauses_are_implied(self):
        # Adding the learned clauses to the formula must not change its models
        checked = 0
        for seed in range(15):
            cnf = phase_transition_3sat(20, seed=seed)
            solver = ModernCDCLSolver(cnf, reduce_db=False)
            solver.solve()
            for clause in solver.learned:
                # cnf ∧ ¬clause must be unsatisfiable
                negation = [Clause([Literal(solver.names[abs(l) - 1], l > 0)]) for l in clause.lits]
                self.assertIsNone(solve_sat(CNFExpression(list(cnf.clauses) + negation)))
                checked += 1
        self.assertGreater(checked, 50)

    def test_minimization_removes_literals(self):
        cnf = pigeon_hole(7)
        _, with_min = get_modern_cdcl_stats(cnf)
        _, without_min = get_modern_cdcl_stats(cnf, minimize_learned=False)
        self.assertGreater(with_min.minimized_literals, 0)
        self.assertEqual(without_min.minimized_literals, 0)
        self.assertLess(with_min.learned_literals / with_min.learned_clauses,
                        without_min.learned_literals / without_min.learned_clauses)

    def test_lbd_is_number_of_decision_levels(self):
        solver = ModernCDCLSolver(phase_transition_3sat(40, seed=5), reduce_db=False)
        solver.solve()
        self.assertTrue(solver.learned)
        for clause in solver.learned:
            self.assertGreaterEqual(clause.lbd, 1)
            self.assertLessEqual(clause.lbd, len(clause.lits))


class TestDatabaseReduction(unittest.TestCase):
    """LBD-based deletion of learned clauses."""

    def test_reduction_deletes_clauses(self):
        _, stats = get_modern_cdcl_stats(pigeon_hole(7), reduce_interval=100, reduce_increment=0)
        self.assertGreater(stats.reductions, 0)
        self.assertGreater(stats.deleted_clauses, 0)

    def test_reduction_can_be_disabled(self):
        _, stats = get_modern_cdcl_stats(pigeon_hole(7), reduce_db=False)
        self.assertEqual(stats.deleted_clauses, 0)

    def test_glue_clauses_survive(self):
        solver = ModernCDCLSolver(pigeon_hole(7), reduce_interval=50, reduce_increment=0)
        deleted_lbds = []
        original = solver._reduce_database

        def recording_reduce():
            before = list(solver.learned)
            original()
            deleted_lbds.extend(c.lbd for c in before if c.deleted)

        solver._reduce_database = recording_reduce
        solver.solve()
        self.assertTrue(deleted_lbds)
        self.assertTrue(all(lbd > solver.glue_limit for lbd in deleted_lbds))


class TestRestarts(unittest.TestCase):
    """Restart strategies."""

    def test_luby_sequence(self):
        self.assertEqual([ModernCDCLSolver._luby(i) for i in range(1, 16)],
                         [1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8])

    def test_no_restarts(self):
        _, stats = get_modern_cdcl_stats(pigeon_hole(7), restart_strategy="none")
        self.assertEqual(stats.restarts, 0)

    def test_luby_restarts_follow_the_sequence(self):
        base = 4
        solver = ModernCDCLSolver(pigeon_hole(7), restart_strategy="luby", restart_base=base)
        points = []
        original = solver._restart

        def recording_restart():
            points.append(solver.stats.conflicts)
            original()

        solver._restart = recording_restart
        solver.solve()
        self.assertGreaterEqual(len(points), 6)
        intervals = [b - a for a, b in zip([0] + points, points)]
        # A restart waits until the solver is not in the middle of a chain of
        # conflicts, so an interval can run slightly over but never under
        for k, interval in enumerate(intervals, start=1):
            self.assertGreaterEqual(interval, ModernCDCLSolver._luby(k) * base)
        self.assertLess(sum(intervals),
                        2 * sum(ModernCDCLSolver._luby(k) * base
                                for k in range(1, len(points) + 1)))

    def test_glucose_restarts_happen(self):
        _, stats = get_modern_cdcl_stats(pigeon_hole(8))
        self.assertGreater(stats.restarts, 0)


class TestPhaseSaving(unittest.TestCase):
    """Phase saving and initial phase."""

    def test_initial_phase_is_used_for_first_decision(self):
        cnf = CNFExpression.parse("(a | b) & (c | d)")
        self.assertFalse(solve_modern_cdcl(cnf, initial_phase=False)['a'])
        self.assertTrue(all(solve_modern_cdcl(cnf, initial_phase=True).values()))

    def test_phases_are_saved_on_backtrack(self):
        solver = ModernCDCLSolver(phase_transition_3sat(40, seed=6), initial_phase=False)
        solver.solve()
        self.assertTrue(any(solver.saved_phase[1:]))

    def test_phases_not_saved_when_disabled(self):
        solver = ModernCDCLSolver(phase_transition_3sat(40, seed=6),
                                  phase_saving=False, initial_phase=False)
        solver.solve()
        self.assertFalse(any(solver.saved_phase[1:]))


if __name__ == '__main__':
    unittest.main()
