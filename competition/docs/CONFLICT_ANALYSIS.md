# Conflict Analysis and Clause Learning

**Technique**: 1-UIP (First Unique Implication Point) learning scheme
**Impact**: Core of CDCL - transforms exponential search to practical solving
**Status**: ✅ Implemented in both Python and C solvers

---

## Overview

**Conflict-Driven Clause Learning (CDCL)** is the foundation of modern SAT solving. When the solver encounters a conflict, it:

1. **Analyzes** the conflict to find the root cause
2. **Learns** a new clause that prevents repeating this mistake
3. **Backtracks** to the appropriate decision level
4. **Continues** search with new knowledge

**Key idea**: Learn from mistakes to prune the search space exponentially.

---

## The Problem: Naive Backtracking

### DPLL Without Learning

**Algorithm**:
```
1. Make decision (x = TRUE)
2. Propagate
3. If conflict → backtrack, try x = FALSE
4. If both fail → backtrack further
```

**Problem**: Repeats the same mistakes over and over!

**Example**:
```
Decision: x1 = TRUE
  → Leads to conflict via x2, x3, x4
  → Backtrack, try x1 = FALSE

Later:
Decision: x1 = TRUE again (different branch)
  → Same conflict via x2, x3, x4
  → Waste time re-discovering same contradiction
```

**Result**: Exponential time on structured problems

### CDCL With Learning

**Algorithm**:
```
1. Make decision (x = TRUE)
2. Propagate
3. If conflict:
   a. Analyze conflict
   b. Learn clause that blocks this conflict path
   c. Backtrack to appropriate level
4. Continue with new clause in database
```

**Benefit**: Each conflict teaches us something new - never repeat the same mistake!

**Result**: Polynomial time on many structured problems

---

## Implication Graph

### What is the Implication Graph?

**Definition**: Directed acyclic graph (DAG) showing how assignments lead to conflict

**Nodes**:
- Decision variables (no incoming edges)
- Implied variables (incoming edges from reason clause)
- Conflict node (special, marks contradiction)

**Edges**:
- From variables in reason clause to implied variable
- Labeled with the clause that caused implication

### Example

**Formula**:
```
C1: (x1 ∨ x2)
C2: (¬x1 ∨ x3)
C3: (¬x2 ∨ x4)
C4: (¬x3 ∨ ¬x4)
```

**Assignment sequence**:
```
Level 1: x1 = FALSE (decision)
  → C1 propagates: x2 = TRUE (reason: C1)

Level 2: x3 = TRUE (decision)
  → C3 propagates: x4 = TRUE (reason: C3)
  → C4 propagates: CONFLICT! (both ¬x3 and ¬x4 true)
```

**Implication graph**:
```
        x1=F@1 ──C1──> x2=T@1
                         │
                        C3
                         ↓
        x3=T@2 ──C4──> x4=T@2 ──C4──> ⊥ (conflict)
          │                              ↑
          └──────────────────────────────┘
```

---

## 1-UIP Learning Scheme

### What is a UIP?

**UIP** = **Unique Implication Point**

**Definition**: A variable at the current decision level such that:
- All paths from the decision to the conflict go through this variable
- Essentially a "cut" in the implication graph

**1-UIP** = **First UIP** (closest to conflict)

**Why 1-UIP?**
- Produces smaller learned clauses than other schemes
- Empirically best performance
- Standard in all modern solvers (MiniSat, Glucose, Kissat, etc.)

### 1-UIP Algorithm

**Goal**: Find asserting clause that:
1. Implies exactly one literal at current decision level (the 1-UIP)
2. All other literals at lower levels
3. Minimal (no redundant literals)

**Steps**:

1. **Initialize**: Start with conflict clause
   ```c
   Clause* conflict = /* conflict clause */;
   for (each lit in conflict) {
       mark_seen(lit);
       if (level(lit) == current_level) {
           pathC++;  // Count current-level literals
       } else {
           add_to_learnt(lit);
       }
   }
   ```

