"""
Tests for the p-bit (probabilistic bit) SAT solver
"""

import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from bsat import CNFExpression, Clause, Literal
from bsat.pbit import PBitSolver, solve_pbit, get_pbit_stats, PBitStats


class TestPBitBasic(unittest.TestCase):
    """Basic tests for the p-bit solver."""

    def test_simple_sat(self):
        """Test on simple satisfiable formula."""
        cnf = CNFExpression.parse("(x | y) & (~x | y) & (x | ~y)")
        result = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_unit_clauses(self):
        """Test formula with unit clauses forcing assignments."""
        cnf = CNFExpression.parse("(x) & (~y) & (x | y | z)")
        result = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))
        self.assertTrue(result['x'])
        self.assertFalse(result['y'])

    def test_3sat_formula(self):
        """Test on a 3SAT formula."""
        cnf = CNFExpression.parse(
            "(a | b | c) & (~a | b | ~c) & (a | ~b | c) & (~a | ~b | ~c)"
        )
        result = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_reproducibility(self):
        """Same seed must give the same solution across runs."""
        cnf = CNFExpression.parse(
            "(a | b | c) & (~a | b) & (a | ~c) & (~b | c) & (b | ~a | ~c)"
        )
        result1 = solve_pbit(cnf, seed=42)
        result2 = solve_pbit(cnf, seed=42)
        self.assertEqual(result1, result2)

    def test_empty_formula(self):
        """Empty formula is trivially satisfiable with empty assignment."""
        cnf = CNFExpression([])
        result = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertEqual(result, {})

    def test_already_satisfied(self):
        """A single tautology-heavy formula should solve immediately."""
        cnf = CNFExpression.parse("(x | ~x) & (y | ~y)")
        result = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))


class TestPBitUNSAT(unittest.TestCase):
    """UNSAT behavior — incomplete solver returns None (not a proof)."""

    def test_simple_unsat(self):
        """(x) ∧ (¬x) is unsatisfiable — solver must give up with None."""
        cnf = CNFExpression.parse("(x) & (~x)")
        result = solve_pbit(cnf, sweeps=50, restarts=3, seed=42)
        # None does not prove UNSAT, but this instance has no solution
        self.assertIsNone(result)

    def test_unsat_3sat(self):
        """All 8 sign patterns over 3 variables — unsatisfiable."""
        clauses = []
        for i in range(8):
            clauses.append(Clause([
                Literal('a', bool(i & 1)),
                Literal('b', bool(i & 2)),
                Literal('c', bool(i & 4)),
            ]))
        cnf = CNFExpression(clauses)
        result = solve_pbit(cnf, sweeps=50, restarts=3, seed=42)
        self.assertIsNone(result)

    def test_empty_clause(self):
        """A formula containing an empty clause is UNSAT immediately."""
        cnf = CNFExpression([Clause([Literal('x')]), Clause([])])
        solver = PBitSolver(cnf, seed=42)
        result = solver.solve()
        self.assertIsNone(result)
        # Early exit: no sweeps should have been spent
        self.assertEqual(solver.get_stats().sweeps, 0)


class TestPBitStatistics(unittest.TestCase):
    """Statistics tracking."""

    def test_stats_tracking(self):
        """Counters are consistent on a successful solve."""
        cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c)")
        solution, stats = get_pbit_stats(cnf, seed=42)
        self.assertIsNotNone(solution)
        self.assertTrue(cnf.evaluate(solution))
        self.assertGreaterEqual(stats.restarts, 1)
        self.assertGreaterEqual(stats.updates, stats.flips)
        self.assertEqual(stats.best_energy, 0)
        self.assertEqual(stats.final_energy, 0)

    def test_stats_on_failure(self):
        """On failure, best energy stays positive and final energy is set."""
        cnf = CNFExpression.parse("(x) & (~x)")
        solution, stats = get_pbit_stats(cnf, sweeps=20, restarts=2, seed=42)
        self.assertIsNone(solution)
        self.assertGreater(stats.best_energy, 0)
        self.assertGreater(stats.final_energy, 0)

    def test_energy_history_recorded(self):
        """Energy history is recorded per sweep for the last restart."""
        cnf = CNFExpression.parse("(x) & (~x)")
        solution, stats = get_pbit_stats(cnf, sweeps=20, restarts=2, seed=42)
        self.assertIsNone(solution)
        self.assertEqual(len(stats.energy_history), 20)
        self.assertEqual(stats.energy_history[-1], stats.final_energy)

    def test_str_representation(self):
        """Stats have a readable multi-line string form."""
        cnf = CNFExpression.parse("(a | b) & (~a | b)")
        _, stats = get_pbit_stats(cnf, seed=42)
        text = str(stats)
        self.assertIn("PBitStats", text)
        self.assertIn("Acceptance rate", text)
        self.assertIn("Best energy", text)


