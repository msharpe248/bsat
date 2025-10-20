#!/usr/bin/env python3
"""
Debug simple test failures.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression
import cdcl_optimized

# Test case 1: (a | b) & (~a | ~b)
print("="*70)
print("Test 1: (a | b) & (~a | ~b)")
print("="*70)
print("Expected: SAT with a=T,b=F OR a=F,b=T")
print()

cnf1 = CNFExpression.parse("(a | b) & (~a | ~b)")
print(f"Formula: {cnf1}")
print(f"Clauses: {cnf1.clauses}")
print()

# Test manually
test_assignments = [
    {'a': True, 'b': False},
    {'a': False, 'b': True},
    {'a': True, 'b': True},
    {'a': False, 'b': False}
]

for assignment in test_assignments:
    result = cnf1.evaluate(assignment)
    print(f"  {assignment}: {result}")

print()

# Now solve with CDCL
solver1 = cdcl_optimized.CDCLSolver(cnf1, use_watched_literals=True, enable_inprocessing=False)
result1 = solver1.solve(max_conflicts=1000)
print(f"CDCL result: {'SAT' if result1 else 'UNSAT'}")
if result1:
    print(f"Assignment: {result1}")
    print(f"Verification: {cnf1.evaluate(result1)}")
print()

# Test case 2: (a | b | c) & (~a) & (~b)
print("="*70)
print("Test 2: (a | b | c) & (~a) & (~b)")
print("="*70)
print("Expected: SAT with a=F, b=F, c=T")
print()

cnf2 = CNFExpression.parse("(a | b | c) & (~a) & (~b)")
print(f"Formula: {cnf2}")
print(f"Clauses: {cnf2.clauses}")
print()

# Test expected solution
expected = {'a': False, 'b': False, 'c': True}
result = cnf2.evaluate(expected)
print(f"Expected solution {expected}: {result}")
print()

# Check each clause individually
for i, clause in enumerate(cnf2.clauses):
    clause_result = any(
        (not lit.negated and expected.get(lit.variable, False)) or
        (lit.negated and not expected.get(lit.variable, True))
        for lit in clause.literals
    )
    print(f"  Clause {i} ({clause}): {clause_result}")
print()

# Now solve with CDCL
solver2 = cdcl_optimized.CDCLSolver(cnf2, use_watched_literals=True, enable_inprocessing=False)
result2 = solver2.solve(max_conflicts=1000)
print(f"CDCL result: {'SAT' if result2 else 'UNSAT'}")
if result2:
    print(f"Assignment: {result2}")
    print(f"Verification: {cnf2.evaluate(result2)}")
else:
    print("Solver said UNSAT, but we know it's SAT!")
    print()
    print("Let's try the original CDCL solver:")
    from bsat import solve_cdcl as solve_original
    result_original = solve_original(cnf2, max_conflicts=1000)
    print(f"Original CDCL result: {'SAT' if result_original else 'UNSAT'}")
    if result_original:
        print(f"Assignment: {result_original}")
        print(f"Verification: {cnf2.evaluate(result_original)}")