2. **Resolution loop**: Resolve backwards through implication graph
   ```c
   Lit p;
   while (pathC > 1) {  // Until only 1 current-level literal remains
       // Find next literal to resolve (from trail, in reverse)
       do {
           p = trail[--trail_pos];
       } while (!seen(p));

       CRef reason = get_reason(p);

       // Resolve with reason clause
       for (each lit in reason) {
           if (!seen(lit)) {
               mark_seen(lit);
               if (level(lit) == current_level) {
                   pathC++;
               } else {
                   add_to_learnt(lit);
               }
           }
       }

       pathC--;  // Resolved one current-level literal
   }

   // p is now the 1-UIP
   learnt[0] = NOT(p);  // Asserting literal
   ```

3. **Result**: Learned clause with 1-UIP as asserting literal

### Example Trace

**Conflict clause**: `(¬x3 ∨ ¬x4)` at level 2

**Implication graph**:
```
x1=F@1 ──C1──> x2=T@1 ──C3──> x4=T@2
                             ↗
                   x3=T@2 (decision)

Conflict: C4 = (¬x3 ∨ ¬x4)
```

**Resolution trace**:

1. **Start**: conflict = `(¬x3 ∨ ¬x4)`
   - pathC = 2 (both at level 2)

2. **Resolve x4**:
   - Reason for x4: C3 = `(¬x2 ∨ x4)`
   - Resolve: `(¬x3 ∨ ¬x4) ∪ (¬x2 ∨ x4)` → `(¬x3 ∨ ¬x2)`
   - pathC = 1 (only x3 at level 2)

3. **1-UIP found**: x3 is the 1-UIP

4. **Learned clause**: `(¬x3 ∨ ¬x2)`
   - Asserting literal: ¬x3 (will be true after backtracking to level 1)
   - Other literals: ¬x2 (at level 1)

---

## Clause Minimization

### The Problem

**Learned clauses often contain redundant literals**

**Example**:
```
Learned: (¬x1 ∨ ¬x2 ∨ ¬x3 ∨ ¬x4)

But:
  reason(x3) = (¬x1 ∨ ¬x2 ∨ ¬x3)

This means x3 is implied by x1 and x2!
So ¬x3 is redundant in the learned clause.
```

**Goal**: Remove redundant literals to get smaller, stronger clauses

### Self-Subsumption

**Idea**: A literal is redundant if it's implied by other literals in the clause

**Algorithm**:
```c
for (i = 1; i < learnt_size; i++) {  // Skip asserting literal
    Lit lit = learnt[i];
    CRef reason = get_reason(lit);

    if (reason == INVALID_CREF) {
        keep(lit);  // Decision variable, must keep
    } else if (can_be_removed(lit, reason)) {
        remove(lit);  // Redundant!
    } else {
        keep(lit);
    }
}
```

**Recursive check**:
```c
bool can_be_removed(Lit lit, CRef reason) {
    // Check if all literals in reason are:
    // 1. Already in learned clause (seen), OR
    // 2. At level 0 (always true), OR
    // 3. Can be recursively removed

    for (each x in reason) {
        if (x == lit) continue;  // Skip self

        if (!seen(x) && level(x) > 0) {
            // Not in learned clause and not level 0
            if (!can_recursive_remove(x)) {
                return false;  // Cannot remove
            }
        }
    }

    return true;  // All conditions met, can remove
}
```

### Example

**Before minimization**:
```
Learned: (¬x1 ∨ ¬x2 ∨ ¬x3 ∨ ¬x4)

Reasons:
  x3: (¬x1 ∨ ¬x2 ∨ ¬x3)  ← x3 implied by x1, x2
  x4: (¬x2 ∨ ¬x4)        ← x4 implied by x2
```

**After minimization**:
```
Learned: (¬x1 ∨ ¬x2)

Removed: ¬x3 (redundant, implied by ¬x1 ∨ ¬x2)
Removed: ¬x4 (redundant, implied by ¬x2)
```

**Benefit**: Smaller clause → stronger constraint → better propagation

### Typical Reduction

**Statistics**: 10-30% of literals removed on average
- Simple instances: ~10%
- Structured instances: ~20-30%
- Heavily constrained: ~30-40%

