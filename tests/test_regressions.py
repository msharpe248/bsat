"""
Regression tests for previously fixed bugs.

Each test class pins down one bug that once produced a wrong answer, an
invalid model, or a crash. Several use randomized cross-checks against an
independent method (brute force or another solver) with fixed seeds.
"""

import itertools
import random
import sys
import unittest
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from bsat import (
    CNFExpression, Clause, Literal,
    DPLLSolver, CDCLSolver,
    solve_sat, solve_cdcl, solve_2sat, solve_horn_sat, solve_davis_putnam,
    find_all_sat_solutions, count_sat_solutions,
    parse_dimacs, decompose_and_preprocess, preprocess_cnf, get_schoening_stats,
)


def random_cnf(rng, max_vars=7, max_len=3, ratio=4.0):
    """Small random CNF with clauses of 1..max_len literals."""
    n = rng.randint(1, max_vars)
    variables = [f"v{i}" for i in range(n)]
    clauses = []
    for _ in range(rng.randint(0, int(n * ratio))):
        size = rng.randint(1, max_len)
        clauses.append(Clause([
            Literal(rng.choice(variables), rng.random() < 0.5) for _ in range(size)
        ]))
    return CNFExpression(clauses)


def brute_force_count(cnf):
    """Count satisfying assignments by trying all of them."""
    variables = sorted(cnf.get_variables())
    return sum(
        cnf.evaluate(dict(zip(variables, bits)))
        for bits in itertools.product([False, True], repeat=len(variables))
    )


class TestSolutionEnumeration(unittest.TestCase):
    """Enumeration used to apply pure literal elimination and skip solutions."""

    def test_simple_disjunction_has_three_solutions(self):
        cnf = CNFExpression.parse("(x | y)")
        self.assertEqual(len(find_all_sat_solutions(cnf)), 3)
        self.assertEqual(count_sat_solutions(cnf), 3)
        self.assertEqual(len(DPLLSolver(cnf).find_all_solutions()), 3)

    def test_matches_brute_force(self):
        rng = random.Random(1)
        for _ in range(300):
            cnf = random_cnf(rng, max_vars=6)
            solutions = find_all_sat_solutions(cnf)
            self.assertEqual(len(solutions), brute_force_count(cnf), str(cnf))
            self.assertTrue(all(cnf.evaluate(s) for s in solutions))
            distinct = {tuple(sorted(s.items())) for s in solutions}
            self.assertEqual(len(distinct), len(solutions))

    def test_max_solutions_limit(self):
        cnf = CNFExpression.parse("(a | b | c)")
        self.assertEqual(len(find_all_sat_solutions(cnf, max_solutions=4)), 4)


class TestDavisPutnamModels(unittest.TestCase):
    """Davis-Putnam used to return assignments that did not satisfy the formula."""

    def test_models_satisfy_formula(self):
        rng = random.Random(3)
        for _ in range(500):
            cnf = random_cnf(rng, max_vars=9, max_len=4, ratio=4.3)
            result = solve_davis_putnam(cnf)
            reference = solve_sat(cnf)
            self.assertEqual(result is None, reference is None, str(cnf))
            if result is not None:
                self.assertTrue(cnf.evaluate(result), str(cnf))
                self.assertEqual(set(result), set(cnf.get_variables()))

    def test_tautological_clause(self):
        cnf = CNFExpression.parse("(x | ~x | a) & (~x | b) & (~a) & (~b | a)")
        result = solve_davis_putnam(cnf)
        self.assertEqual(result is None, solve_sat(cnf) is None)
        if result is not None:
            self.assertTrue(cnf.evaluate(result))


