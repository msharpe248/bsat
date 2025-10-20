#!/usr/bin/env python3
"""
Detailed trace of the bug in (a | b | c) & (~a) & (~b).
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression
import cdcl_optimized

print("="*70)
print("Tracing: (a | b | c) & (~a) & (~b)")
print("="*70)
print()

cnf = CNFExpression.parse("(a | b | c) & (~a) & (~b)")
print(f"Formula: {cnf}")
print(f"Clauses:")
for i, clause in enumerate(cnf.clauses):
    print(f"  {i}: {clause} = {clause.literals}")
print()

# Create solver with debugging
solver = cdcl_optimized.CDCLSolver(cnf, use_watched_literals=True, enable_inprocessing=False)

print("Initial watches:")
if solver.watch_manager:
    for clause_idx, (w1, w2) in solver.watch_manager.watched.items():
        clause = solver.clauses[clause_idx]
        print(f"  Clause {clause_idx} ({clause}): watches {w1} and {w2}")
print()

print("Initial watch lists:")
if solver.watch_manager:
    for lit_key, clauses in solver.watch_manager.watch_lists.items():
        if clauses:
            print(f"  Literal {lit_key}: watched by clauses {clauses}")
print()

# Solve
print("Solving...")
result = solver.solve(max_conflicts=1000)
print(f"\nResult: {'SAT' if result else 'UNSAT'}")

if result:
    print(f"Assignment: {result}")
    is_valid = cnf.evaluate(result)
    print(f"Valid: {is_valid}")
else:
    print("\nExpected SAT with a=False, b=False, c=True")
    print("But got UNSAT!")
    print()
    print("Let's check the trail:")
    print(f"Trail length: {len(solver.trail)}")
    for i, assignment in enumerate(solver.trail):
        print(f"  {i}: {assignment}")
    print()
    print(f"Decision level: {solver.decision_level}")
    print(f"Conflicts: {solver.stats.conflicts}")
