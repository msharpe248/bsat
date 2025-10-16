#!/usr/bin/env python3
"""
Test script for LA-CDCL adaptive lookahead frequency.
Should adjust frequency based on conflict rate: high conflict = freq 1, low conflict = freq 8
"""

import sys
import os
import time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from la_cdcl import LACDCLSolver
from benchmark import ProblemGenerator

def test_adaptive_lookahead():
    print("=" * 70)
    print("Testing LA-CDCL Adaptive Lookahead Frequency")
    print("=" * 70)

    # Test 1: Easy SAT problem (low conflicts expected)
    print("\n1. Easy SAT Problem (30 vars, ratio=3.0)")
    cnf = ProblemGenerator.random_3sat(30, 90, seed=42)  # Well below phase transition

    # With adaptive lookahead
    start = time.time()
    solver = LACDCLSolver(cnf, adaptive_lookahead=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Decisions: {stats['decisions_made']}")
    print(f"   Conflicts: {stats['conflicts_total']} ({stats['conflict_rate']:.1f}%)")
    print(f"   Lookahead used: {stats['lookahead_used']} ({stats['lookahead_percentage']:.1f}%)")
    print(f"   Avg frequency: {stats['avg_lookahead_frequency']:.1f}")
    print(f"   Final frequency: {stats['current_frequency']}")
    print(f"   Frequency adjustments: {stats['frequency_adjustments']}")
    print(f"   Expected: Low conflict rate → higher frequency (less lookahead)")

    # Compare with fixed frequency=1
    start = time.time()
    solver_fixed = LACDCLSolver(cnf, lookahead_frequency=1, adaptive_lookahead=False)
    result_fixed = solver_fixed.solve()
    elapsed_fixed = time.time() - start
    stats_fixed = solver_fixed.get_statistics()

    print(f"   Fixed freq=1: {elapsed_fixed:.4f}s ({stats_fixed['lookahead_used']} lookaheads)")
    speedup = elapsed_fixed / elapsed if elapsed > 0 else 1.0
    print(f"   Speedup: {speedup:.2f}×")

    # Test 2: Hard SAT problem near phase transition (high conflicts expected)
    print("\n2. Hard SAT Problem (30 vars, ratio=4.2)")
    cnf = ProblemGenerator.random_3sat(30, 126, seed=42)  # Near phase transition

    start = time.time()
    solver = LACDCLSolver(cnf, adaptive_lookahead=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Decisions: {stats['decisions_made']}")
    print(f"   Conflicts: {stats['conflicts_total']} ({stats['conflict_rate']:.1f}%)")
    print(f"   Lookahead used: {stats['lookahead_used']} ({stats['lookahead_percentage']:.1f}%)")
    print(f"   Avg frequency: {stats['avg_lookahead_frequency']:.1f}")
    print(f"   Final frequency: {stats['current_frequency']}")
    print(f"   Frequency adjustments: {stats['frequency_adjustments']}")
    print(f"   Expected: High conflict rate → lower frequency (more lookahead)")

    # Test 3: Very easy problem (sparse)
    print("\n3. Very Easy Problem (25 vars, ratio=2.5)")
    cnf = ProblemGenerator.random_3sat(25, 63, seed=42)

    start = time.time()
    solver = LACDCLSolver(cnf, adaptive_lookahead=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Decisions: {stats['decisions_made']}")
    print(f"   Conflicts: {stats['conflicts_total']} ({stats['conflict_rate']:.1f}%)")
    print(f"   Lookahead used: {stats['lookahead_used']} ({stats['lookahead_percentage']:.1f}%)")
    print(f"   Avg frequency: {stats['avg_lookahead_frequency']:.1f}")
    print(f"   Final frequency: {stats['current_frequency']}")
    print(f"   Expected: Very low conflict rate → freq=8 (minimal lookahead)")

    # Test 4: Modular problem (should have low conflicts within modules)
    print("\n4. Modular Problem (3 modules × 5 vars)")
    cnf = ProblemGenerator.modular_problem(3, 5, 3, seed=42)

    start = time.time()
    solver = LACDCLSolver(cnf, adaptive_lookahead=True)
    result = solver.solve()
    elapsed = time.time() - start
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Decisions: {stats['decisions_made']}")
    print(f"   Conflicts: {stats['conflicts_total']} ({stats['conflict_rate']:.1f}%)")
    print(f"   Lookahead used: {stats['lookahead_used']} ({stats['lookahead_percentage']:.1f}%)")
    print(f"   Avg frequency: {stats['avg_lookahead_frequency']:.1f}")
    print(f"   Final frequency: {stats['current_frequency']}")
    print(f"   Frequency adjustments: {stats['frequency_adjustments']}")

    print("\n" + "=" * 70)
    print("✅ Test Complete!")
    print("Expected: Adaptive frequency reduces overhead on easy problems")
    print("High conflict → freq=1 (always lookahead)")
    print("Low conflict → freq=8 (rarely lookahead)")
    print("=" * 70)

if __name__ == "__main__":
    test_adaptive_lookahead()
