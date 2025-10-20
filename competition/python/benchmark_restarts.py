#!/usr/bin/env python3
"""
Benchmark: Luby vs Glucose Restart Strategies

Compares the two restart strategies on the same benchmark suite:
- Luby sequence: Fixed geometric restart intervals
- Glucose: Adaptive restarts based on LBD trends

Both use the same optimized solver (two-watched literals + LBD).
"""

import sys
import os
import time
from pathlib import Path

# Add src to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat import CNFExpression
from bsat.dimacs import read_dimacs_file
import cdcl_optimized


def benchmark_instance(cnf: CNFExpression, name: str, max_conflicts: int = 10000):
    """
    Benchmark a single instance with both restart strategies.

    Returns:
        dict with results for both strategies
    """
    print(f"\n{'='*70}")
    print(f"Benchmarking: {name}")
    print(f"{'='*70}")
    print(f"  Variables: {len(cnf.get_variables())}, Clauses: {len(cnf.clauses)}")
    print()

    results = {}

    # Test 1: Luby sequence restarts
    print("  Testing Luby sequence...")
    solver_luby = cdcl_optimized.CDCLSolver(
        cnf,
        use_watched_literals=True,
        restart_strategy='luby',
        restart_base=100
    )
    start = time.time()
    result_luby = solver_luby.solve(max_conflicts=max_conflicts)
    time_luby = time.time() - start

    sat_status_luby = "SAT" if result_luby else "UNSAT/TIMEOUT"
    print(f"      {sat_status_luby} in {time_luby:.3f}s")
    print(f"      Decisions: {solver_luby.stats.decisions}, Conflicts: {solver_luby.stats.conflicts}")
    print(f"      Restarts: {solver_luby.stats.restarts}")

    results['luby'] = {
        'result': result_luby,
        'time': time_luby,
        'decisions': solver_luby.stats.decisions,
        'conflicts': solver_luby.stats.conflicts,
        'restarts': solver_luby.stats.restarts,
        'learned_clauses': solver_luby.stats.learned_clauses,
    }

    # Test 2: Glucose adaptive restarts
    print("  Testing Glucose adaptive...")
    solver_glucose = cdcl_optimized.CDCLSolver(
        cnf,
        use_watched_literals=True,
        restart_strategy='glucose',
        glucose_lbd_window=50,
        glucose_k=0.8
    )
    start = time.time()
    result_glucose = solver_glucose.solve(max_conflicts=max_conflicts)
    time_glucose = time.time() - start

    sat_status_glucose = "SAT" if result_glucose else "UNSAT/TIMEOUT"
    print(f"      {sat_status_glucose} in {time_glucose:.3f}s")
    print(f"      Decisions: {solver_glucose.stats.decisions}, Conflicts: {solver_glucose.stats.conflicts}")
    print(f"      Restarts: {solver_glucose.stats.restarts}")
    if hasattr(solver_glucose, 'lbd_count') and solver_glucose.lbd_count > 0:
        avg_lbd = solver_glucose.lbd_sum / solver_glucose.lbd_count
        print(f"      Average LBD: {avg_lbd:.2f}")

    results['glucose'] = {
        'result': result_glucose,
        'time': time_glucose,
        'decisions': solver_glucose.stats.decisions,
        'conflicts': solver_glucose.stats.conflicts,
        'restarts': solver_glucose.stats.restarts,
        'learned_clauses': solver_glucose.stats.learned_clauses,
    }

    # Comparison
    if time_luby > 0:
        speedup = time_luby / time_glucose
        print(f"\n  Speedup (Glucose vs Luby): {speedup:.2f}×")
        if speedup > 1.0:
            print(f"      ✅ Glucose is {speedup:.2f}× faster")
        elif speedup < 1.0:
            print(f"      ⚠️  Luby is {1.0/speedup:.2f}× faster")
        else:
            print(f"      ≈ Similar performance")

    return results


def benchmark_suite(suite_name: str, instances_dir: str, max_instances: int = None):
    """Benchmark all instances in a suite."""
    print(f"\n{'#'*70}")
    print(f"# BENCHMARK SUITE: {suite_name}")
    print(f"{'#'*70}\n")

    instances = sorted(Path(instances_dir).glob("*.cnf"))
    if max_instances:
        instances = instances[:max_instances]

    print(f"Found {len(instances)} instances")

    all_results = []

    for instance_path in instances:
        name = instance_path.stem
        cnf = read_dimacs_file(str(instance_path))
        results = benchmark_instance(cnf, name)
        all_results.append((name, results))

    # Summary
    print(f"\n{'='*70}")
    print(f"SUMMARY: {suite_name}")
    print(f"{'='*70}\n")

    total_time_luby = sum(r['luby']['time'] for _, r in all_results)
    total_time_glucose = sum(r['glucose']['time'] for _, r in all_results)

    total_restarts_luby = sum(r['luby']['restarts'] for _, r in all_results)
    total_restarts_glucose = sum(r['glucose']['restarts'] for _, r in all_results)

    total_conflicts_luby = sum(r['luby']['conflicts'] for _, r in all_results)
    total_conflicts_glucose = sum(r['glucose']['conflicts'] for _, r in all_results)

    print(f"LUBY:")
    print(f"  Total time: {total_time_luby:.3f}s")
    print(f"  Average time: {total_time_luby/len(all_results):.3f}s")
    print(f"  Total restarts: {total_restarts_luby}")
    print(f"  Total conflicts: {total_conflicts_luby}")
    print()

    print(f"GLUCOSE:")
    print(f"  Total time: {total_time_glucose:.3f}s")
    print(f"  Average time: {total_time_glucose/len(all_results):.3f}s")
    print(f"  Total restarts: {total_restarts_glucose}")
    print(f"  Total conflicts: {total_conflicts_glucose}")
    print()

    if total_time_luby > 0:
        overall_speedup = total_time_luby / total_time_glucose
        print(f"OVERALL SPEEDUP (Glucose vs Luby): {overall_speedup:.2f}×")
        if overall_speedup > 1.0:
            print(f"  ✅ Glucose is {overall_speedup:.2f}× faster overall")
        elif overall_speedup < 1.0:
            print(f"  ⚠️  Luby is {1.0/overall_speedup:.2f}× faster overall")
        else:
            print(f"  ≈ Similar overall performance")

    print()
    return all_results


def main():
    """Run restart strategy comparison benchmarks."""
    print("="*70)
    print(" RESTART STRATEGY BENCHMARK")
    print(" Comparing: Luby Sequence vs Glucose Adaptive")
    print("="*70)

    # Benchmark suites
    suites = [
        ("Simple Tests", "../../dataset/simple_tests/simple_suite", 9),
        ("Medium Tests", "../../dataset/medium_tests/medium_suite", 10),
        ("SAT Competition 2025 Small", "../../dataset/sat_competition2025_small", 3),
    ]

    all_suite_results = []

    for suite_name, instances_dir, max_instances in suites:
        results = benchmark_suite(suite_name, instances_dir, max_instances)
        all_suite_results.append((suite_name, results))

    print("\n" + "="*70)
    print(" BENCHMARK COMPLETE!")
    print("="*70)


if __name__ == "__main__":
    main()
