#!/usr/bin/env python3
"""
Benchmark: Adaptive Random Phase Selection

Compares three configurations:
1. No random phase (Week 4 baseline - phase saving only)
2. Static 5% random phase (Week 6 - always enabled)
3. Adaptive random phase (Week 7 - auto-enable when stuck)

Goal: Show that adaptive gives best of both worlds (no overhead on easy, help on hard).
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
    Benchmark a single instance with three configurations.

    Args:
        cnf: CNF formula
        name: Instance name
        max_conflicts: Maximum conflicts before timeout
        num_runs: Number of runs to average

    Returns:
        dict with results for all three configurations
    """
    print(f"\n{'='*70}")
    print(f"Benchmarking: {name}")
    print(f"{'='*70}")
    print(f"  Variables: {len(cnf.get_variables())}, Clauses: {len(cnf.clauses)}")
    print(f"  Runs per config: {num_runs}")
    print()

    results = {}

    # Config 1: No random phase (baseline)
    print("  [1] No random phase (Week 4 baseline):")
    times = []
    solver_ref = None
    for i in range(num_runs):
        solver = cdcl_optimized.CDCLSolver(
            cnf,
            use_watched_literals=True,
            phase_saving=True,
            restart_postponing=True,
            random_phase_freq=0.0,
            adaptive_random_phase=False  # Explicitly disable
        )
        start = time.time()
        result = solver.solve(max_conflicts=max_conflicts)
        elapsed = time.time() - start
        times.append(elapsed)
        if i == 0:
            solver_ref = solver
            result_ref = result

    avg_time = sum(times) / len(times)
    print(f"      {('SAT' if result_ref else 'UNSAT/TIMEOUT')} in {avg_time:.4f}s (avg)")
    print(f"      Conflicts: {solver_ref.stats.conflicts}, Restarts: {solver_ref.stats.restarts}")

    results['no_random'] = {
        'result': result_ref,
        'time': avg_time,
        'conflicts': solver_ref.stats.conflicts,
        'restarts': solver_ref.stats.restarts,
        'adaptive_enabled': False,
    }

    # Config 2: Static 5% random phase (Week 6)
    print("  [2] Static 5% random phase (Week 6):")
    times = []
    solver_static = None
    for i in range(num_runs):
        solver = cdcl_optimized.CDCLSolver(
            cnf,
            use_watched_literals=True,
            phase_saving=True,
            restart_postponing=True,
            random_phase_freq=0.05,
            random_seed=42,
            adaptive_random_phase=False  # Disable adaptive (use static 5%)
        )
        start = time.time()
        result = solver.solve(max_conflicts=max_conflicts)
        elapsed = time.time() - start
        times.append(elapsed)
        if i == 0:
            solver_static = solver
            result_static = result

    avg_time = sum(times) / len(times)
    print(f"      {('SAT' if result_static else 'UNSAT/TIMEOUT')} in {avg_time:.4f}s (avg)")
    print(f"      Conflicts: {solver_static.stats.conflicts}, Restarts: {solver_static.stats.restarts}")

    results['static'] = {
        'result': result_static,
        'time': avg_time,
        'conflicts': solver_static.stats.conflicts,
        'restarts': solver_static.stats.restarts,
        'adaptive_enabled': False,
    }

    # Config 3: Adaptive random phase (Week 7 - NEW!)
    print("  [3] Adaptive random phase (Week 7 - NEW!):")
    times = []
    solver_adaptive = None
    for i in range(num_runs):
        solver = cdcl_optimized.CDCLSolver(
            cnf,
            use_watched_literals=True,
            phase_saving=True,
            restart_postponing=True,
            random_phase_freq=0.0,  # Start disabled
            adaptive_random_phase=True,  # Auto-enable when stuck
            adaptive_threshold=1000,
            adaptive_restart_ratio=0.2
        )
        start = time.time()
        result = solver.solve(max_conflicts=max_conflicts)
        elapsed = time.time() - start
        times.append(elapsed)
        if i == 0:
            solver_adaptive = solver
            result_adaptive = result

    avg_time = sum(times) / len(times)
    print(f"      {('SAT' if result_adaptive else 'UNSAT/TIMEOUT')} in {avg_time:.4f}s (avg)")
    print(f"      Conflicts: {solver_adaptive.stats.conflicts}, Restarts: {solver_adaptive.stats.restarts}")
    print(f"      Adaptive enabled: {solver_adaptive.adaptive_enabled}")

    results['adaptive'] = {
        'result': result_adaptive,
        'time': avg_time,
        'conflicts': solver_adaptive.stats.conflicts,
        'restarts': solver_adaptive.stats.restarts,
        'adaptive_enabled': solver_adaptive.adaptive_enabled,
    }

    # Comparison
    print(f"\n  Comparison:")
    print(f"    No random:  {results['no_random']['time']:.4f}s")
    print(f"    Static 5%:  {results['static']['time']:.4f}s")
    print(f"    Adaptive:   {results['adaptive']['time']:.4f}s")

    if results['adaptive']['time'] > 0:
        # Compare adaptive vs no random
        speedup_vs_no = results['no_random']['time'] / results['adaptive']['time']
        # Compare adaptive vs static
        speedup_vs_static = results['static']['time'] / results['adaptive']['time']

        print(f"\n  Speedup with adaptive:")
        print(f"    vs No random: {speedup_vs_no:.2f}×")
        print(f"    vs Static 5%: {speedup_vs_static:.2f}×")

        if solver_adaptive.adaptive_enabled:
            print(f"    ✅ Adaptive ENABLED (detected stuck state)")
        else:
            print(f"    ✅ Adaptive NOT enabled (instance too easy)")

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

    total_time_no = sum(r['no_random']['time'] for _, r in all_results)
    total_time_static = sum(r['static']['time'] for _, r in all_results)
    total_time_adaptive = sum(r['adaptive']['time'] for _, r in all_results)

    print(f"NO RANDOM PHASE (baseline):")
    print(f"  Total time: {total_time_no:.3f}s")
    print()

    print(f"STATIC 5% RANDOM PHASE (Week 6):")
    print(f"  Total time: {total_time_static:.3f}s")
    print()

    print(f"ADAPTIVE RANDOM PHASE (Week 7 - NEW!):")
    print(f"  Total time: {total_time_adaptive:.3f}s")
    adaptive_count = sum(1 for _, r in all_results if r['adaptive']['adaptive_enabled'])
    print(f"  Adaptive enabled on: {adaptive_count}/{len(all_results)} instances")
    print()

    if total_time_adaptive > 0:
        speedup_vs_no = total_time_no / total_time_adaptive
        speedup_vs_static = total_time_static / total_time_adaptive

        print(f"OVERALL SPEEDUP:")
        print(f"  Adaptive vs No random: {speedup_vs_no:.2f}×")
        print(f"  Adaptive vs Static 5%: {speedup_vs_static:.2f}×")
        print()

        if speedup_vs_no > 1.0 and speedup_vs_static >= 0.95:
            print("  ✅ BEST OF BOTH WORLDS!")
            print("     - Faster than no random (helps on stuck instances)")
            print("     - Similar/better than static 5% (no overhead on easy instances)")
        elif speedup_vs_no > 1.0:
            print("  ✅ Better than no random")
        elif speedup_vs_static > 1.0:
            print("  ✅ Better than static 5%")

    print()
    return all_results


def main():
    """Run adaptive random phase comparison benchmarks."""
    print("="*70)
    print(" ADAPTIVE RANDOM PHASE SELECTION BENCHMARK")
    print(" Comparing: No Random vs Static 5% vs Adaptive")
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
