#!/usr/bin/env python3
"""
Test script for BB-CDCL early UNSAT detection fix.
Should now be fast on UNSAT (0.01s) instead of slow (6.3s).
"""

import sys
import os
import time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from bb_cdcl import BBCDCLSolver
from benchmark import ProblemGenerator

def test_unsat_detection():
    print("=" * 70)
    print("Testing BB-CDCL Early UNSAT Detection Fix")
    print("=" * 70)

    # Test 1: UNSAT problem (should be fast now!)
    print("\n1. Strong Backbone UNSAT (18 vars, 70% backbone)")
    cnf = ProblemGenerator.backbone_problem(18, 0.7, seed=44)

    start = time.time()
    solver = BBCDCLSolver(cnf, num_samples=100)
    result = solver.solve()
    elapsed = time.time() - start

    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s (was 6.3s before fix!)")
    print(f"   Quick UNSAT detected: {stats.get('quick_unsat_detected', False)}")
    print(f"   Quick check time: {stats.get('quick_check_time', 0):.4f}s")
    print(f"   Samples taken: {stats.get('num_samples', 0)} (was 100 before fix!)")
    print(f"   Backbone detection time: {stats.get('backbone_detection_time', 0):.4f}s")

    # Test 2: SAT problem (should still work normally)
    print("\n2. Backbone Problem SAT (15 vars, 50% backbone)")
    cnf = ProblemGenerator.backbone_problem(15, 0.5, seed=42)

    start = time.time()
    solver = BBCDCLSolver(cnf, num_samples=100)
    result = solver.solve()
    elapsed = time.time() - start

    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s")
    print(f"   Quick UNSAT detected: {stats.get('quick_unsat_detected', False)}")
    print(f"   Samples taken: {stats.get('num_samples', 0)}")
    print(f"   Backbone detected: {stats.get('num_backbone_detected', 0)} ({stats.get('backbone_percentage', 0):.1f}%)")

    # Test 3: Easy UNSAT
    print("\n3. Simple UNSAT (a) & (~a)")
    cnf = CNFExpression.parse("(a) & (~a)")

    start = time.time()
    solver = BBCDCLSolver(cnf, num_samples=100)
    result = solver.solve()
    elapsed = time.time() - start

    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Time: {elapsed:.4f}s (should be very fast!)")
    print(f"   Quick UNSAT detected: {stats.get('quick_unsat_detected', False)}")
    print(f"   Samples taken: {stats.get('num_samples', 0)}")

    print("\n" + "=" * 70)
    print("✅ Test Complete!")
    print("Expected: UNSAT instances should be fast (< 0.1s) vs slow (6.3s before)")
    print("=" * 70)

if __name__ == "__main__":
    test_unsat_detection()
