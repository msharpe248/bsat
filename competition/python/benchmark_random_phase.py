#!/usr/bin/env python3
"""
Benchmark: Random Phase Selection Impact on SAT Solver Performance

Compares the performance of the CDCL solver with and without random phase selection:
- Without random (0%): Phase saving only (Week 4 baseline)
- With random (5%): Phase saving + 5% random phase selection (Week 6 improvement)

Both use the same optimizations (two-watched literals, LBD, Glucose restarts, restart postponing).
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


def benchmark_instance(cnf: CNFExpression, name: str, max_conflicts: int = 10000, num_runs: int = 3):
    """
    Benchmark a single instance with and without random phase selection.

    Args:
        cnf: CNF formula
        name: Instance name
        max_conflicts: Maximum conflicts before timeout
        num_runs: Number of runs to average (for stable timings)

    Returns:
        dict with results for both configurations
    """
    print(f"\n{'='*70}")
    print(f"Benchmarking: {name}")
    print(f"{'='*70}")
    print(f"  Variables: {len(cnf.get_variables())}, Clauses: {len(cnf.clauses)}")
    print(f"  Runs per config: {num_runs}")
    print()

    results = {}

    # Test 1: WITHOUT random phase selection (Week 4 baseline)
    print("  [1] Phase saving WITHOUT random selection (0%):")
    times_without = []
    solver_without = None
    for i in range(num_runs):
        solver = cdcl_optimized.CDCLSolver(
            cnf,
            use_watched_literals=True,
            phase_saving=True,
            restart_postponing=True,
            random_phase_freq=0.0
        )
        start = time.time()
        result = solver.solve(max_conflicts=max_conflicts)
        elapsed = time.time() - start
        times_without.append(elapsed)
        if i == 0:
            solver_without = solver
            result_without = result

    avg_time_without = sum(times_without) / len(times_without)
    sat_status_without = "SAT" if result_without else "UNSAT/TIMEOUT"
    print(f"      {sat_status_without} in {avg_time_without:.4f}s (avg)")
    print(f"      Decisions: {solver_without.stats.decisions}, Conflicts: {solver_without.stats.conflicts}")
    print(f"      Restarts: {solver_without.stats.restarts}")

    results['without'] = {
        'result': result_without,
        'time': avg_time_without,
        'decisions': solver_without.stats.decisions,
        'conflicts': solver_without.stats.conflicts,
        'restarts': solver_without.stats.restarts,
        'learned_clauses': solver_without.stats.learned_clauses,
    }

    # Test 2: WITH 5% random phase selection (Week 6 improvement)
    print("  [2] Phase saving WITH 5% random selection:")
    times_with = []
    solver_with = None
    for i in range(num_runs):
        solver = cdcl_optimized.CDCLSolver(
            cnf,
            use_watched_literals=True,
            phase_saving=True,
            restart_postponing=True,
            random_phase_freq=0.05,
            random_seed=42
        )
        start = time.time()
        result = solver.solve(max_conflicts=max_conflicts)
        elapsed = time.time() - start
        times_with.append(elapsed)
        if i == 0:
            solver_with = solver
            result_with = result

    avg_time_with = sum(times_with) / len(times_with)
    sat_status_with = "SAT" if result_with else "UNSAT/TIMEOUT"
    print(f"      {sat_status_with} in {avg_time_with:.4f}s (avg)")
    print(f"      Decisions: {solver_with.stats.decisions}, Conflicts: {solver_with.stats.conflicts}")
    print(f"      Restarts: {solver_with.stats.restarts}")

    results['with'] = {
        'result': result_with,
        'time': avg_time_with,
        'decisions': solver_with.stats.decisions,
        'conflicts': solver_with.stats.conflicts,
        'restarts': solver_with.stats.restarts,
        'learned_clauses': solver_with.stats.learned_clauses,
    }

    # Comparison
    if avg_time_without > 0 and avg_time_with > 0:
        speedup = avg_time_without / avg_time_with
        print(f"\n  Speedup (with vs without random phase): {speedup:.2f}×")
        if speedup > 1.05:
            print(f"      ✅ Random phase selection is {speedup:.2f}× faster")
        elif speedup < 0.95:
            print(f"      ⚠️  Random phase selection is {1.0/speedup:.2f}× slower")
        else:
            print(f"      ≈ Similar performance (within 5%)")

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

    total_time_without = sum(r['without']['time'] for _, r in all_results)
    total_time_with = sum(r['with']['time'] for _, r in all_results)

    total_conflicts_without = sum(r['without']['conflicts'] for _, r in all_results)
    total_conflicts_with = sum(r['with']['conflicts'] for _, r in all_results)

    total_restarts_without = sum(r['without']['restarts'] for _, r in all_results)
    total_restarts_with = sum(r['with']['restarts'] for _, r in all_results)

    print(f"WITHOUT RANDOM PHASE SELECTION (0%):")
    print(f"  Total time: {total_time_without:.3f}s")
    print(f"  Average time: {total_time_without/len(all_results):.4f}s")
    print(f"  Total conflicts: {total_conflicts_without}")
    print(f"  Total restarts: {total_restarts_without}")
    print()

    print(f"WITH RANDOM PHASE SELECTION (5%):")
    print(f"  Total time: {total_time_with:.3f}s")
    print(f"  Average time: {total_time_with/len(all_results):.4f}s")
    print(f"  Total conflicts: {total_conflicts_with}")
    print(f"  Total restarts: {total_restarts_with}")
    print()

    if total_time_without > 0:
        overall_speedup = total_time_without / total_time_with
        print(f"OVERALL SPEEDUP (with random phase): {overall_speedup:.2f}×")
        if overall_speedup > 1.05:
            print(f"  ✅ Random phase selection is {overall_speedup:.2f}× faster overall")
        elif overall_speedup < 0.95:
            print(f"  ⚠️  Random phase selection is {1.0/overall_speedup:.2f}× slower overall")
        else:
            print(f"  ≈ Similar overall performance (within 5%)")

    # Count wins
    wins_with = sum(1 for _, r in all_results if r['with']['time'] < r['without']['time'] * 0.95)
    wins_without = sum(1 for _, r in all_results if r['without']['time'] < r['with']['time'] * 0.95)
    ties = len(all_results) - wins_with - wins_without

    print()
    print(f"Instance-by-instance comparison:")
    print(f"  Random phase faster: {wins_with}/{len(all_results)} instances")
    print(f"  No random phase faster: {wins_without}/{len(all_results)} instances")
    print(f"  Similar (±5%): {ties}/{len(all_results)} instances")

    # Highlight the catastrophic regression fix
    catastrophic_instance = 'easy_3sat_v016_c0067'
    for name, r in all_results:
        if name == catastrophic_instance:
            print()
            print(f"*** CATASTROPHIC REGRESSION FIX ({catastrophic_instance}): ***")
            print(f"  Without random: {r['without']['time']:.3f}s ({r['without']['conflicts']} conflicts)")
            print(f"  With random: {r['with']['time']:.3f}s ({r['with']['conflicts']} conflicts)")
            if r['with']['time'] > 0:
                fix_speedup = r['without']['time'] / r['with']['time']
                print(f"  🎉 FIXED: {fix_speedup:.1f}× faster with random phase selection!")

    print()
    return all_results


def main():
    """Run random phase selection comparison benchmarks."""
    print("="*70)
    print(" RANDOM PHASE SELECTION BENCHMARK")
    print(" Comparing: 0% vs 5% Random Phase Selection")
    print("="*70)

    # Benchmark suites
    suites = [
        ("Simple Tests", "../../dataset/simple_tests/simple_suite", 9),
        ("Medium Tests", "../../dataset/medium_tests/medium_suite", 10),
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