class TestDecomposeUnsatComponent(unittest.TestCase):
    """decompose_and_preprocess used to drop components it proved UNSAT."""

    def test_unsat_component_is_reported(self):
        cnf = CNFExpression.parse("a & ~a & (c | d) & (~c | d)")
        components, _, _ = decompose_and_preprocess(cnf)
        self.assertTrue(any(solve_sat(comp) is None for comp in components))

    def test_solving_components_matches_direct_solve(self):
        rng = random.Random(5)
        for _ in range(300):
            cnf = random_cnf(rng, max_vars=8)
            reference = solve_cdcl(cnf)
            components, forced, _ = decompose_and_preprocess(cnf)

            solution = dict(forced)
            satisfiable = True
            for comp in components:
                sol = solve_sat(comp)
                if sol is None:
                    satisfiable = False
                    break
                solution.update(sol)

            self.assertEqual(satisfiable, reference is not None, str(cnf))
            if satisfiable:
                full = {v: solution.get(v, False) for v in cnf.get_variables()}
                self.assertTrue(cnf.evaluate(full), str(cnf))


class TestSelfSubsumption(unittest.TestCase):
    """Self-subsumption replaced (¬a ∨ D) by C instead of D, losing solutions."""

    def test_strengthened_clause_keeps_its_other_literals(self):
        # (¬a ∨ b) strengthens (a ∨ b ∨ c) to (b ∨ c), not to (b)
        cnf = CNFExpression.parse("(a | b | c) & (~a | b)")
        result = preprocess_cnf(cnf, unit_propagation=False, pure_literal=False,
                                subsumption=False, self_subsumption=True)
        clauses = {frozenset((l.variable, l.negated) for l in c.literals)
                   for c in result.simplified.clauses}
        self.assertIn(frozenset({('b', False), ('c', False)}), clauses)

    def test_satisfiable_formula_not_reported_unsat(self):
        cnf = CNFExpression.parse(
            "(~a | ~d) & (~d | ~c) & (~c | d) & (c | d | a) & (~d | ~b) & (~d | b)"
        )
        self.assertIsNotNone(solve_sat(cnf))
        self.assertIsNot(preprocess_cnf(cnf).is_sat, False)

    def test_preprocessing_matches_direct_solve(self):
        rng = random.Random(3)
        for _ in range(2000):
            cnf = random_cnf(rng, max_vars=6)
            reference = solve_sat(cnf)
            result = preprocess_cnf(cnf)
            if result.is_sat is False:
                self.assertIsNone(reference, str(cnf))
                continue
            solution = solve_sat(result.simplified)
            self.assertEqual(solution is None, reference is None, str(cnf))
            if solution is not None:
                full = {v: False for v in cnf.get_variables()}
                full.update(solution)
                full.update(result.assignments)
                self.assertTrue(cnf.evaluate(full), str(cnf))


class TestDimacsParsing(unittest.TestCase):
    """The parser used to drop empty clauses and reject common file layouts."""

    def test_empty_clause_is_kept(self):
        cnf = parse_dimacs("p cnf 2 2\n1 2 0\n0\n")
        self.assertEqual(len(cnf.clauses), 2)
        self.assertIsNone(solve_sat(cnf))

    def test_clause_spanning_lines(self):
        cnf = parse_dimacs("p cnf 3 2\n1 -2\n3 0\n-1 0\n")
        self.assertEqual(len(cnf.clauses), 2)
        self.assertEqual(len(cnf.clauses[0].literals), 3)

    def test_satlib_trailer(self):
        cnf = parse_dimacs("c SATLIB style\np cnf 3 2\n 1 -2 3 0\n-1 2 0\n%\n0\n")
        self.assertEqual(len(cnf.clauses), 2)
        self.assertIsNotNone(solve_sat(cnf))


