#!/usr/bin/env python3
"""
Quick test for inprocessing integration.
"""

import sys
import os

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression
import cdcl_optimized

# Test 1: Simple SAT instance
print("Test 1: Simple SAT instance")
cnf = CNFExpression.parse("(a | b) & (a | ~b) & (~a | c)")
solver = cdcl_optimized.CDCLSolver(
    cnf,
    use_watched_literals=True,
    enable_inprocessing=True,
    inprocessing_interval=100
)
result = solver.solve(max_conflicts=1000)
print(f"Result: {'SAT' if result else 'UNSAT'}")
print(f"Stats: {solver.stats}")
print()

# Test 2: Instance that needs subsumption
print("Test 2: Instance with subsumable clauses")
# (a | b) subsumes (a | b | c)
cnf2 = CNFExpression.parse("(a | b) & (a | b | c) & (~a | ~b)")
solver2 = cdcl_optimized.CDCLSolver(
    cnf2,
    use_watched_literals=True,
    enable_inprocessing=True,
    inprocessing_interval=100
)
result2 = solver2.solve(max_conflicts=1000)
print(f"Result: {'SAT' if result2 else 'UNSAT'}")
print(f"Stats: {solver2.stats}")
print()

# Test 3: Larger instance from benchmark
print("Test 3: Benchmark instance (if available)")
try:
    from bsat.dimacs import read_dimacs_file
    cnf3 = read_dimacs_file("../../dataset/simple_tests/simple_suite/sat_v10.cnf")
    solver3 = cdcl_optimized.CDCLSolver(
        cnf3,
        use_watched_literals=True,
        enable_inprocessing=True,
        inprocessing_interval=50
    )
    result3 = solver3.solve(max_conflicts=1000)
    print(f"Result: {'SAT' if result3 else 'UNSAT'}")
    print(f"Stats: {solver3.stats}")
except Exception as e:
    print(f"Skipping benchmark test: {e}")
print()

print("✅ All tests completed!")
