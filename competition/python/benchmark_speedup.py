#!/usr/bin/env python3
"""
Benchmark Speedup: Original CDCL vs Optimized Competition Solver

Compares three configurations:
1. Original CDCL (from src/bsat) - baseline
2. Optimized with two-watched literals only
3. Optimized with two-watched + LBD

Measures:
- Wall-clock time
- Clauses checked (propagation efficiency)
- Conflicts, decisions, learned clauses
- Memory usage

Expected speedup:
- Two-watched: 50-100× fewer clauses checked
- Two-watched + LBD: 100-300× overall speedup
"""

import sys
import os
import time
import tracemalloc
from typing import Dict, Optional, Tuple, List
from dataclasses import dataclass

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression
from bsat import solve_cdcl as solve_original, get_cdcl_stats as get_original_stats
from bsat.dimacs import read_dimacs_file
import cdcl_optimized


@dataclass
class BenchmarkResult:
    """Result of benchmarking a single solver on a single instance."""
    solver_name: str
    instance_name: str
    result: str  # "SAT", "UNSAT", "TIMEOUT", "ERROR"
    time_seconds: float
    clauses_checked: int
    decisions: int
    conflicts: int
    propagations: int
    learned_clauses: int
    glue_clauses: int  # Only for optimized solvers
    deleted_clauses: int  # Only for optimized solvers
    memory_mb: float
    error_message: str = ""

    def speedup_vs(self, baseline: 'BenchmarkResult') -> float:
        """Calculate speedup vs baseline."""
        if self.time_seconds == 0 or baseline.time_seconds == 0:
            return 0.0
        return baseline.time_seconds / self.time_seconds

    def clauses_checked_ratio(self, baseline: 'BenchmarkResult') -> float:
        """Calculate ratio of clauses checked vs baseline."""
        if baseline.clauses_checked == 0:
            return 0.0
        return self.clauses_checked / baseline.clauses_checked


def run_solver(solver_name: str, cnf: CNFExpression, timeout: float = 60.0) -> BenchmarkResult:
    """
    Run a single solver on a CNF instance and collect statistics.

    Args:
        solver_name: "original", "watched", or "watched+lbd"
        cnf: CNF formula to solve
        timeout: Timeout in seconds

    Returns:
        BenchmarkResult with all metrics
    """
    instance_name = "unknown"

    try:
        # Start memory tracking
        tracemalloc.start()
        start_time = time.perf_counter()

        if solver_name == "original":
            # Original CDCL from src/bsat
            solution, stats = get_original_stats(cnf, max_conflicts=10000)
            clauses_checked = 0  # Original doesn't track this
            glue_clauses = 0
            deleted_clauses = 0

        elif solver_name == "watched":
            # Optimized with two-watched literals, no LBD
            solver = cdcl_optimized.CDCLSolver(
                cnf,
                use_watched_literals=True,
                learned_clause_limit=100000  # Effectively disable LBD deletion
            )
            solution = solver.solve(max_conflicts=10000)
            stats = solver.stats
            clauses_checked = stats.clauses_checked
            glue_clauses = stats.glue_clauses
            deleted_clauses = stats.deleted_clauses

        elif solver_name == "watched+lbd":
            # Optimized with two-watched literals AND LBD
            solver = cdcl_optimized.CDCLSolver(
                cnf,
                use_watched_literals=True,
                learned_clause_limit=10000  # Enable LBD-based deletion
            )
            solution = solver.solve(max_conflicts=10000)
            stats = solver.stats
            clauses_checked = stats.clauses_checked
            glue_clauses = stats.glue_clauses
            deleted_clauses = stats.deleted_clauses

        else:
            raise ValueError(f"Unknown solver: {solver_name}")

        end_time = time.perf_counter()
        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()

        # Build result
        result = BenchmarkResult(
            solver_name=solver_name,
            instance_name=instance_name,
            result="SAT" if solution is not None else "UNSAT",
            time_seconds=end_time - start_time,
            clauses_checked=clauses_checked,
            decisions=stats.decisions,
            conflicts=stats.conflicts,
            propagations=stats.propagations,
            learned_clauses=stats.learned_clauses,
            glue_clauses=glue_clauses,
            deleted_clauses=deleted_clauses,
            memory_mb=peak / (1024 * 1024)
        )

        return result

    except Exception as e:
        tracemalloc.stop()
        return BenchmarkResult(
            solver_name=solver_name,
            instance_name=instance_name,
            result="ERROR",
            time_seconds=0.0,
            clauses_checked=0,
            decisions=0,
            conflicts=0,
            propagations=0,
            learned_clauses=0,
            glue_clauses=0,
            deleted_clauses=0,
            memory_mb=0.0,
            error_message=str(e)
        )


