#!/usr/bin/env python3
"""
Test script for BB-CDCL adaptive sampling.
Should adjust sample count based on problem difficulty: 10-20 (easy), 30-50 (medium), 80-120 (hard)
"""

import sys
import os
import time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from bb_cdcl import BBCDCLSolver
from benchmark import ProblemGenerator

def test_adaptive_sampling():
    print("=" * 70)
    print("Testing BB-CDCL Adaptive Sampling")
    print("=" * 70)

    # Test 1: Easy problem (small, low ratio)
    print("\n1. Easy Problem (10 vars, ratio=2.5)")
    cnf = ProblemGenerator.random_3sat(10, 25, seed=42)  # 10 vars, 25 clauses = 2.5 ratio

    # With adaptive sampling
    start = time.time()
    solver = BBCDCLSolver(cnf, adaptive_sampling=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Difficulty: {stats['problem_difficulty']}")
    print(f"   Samples used: {stats['num_samples']} (adaptive)")
    print(f"   Expected: 10-20 samples for easy problem")

    # Compare with fixed 100 samples
    start = time.time()
    solver_fixed = BBCDCLSolver(cnf, num_samples=100, adaptive_sampling=False)
    result_fixed = solver_fixed.solve()
    elapsed_fixed = time.time() - start
    stats_fixed = solver_fixed.get_statistics()

    print(f"   Fixed 100 samples: {elapsed_fixed:.4f}s ({stats_fixed['num_samples']} samples)")
    speedup = elapsed_fixed / elapsed if elapsed > 0 else 1.0
    print(f"   Speedup: {speedup:.2f}×")

    # Test 2: Medium problem
    print("\n2. Medium Problem (30 vars, ratio=4.0)")
    cnf = ProblemGenerator.random_3sat(30, 120, seed=42)  # 30 vars, 120 clauses = 4.0 ratio

    start = time.time()
    solver = BBCDCLSolver(cnf, adaptive_sampling=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Difficulty: {stats['problem_difficulty']}")
    print(f"   Samples used: {stats['num_samples']} (adaptive)")
    print(f"   Expected: 30-50 samples for medium problem")

    # Test 3: Hard problem (large size)
    print("\n3. Hard Problem (60 vars, ratio=4.2)")
    cnf = ProblemGenerator.random_3sat(60, 250, seed=42)  # 60 vars, 250 clauses = 4.17 ratio

    start = time.time()
    solver = BBCDCLSolver(cnf, adaptive_sampling=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Difficulty: {stats['problem_difficulty']}")
    print(f"   Samples used: {stats['num_samples']} (adaptive)")
    print(f"   Expected: 80-120 samples for hard problem")

    # Test 4: Very hard problem (high ratio)
    print("\n4. Very Hard Problem (20 vars, ratio=5.0)")
    cnf = ProblemGenerator.random_3sat(20, 100, seed=42)  # 20 vars, 100 clauses = 5.0 ratio

    start = time.time()
    solver = BBCDCLSolver(cnf, adaptive_sampling=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Difficulty: {stats['problem_difficulty']}")
    print(f"   Samples used: {stats['num_samples']} (adaptive)")
    print(f"   Expected: 80-120 samples for hard problem (high ratio)")

    # Test 5: Backbone problem (should detect backbone efficiently)
    print("\n5. Backbone Problem with Adaptive Sampling (15 vars, 50% backbone)")
    cnf = ProblemGenerator.backbone_problem(15, 0.5, seed=42)

    start = time.time()
    solver = BBCDCLSolver(cnf, adaptive_sampling=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Difficulty: {stats['problem_difficulty']}")
    print(f"   Samples used: {stats['num_samples']} (adaptive)")
    print(f"   Backbone detected: {stats['num_backbone_detected']} ({stats['backbone_percentage']:.1f}%)")

    print("\n" + "=" * 70)
    print("✅ Test Complete!")
    print("Expected: Easy problems use fewer samples (10-20) vs hard (80-120)")
    print("=" * 70)

if __name__ == "__main__":
    test_adaptive_sampling()
