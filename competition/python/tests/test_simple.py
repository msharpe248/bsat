#!/usr/bin/env python3
"""
Simple debug test for two-watched literals
"""

import sys
import os

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat.cnf import CNFExpression
import cdcl_optimized

# Test 1: Single variable
print("Test 1: Single variable (x)")
cnf = CNFExpression.parse("(x)")
print(f"  Clauses: {cnf.clauses}")
print(f"  Variables: {cnf.get_variables()}")

result = cdcl_optimized.solve_cdcl(cnf, use_watched_literals=True)
print(f"  Result: {result}")
print(f"  Expected: SAT")

if result is None:
    print("  ❌ FAILED - Expected SAT, got UNSAT")
else:
    print(f"  ✅ PASSED - Got SAT: {result}")

# Test 2: Trivial UNSAT
print("\nTest 2: Trivial UNSAT (x) & (~x)")
cnf = CNFExpression.parse("(x) & (~x)")
print(f"  Clauses: {cnf.clauses}")

result = cdcl_optimized.solve_cdcl(cnf, use_watched_literals=True)
print(f"  Result: {result}")
print(f"  Expected: UNSAT")

if result is None:
    print("  ✅ PASSED - Got UNSAT")
else:
    print(f"  ❌ FAILED - Expected UNSAT, got SAT: {result}")

print("\nSimple tests complete!")