def benchmark_instance(instance_path: str, instance_name: str, verbose: bool = True) -> List[BenchmarkResult]:
    """
    Benchmark all three solvers on a single instance.

    Returns:
        List of BenchmarkResults (one per solver)
    """
    if verbose:
        print(f"\n{'='*70}")
        print(f"Benchmarking: {instance_name}")
        print(f"{'='*70}")

    # Load instance
    try:
        if instance_path.endswith('.cnf'):
            cnf = read_dimacs_file(instance_path)
        else:
            with open(instance_path) as f:
                cnf = CNFExpression.parse(f.read())
    except Exception as e:
        print(f"  ❌ Failed to load: {e}")
        return []

    num_vars = len(cnf.get_variables())
    num_clauses = len(cnf.clauses)

    if verbose:
        print(f"  Variables: {num_vars}, Clauses: {num_clauses}")

    results = []

    # Test each solver
    for solver_name in ["original", "watched", "watched+lbd"]:
        if verbose:
            print(f"\n  Testing {solver_name}...", end=" ", flush=True)

        result = run_solver(solver_name, cnf)
        result.instance_name = instance_name
        results.append(result)

        if verbose:
            if result.result == "ERROR":
                print(f"❌ ERROR: {result.error_message}")
            else:
                print(f"✅ {result.result} in {result.time_seconds:.3f}s")
                print(f"      Decisions: {result.decisions}, Conflicts: {result.conflicts}")
                if result.clauses_checked > 0:
                    print(f"      Clauses checked: {result.clauses_checked:,}")
                if result.glue_clauses > 0:
                    print(f"      Glue clauses: {result.glue_clauses}/{result.learned_clauses}")

    # Print comparison
    if verbose and len(results) == 3:
        baseline = results[0]  # original
        watched = results[1]
        watched_lbd = results[2]

        print(f"\n  Speedup vs original:")
        print(f"    Watched only:     {watched.speedup_vs(baseline):.1f}×")
        print(f"    Watched + LBD:    {watched_lbd.speedup_vs(baseline):.1f}×")

        if watched.clauses_checked > 0:
            ratio = watched.clauses_checked_ratio(baseline) if baseline.clauses_checked > 0 else 0
            reduction = (1 - ratio) * 100
            print(f"\n  Clauses checked reduction (watched):")
            print(f"    Baseline: {baseline.clauses_checked if baseline.clauses_checked > 0 else 'N/A'}")
            print(f"    Watched:  {watched.clauses_checked:,} ({reduction:.1f}% reduction)")

    return results


def run_benchmark_suite(suite_name: str, suite_path: str, max_instances: int = None):
    """Run benchmark on a full suite of instances."""
    print(f"\n{'#'*70}")
    print(f"# BENCHMARK SUITE: {suite_name}")
    print(f"{'#'*70}")

    # Find all CNF files
    import glob
    cnf_files = glob.glob(os.path.join(suite_path, "**/*.cnf"), recursive=True)
    cnf_files.sort()

    if max_instances:
        cnf_files = cnf_files[:max_instances]

    print(f"\nFound {len(cnf_files)} instances")

    all_results = []

    for cnf_file in cnf_files:
        instance_name = os.path.basename(cnf_file).replace('.cnf', '')
        results = benchmark_instance(cnf_file, instance_name, verbose=True)
        all_results.extend(results)

    # Summary statistics
    print(f"\n{'='*70}")
    print(f"SUMMARY: {suite_name}")
    print(f"{'='*70}\n")

    # Group by solver
    from collections import defaultdict
    by_solver = defaultdict(list)
    for result in all_results:
        if result.result != "ERROR":
            by_solver[result.solver_name].append(result)

    # Calculate aggregate statistics
    for solver_name in ["original", "watched", "watched+lbd"]:
        results = by_solver[solver_name]
        if not results:
            continue

        total_time = sum(r.time_seconds for r in results)
        avg_time = total_time / len(results)
        total_clauses = sum(r.clauses_checked for r in results)

        print(f"{solver_name.upper()}:")
        print(f"  Instances solved: {len(results)}")
        print(f"  Total time: {total_time:.3f}s")
        print(f"  Average time: {avg_time:.3f}s")
        if total_clauses > 0:
            print(f"  Total clauses checked: {total_clauses:,}")
        print()

    # Speedup comparison
    if "original" in by_solver and "watched+lbd" in by_solver:
        baseline_time = sum(r.time_seconds for r in by_solver["original"])
        optimized_time = sum(r.time_seconds for r in by_solver["watched+lbd"])
        if baseline_time > 0:
            overall_speedup = baseline_time / optimized_time
            print(f"OVERALL SPEEDUP (watched+lbd vs original): {overall_speedup:.1f}×")


def main():
    """Run all benchmarks."""
    print("="*70)
    print(" COMPETITION SOLVER BENCHMARK")
    print(" Comparing: Original vs Watched vs Watched+LBD")
    print("="*70)

    # Get dataset path
    dataset_path = os.path.join(os.path.dirname(__file__), "../../dataset")

    # Benchmark 1: Simple tests (9 instances, quick validation)
    print("\n[1/3] Simple Test Suite (9 instances)")
    run_benchmark_suite(
        "Simple Tests",
        os.path.join(dataset_path, "simple_tests/simple_suite"),
        max_instances=9
    )

    # Benchmark 2: Medium tests (first 10 instances for now)
    print("\n\n[2/3] Medium Test Suite (first 10 instances)")
    run_benchmark_suite(
        "Medium Tests",
        os.path.join(dataset_path, "medium_tests/medium_suite"),
        max_instances=10
    )

    # Benchmark 3: Competition instances (first 3, very challenging)
    print("\n\n[3/3] SAT Competition 2025 Small (first 3 instances)")
    run_benchmark_suite(
        "SAT Competition 2025 Small",
        os.path.join(dataset_path, "sat_competition2025_small"),
        max_instances=3
    )

    print("\n" + "="*70)
    print(" BENCHMARK COMPLETE!")
    print("="*70)


if __name__ == "__main__":
    main()
