# SAT Preprocessing Techniques

**Purpose**: Simplify CNF formula before main CDCL search
**Impact**: Can reduce problem size by 50-90% on some instances
**Status**: ✅ Implemented in C solver (BCE, unit propagation, pure literals)

---

## Overview

**Preprocessing** = Formula simplification before search begins

**Goal**: Transform the formula into an equivalent or equisatisfiable simpler form

**Benefits**:
- Fewer variables → smaller search space
- Fewer clauses → faster propagation
- Sometimes solves problem entirely (reduces to empty or unit clauses)

**Common techniques**:
1. Unit propagation at level 0
2. Pure literal elimination
3. Blocked Clause Elimination (BCE)
4. Variable elimination (not yet implemented)
5. Subsumption and self-subsumption (not yet implemented)

---

## Unit Propagation (Level 0)

### What It Does

**Goal**: Find and propagate unit clauses before search

**Unit clause** = Clause with only 1 literal: `(x)` or `(¬x)`

**Propagation**: If `(x)` is a clause, then x must be TRUE

### Algorithm

```c
bool preprocess_unit_propagation(Solver* s) {
    bool changed = true;

    while (changed) {
        changed = false;

        for (each clause C) {
            if (is_satisfied(C)) continue;

            int unassigned_count = 0;
            Lit unassigned_lit = INVALID_LIT;

            for (each lit in C) {
                if (value(lit) == TRUE) {
                    mark_satisfied(C);
                    break;
                }
                if (value(lit) == UNDEF) {
                    unassigned_count++;
                    unassigned_lit = lit;
                }
            }

            if (unassigned_count == 0) {
                // Conflict at level 0 → UNSAT
                return false;
            }

            if (unassigned_count == 1) {
                // Unit clause! Propagate
                assign(unassigned_lit, 0, C);
                changed = true;
            }
        }
    }

    return true;  // No conflict
}
```

### Effect

**Input**:
```
(x)          ← Unit clause
(¬x ∨ y)
(¬y ∨ z)
```

**After unit propagation**:
```
x = TRUE
(y)          ← New unit clause (from ¬x ∨ y)
(¬y ∨ z)
```

**Continue**:
```
x = TRUE, y = TRUE
(z)          ← New unit clause
```

**Final**:
```
x = TRUE, y = TRUE, z = TRUE
Empty formula (all clauses satisfied)
→ SAT! (solved without search)
```

### Performance Impact

**Typical reduction**: 5-20% of variables assigned
**Best case**: Solves entire problem
**Cost**: O(m × n) where m = clauses, n = avg clause length

---

## Pure Literal Elimination

### What It Does

**Goal**: Find variables that appear with only one polarity

**Pure literal** = Variable that appears only positive or only negative

**Rule**: If x appears only as x (never ¬x), assign x = TRUE

### Algorithm

```c
void preprocess_pure_literals(Solver* s) {
    // Track polarities
    bool appears_pos[num_vars + 1] = {false};
    bool appears_neg[num_vars + 1] = {false};

    // Scan all clauses
    for (each clause C) {
        if (is_satisfied(C)) continue;

        for (each lit in C) {
            Var v = var(lit);
            if (sign(lit) == 0) {
                appears_pos[v] = true;
            } else {
                appears_neg[v] = true;
            }
        }
    }

    // Find and assign pure literals
    for (Var v = 1; v <= num_vars; v++) {
        if (value(v) != UNDEF) continue;  // Already assigned

        if (appears_pos[v] && !appears_neg[v]) {
            // Pure positive
            assign(lit_make(v, 0), 0, INVALID_CREF);
        } else if (appears_neg[v] && !appears_pos[v]) {
            // Pure negative
            assign(lit_make(v, 1), 0, INVALID_CREF);
        }
    }
}
```

### Effect

**Input**:
```
(x ∨ y)      ← x appears positive
(x ∨ z)      ← x appears positive
(¬y ∨ z)
(¬z ∨ w)
```