---

## Backtrack Level Computation

### Non-Chronological Backtracking

**Key idea**: Don't backtrack one level at a time - jump directly to the right level!

**Algorithm**:

1. **Learned clause**: `(L0 ∨ L1 ∨ L2 ∨ ... ∨ Ln)`
   - L0 = asserting literal (1-UIP)
   - L1, ..., Ln = other literals

2. **Find second-highest level**:
   ```c
   Level backtrack_level = 0;

   if (learnt_size == 1) {
       backtrack_level = 0;  // Unit clause, backtrack to level 0
   } else {
       // Find second-highest level in learned clause
       for (i = 1; i < learnt_size; i++) {
           Level lvl = level(learnt[i]);
           if (lvl > backtrack_level) {
               backtrack_level = lvl;
           }
       }
   }
   ```

3. **Backtrack to that level**:
   ```c
   solver_backtrack(backtrack_level);
   ```

4. **Assert learned clause**:
   ```c
   // After backtracking, learned clause is unit
   // (only L0 is unassigned, all others are false)
   enqueue(learnt[0], learned_clause_ref);
   ```

### Why This Works

**Learned clause becomes unit** after backtracking to second-highest level:
- L0 (asserting literal): unassigned
- L1, ..., Ln: all assigned FALSE (at or below backtrack level)

**Result**: Immediate unit propagation with learned clause as reason

### Example

**Learned clause**: `(¬x1 ∨ ¬x2 ∨ ¬x3)`

**Assignment levels**:
- x1: level 5
- x2: level 3
- x3: level 1

**Backtrack level**: max(3, 1) = 3

**After backtracking to level 3**:
- x1: unassigned (was at level 5, above backtrack level)
- x2: FALSE (assigned at level 3)
- x3: FALSE (assigned at level 1, below backtrack level)

**Clause is now unit**: `(¬x1)` → propagate x1 = FALSE

---

## Complete Conflict Analysis Function

### Implementation

```c
void solver_analyze(Solver* s, CRef conflict,
                    Lit* learnt, uint32_t* learnt_size,
                    Level* bt_level) {

    // 1. Initialize
    bool seen[s->num_vars + 1] = {false};
    uint32_t pathC = 0;
    Lit p = INVALID_LIT;
    *learnt_size = 0;

    // Start with conflict clause
    for (each lit in conflict_clause) {
        Var v = var(lit);
        if (!seen[v]) {
            seen[v] = true;
            if (vars[v].level == s->decision_level) {
                pathC++;
            } else if (vars[v].level > 0) {
                learnt[(*learnt_size)++] = lit;
            }
        }
    }

    // 2. Resolution loop
    uint32_t trail_pos = s->trail_size;

    while (pathC > 0) {
        // Find next literal from trail
        do {
            p = s->trail[--trail_pos].lit;
        } while (!seen[var(p)]);

        pathC--;

        if (pathC == 0) {
            // Found 1-UIP
            learnt[0] = lit_neg(p);
            break;
        }

        // Resolve with reason
        CRef reason = vars[var(p)].reason;
        for (each lit in reason_clause) {
            Var v = var(lit);
            if (!seen[v]) {
                seen[v] = true;
                if (vars[v].level == s->decision_level) {
                    pathC++;
                } else if (vars[v].level > 0) {
                    learnt[(*learnt_size)++] = lit;
                }
            }
        }
    }

    // 3. Minimize learned clause
    minimize_clause(s, learnt, learnt_size, seen);

    // 4. Compute backtrack level
    if (*learnt_size == 1) {
        *bt_level = 0;
    } else {
        *bt_level = 0;
        for (uint32_t i = 1; i < *learnt_size; i++) {
            Level lvl = vars[var(learnt[i])].level;
            if (lvl > *bt_level) {
                *bt_level = lvl;
            }
        }
    }

    // 5. Bump variable activities (VSIDS)
    for (uint32_t i = 0; i < *learnt_size; i++) {
        bump_var_activity(s, var(learnt[i]));
    }
}
```

---

## Performance Analysis

