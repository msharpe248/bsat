"""
Performance Comparison and Benchmarking Utilities

Compare different SAT solvers on benchmark instances and analyze performance.
"""

import time
import sys
from pathlib import Path
from typing import Dict, List, Optional, Callable, Tuple
from dataclasses import dataclass

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from tests.benchmarks import (
    get_benchmark, list_benchmarks, benchmark_stats,
    random_3sat, phase_transition_3sat, pigeon_hole
)
from bsat import (
    CNFExpression, solve_sat, solve_cdcl, solve_walksat,
    get_cdcl_stats, is_2sat, solve_2sat
)


@dataclass
class BenchmarkResult:
    """Results from running a solver on a benchmark."""
    solver_name: str
    benchmark_name: str
    satisfiable: Optional[bool]  # None if unknown (e.g., WalkSAT timeout)
    time_seconds: float
    decisions: Optional[int] = None
    conflicts: Optional[int] = None
    propagations: Optional[int] = None
    learned_clauses: Optional[int] = None
    error: Optional[str] = None


def run_solver(
    solver_func: Callable,
    cnf: CNFExpression,
    solver_name: str,
    benchmark_name: str,
    timeout: float = 60.0,
    **solver_kwargs
) -> BenchmarkResult:
    """
    Run a solver on a CNF instance and collect results.

    Args:
        solver_func: Solver function to call
        cnf: CNF formula
        solver_name: Name of solver for reporting
        benchmark_name: Name of benchmark
        timeout: Maximum time in seconds
        **solver_kwargs: Additional arguments for solver

    Returns:
        BenchmarkResult with timing and statistics
    """
    start_time = time.time()

    try:
        result = solver_func(cnf, **solver_kwargs)
        elapsed = time.time() - start_time

        if elapsed > timeout:
            return BenchmarkResult(
                solver_name=solver_name,
                benchmark_name=benchmark_name,
                satisfiable=None,
                time_seconds=elapsed,
                error="Timeout"
            )

        return BenchmarkResult(
            solver_name=solver_name,
            benchmark_name=benchmark_name,
            satisfiable=(result is not None),
            time_seconds=elapsed
        )

    except Exception as e:
        elapsed = time.time() - start_time
        return BenchmarkResult(
            solver_name=solver_name,
            benchmark_name=benchmark_name,
            satisfiable=None,
            time_seconds=elapsed,
            error=str(e)
        )


def run_cdcl_with_stats(cnf: CNFExpression, benchmark_name: str) -> BenchmarkResult:
    """Run CDCL and collect detailed statistics."""
    start_time = time.time()

    try:
        result, stats = get_cdcl_stats(cnf)
        elapsed = time.time() - start_time

        return BenchmarkResult(
            solver_name="CDCL",
            benchmark_name=benchmark_name,
            satisfiable=(result is not None),
            time_seconds=elapsed,
            decisions=stats.decisions,
            conflicts=stats.conflicts,
            propagations=stats.propagations,
            learned_clauses=stats.learned_clauses
        )

    except Exception as e:
        elapsed = time.time() - start_time
        return BenchmarkResult(
            solver_name="CDCL",
            benchmark_name=benchmark_name,
            satisfiable=None,
            time_seconds=elapsed,
            error=str(e)
        )


def compare_solvers(
    benchmark_name: str,
    solvers: Optional[List[str]] = None
) -> List[BenchmarkResult]:
    """
    Compare multiple solvers on a benchmark.

    Args:
        benchmark_name: Name of benchmark to test
        solvers: List of solver names (default: all)

    Returns:
        List of BenchmarkResults
    """
    if solvers is None:
        solvers = ["DPLL", "CDCL", "WalkSAT"]

    cnf = get_benchmark(benchmark_name)
    results = []

    for solver_name in solvers:
        if solver_name == "DPLL":
            result = run_solver(solve_sat, cnf, "DPLL", benchmark_name)
            results.append(result)

        elif solver_name == "CDCL":
            result = run_cdcl_with_stats(cnf, benchmark_name)
            results.append(result)

        elif solver_name == "WalkSAT":
            result = run_solver(
                solve_walksat, cnf, "WalkSAT", benchmark_name,
                max_flips=10000, seed=42
            )
            results.append(result)

        elif solver_name == "2SAT" and is_2sat(cnf):
            result = run_solver(solve_2sat, cnf, "2SAT", benchmark_name)
            results.append(result)

    return results