**Analysis**:
- x: appears only positive → pure positive
- y: appears both positive and negative → not pure
- z: appears both positive and negative → not pure
- w: appears only positive → pure positive

**After elimination**:
```
x = TRUE, w = TRUE

Remaining formula (after simplification):
(¬y ∨ z)     ← Only unsatisfied clause
```

### Performance Impact

**Typical reduction**: 1-10% of variables (rare on SAT competition instances)
**Best case**: 50%+ on special instances (e.g., graph coloring)
**Cost**: O(m × n)

**Note**: Pure literals are rare in modern benchmarks but cheap to find

---

## Blocked Clause Elimination (BCE)

### What It Does

**Goal**: Remove clauses that are "blocked" and cannot contribute to conflicts

**Blocked clause** = Clause C with a literal L such that:
- For every clause D containing ¬L
- The resolvent of C and D is a tautology

**Tautology** = Clause containing both x and ¬x (always true)

### Intuition

If resolving C with any clause containing ¬L produces a tautology, then:
- C can never participate in a useful resolution
- C can be safely removed without affecting satisfiability

### Example

**Formula**:
```
C1: (x ∨ y ∨ z)
C2: (¬x ∨ a)
C3: (¬x ∨ b)
```

**Check if C1 is blocked on x**:

Resolve C1 with C2:
```
(x ∨ y ∨ z) ⊗ (¬x ∨ a) = (y ∨ z ∨ a)
```
Not a tautology.

