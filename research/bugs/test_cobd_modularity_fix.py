#!/usr/bin/env python3
"""
Test script for CoBD-SAT modularity fix.
Should now get Q > 0 on modular problems instead of Q=0.00
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from cobd_sat import CoBDSATSolver
from benchmark import ProblemGenerator

def test_modularity():
    print("=" * 70)
    print("Testing CoBD-SAT Modularity Fix")
    print("=" * 70)

    # Test 1: Modular problem
    print("\n1. Modular Problem (3 modules × 3 vars)")
    cnf = ProblemGenerator.modular_problem(3, 3, 2, seed=42)
    solver = CoBDSATSolver(cnf)
    result = solver.solve()
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Modularity Q: {stats['modularity']:.4f}")
    print(f"   Communities: {stats['num_communities']}")
    print(f"   Interface %: {stats['interface_percentage']:.1f}%")
    print(f"   Used decomposition: {stats['used_decomposition']}")
    if not stats['used_decomposition']:
        print(f"   Fallback reason: {stats['fallback_reason']}")

    # Test 2: Larger modular problem
    print("\n2. Larger Modular Problem (4 modules × 4 vars)")
    cnf = ProblemGenerator.modular_problem(4, 4, 3, seed=42)
    solver = CoBDSATSolver(cnf)
    result = solver.solve()
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Modularity Q: {stats['modularity']:.4f}")
    print(f"   Communities: {stats['num_communities']}")
    print(f"   Interface %: {stats['interface_percentage']:.1f}%")
    print(f"   Used decomposition: {stats['used_decomposition']}")
    if not stats['used_decomposition']:
        print(f"   Fallback reason: {stats['fallback_reason']}")

    # Test 3: Random problem (should have low Q)
    print("\n3. Random 3-SAT (should have low Q)")
    cnf = ProblemGenerator.random_3sat(10, 35, seed=42)
    solver = CoBDSATSolver(cnf)
    result = solver.solve()
    stats = solver.get_statistics()

    print(f"   Result: {'SAT' if result else 'UNSAT'}")
    print(f"   Modularity Q: {stats['modularity']:.4f}")
    print(f"   Communities: {stats['num_communities']}")
    print(f"   Interface %: {stats['interface_percentage']:.1f}%")
    print(f"   Used decomposition: {stats['used_decomposition']}")
    if not stats['used_decomposition']:
        print(f"   Fallback reason: {stats['fallback_reason']}")

    print("\n" + "=" * 70)
    print("✅ Test Complete!")
    print("Expected: Modular problems should have Q > 0 (was Q=0.00 before fix)")
    print("=" * 70)

if __name__ == "__main__":
    test_modularity()
