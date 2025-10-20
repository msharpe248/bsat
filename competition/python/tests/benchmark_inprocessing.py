#!/usr/bin/env python3
"""
Benchmark Inprocessing Impact

Compares solver performance with and without inprocessing on medium instances.

Expected impact: 2-5× speedup on structured instances with many learned clauses.
"""

import sys
import os
import time
from typing import List, Tuple

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression
from bsat.dimacs import read_dimacs_file
import cdcl_optimized


def benchmark_instance(instance_path: str, instance_name: str) -> Tuple[dict, dict]:
    """
    Benchmark an instance with and without inprocessing.

    Returns:
        Tuple of (result_without_inproc, result_with_inproc)
    """
    print(f"\\n{'='*70}")
    print(f"Benchmarking: {instance_name}")
    print(f"{'='*70}")

    # Load instance
    try:
        cnf = read_dimacs_file(instance_path)
        num_vars = len(cnf.get_variables())
        num_clauses = len(cnf.clauses)
        print(f"  Variables: {num_vars}, Clauses: {num_clauses}")
    except Exception as e:
        print(f"  ❌ Failed to load: {e}")
        return None, None

    results = {}

    # Test WITHOUT inprocessing
    print(f"\\n  [1/2] WITHOUT inprocessing...")
    try:
        solver = cdcl_optimized.CDCLSolver(
            cnf,
            use_watched_literals=True,
            enable_inprocessing=False
        )
        start_time = time.perf_counter()
        result = solver.solve(max_conflicts=10000)
        elapsed = time.perf_counter() - start_time

        results['without'] = {
            'result': 'SAT' if result else 'UNSAT/TIMEOUT',
            'time': elapsed,
            'conflicts': solver.stats.conflicts,
            'decisions': solver.stats.decisions,
            'learned': solver.stats.learned_clauses,
            'deleted': solver.stats.deleted_clauses,
            'restarts': solver.stats.restarts
        }

        print(f"    Result: {results['without']['result']}")
        print(f"    Time: {elapsed:.3f}s")
        print(f"    Conflicts: {solver.stats.conflicts}")
        print(f"    Learned clauses: {solver.stats.learned_clauses}")
        print(f"    Deleted clauses: {solver.stats.deleted_clauses}")
        print(f"    Restarts: {solver.stats.restarts}")

    except Exception as e:
        print(f"    ❌ Error: {e}")
        results['without'] = None

    # Test WITH inprocessing
    print(f"\\n  [2/2] WITH inprocessing...")
    try:
        solver = cdcl_optimized.CDCLSolver(
            cnf,
            use_watched_literals=True,
            enable_inprocessing=True,
            inprocessing_interval=2000  # Every 2000 conflicts
        )
        start_time = time.perf_counter()
        result = solver.solve(max_conflicts=10000)
        elapsed = time.perf_counter() - start_time

        results['with'] = {
            'result': 'SAT' if result else 'UNSAT/TIMEOUT',
            'time': elapsed,
            'conflicts': solver.stats.conflicts,
            'decisions': solver.stats.decisions,
            'learned': solver.stats.learned_clauses,
            'deleted': solver.stats.deleted_clauses,
            'restarts': solver.stats.restarts,
            'inproc_calls': solver.stats.inprocessing_calls,
            'inproc_subsumed': solver.stats.inprocessing_subsumed,
            'inproc_eliminated': solver.stats.inprocessing_eliminated_vars
        }

        print(f"    Result: {results['with']['result']}")
        print(f"    Time: {elapsed:.3f}s")
        print(f"    Conflicts: {solver.stats.conflicts}")
        print(f"    Learned clauses: {solver.stats.learned_clauses}")
        print(f"    Deleted clauses: {solver.stats.deleted_clauses}")
        print(f"    Restarts: {solver.stats.restarts}")
        print(f"    Inprocessing calls: {solver.stats.inprocessing_calls}")
        print(f"    Inprocessing subsumed: {solver.stats.inprocessing_subsumed}")
        print(f"    Inprocessing eliminated vars: {solver.stats.inprocessing_eliminated_vars}")

    except Exception as e:
        print(f"    ❌ Error: {e}")
        import traceback
        traceback.print_exc()
        results['with'] = None

    # Compare
    if results['without'] and results['with']:
        speedup = results['without']['time'] / results['with']['time'] if results['with']['time'] > 0 else 0
        print(f"\\n  SPEEDUP: {speedup:.2f}×")

        if speedup > 1.1:
            print(f"  ✅ Inprocessing helped! ({speedup:.2f}× faster)")
        elif speedup > 0.9:
            print(f"  → Neutral (similar performance)")
        else:
            print(f"  ⚠️  Inprocessing slowed down ({1/speedup:.2f}× slower)")

    return results['without'], results['with']


def main():
    """Run inprocessing benchmarks."""
    print("="*70)
    print(" INPROCESSING BENCHMARK")
    print(" Comparing: Watched+LBD vs Watched+LBD+Inprocessing")
    print("="*70)

    dataset_path = os.path.join(os.path.dirname(__file__), "../../dataset")

    # Test on medium instances that have enough conflicts to trigger inprocessing
    test_instances = [
        ("medium_tests/medium_suite/easy_3sat_v014_c0058.cnf", "easy_3sat_v014_c0058"),
        ("medium_tests/medium_suite/easy_3sat_v016_c0067.cnf", "easy_3sat_v016_c0067"),
        ("medium_tests/medium_suite/easy_3sat_v020_c0084.cnf", "easy_3sat_v020_c0084"),
        ("medium_tests/medium_suite/easy_3sat_v022_c0092.cnf", "easy_3sat_v022_c0092"),
        ("medium_tests/medium_suite/easy_3sat_v024_c0100.cnf", "easy_3sat_v024_c0100"),
    ]

    all_results = []

    for rel_path, name in test_instances:
        full_path = os.path.join(dataset_path, rel_path)
        if os.path.exists(full_path):
            without, with_inproc = benchmark_instance(full_path, name)
            all_results.append((name, without, with_inproc))
        else:
            print(f"\\n⚠️  Skipping {name} (file not found)")

    # Summary
    print(f"\\n{'='*70}")
    print(" SUMMARY")
    print(f"{'='*70}")

    speedups = []
    for name, without, with_inproc in all_results:
        if without and with_inproc:
            speedup = without['time'] / with_inproc['time'] if with_inproc['time'] > 0 else 0
            speedups.append(speedup)
            print(f"{name}: {speedup:.2f}× speedup")

    if speedups:
        avg_speedup = sum(speedups) / len(speedups)
        print(f"\\nAverage speedup: {avg_speedup:.2f}×")

        if avg_speedup > 1.2:
            print("✅ Inprocessing is beneficial overall!")
        elif avg_speedup > 0.9:
            print("→ Inprocessing has neutral impact")
        else:
            print("⚠️  Inprocessing may need tuning")
    else:
        print("No valid results to compare")

    print(f"\\n{'='*70}")
    print(" BENCHMARK COMPLETE!")
    print(f"{'='*70}")


if __name__ == "__main__":
    main()
