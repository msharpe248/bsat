# Python CDCL Solver Bug Report - FIXED

## Summary

The Python CDCL solver (`src/bsat/cdcl.py`) had a **critical soundness bug** that caused it to incorrectly report SAT instances as UNSAT.

**STATUS: ✅ FIXED** - All bugs identified and corrected.

## Evidence

**Instance:** `dataset/simple_tests/simple_suite/random3sat_v15_c64.cnf`

**Test Results:**
- Python CDCL: **UNSATISFIABLE** ❌ WRONG (before fix)
- Python DPLL: **SATISFIABLE** ✅ CORRECT

**Verified Solutions:**
Multiple valid satisfying assignments exist and have been verified:

```python
# Solution 1 (from DPLL):
{'x1': True, 'x2': True, 'x3': True, 'x4': True, 'x5': True,
 'x6': False, 'x7': False, 'x8': False, 'x9': True, 'x10': False,
 'x11': True, 'x12': True, 'x13': True, 'x14': True, 'x15': True}
```

Both solutions have been verified as valid: `cnf.evaluate(solution) == True`

## Bugs Found

### Bug 1: Accessing trail[-1] when search fails (FIXED)

**Location:** `src/bsat/cdcl.py:249-264`

**Problem:**
```python
while antecedent_idx >= 0:
    assignment = self.trail[antecedent_idx]
    if assignment.variable in seen and assignment.decision_level == self.decision_level:
        break
    antecedent_idx -= 1

counter -= 1
if counter <= 0:
    assignment = self.trail[antecedent_idx]  # BUG: accesses trail[-1] if not found!
```

If the search loop doesn't find a variable, `antecedent_idx` becomes `-1`. Python's negative indexing means `trail[-1]` accesses the LAST element instead of indicating an error.

**Fix Applied:**
```python
# Check if we found a variable
if antecedent_idx < 0:
    break

counter -= 1
if counter == 0:  # Changed from <= to ==
    assignment = self.trail[antecedent_idx]
```

### Bug 2: Incorrect learned clause construction (FIXED)

**Location:** `src/bsat/cdcl.py:246` and `:264`

**Problem:**
Learned clauses were constructed with incorrect literal polarities:

1. **Line 246:** Literals from earlier decision levels were being negated when added to learned clause:
   ```python
   # WRONG:
   learned_literals.append(Literal(lit.variable, not lit.negated))
   ```
   This caused literals that should be `¬x` to become `x`, making learned clauses satisfied (not unit) after backtracking.

2. **Line 264:** The 1UIP asserting literal was constructed backwards:
   ```python
   # WRONG:
   learned_literals.append(Literal(assignment.variable, not assignment.value))
   ```
   If `x10=True` in assignment, we want `¬x10` in learned clause, but this created positive `x10`.

**Result:** Learned clause like `(x1 ∨ x10)` instead of correct `(¬x1 ∨ ¬x10)`, causing:
- Clause satisfied immediately after backtracking (not unit)
- No propagation of learned knowledge
- Same conflict repeated infinitely
- Eventually claim UNSAT when instance is SAT

**Fix Applied:**
```python
# Line 246 - Add literal as-is from conflict clause:
learned_literals.append(lit)

# Line 264 - Negate assignment value correctly:
learned_literals.append(Literal(assignment.variable, assignment.value))
```

Now learned clauses are asserting at backtrack level as required.

### Bug 3: Missing propagation after backtracking (FIXED)

**Location:** `src/bsat/cdcl.py:406-423`

**Problem:**
After backtracking and adding a learned clause, the solver did not propagate before making the next decision:

```python
# WRONG:
# Add learned clause
self._add_learned_clause(learned_clause)

# Backtrack
self._unassign_to_level(backtrack_level)

# ... decay VSIDS ...

# Check for restart
if self._should_restart():
    self._restart()
    conflict = self._propagate()  # ← Only propagated on restart!
```

Propagation only happened if a restart occurred. Otherwise, the solver immediately made a new decision without propagating the learned clause.

**Result:** Even with correct learned clause, it wasn't used to prevent repeating the same conflict.

**Fix Applied:**
```python
# Backtrack BEFORE adding learned clause
self._unassign_to_level(backtrack_level)

# Add learned clause
self._add_learned_clause(learned_clause)

# CRITICAL: Propagate immediately
conflict = self._propagate()
if conflict is not None:
    # Handle new conflict
    ...
```

Now propagation always happens after learning, ensuring learned clauses prevent redundant conflicts.

## Impact

**Severity:** CRITICAL - Soundness violation (WAS)
**Type:** False UNSAT (claiming unsatisfiable when solution exists)
**Instances Affected:** `random3sat_v15_c64.cnf`, `random3sat_v30_c129.cnf`, and others

**Resolution:** ✅ All bugs fixed. Python CDCL now passes all correctness tests.

## Test Results After Fix

```
Testing Python CDCL on all simple test instances...
✅ random3sat_v10_c43.cnf: SAT
✅ random3sat_v15_c64.cnf: SAT  ← Previously claimed UNSAT
✅ random3sat_v20_c86.cnf: SAT
✅ random3sat_v30_c129.cnf: SAT  ← Previously timed out
✅ random3sat_v5_c21.cnf: SAT
✅ random3sat_v7_c30.cnf: SAT
✅ sat_v10.cnf: SAT
✅ sat_v20.cnf: SAT
✅ sat_v30.cnf: SAT

Results: 9/9 passed
✅ ALL TESTS PASSED!
```

## Lessons Learned

1. **Literal polarity in learned clauses is critical** - Negating when you shouldn't completely breaks CDCL
2. **Propagation after backtracking is mandatory** - Without it, learned clauses are useless
3. **Detailed debugging traces are essential** - The bugs were subtle and required step-by-step analysis
4. **Testing against reference implementations** - Comparing with other solvers can reveal subtle differences
