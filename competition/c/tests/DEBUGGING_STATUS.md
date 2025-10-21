# C CDCL Debugging Session Status - UPDATED

## Current Status: 8/9 Tests Passing (88.9%)

All 9 instances are SAT, but C solver claims one is UNSAT (random3sat_v15_c64.cnf with original code).

## Progress Made This Session

- ✅ Fixed infinite loop (by not removing variables from `seen` set)
- ✅ Deep-traced conflict analysis to understand resolution process
- ✅ Tested all 4 literal polarity combinations systematically
- ✅ Compared learned clauses between Python and C
- ✅ Verified DIMACS parsing is correct
- ✅ Verified literal encoding is correct

## Tested Literal Polarity Combinations

Tested all 4 combinations of negation at two critical lines in `src/analyze.c`:

| Option | Line 90 (earlier levels) | Line 205 (1UIP) | Result | Fails On |
|--------|--------------------------|-----------------|--------|----------|
| 1 (original) | AS-IS | NEGATE | 8/9 | random3sat_v15_c64.cnf |
| 2 | NEGATE | NEGATE | 8/9 | random3sat_v30_c129.cnf |
| 3 | AS-IS | AS-IS | 6/9 | v20, v30, v7 |
| 4 | NEGATE | AS-IS | 8/9 | random3sat_v30_c129.cnf |

**Conclusion**: The bug is NOT a simple polarity inversion at these two lines. All options peak at 8/9.

## What We Know

### Literal Encoding (Verified):
```c
// In dimacs.c:
Literal lit = LIT_FROM_VAR(var, negated);  // negated = (lit_int < 0)
// DIMACS -5 → LIT_FROM_VAR(5, true) → literal with LSB=1 → represents ¬x5 ✓
// DIMACS  5 → LIT_FROM_VAR(5, false) → literal with LSB=0 → represents x5 ✓

// In analyze.c:
resolve_lit = LIT_FROM_VAR(var, assign->value);
// If x=True: LIT_FROM_VAR(x, true) → ¬x (the FALSE literal when x=True) ✓
// If x=False: LIT_FROM_VAR(x, false) → x (the FALSE literal when x=False) ✓
```

### Unit Propagation (Verified):
```c
// In propagate.c line 118:
bool unit_value = !LIT_IS_NEG(w0);
// Positive literal x → assigns x=True ✓
// Negated literal ¬x → assigns x=False ✓
```

### Python Comparison:
Python (correct implementation) adds literals from reason clauses AS-IS:
```python
# Line 625: literals from earlier levels
learned_literals.append(Literal(lit.variable, lit.negated))  # AS-IS

# Line 638: 1UIP literal
learned_literals.append(Literal(assignment.variable, assignment.value))
# Creates FALSE literal under current assignment
```

## Hypothesis CONFIRMED: The Bug is NOT in Literal Polarity

Since all polarity combinations give either 6/9 or 8/9 (never reaching 9/9), the bug is NOT a simple polarity inversion.

Even option 3 (AS-IS at both lines, matching Python's semantics exactly) fails with 6/9.

**The bug must be in a different part of the codebase:**

1. **Watch list management** - Most likely suspect: learned clauses might not be watched correctly
2. **Backtracking** - State restoration might have issues
3. **Propagation** - Unit propagation might have edge cases
4. **Clause database** - Interaction with clause deletion/LBD scoring
5. **Resolution algorithm** - Some subtle edge case in conflict analysis beyond simple polarity

## Recommended Next Steps

1. **Deep trace comparison**: Run Python and C side-by-side on failing instance with identical decision sequence, compare:
   - Conflict clauses encountered
   - Learned clauses generated
   - Backtrack levels
   - Propagations

2. **Simplify test case**: Try to minimize random3sat_v15_c64.cnf to smallest instance that reproduces bug

3. **Reference implementation**: Compare with MiniSat or similar mature C solver

4. **Assertion checking**: Add assertions to verify invariants:
   - Learned clauses are indeed asserting at backtrack level
   - All literals in learned clause have decision level ≤ current level
   - 1UIP literal is correctly identified

## Files Modified (Uncommitted)

- `competition/c/src/analyze.c` - Currently has Option 2 (both negated)

To revert to original:
```bash
cd /Users/msharpe/python/bsat/competition/c
git checkout src/analyze.c
```

## Token Usage

Approximately 100K/200K tokens used in deep debugging session.

## Final Summary

**Achievement**: Improved from 0% (infinite loops) to 88.9% (8/9 tests passing)

**Status**: Bug is NOT in literal polarity. All evidence points to an issue in:
- Watch list management (most likely)
- Propagation/backtracking logic
- Some other part of the CDCL algorithm

**Recommendation**: Focus next debugging session on watch lists and propagation rather than conflict analysis.

**Best Configuration**: Original code (option 1) gives 8/9 passing.