class TestPBitParameters(unittest.TestCase):
    """Parameter handling."""

    def setUp(self):
        self.cnf = CNFExpression.parse(
            "(a | b | c) & (~a | b) & (a | ~c) & (~b | c)"
        )

    def test_geometric_schedule(self):
        result = solve_pbit(self.cnf, schedule='geometric', seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(self.cnf.evaluate(result))

    def test_linear_schedule(self):
        result = solve_pbit(self.cnf, schedule='linear', seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(self.cnf.evaluate(result))

    def test_invalid_schedule_raises(self):
        with self.assertRaises(ValueError):
            solve_pbit(self.cnf, schedule='exponential', seed=42)

    def test_constant_beta(self):
        """beta_min == beta_max gives constant-temperature Gibbs sampling."""
        result = solve_pbit(self.cnf, beta_min=2.0, beta_max=2.0, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(self.cnf.evaluate(result))

    def test_restart_and_sweep_limits(self):
        """On an UNSAT instance the full budget is spent, no more."""
        cnf = CNFExpression.parse("(x) & (~x)")
        _, stats = get_pbit_stats(cnf, sweeps=15, restarts=4, seed=42)
        self.assertEqual(stats.restarts, 4)
        self.assertEqual(stats.sweeps, 15 * 4)


class TestPBitEdgeCases(unittest.TestCase):
    """Edge cases."""

    def test_single_variable(self):
        cnf = CNFExpression.parse("(x)")
        result = solve_pbit(cnf, seed=42)
        self.assertEqual(result, {'x': True})

    def test_many_clauses(self):
        """Many overlapping clauses on few variables."""
        clauses = []
        for i in range(50):
            clauses.append(Clause([
                Literal('a', i % 2 == 0),
                Literal('b', i % 3 == 0),
                Literal('c', i % 5 == 0),
                Literal('d', i % 7 == 0),
            ]))
        cnf = CNFExpression(clauses)
        result = solve_pbit(cnf, seed=42)
        if result is not None:
            self.assertTrue(cnf.evaluate(result))

    def test_large_clauses(self):
        """5-SAT clauses — arbitrary clause lengths are supported directly."""
        cnf = CNFExpression.parse(
            "(a | b | c | d | e) & (~a | ~b | c | d | e) & (a | ~c | ~d | e | ~b)"
        )
        result = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_repeated_variable_in_clause(self):
        """Clauses with a repeated literal still solve correctly."""
        cnf = CNFExpression([
            Clause([Literal('x'), Literal('x')]),
            Clause([Literal('x', True), Literal('y')]),
        ])
        result = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))


class TestPBitTheory(unittest.TestCase):
    """Tests grounded in the algorithm's theory."""

    def test_high_beta_is_greedy(self):
        """At very high β the sampler is greedy: almost no uphill flips.

        On an easy instance greedy descent should reach energy 0 while
        accepting few flips relative to updates evaluated.
        """
        cnf = CNFExpression.parse("(a | b) & (~a | b) & (a | ~b)")
        solution, stats = get_pbit_stats(cnf, beta_min=20.0, beta_max=20.0, seed=42)
        self.assertIsNotNone(solution)
        self.assertTrue(cnf.evaluate(solution))
        self.assertEqual(stats.best_energy, 0)

    def test_agrees_with_dpll(self):
        """On satisfiable instances p-bit finds a valid solution like DPLL."""
        from bsat import solve_sat
        cnf = CNFExpression.parse(
            "(a | b | c) & (~a | ~b) & (b | ~c) & (a | c)"
        )
        dpll = solve_sat(cnf)
        self.assertIsNotNone(dpll)
        pbit = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(pbit)
        self.assertTrue(cnf.evaluate(pbit))

    def test_agrees_with_cdcl(self):
        """On satisfiable instances p-bit finds a valid solution like CDCL."""
        from bsat import solve_cdcl
        cnf = CNFExpression.parse(
            "(p | q | r) & (~p | q) & (p | ~r) & (~q | r)"
        )
        cdcl = solve_cdcl(cnf)
        self.assertIsNotNone(cdcl)
        pbit = solve_pbit(cnf, seed=42)
        self.assertIsNotNone(pbit)
        self.assertTrue(cnf.evaluate(pbit))

    def test_temperature_controls_acceptance(self):
        """Gibbs sampling: acceptance is high at low β (hot, random) and
        low at high β (cold, greedy). Measured on an UNSAT instance whose
        energy landscape has a gradient — at the minimum (x=True, energy 1)
        a flip costs ΔE=+1, which cold sampling rejects — so neither run
        terminates early."""
        cnf = CNFExpression([
            Clause([Literal('x')]),
            Clause([Literal('x')]),
            Clause([Literal('x', True)]),
        ])
        _, hot = get_pbit_stats(cnf, sweeps=200, restarts=1,
                                beta_min=0.05, beta_max=0.05, seed=42)
        _, cold = get_pbit_stats(cnf, sweeps=200, restarts=1,
                                 beta_min=8.0, beta_max=8.0, seed=42)
        hot_rate = hot.flips / hot.updates
        cold_rate = cold.flips / cold.updates
        self.assertGreater(hot_rate, cold_rate)

    def test_annealed_schedule_solves(self):
        """An annealed run reaches the ground state (energy 0) on a
        satisfiable instance."""
        cnf = CNFExpression.parse(
            "(a | b | c) & (~a | b) & (a | ~c) & (~b | c) & (b | ~a | ~c)"
        )
        _, annealed = get_pbit_stats(cnf, sweeps=100, restarts=1,
                                     beta_min=0.05, beta_max=6.0, seed=42)
        self.assertEqual(annealed.best_energy, 0)


if __name__ == '__main__':
    unittest.main()
