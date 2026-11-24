"""Tests for the Python ctypes bindings to BSAT's native C11 solver (libbsat)."""

import random
import tempfile
import unittest
from pathlib import Path

from bsat import (
    BSAT_SAT,
    BSAT_UNSAT,
    CNFExpression,
    Clause,
    Literal,
    CSATSolver,
    solve_c,
    get_c_solver_stats,
    is_c_solver_available,
    solve_cdcl,
)


@unittest.skipUnless(is_c_solver_available(), "libbsat shared library not built")
class TestCSolver(unittest.TestCase):
    def test_empty_and_trivial(self):
        self.assertEqual(solve_c(CNFExpression([])), {})
        self.assertIsNone(solve_c(CNFExpression([Clause([])])))

        sat_cnf = CNFExpression([Clause([Literal("x", False)])])
        sol = solve_c(sat_cnf)
        self.assertIsNotNone(sol)
        self.assertTrue(sol["x"])
        self.assertTrue(sat_cnf.evaluate(sol))

        unsat_cnf = CNFExpression([
            Clause([Literal("x", False)]),
            Clause([Literal("x", True)]),
        ])
        self.assertIsNone(solve_c(unsat_cnf))

    def test_parity_with_python_cdcl(self):
        rng = random.Random(20261124)
        for _ in range(40):
            nv = rng.randint(6, 12)
            nc = rng.randint(nv * 2, nv * 5)
            vars_list = [f"v{i}" for i in range(nv)]
            clauses = []
            for _ in range(nc):
                k = rng.randint(2, 3)
                chosen = rng.sample(vars_list, k)
                clauses.append(Clause([Literal(v, rng.choice([True, False])) for v in chosen]))
            cnf = CNFExpression(clauses)
            py_sol = solve_cdcl(cnf)
            c_sol, stats = get_c_solver_stats(cnf)
            if py_sol is None:
                self.assertIsNone(c_sol)
                self.assertEqual(stats.result, BSAT_UNSAT)
            else:
                self.assertIsNotNone(c_sol)
                self.assertEqual(stats.result, BSAT_SAT)
                self.assertTrue(cnf.evaluate(c_sol))

    def test_incremental_assumptions_and_failed_core(self):
        # (a -> b) & (b -> c) & (d | e)
        cnf = CNFExpression([
            Clause([Literal("a", True), Literal("b", False)]),
            Clause([Literal("b", True), Literal("c", False)]),
            Clause([Literal("d", False), Literal("e", False)]),
        ])
        with CSATSolver(cnf, reuse_learnts=True) as solver:
            # Assume a=T, c=T, d=F -> SAT with b=T, e=T
            sol = solver.solve([Literal("a", False), Literal("c", False), Literal("d", True)])
            self.assertIsNotNone(sol)
            self.assertEqual(solver.last_status, BSAT_SAT)
            self.assertTrue(sol["a"] and sol["b"] and sol["c"] and sol["e"] and not sol["d"])

            # Assume a=T, c=F, d=T -> UNSAT; core must be a sufficient subset of assumps
            assumps = [Literal("a", False), Literal("c", True), Literal("d", False)]
            self.assertIsNone(solver.solve(assumps))
            self.assertEqual(solver.last_status, BSAT_UNSAT)
            core = solver.get_failed_assumptions(assumps)
            self.assertTrue(len(core) > 0)
            self.assertTrue(set(core).issubset(set(assumps)))
            self.assertIn(Literal("a", False), core)
            self.assertIn(Literal("c", True), core)
            # Re-solving under just the returned core must still be UNSAT.
            self.assertEqual(solver.solve_status(core), BSAT_UNSAT)

            # Add permanent clause forcing ~a, then re-solve with ~c -> SAT!
            solver.add_clause(Clause([Literal("a", True)]))
            sol2 = solver.solve([Literal("c", True)])
            self.assertIsNotNone(sol2)
            self.assertFalse(sol2["a"])
            self.assertFalse(sol2["c"])

            stats = solver.get_stats()
            self.assertEqual(stats.status, "SAT")
            self.assertGreaterEqual(stats.reused_preparations, 0)
            self.assertTrue(solver.checkpoint())

    def test_dimacs_int_clauses_and_certificates(self):
        with tempfile.TemporaryDirectory(prefix="bsat-py-cert-") as td:
            cnf_out = Path(td) / "query.cnf"
            proof_out = Path(td) / "query.drat"
            with CSATSolver(certificates=True) as solver:
                # Pigeonhole 3 pigeons, 2 holes (UNSAT)
                solver.add_clause([1, 2])
                solver.add_clause([3, 4])
                solver.add_clause([5, 6])
                solver.add_clause([-1, -3])
                solver.add_clause([-1, -5])
                solver.add_clause([-3, -5])
                solver.add_clause([-2, -4])
                solver.add_clause([-2, -6])
                solver.add_clause([-4, -6])
                self.assertEqual(solver.solve_status(), BSAT_UNSAT)
                self.assertTrue(solver.export_query(cnf_out, proof_out))
                self.assertTrue(cnf_out.is_file() and cnf_out.stat().st_size > 0)
                self.assertTrue(proof_out.is_file() and proof_out.stat().st_size > 0)


if __name__ == "__main__":
    unittest.main()