### Time Complexity

**Per conflict**:
- Resolution loop: O(conflict graph size)
- Clause minimization: O(clause size × reason depth)
- Backtrack level: O(clause size)

**Total**: O(conflict graph size) amortized

### Space Complexity

- Seen flags: O(n) where n = number of variables
- Learned clause: O(conflict graph size)
- Stack for minimization: O(depth)

### Measured Impact

**Without learning** (pure DPLL):
- Many problems unsolvable (exponential time)
- Repeats same conflicts thousands of times

**With learning** (CDCL):
- Structured problems: Often polynomial time
- Industrial instances: 1000-1000000× faster
- SAT Competition instances: Only solvable with learning

**Clause minimization**:
- Speedup: 10-30% overall
- Learned clause size reduction: 10-30%
- Stronger propagation from smaller clauses

---

## Advanced Techniques

### 1. All-UIP Learning

**Idea**: Learn multiple UIPs, not just first

**Benefit**: More clauses learned, more pruning
**Cost**: More memory, more overhead
**Status**: Rarely used (1-UIP is best in practice)

### 2. Decision-Only Learning

**Idea**: Only keep decision variables in learned clause

**Benefit**: Smaller clauses
**Cost**: Less information
**Status**: Obsolete (clause minimization is better)

### 3. Asserting vs Non-Asserting

**Asserting clause**: Becomes unit after backtracking
**Non-asserting clause**: Doesn't immediately propagate

**Modern solvers**: Only learn asserting clauses (1-UIP guarantees this)

### 4. On-the-Fly Subsumption

**Idea**: During conflict analysis, check if learned clause subsumes existing clauses

**Implementation**: See LBD_AND_CLAUSE_MANAGEMENT.md

**Benefit**: Reduces database size without explicit reduction

---

## Statistics and Debugging

### Key Metrics

```c
struct {
    uint64_t conflicts;          // Total conflicts
    uint64_t learned_clauses;    // Clauses learned
    uint64_t learned_literals;   // Total literals in learned clauses
    uint64_t minimized_literals; // Literals removed by minimization
} stats;
```

### Analysis

**Healthy solver**:
- Average learned clause size: 5-20 literals
- Minimization rate: 10-30%
- Conflicts per decision: 0.1-0.5

**Problematic patterns**:
- Very large learned clauses (> 100 literals): Poor problem structure
- Low minimization (< 5%): Redundancy check may be broken
- Many conflicts, no progress: May need better restarts

---

## References

### Papers

1. **GRASP** (1996) - Marques-Silva & Sakallah
   - Original CDCL algorithm
   - Conflict analysis and clause learning

2. **Chaff** (2001) - Moskewicz et al.
   - 1-UIP learning scheme
   - Modern conflict analysis

3. **zChaff** (2001) - Zhang et al.
   - "Efficient Conflict Driven Learning in a Boolean Satisfiability Solver"
   - Detailed analysis of learning schemes

4. **MiniSat** (2003) - Eén & Sörensson
   - Clean implementation of 1-UIP
   - Clause minimization algorithm

### Source Code

- **MiniSat**: https://github.com/niklasso/minisat
- **Glucose**: http://www.labri.fr/perso/lsimon/glucose/
- **Kissat**: https://github.com/arminbiere/kissat

---

## Summary

**Conflict analysis and clause learning** are the heart of CDCL:

✅ **Exponential to polynomial**: Transforms intractable problems into solvable ones
✅ **1-UIP is optimal**: Produces smallest, strongest learned clauses
✅ **Clause minimization**: Removes 10-30% of redundant literals
✅ **Non-chronological backtracking**: Jumps directly to the right level

**Key principles**:
- Analyze implication graph to find root cause (1-UIP)
- Learn asserting clause that blocks this conflict path
- Minimize clause to remove redundant literals
- Backtrack to second-highest level (clause becomes unit)

**Implementation**: Core of CDCL loop, executed after every conflict.

This technique, combined with two-watched literals, VSIDS, and restarts, enables modern SAT solvers to handle industrial problems with millions of variables and clauses.
