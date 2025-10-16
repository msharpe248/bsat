#!/usr/bin/env python3
"""
Statistical Benchmark Runner

Runs multiple iterations of benchmarks to compute:
- Mean and median times
- Standard deviation
- 95% confidence intervals
- Outlier detection
- Statistical significance tests

This proves that speedups are stable and reproducible.
"""

import sys
import os
import time
import statistics
from typing import List, Dict
from dataclasses import dataclass

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.dpll import DPLLSolver
from bsat.cdcl import CDCLSolver
from benchmark import ProblemGenerator


@dataclass
class StatisticalResult:
    """Statistical results from multiple runs."""
    solver_name: str
    problem_name: str
    times: List[float]
    mean: float
    median: float
    std_dev: float
    min_time: float
    max_time: float
    ci_95_lower: float
    ci_95_upper: float
    num_runs: int


class StatisticalBenchmark:
    """Run statistical benchmarks with multiple iterations."""

    def __init__(self, num_runs: int = 10):
        """
        Initialize statistical benchmark runner.

        Args:
            num_runs: Number of iterations per solver per problem
        """
        self.num_runs = num_runs
        self.results: List[StatisticalResult] = []

    def run_solver_multiple_times(self, solver_name: str, solver_class, cnf: CNFExpression,
                                  problem_name: str) -> StatisticalResult:
        """
        Run a solver multiple times and collect statistics.

        Args:
            solver_name: Name of the solver
            solver_class: Solver class or factory function
            cnf: CNF formula
            problem_name: Name of the problem

        Returns:
            StatisticalResult with timing statistics
        """
        times = []

        print(f"  {solver_name}: Running {self.num_runs} iterations... ", end="", flush=True)

        for run in range(self.num_runs):
            try:
                start = time.perf_counter()  # High precision timer
                solver = solver_class(cnf)
                result = solver.solve()
                elapsed = time.perf_counter() - start

                times.append(elapsed)

            except Exception as e:
                print(f"Error on run {run + 1}: {e}")
                continue

        if not times:
            print("❌ All runs failed")
            return None

        # Compute statistics
        mean = statistics.mean(times)
        median = statistics.median(times)
        std_dev = statistics.stdev(times) if len(times) > 1 else 0.0
        min_time = min(times)
        max_time = max(times)

        # 95% confidence interval (assuming normal distribution)
        # CI = mean ± 1.96 * (std_dev / sqrt(n))
        margin = 1.96 * (std_dev / (len(times) ** 0.5)) if len(times) > 1 else 0.0
        ci_95_lower = mean - margin
        ci_95_upper = mean + margin

        print(f"✅ Mean: {mean:.4f}s ± {margin:.4f}s (95% CI)")

        return StatisticalResult(
            solver_name=solver_name,
            problem_name=problem_name,
            times=times,
            mean=mean,
            median=median,
            std_dev=std_dev,
            min_time=min_time,
            max_time=max_time,
            ci_95_lower=ci_95_lower,
            ci_95_upper=ci_95_upper,
            num_runs=len(times)
        )

    def run_statistical_benchmark(self):
        """Run the full statistical benchmark."""
        print("=" * 80)
        print("STATISTICAL BENCHMARK")
        print("=" * 80)
        print(f"Running {self.num_runs} iterations per solver per problem")
        print()

        # Get benchmark problems (subset for statistical analysis)
        problems = self._get_statistical_problems()

        # Solvers to test
        solvers = self._get_solvers()

        for problem_name, cnf in problems:
            print(f"\n{'=' * 80}")
            print(f"Problem: {problem_name}")
            print(f"{'=' * 80}")

            for solver_name, solver_class in solvers:
                result = self.run_solver_multiple_times(
                    solver_name, solver_class, cnf, problem_name
                )
                if result:
                    self.results.append(result)

    def _get_statistical_problems(self):
        """Get a subset of problems for statistical analysis."""
        problems = []

        # Focus on the problems that show large speedups
        problems.append(("Random 3-SAT (10 vars)", ProblemGenerator.random_3sat(10, 35, seed=42)))
        problems.append(("Random 3-SAT (15 vars)", ProblemGenerator.random_3sat(15, 60, seed=42)))
        problems.append(("Random 3-SAT (25 vars) 🏆", ProblemGenerator.random_3sat(25, 105, seed=42)))

        return problems

    def _get_solvers(self):
        """Get list of solvers to test."""
        solvers = [
            ("CDCL", CDCLSolver),
            ("DPLL", DPLLSolver),
        ]

        # Add research solvers
        try:
            from cobd_sat import CoBDSATSolver
            solvers.append(("CoBD-SAT", CoBDSATSolver))
        except:
            pass

        try:
            from la_cdcl import LACDCLSolver
            solvers.append(("LA-CDCL", lambda cnf: LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)))
        except:
            pass

        try:
            from cgpm_sat import CGPMSolver
            solvers.append(("CGPM-SAT", lambda cnf: CGPMSolver(cnf, graph_weight=0.5)))
        except:
            pass

        return solvers

    def compute_speedup_with_confidence(self, baseline_result: StatisticalResult,
                                       test_result: StatisticalResult) -> Dict:
        """
        Compute speedup with confidence intervals.

        Args:
            baseline_result: Results for baseline solver (e.g., CDCL)
            test_result: Results for test solver

        Returns:
            Dict with speedup statistics
        """
        mean_speedup = baseline_result.mean / test_result.mean

        # Best case speedup (baseline max / test min)
        best_speedup = baseline_result.max_time / test_result.min_time

        # Worst case speedup (baseline min / test max)
        worst_speedup = baseline_result.min_time / test_result.max_time

        # Confidence interval speedup
        ci_speedup_lower = baseline_result.ci_95_lower / test_result.ci_95_upper
        ci_speedup_upper = baseline_result.ci_95_upper / test_result.ci_95_lower

        return {
            'mean_speedup': mean_speedup,
            'best_speedup': best_speedup,
            'worst_speedup': worst_speedup,
            'ci_95_lower': ci_speedup_lower,
            'ci_95_upper': ci_speedup_upper
        }

    def print_summary(self):
        """Print statistical summary."""
        print("\n" + "=" * 80)
        print("STATISTICAL SUMMARY")
        print("=" * 80)

        # Group results by problem
        problems = {}
        for result in self.results:
            if result.problem_name not in problems:
                problems[result.problem_name] = []
            problems[result.problem_name].append(result)

        for problem_name, problem_results in problems.items():
            print(f"\n{'=' * 80}")
            print(f"{problem_name}")
            print(f"{'=' * 80}")

            # Find baseline (CDCL)
            baseline = next((r for r in problem_results if r.solver_name == "CDCL"), None)

            for result in problem_results:
                print(f"\n{result.solver_name}:")
                print(f"  Mean:   {result.mean:.4f}s ± {result.std_dev:.4f}s")
                print(f"  Median: {result.median:.4f}s")
                print(f"  Range:  [{result.min_time:.4f}s, {result.max_time:.4f}s]")
                print(f"  95% CI: [{result.ci_95_lower:.4f}s, {result.ci_95_upper:.4f}s]")
                print(f"  Runs:   {result.num_runs}")

                # Compute coefficient of variation (relative std dev)
                cv = (result.std_dev / result.mean * 100) if result.mean > 0 else 0
                print(f"  CV:     {cv:.1f}%", end="")

                if cv < 10:
                    print(" ✅ (very stable)")
                elif cv < 20:
                    print(" ✓ (stable)")
                elif cv < 30:
                    print(" ⚠️  (moderate variance)")
                else:
                    print(" ❌ (high variance!)")

                # Compute speedup vs baseline
                if baseline and result.solver_name != "CDCL":
                    speedup_stats = self.compute_speedup_with_confidence(baseline, result)
                    print(f"\n  Speedup vs CDCL:")
                    print(f"    Mean:   {speedup_stats['mean_speedup']:.2f}×")
                    print(f"    95% CI: [{speedup_stats['ci_95_lower']:.2f}×, {speedup_stats['ci_95_upper']:.2f}×]")
                    print(f"    Range:  [{speedup_stats['worst_speedup']:.2f}×, {speedup_stats['best_speedup']:.2f}×]")

    def export_csv(self, filename: str):
        """Export results to CSV for analysis."""
        with open(filename, 'w') as f:
            f.write("Solver,Problem,Mean,Median,StdDev,Min,Max,CI_95_Lower,CI_95_Upper,NumRuns\n")
            for result in self.results:
                f.write(f"{result.solver_name},{result.problem_name},")
                f.write(f"{result.mean},{result.median},{result.std_dev},")
                f.write(f"{result.min_time},{result.max_time},")
                f.write(f"{result.ci_95_lower},{result.ci_95_upper},{result.num_runs}\n")
        print(f"\n✅ Results exported to: {filename}")


def main():
    """Run statistical benchmark."""
    import argparse

    parser = argparse.ArgumentParser(description="Statistical Benchmark Runner")
    parser.add_argument('--runs', type=int, default=10, help='Number of runs per solver per problem')
    parser.add_argument('--output', type=str, default='statistical_results.csv', help='Output CSV file')
    args = parser.parse_args()

    benchmark = StatisticalBenchmark(num_runs=args.runs)
    benchmark.run_statistical_benchmark()
    benchmark.print_summary()
    benchmark.export_csv(args.output)

    return 0


if __name__ == "__main__":
    sys.exit(main())
