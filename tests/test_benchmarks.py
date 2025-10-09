"""
Comprehensive Test Suite Using Benchmark Instances

Tests all solvers against benchmark instances to verify correctness
and track performance.
"""

import unittest
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from tests.benchmarks import (
    get_benchmark, list_benchmarks, benchmark_stats,
    random_3sat, phase_transition_3sat, pigeon_hole,
    graph_coloring_hard, xor_chain
)
from bsat import (
    solve_sat, solve_2sat, solve_horn_sat, solve_cdcl,
    solve_walksat, solve_xorsat, is_2sat, is_horn_formula
)


class TestBenchmarkGeneration(unittest.TestCase):
    """Test benchmark generation functions."""

    def test_random_3sat(self):
        """Test random 3SAT generation."""
        cnf = random_3sat(10, 20, seed=42)
        self.assertEqual(len(cnf.clauses), 20)

        # All clauses should have 3 literals
        for clause in cnf.clauses:
            self.assertEqual(len(clause.literals), 3)

        # Reproducibility
        cnf2 = random_3sat(10, 20, seed=42)
        self.assertEqual(str(cnf), str(cnf2))

    def test_phase_transition(self):
        """Test phase transition instance generation."""
        cnf = phase_transition_3sat(20, ratio=4.26, seed=42)
        stats = benchmark_stats(cnf)

        # Should have ratio ≈ 4.26
        self.assertAlmostEqual(stats['ratio'], 4.26, places=1)

    def test_pigeon_hole(self):
        """Test pigeon-hole generation."""
        cnf = pigeon_hole(4)  # 4 pigeons, 3 holes

        stats = benchmark_stats(cnf)
        # 4 clauses for "each pigeon in at least one hole"
        # 3 * C(4,2) = 3 * 6 = 18 clauses for "at most one pigeon per hole"
        self.assertEqual(stats['num_clauses'], 4 + 18)

    def test_graph_coloring(self):
        """Test graph coloring generation."""
        cnf = graph_coloring_hard(5, 3, density=0.5, seed=42)

        # Should have constraints for 5 vertices, 3 colors
        stats = benchmark_stats(cnf)
        self.assertGreater(stats['num_clauses'], 0)

        # Reproducibility
        cnf2 = graph_coloring_hard(5, 3, density=0.5, seed=42)
        self.assertEqual(str(cnf), str(cnf2))

    def test_xor_chain(self):
        """Test XOR chain generation."""
        cnf = xor_chain(5, value=True)
        stats = benchmark_stats(cnf)
        self.assertGreater(stats['num_clauses'], 0)


class TestBenchmarkSuite(unittest.TestCase):
    """Test the benchmark suite."""

    def test_list_benchmarks(self):
        """Test listing benchmarks."""
        benchmarks = list_benchmarks()
        self.assertGreater(len(benchmarks), 0)
        self.assertIn("easy_sat_1", benchmarks)
        self.assertIn("pigeon_hole_5", benchmarks)

    def test_get_benchmark(self):
        """Test getting benchmarks."""
        cnf = get_benchmark("easy_sat_1")
        self.assertIsNotNone(cnf)

        with self.assertRaises(ValueError):
            get_benchmark("nonexistent")

    def test_benchmark_stats(self):
        """Test benchmark statistics."""
        cnf = get_benchmark("easy_sat_1")
        stats = benchmark_stats(cnf)

        self.assertIn('num_variables', stats)
        self.assertIn('num_clauses', stats)
        self.assertIn('ratio', stats)
        self.assertGreater(stats['num_variables'], 0)