def run_benchmark_suite(
    benchmarks: Optional[List[str]] = None,
    solvers: Optional[List[str]] = None
) -> Dict[str, List[BenchmarkResult]]:
    """
    Run full benchmark suite comparing solvers.

    Args:
        benchmarks: List of benchmark names (default: all)
        solvers: List of solver names (default: DPLL, CDCL)

    Returns:
        Dictionary mapping benchmark name to results
    """
    if benchmarks is None:
        benchmarks = list_benchmarks()

    if solvers is None:
        solvers = ["DPLL", "CDCL"]

    all_results = {}

    for benchmark_name in benchmarks:
        print(f"Running: {benchmark_name}...")
        results = compare_solvers(benchmark_name, solvers)
        all_results[benchmark_name] = results

    return all_results


def print_comparison(results: List[BenchmarkResult]):
    """Pretty print solver comparison results."""
    if not results:
        return

    benchmark_name = results[0].benchmark_name
    cnf = get_benchmark(benchmark_name)
    stats = benchmark_stats(cnf)

    print(f"\n{'='*70}")
    print(f"Benchmark: {benchmark_name}")
    print(f"{'='*70}")
    print(f"Variables: {stats['num_variables']}")
    print(f"Clauses: {stats['num_clauses']}")
    print(f"Ratio: {stats['ratio']:.2f}")
    print(f"\nResults:")
    print(f"{'-'*70}")

    for result in results:
        sat_str = "SAT" if result.satisfiable else "UNSAT" if result.satisfiable is False else "UNKNOWN"
        print(f"\n{result.solver_name}:")
        print(f"  Result: {sat_str}")
        print(f"  Time: {result.time_seconds:.4f}s")

        if result.decisions is not None:
            print(f"  Decisions: {result.decisions}")
        if result.conflicts is not None:
            print(f"  Conflicts: {result.conflicts}")
        if result.propagations is not None:
            print(f"  Propagations: {result.propagations}")
        if result.learned_clauses is not None:
            print(f"  Learned clauses: {result.learned_clauses}")
        if result.error:
            print(f"  Error: {result.error}")


def print_summary_table(all_results: Dict[str, List[BenchmarkResult]]):
    """Print summary table of all results."""
    print(f"\n{'='*80}")
    print(f"BENCHMARK SUMMARY")
    print(f"{'='*80}")
    print(f"{'Benchmark':<25} {'Solver':<12} {'Result':<10} {'Time (s)':<12}")
    print(f"{'-'*80}")

    for benchmark_name, results in sorted(all_results.items()):
        for i, result in enumerate(results):
            bench_str = benchmark_name if i == 0 else ""
            sat_str = "SAT" if result.satisfiable else "UNSAT" if result.satisfiable is False else "UNKNOWN"

            print(f"{bench_str:<25} {result.solver_name:<12} {sat_str:<10} {result.time_seconds:>10.4f}")

        if len(results) > 1:
            print(f"{'-'*80}")


def analyze_performance_trends(all_results: Dict[str, List[BenchmarkResult]]):
    """Analyze performance trends across benchmarks."""
    print(f"\n{'='*70}")
    print(f"PERFORMANCE ANALYSIS")
    print(f"{'='*70}")

    # Group by solver
    solver_times = {}
    for benchmark_name, results in all_results.items():
        for result in results:
            if result.solver_name not in solver_times:
                solver_times[result.solver_name] = []
            solver_times[result.solver_name].append(result.time_seconds)

    print(f"\nAverage solve times:")
    for solver, times in sorted(solver_times.items()):
        avg_time = sum(times) / len(times)
        max_time = max(times)
        min_time = min(times)
        print(f"  {solver}:")
        print(f"    Average: {avg_time:.4f}s")
        print(f"    Min: {min_time:.4f}s")
        print(f"    Max: {max_time:.4f}s")