**Not blocked** (this example doesn't work for BCE)

**Better example**:
```
C1: (x ∨ ¬y ∨ z)
C2: (¬x ∨ y)
```

Resolve C1 with C2:
```
(x ∨ ¬y ∨ z) ⊗ (¬x ∨ y) = (¬y ∨ z ∨ y) = (y ∨ ¬y ∨ z)
```
Contains both y and ¬y → tautology!

**C1 is blocked on x** → can be removed

### Algorithm

```c
void preprocess_bce(Solver* s) {
    for (each clause C) {
        if (is_satisfied(C)) continue;

        for (each literal L in C) {
            bool blocked = true;

            // Check all clauses containing ¬L
            for (each clause D containing neg(L)) {
                if (is_satisfied(D)) continue;

                // Try to resolve C and D on L
                bool is_tautology = false;

                for (each lit1 in C) {
                    if (lit1 == L) continue;

                    for (each lit2 in D) {
                        if (lit2 == neg(L)) continue;

                        if (lit1 == neg(lit2)) {
                            // Found x and ¬x in resolvent
                            is_tautology = true;
                            break;
                        }
                    }
                    if (is_tautology) break;
                }

                if (!is_tautology) {
                    blocked = false;
                    break;
                }
            }

            if (blocked) {
                // C is blocked on L, can remove C
                mark_deleted(C);
                stats.blocked_clauses++;
                break;
            }
        }
    }
}
```

### Performance Impact

**Typical reduction**: 5-30% of clauses on structured instances
**Best case**: 80%+ on some crafted instances
**Cost**: O(m² × n²) in worst case (expensive!)

**Note**: Very effective on application instances (circuit, planning, etc.)

### Optimizations

1. **Occurrence lists**: Precompute which clauses contain each literal
   - Speeds up finding clauses with ¬L
   - O(1) lookup instead of O(m) scan

2. **Early termination**: If resolvent is large, likely not a tautology
   ```c
   if (|C| + |D| - 2 > threshold) skip;
   ```

3. **Asymmetric BCE**: Check only one direction
   - Faster but less powerful
   - Still effective in practice

4. **Lazy evaluation**: Only check popular literals
   - Literals in many clauses are more likely to be blocked

---

## Variable Elimination (Not Yet Implemented)

### What It Does

**Goal**: Eliminate variables by resolution, if result is smaller

**Algorithm**:
1. Choose variable x
2. Resolve all pairs of clauses (C with x, D with ¬x)
3. If total clauses decrease, keep resolvents and delete originals

### Example

**Input**:
```
(x ∨ a)
(x ∨ b)
(¬x ∨ c)
(¬x ∨ d)
```

**Resolve all pairs**:
```
(x ∨ a) ⊗ (¬x ∨ c) = (a ∨ c)
(x ∨ a) ⊗ (¬x ∨ d) = (a ∨ d)
(x ∨ b) ⊗ (¬x ∨ c) = (b ∨ c)
(x ∨ b) ⊗ (¬x ∨ d) = (b ∨ d)
```

**Result**: 4 clauses with x → 4 clauses without x

**Decision**: Don't eliminate (same size, no benefit)

**Better example**:
```
(x ∨ a)
(¬x ∨ b)
```

Resolve: `(a ∨ b)`

**Result**: 2 clauses → 1 clause (eliminate!)

### Performance Impact

**Typical reduction**: 10-50% of variables on structured instances
**Cost**: Very expensive (O(2^n) in worst case)

**Status**: Not implemented (complex, high overhead)

---

## Subsumption (Not Yet Implemented)

### What It Does

**Goal**: Remove subsumed clauses

**Subsumption**: Clause C subsumes D if all literals of C are in D

**Example**:
```
C: (x ∨ y)
D: (x ∨ y ∨ z)
```

C subsumes D (C is stronger, D is redundant)

**Action**: Delete D, keep C

### Algorithm

```c
void preprocess_subsumption(Solver* s) {
    for (each clause C) {
        for (each clause D) {
            if (C == D) continue;

            if (C subsumes D) {
                mark_deleted(D);
                stats.subsumed_clauses++;
            }
        }
    }
}
```

### Performance Impact

**Typical reduction**: 5-20% of clauses
**Cost**: O(m² × n)

**Status**: On-the-fly subsumption is implemented during conflict analysis

---

## Complete Preprocessing Pipeline

### Implementation

```c
bool solver_preprocess(Solver* s) {
    bool progress = true;
    uint32_t iterations = 0;

    while (progress && iterations < MAX_ITERATIONS) {
        progress = false;

        // 1. Unit propagation (cheap, always do)
        if (!preprocess_unit_propagation(s)) {
            return false;  // UNSAT at level 0
        }

        // 2. Pure literal elimination (cheap)
        uint32_t pure_count = preprocess_pure_literals(s);
        if (pure_count > 0) {
            progress = true;
            stats.pure_literals += pure_count;
        }

        // 3. Blocked Clause Elimination (expensive, limit iterations)
        if (opts.use_bce && iterations < MAX_BCE_ITERATIONS) {
            uint32_t blocked_count = preprocess_bce(s);
            if (blocked_count > 0) {
                progress = true;
                stats.blocked_clauses += blocked_count;
            }
        }

        iterations++;
    }

    return true;  // Preprocessing successful
}
```

### Iteration Strategy

**Why iterate?**
- Removing clauses may create new unit clauses
- Assigning variables may create new pure literals
- Changes propagate through the formula

**Example**:
```
Initial: (x), (¬x ∨ y), (¬y ∨ z), (¬z ∨ a)

Iteration 1:
  Unit propagation: x = TRUE
  New unit: (y)

Iteration 2:
  Unit propagation: y = TRUE
  New unit: (z)

Iteration 3:
  Unit propagation: z = TRUE
  New unit: (a)

Final: x=T, y=T, z=T, a=T (solved!)
```

### Termination

**Stop when**:
1. No changes in iteration (fixpoint reached)
2. Maximum iterations exceeded (prevent infinite loop)
3. Conflict detected (UNSAT)

---

## Performance Analysis

### Preprocessing Time vs Solving Time

**Tradeoff**: Preprocessing costs time upfront but speeds up search

**Typical results**:
- Preprocessing: 1-10% of total time
- Reduction: 10-50% of variables/clauses
- Net speedup: 2-10× overall

**Best case**: Solves problem entirely during preprocessing
**Worst case**: No reduction, wasted time

### When Preprocessing Helps

**Good for preprocessing**:
- Structured instances (circuit, planning, scheduling)
- Many unit clauses and pure literals
- Blocked clauses (applications)

**Not good for preprocessing**:
- Random instances (few pure literals, few blocked clauses)
- Hard unsatisfiable instances (preprocessing can't simplify)

### Tuning

**Aggressive preprocessing** (many iterations, expensive techniques):
- Good for: Large structured instances
- Cost: 5-20% of total time
- Benefit: 10-50% reduction

**Light preprocessing** (unit prop + pure literals only):
- Good for: Random instances, tight time limits
- Cost: < 1% of total time
- Benefit: 5-20% reduction

---

## Statistics and Debugging

### Key Metrics

```c
struct {
    uint64_t pure_literals;      // Pure literals found
    uint64_t blocked_clauses;    // Clauses removed by BCE
    uint64_t subsumed_clauses;   // Clauses removed by subsumption
    uint64_t variables_eliminated; // Variables eliminated (if implemented)
} preprocess_stats;
```

### Analysis

**Healthy preprocessing**:
- 10-50% clause reduction on structured instances
- 5-20% variable reduction
- < 10% of total solving time

**Ineffective preprocessing**:
- < 5% reduction
- May indicate random instance or hard problem
- Consider disabling expensive techniques

---

## Advanced Techniques (Future Work)

### 1. Bounded Variable Elimination (BVE)

**Idea**: Only eliminate variables if resolvent size is bounded

**Benefit**: Prevents exponential blowup
**Status**: Not implemented

### 2. Failed Literal Probing

**Idea**: Try assigning each literal at level 1, see if it leads to immediate conflict

**Benefit**: Finds hidden unit clauses
**Cost**: Expensive (O(n) unit propagations)
**Status**: Not implemented

### 3. Hyper-Binary Resolution (HBR)

**Idea**: Derive binary clauses from longer clauses

**Benefit**: Binary clauses propagate faster
**Status**: Not implemented

### 4. Preprocessing Portfolios

**Idea**: Different preprocessing for different instance types

**Example**: Aggressive for structured, light for random
**Status**: Not implemented

---

## References

### Papers

1. **Blocked Clause Elimination**
   - Järvisalo, Biere, Heule (2010)
   - "Blocked Clause Elimination"

2. **Variable Elimination**
   - Eén & Biere (2005)
   - "Effective Preprocessing in SAT Through Variable and Clause Elimination"

3. **SatELite Preprocessor**
   - Eén & Biere (2005)
   - Standalone preprocessor for SAT

4. **Survey**
   - Biere, Heule, van Maaren (2009)
   - "Handbook of Satisfiability" - Chapter on Preprocessing

### Tools

- **SatELite**: Classic standalone preprocessor
- **Coprocessor**: Modern preprocessor (Armin Biere)
- **MiniSat simplification**: Built-in preprocessing

---

## Summary

**Preprocessing is essential** for modern SAT solving:

✅ **Reduces problem size**: 10-50% on structured instances
✅ **Sometimes solves entirely**: Unit propagation chains
✅ **Low overhead**: 1-10% of total time
✅ **Enables harder instances**: Smaller formulas are easier to solve

**Key techniques**:
1. **Unit propagation**: Find and propagate unit clauses (always do this)
2. **Pure literal elimination**: Assign variables with single polarity (cheap)
3. **Blocked Clause Elimination**: Remove irrelevant clauses (powerful but expensive)

**Implementation**: Run before main CDCL search, iterate to fixpoint.

**Best practice**: Light preprocessing (unit prop + pure literals) for all instances, aggressive preprocessing (BCE) for structured instances.

This completes the major optimizations needed for competitive SAT solving. Combined with CDCL core (two-watched literals, 1-UIP learning, VSIDS, restarts, clause management), preprocessing enables solving of industrial-scale problems.