class TestDPLLOnBenchmarks(unittest.TestCase):
    """Test DPLL solver on benchmarks."""

    def test_easy_sat(self):
        """DPLL should solve easy SAT instances."""
        cnf = get_benchmark("easy_sat_1")
        result = solve_sat(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_easy_unsat(self):
        """DPLL should detect easy UNSAT instances."""
        cnf = get_benchmark("easy_unsat_1")
        result = solve_sat(cnf)
        self.assertIsNone(result)

    def test_pigeon_hole(self):
        """DPLL should detect pigeon-hole UNSAT."""
        cnf = get_benchmark("pigeon_hole_5")
        result = solve_sat(cnf)
        self.assertIsNone(result)


class TestCDCLOnBenchmarks(unittest.TestCase):
    """Test CDCL solver on benchmarks."""

    def test_easy_sat(self):
        """CDCL should solve easy SAT instances."""
        cnf = get_benchmark("easy_sat_1")
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_medium_sat(self):
        """CDCL should solve medium SAT instances."""
        cnf = get_benchmark("medium_sat")
        result = solve_cdcl(cnf)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_phase_transition(self):
        """CDCL should handle phase transition instances."""
        cnf = get_benchmark("phase_transition_20")
        result = solve_cdcl(cnf)
        # May be SAT or UNSAT, just check it terminates
        if result:
            self.assertTrue(cnf.evaluate(result))

    def test_pigeon_hole(self):
        """CDCL should efficiently prove pigeon-hole UNSAT."""
        cnf = get_benchmark("pigeon_hole_5")
        result = solve_cdcl(cnf)
        self.assertIsNone(result)

    def test_graph_coloring_unsat(self):
        """CDCL should detect graph coloring UNSAT."""
        cnf = get_benchmark("graph_coloring_unsat")
        result = solve_cdcl(cnf)
        self.assertIsNone(result)


class TestWalkSATOnBenchmarks(unittest.TestCase):
    """Test WalkSAT solver on benchmarks."""

    def test_easy_sat(self):
        """WalkSAT should find easy SAT solutions."""
        cnf = get_benchmark("easy_sat_1")
        result = solve_walksat(cnf, max_flips=1000, seed=42)
        self.assertIsNotNone(result)
        self.assertTrue(cnf.evaluate(result))

    def test_medium_sat(self):
        """WalkSAT should find medium SAT solutions."""
        cnf = get_benchmark("medium_sat")
        result = solve_walksat(cnf, max_flips=10000, seed=42)
        # WalkSAT is incomplete, but should often find solutions
        if result:
            self.assertTrue(cnf.evaluate(result))


class TestSpecializedSolvers(unittest.TestCase):
    """Test specialized solvers on appropriate benchmarks."""

    def test_2sat_detection(self):
        """Test 2SAT detection on benchmarks."""
        # Most benchmarks are 3SAT
        cnf = get_benchmark("easy_sat_1")
        self.assertFalse(is_2sat(cnf))

    def test_horn_detection(self):
        """Test Horn-SAT detection."""
        # Most benchmarks are not Horn
        cnf = get_benchmark("easy_sat_1")
        # Usually not Horn, but check it doesn't crash
        is_horn_formula(cnf)


class TestBenchmarkConsistency(unittest.TestCase):
    """Test that all solvers agree on SAT/UNSAT for each benchmark."""

    def test_solver_agreement(self):
        """All complete solvers should agree on SAT/UNSAT."""
        test_cases = [
            "easy_sat_1",
            "easy_unsat_1",
            "pigeon_hole_5",
        ]

        for name in test_cases:
            with self.subTest(benchmark=name):
                cnf = get_benchmark(name)

                dpll_result = solve_sat(cnf)
                cdcl_result = solve_cdcl(cnf)

                # Both should agree on SAT/UNSAT
                if dpll_result is None:
                    self.assertIsNone(cdcl_result, f"Solvers disagree on {name}")
                else:
                    self.assertIsNotNone(cdcl_result, f"Solvers disagree on {name}")
                    self.assertTrue(cnf.evaluate(dpll_result))
                    self.assertTrue(cnf.evaluate(cdcl_result))


class TestLargeBenchmarks(unittest.TestCase):
    """Test on larger/harder instances."""

    def test_phase_transition_30(self):
        """Test 30-variable phase transition instance."""
        cnf = get_benchmark("phase_transition_30")
        result = solve_cdcl(cnf)
        # Should terminate (may be SAT or UNSAT)
        if result:
            self.assertTrue(cnf.evaluate(result))

    def test_pigeon_hole_6(self):
        """Test 6-pigeon instance (harder)."""
        cnf = get_benchmark("pigeon_hole_6")
        result = solve_cdcl(cnf)
        self.assertIsNone(result)  # Should be UNSAT


if __name__ == '__main__':
    unittest.main()
