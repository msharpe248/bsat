#!/usr/bin/env python3
"""
Test script for CGPM-SAT graph metrics caching.
Should show high cache hit rate (> 90%) and reduced overhead
"""

import sys
import os
import time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from cgpm_sat import CGPMSolver
from benchmark import ProblemGenerator

def test_cgpm_caching():
    print("=" * 70)
    print("Testing CGPM-SAT Graph Metrics Caching")
    print("=" * 70)

    # Test 1: Easy problem
    print("\n1. Easy SAT Problem (20 vars, ratio=3.0)")
    cnf = ProblemGenerator.random_3sat(20, 60, seed=42)

    start = time.time()
    solver = CGPMSolver(cnf)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Decisions: {stats['decisions_made']}")
    print(f"   Graph queries: {stats['graph_queries']}")
    print(f"   Cache hits: {stats['graph_cache_hits']}")
    print(f"   Cache misses: {stats['graph_cache_misses']}")
    print(f"   Cache hit rate: {stats['cache_hit_rate']:.1f}%")
    print(f"   Graph overhead: {stats['graph_overhead_percentage']:.1f}%")
    print(f"   Expected: High cache hit rate (> 80%)")

    # Test 2: Medium problem with more decisions
    print("\n2. Medium SAT Problem (30 vars, ratio=3.5)")
    cnf = ProblemGenerator.random_3sat(30, 105, seed=42)

    start = time.time()
    solver = CGPMSolver(cnf)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Decisions: {stats['decisions_made']}")
    print(f"   Graph queries: {stats['graph_queries']}")
    print(f"   Cache hits: {stats['graph_cache_hits']}")
    print(f"   Cache misses: {stats['graph_cache_misses']}")
    print(f"   Cache hit rate: {stats['cache_hit_rate']:.1f}%")
    print(f"   Graph overhead: {stats['graph_overhead_percentage']:.1f}%")
    print(f"   Graph updates: {stats['graph_updates']}")
    print(f"   Expected: High cache hit rate, some updates")

    # Test 3: Structured problem (modular)
    print("\n3. Modular Problem (3 modules × 5 vars)")
    cnf = ProblemGenerator.modular_problem(3, 5, 3, seed=42)

    start = time.time()
    solver = CGPMSolver(cnf)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Decisions: {stats['decisions_made']}")
    print(f"   Graph queries: {stats['graph_queries']}")
    print(f"   Cache hits: {stats['graph_cache_hits']}")
    print(f"   Cache misses: {stats['graph_cache_misses']}")
    print(f"   Cache hit rate: {stats['cache_hit_rate']:.1f}%")
    print(f"   Graph influence rate: {stats['graph_influence_rate']:.1f}%")

    # Test 4: Test with different update frequencies
    print("\n4. Comparing Update Frequencies (20 vars)")
    cnf = ProblemGenerator.random_3sat(20, 60, seed=42)

    # Frequent updates (every 5 clauses)
    start = time.time()
    solver_freq5 = CGPMSolver(cnf, update_frequency=5)
    result_freq5 = solver_freq5.solve()
    elapsed_freq5 = time.time() - start
    stats_freq5 = solver_freq5.get_statistics()

    print(f"   Update freq=5:")
    print(f"     Time: {elapsed_freq5:.4f}s")
    print(f"     Cache hit rate: {stats_freq5['cache_hit_rate']:.1f}%")
    print(f"     Graph updates: {stats_freq5['graph_updates']}")
    print(f"     Graph overhead: {stats_freq5['graph_overhead_percentage']:.1f}%")

    # Less frequent updates (every 20 clauses, default=10)
    start = time.time()
    solver_freq20 = CGPMSolver(cnf, update_frequency=20)
    result_freq20 = solver_freq20.solve()
    elapsed_freq20 = time.time() - start
    stats_freq20 = solver_freq20.get_statistics()

    print(f"   Update freq=20:")
    print(f"     Time: {elapsed_freq20:.4f}s")
    print(f"     Cache hit rate: {stats_freq20['cache_hit_rate']:.1f}%")
    print(f"     Graph updates: {stats_freq20['graph_updates']}")
    print(f"     Graph overhead: {stats_freq20['graph_overhead_percentage']:.1f}%")
    print(f"   Expected: freq=20 has higher cache hit rate, lower overhead")

    print("\n" + "=" * 70)
    print("✅ Test Complete!")
    print("Expected: Cache hit rate > 80%, reduced overhead with less frequent updates")
    print("=" * 70)

if __name__ == "__main__":
    test_cgpm_caching()
