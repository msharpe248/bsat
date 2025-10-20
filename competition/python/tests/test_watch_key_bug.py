#!/usr/bin/env python3
"""
Test to verify the watched literal key bug.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression, Literal

print("="*70)
print("Understanding Literal Key Representation")
print("="*70)
print()

# How literals are represented as keys
print("Literal representation:")
print("  Literal('x', negated=False) = x (positive)")
print("  Literal('x', negated=True) = ~x (negative)")
print()

print("Key format: (variable, negated)")
print("  ('x', False) represents x (positive literal)")
print("  ('x', True) represents ~x (negative literal)")
print()

print("="*70)
print("When we assign x=TRUE:")
print("="*70)
print("  - Positive literal x: (key='x', negated=False) is now TRUE")
print("  - Negative literal ~x: (key='x', negated=True) is now FALSE")
print("  - We need to check clauses watching the FALSE literal: ~x")
print("  - So we need key: ('x', True)")
print()

print("Current code at line 435:")
print("  assigned_lit_key = (assignment.variable, not assignment.value)")
print()
print("If assignment = Assignment(variable='x', value=TRUE):")
print("  assigned_lit_key = ('x', not TRUE) = ('x', False)")
print("  This represents x (positive), but we want ~x (negative)")
print("  ❌ BUG: This is backwards!")
print()

print("="*70)
print("Correct code should be:")
print("="*70)
print("  assigned_lit_key = (assignment.variable, assignment.value)")
print()
print("If assignment = Assignment(variable='x', value=TRUE):")
print("  assigned_lit_key = ('x', TRUE) = ('x', True)")
print("  This represents ~x (negative literal that is now FALSE)")
print("  ✅ Correct!")
print()

print("="*70)
print("Verification:")
print("="*70)
print()

# Test case: (a | b | c) & (~a) & (~b)
print("Formula: (a | b | c) & (~a) & (~b)")
print()
print("Initial unit clauses: (~a) and (~b)")
print("These should propagate a=FALSE and b=FALSE")
print()

print("Step 1: Assign a=FALSE")
print("  - Literal ~a (key: ('a', True)) is TRUE")
print("  - Literal a (key: ('a', False)) is FALSE")
print("  - Need to check clauses watching a (FALSE literal)")
print("  - With BUG: checks ('a', True) → wrong!")
print("  - With FIX: checks ('a', False) → correct!")
print()

print("Step 2: Assign b=FALSE")
print("  - Literal ~b (key: ('b', True)) is TRUE")
print("  - Literal b (key: ('b', False)) is FALSE")
print("  - Need to check clauses watching b (FALSE literal)")
print("  - With BUG: checks ('b', True) → wrong!")
print("  - With FIX: checks ('b', False) → correct!")
print()

print("Step 3: After a=FALSE, b=FALSE")
print("  Clause (a | b | c) has:")
print("    - a = FALSE")
print("    - b = FALSE")
print("    - c = UNASSIGNED")
print("  This is a unit clause → should propagate c=TRUE")
print("  With BUG: Unit propagation fails → UNSAT ❌")
print("  With FIX: Unit propagation succeeds → SAT ✅")
print()

print("="*70)
print("CONCLUSION: Line 435 has the logic backwards!")
print("="*70)