class TestHornSat(unittest.TestCase):
    """Horn-SAT was a quadratic rescan loop; it is now counter-based."""

    def test_matches_dpll(self):
        rng = random.Random(7)
        for _ in range(500):
            n = rng.randint(1, 8)
            variables = [f"v{i}" for i in range(n)]
            clauses = []
            for _ in range(rng.randint(0, n * 3)):
                body = rng.sample(variables, rng.randint(0, min(3, n)))
                literals = [Literal(v, True) for v in body]
                if rng.random() < 0.7 or not literals:
                    literals.append(Literal(rng.choice(variables)))
                clauses.append(Clause(literals))
            cnf = CNFExpression(clauses)

            result = solve_horn_sat(cnf)
            self.assertEqual(result is None, solve_sat(cnf) is None, str(cnf))
            if result is not None:
                self.assertTrue(cnf.evaluate(result), str(cnf))

    def test_long_chain_in_reverse_order(self):
        # Worst case for a rescan loop: each pass would discover one variable
        n = 20000
        clauses = [Clause([Literal(f"v{i}", True), Literal(f"v{i+1}")])
                   for i in reversed(range(n))]
        clauses.append(Clause([Literal("v0")]))
        result = solve_horn_sat(CNFExpression(clauses))
        self.assertTrue(all(result.values()))


class TestTwoSat(unittest.TestCase):
    """2SAT used recursive DFS (recursion limit) and varied between runs."""

    def test_long_implication_chain(self):
        n = 20000
        cnf = CNFExpression([
            Clause([Literal(f"v{i}", True), Literal(f"v{i+1}")]) for i in range(n)
        ])
        result = solve_2sat(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_matches_dpll(self):
        rng = random.Random(11)
        for _ in range(500):
            n = rng.randint(1, 8)
            variables = [f"v{i}" for i in range(n)]
            cnf = CNFExpression([
                Clause([Literal(rng.choice(variables), rng.random() < 0.5),
                        Literal(rng.choice(variables), rng.random() < 0.5)])
                for _ in range(rng.randint(1, n * 3))
            ])
            result = solve_2sat(cnf)
            self.assertEqual(result is None, solve_sat(cnf) is None, str(cnf))
            if result is not None:
                self.assertTrue(cnf.evaluate(result), str(cnf))


class TestCDCLRestarts(unittest.TestCase):
    """Restart thresholds were absolute conflict counts, so restarts bunched up."""

    def test_luby_sequence(self):
        solver = CDCLSolver(CNFExpression.parse("(a | b)"))
        self.assertEqual([solver._luby(i) for i in range(1, 16)],
                         [1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8])

    def test_intervals_measured_from_previous_restart(self):
        rng = random.Random(2)
        variables = [f"v{i}" for i in range(60)]
        cnf = CNFExpression([
            Clause([Literal(v, rng.random() < 0.5) for v in rng.sample(variables, 3)])
            for _ in range(256)
        ])
        base = 5
        solver = CDCLSolver(cnf, restart_base=base)

        restart_points = []
        original_restart = solver._restart

        def recording_restart():
            restart_points.append(solver.stats.conflicts)
            original_restart()

        solver._restart = recording_restart
        solver.solve()

        self.assertGreaterEqual(len(restart_points), 4)
        intervals = [b - a for a, b in zip([0] + restart_points, restart_points)]
        for k, interval in enumerate(intervals, start=1):
            # Never earlier than the Luby interval
            self.assertGreaterEqual(interval, solver._luby(k) * base)


class TestSchoeningReproducibility(unittest.TestCase):
    """Variable order came from a set, so a seed gave different runs per process."""

    def test_variables_are_sorted(self):
        from bsat.schoening import SchoeningSolver
        cnf = CNFExpression.parse("(zeta | alpha | mid) & (~alpha | beta | ~zeta)")
        solver = SchoeningSolver(cnf, seed=1)
        self.assertEqual(solver.variables, sorted(solver.variables))

    def test_same_seed_same_result(self):
        cnf = CNFExpression.parse(
            "(a | b | c) & (~a | b | ~c) & (a | ~b | c) & (~a | ~b | d) & (~d | e | a)"
        )
        first, stats1 = get_schoening_stats(cnf, seed=42)
        second, stats2 = get_schoening_stats(cnf, seed=42)
        self.assertEqual(first, second)
        self.assertEqual(stats1.total_flips, stats2.total_flips)


if __name__ == '__main__':
    unittest.main()