def example1_single_comparison():
    """Example 1: Compare solvers on a single benchmark."""
    print("="*70)
    print("Example 1: Single Benchmark Comparison")
    print("="*70)

    results = compare_solvers("phase_transition_20", solvers=["DPLL", "CDCL"])
    print_comparison(results)


def example2_dpll_vs_cdcl():
    """Example 2: Compare DPLL vs CDCL on multiple benchmarks."""
    print("\n" + "="*70)
    print("Example 2: DPLL vs CDCL on Multiple Benchmarks")
    print("="*70)

    benchmarks = [
        "easy_sat_1",
        "medium_sat",
        "phase_transition_20",
        "pigeon_hole_5",
    ]

    all_results = run_benchmark_suite(benchmarks, solvers=["DPLL", "CDCL"])
    print_summary_table(all_results)
    analyze_performance_trends(all_results)


def example3_walksat_comparison():
    """Example 3: Include WalkSAT in comparison."""
    print("\n" + "="*70)
    print("Example 3: DPLL vs CDCL vs WalkSAT")
    print("="*70)

    benchmarks = [
        "easy_sat_1",
        "medium_sat",
    ]

    all_results = run_benchmark_suite(benchmarks, solvers=["DPLL", "CDCL", "WalkSAT"])
    print_summary_table(all_results)


def example4_scaling_analysis():
    """Example 4: Analyze how solvers scale with problem size."""
    print("\n" + "="*70)
    print("Example 4: Scaling Analysis")
    print("="*70)

    print("\nGenerating random 3SAT instances of increasing size...")

    sizes = [10, 20, 30, 40]
    results_by_size = {}

    for size in sizes:
        cnf = random_3sat(size, int(size * 4.26), seed=42)  # Phase transition
        benchmark_name = f"random_{size}"

        print(f"\nSize {size}:")
        dpll_res = run_solver(solve_sat, cnf, "DPLL", benchmark_name, timeout=10.0)
        cdcl_res = run_cdcl_with_stats(cnf, benchmark_name)

        results_by_size[benchmark_name] = [dpll_res, cdcl_res]

        print(f"  DPLL: {dpll_res.time_seconds:.4f}s")
        print(f"  CDCL: {cdcl_res.time_seconds:.4f}s")
        if cdcl_res.decisions:
            print(f"  CDCL decisions: {cdcl_res.decisions}")

    print_summary_table(results_by_size)


def example5_pigeon_hole_comparison():
    """Example 5: Compare on pigeon-hole (UNSAT proofs)."""
    print("\n" + "="*70)
    print("Example 5: Pigeon-Hole Instances (UNSAT Proofs)")
    print("="*70)

    benchmarks = ["pigeon_hole_5", "pigeon_hole_6"]
    all_results = run_benchmark_suite(benchmarks, solvers=["DPLL", "CDCL"])

    print_summary_table(all_results)

    print("\nNote: CDCL typically proves UNSAT faster due to clause learning.")


def example6_detailed_cdcl_stats():
    """Example 6: Detailed CDCL statistics."""
    print("\n" + "="*70)
    print("Example 6: Detailed CDCL Statistics")
    print("="*70)

    benchmarks = [
        "phase_transition_20",
        "pigeon_hole_5",
        "graph_coloring_unsat"
    ]

    for name in benchmarks:
        result = run_cdcl_with_stats(get_benchmark(name), name)
        print(f"\n{name}:")
        print(f"  Time: {result.time_seconds:.4f}s")
        print(f"  Decisions: {result.decisions}")
        print(f"  Conflicts: {result.conflicts}")
        print(f"  Propagations: {result.propagations}")
        print(f"  Learned clauses: {result.learned_clauses}")
        print(f"  Result: {'SAT' if result.satisfiable else 'UNSAT'}")


if __name__ == '__main__':
    example1_single_comparison()
    example2_dpll_vs_cdcl()
    example3_walksat_comparison()
    example4_scaling_analysis()
    example5_pigeon_hole_comparison()
    example6_detailed_cdcl_stats()

    print("\n" + "="*70)
    print("All benchmark comparisons completed!")
    print("="*70)
