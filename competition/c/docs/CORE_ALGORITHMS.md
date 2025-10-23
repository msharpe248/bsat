# Core CDCL Algorithms

Detailed documentation of the main algorithms in the C solver (src/solver.c).

## Main Search Loop

### solver_solve() - Main Entry Point

**Location:** `src/solver.c:1587-1690`

**Algorithm:**
```c
lbool solver_solve(Solver* s) {
    1. Preprocessing:
       - Unit propagation at level 0
       - Pure literal elimination
       - Blocked clause elimination (BCE)

    2. Main CDCL loop:
       while (true) {
           a. Propagate: solver_propagate()
              → If conflict at level 0: return UNSAT
              → If conflict: goto conflict analysis

           b. Check termination:
              → If all variables assigned: return SAT
              → If resource limits exceeded: return UNKNOWN

           c. Restart check: solver_should_restart()
              → If true: backtrack to level 0

           d. Clause database reduction check
              → If conflicts >= reduce_interval: solver_reduce_db()

           e. Make decision: solver_decide()
       }

    3. Conflict analysis:
       a. Analyze conflict: solver_analyze()
          → Learn 1-UIP clause
          → Determine backtrack level

       b. Backtrack: solver_backtrack(bt_level)

       c. Assert learned clause

       d. Continue search
}
```

**Key invariants:**
- Solver state is always consistent (no contradictions)
- Trail contains complete assignment history
- Watch lists are valid for all clauses

---

## Unit Propagation

### solver_propagate() - Boolean Constraint Propagation (BCP)

**Location:** `src/solver.c:588-714`

**Purpose:** Propagate implications until fixpoint or conflict

**Algorithm:**
```c
CRef solver_propagate(Solver* s) {
    while (qhead < trail_size) {
        Lit p = trail[qhead++].lit;     // Next literal to propagate
        Lit neg_p = lit_neg(p);         // Negation of p

        // Scan all clauses watching ¬p
        Watch* ws = watches[neg_p];
        Watch* prev = NULL;

        for (each watch w in ws) {
            CRef cr = w->cref;
            Lit blocker = w->blocker;

            // OPTIMIZATION: Check blocker first
            if (value[var(blocker)] == TRUE) {
                // Clause already satisfied, keep watch
                prev = w;
                continue;
            }

            // Load clause
            Lit* lits = clause_lits(cr);
            uint32_t size = clause_size(cr);

            // Ensure first two lits are watched
            if (lits[0] == neg_p) {
                swap(lits[0], lits[1]);
            }

            // Check if first literal satisfies clause
            if (value[var(lits[0])] == TRUE) {
                w->blocker = lits[0];
                prev = w;
                continue;
            }

            // Search for new watch
            bool found = false;
            for (i = 2; i < size; i++) {
                if (value[var(lits[i])] != FALSE) {
                    // Found new watch!
                    swap(lits[1], lits[i]);
                    watch_add(lits[1], cr, lits[0]);
                    remove_watch(w);  // Remove from current list
                    found = true;
                    break;
                }
            }

            if (!found) {
                // No new watch found
                if (value[var(lits[0])] == FALSE) {
                    // Conflict!
                    return cr;
                } else {
                    // Unit propagation: lits[0] must be true
                    enqueue(lits[0], cr);
                    w->blocker = lits[0];
                    prev = w;
                }
            }
        }
    }

    return INVALID_CREF;  // No conflict
}
```

**Complexity:** O(1) amortized per assigned variable

**Key optimizations:**
1. **Blocker literal**: Avoids clause lookup when clause already satisfied
2. **Two-watched literals**: Only touch clauses when both watches false
3. **Watch list compaction**: Remove stale watches while scanning

---

## Conflict Analysis

### solver_analyze() - 1-UIP Learning

**Location:** `src/solver.c:1731-1961`

**Purpose:** Learn a conflict clause and determine backtrack level

**Algorithm (1-UIP scheme):**
```c
void solver_analyze(Solver* s, CRef conflict,
                    Lit* learnt, uint32_t* learnt_size,
                    Level* bt_level) {

    1. Initialize:
       - Add all literals from conflict clause to analysis
       - Mark them as "seen"
       - pathC = 0 (counts literals at current level)

    2. Resolution loop (trace implication graph):
       while (true) {
           // Resolve with reason of last assignment
           CRef reason = get_reason_for_conflict();

           for (each lit in reason) {
               Var v = var(lit);
               if (!seen[v]) {
                   seen[v] = true;

                   if (level[v] == current_level) {
                       pathC++;  // Another literal at current level
                   } else if (level[v] > 0) {
                       learnt[learnt_size++] = lit;  // Add to learned clause
                       lbd_levels[level[v]] = true;  // Track for LBD
                   }
               }
           }

           // Find next literal to resolve
           do {
               p = trail[trail_pos--].lit;
           } while (!seen[var(p)]);

           pathC--;

           // Check for 1-UIP (First Unique Implication Point)
           if (pathC == 0) {
               learnt[0] = lit_neg(p);  // Asserting literal
               break;
           }
       }

    3. Compute LBD (Literal Block Distance):
       lbd = count of distinct decision levels in learnt clause

    4. Minimize learned clause:
       - Remove literals implied by other literals (self-subsumption)
       - Recursive minimization to reduce clause size

    5. Determine backtrack level:
       if (learnt_size == 1) {
           bt_level = 0;  // Unit clause
       } else {
           bt_level = max_level_in_learnt_except_first();
       }

    6. Update VSIDS activities:
       for (each var in learnt) {
           bump_activity(var);
       }
}
```

**Example:**
```
Implication graph at conflict:
  x1@1 → x3@2 → x5@3 (conflict)
  x2@1 → x4@2 → x5@3 (conflict)

Resolution trace:
  1. Start with conflict clause: (¬x5)
  2. Resolve with reason(x5): (¬x3 ∨ ¬x4 ∨ ¬x5) → (¬x3 ∨ ¬x4)
  3. Resolve with reason(x4): (¬x2 ∨ ¬x4) → (¬x3 ∨ ¬x2)
  4. Resolve with reason(x3): (¬x1 ∨ ¬x3) → (¬x1 ∨ ¬x2)  [1-UIP!]

Learned clause: (¬x1 ∨ ¬x2)
LBD: 1 (both literals at level 1)
Backtrack level: 1
Asserting literal: ¬x2 (or ¬x1, depending on order)
```

---

## Clause Minimization

### minimize_clause() - Self-Subsumption

**Location:** `src/solver.c:1888-1935`

**Purpose:** Remove redundant literals from learned clause

**Algorithm:**
```c
void minimize_clause(Solver* s, Lit* learnt, uint32_t* size) {
    uint32_t i, j;

    // Remove literals implied by other literals
    for (i = j = 1; i < *size; i++) {
        Lit lit = learnt[i];
        Var v = var(lit);
        CRef reason = vars[v].reason;

        if (reason == INVALID_CREF) {
            // Decision variable, keep
            learnt[j++] = lit;
        } else if (!can_be_removed(s, v, reason)) {
            // Not redundant, keep
            learnt[j++] = lit;
        } else {
            // Redundant, remove
            s->stats.minimized_literals++;
        }
    }

    *size = j;
}

bool can_be_removed(Solver* s, Var v, CRef reason) {
    // Check if all literals in reason are at lower levels
    // or already in learned clause

    Lit* lits = clause_lits(reason);
    uint32_t size = clause_size(reason);

    for (uint32_t i = 0; i < size; i++) {
        Lit lit = lits[i];
        Var x = var(lit);

        if (x == v) continue;  // Skip variable itself

        if (!seen[x] && vars[x].level > 0) {
            // Literal not in learned clause and not at level 0
            // Check if it can be recursively removed
            if (!can_recursive_remove(s, x)) {
                return false;
            }
        }
    }

    return true;
}
```

**Example:**
```
Before minimization: (¬x1 ∨ ¬x2 ∨ ¬x3 ∨ ¬x4)
  reason(x3) = (¬x1 ∨ ¬x2 ∨ ¬x3)
  → x3 is implied by x1 and x2 (already in clause)
  → Remove x3

After minimization: (¬x1 ∨ ¬x2 ∨ ¬x4)
Reduction: 25% fewer literals
```

**Typical reduction:** 10-30% of literals removed

---

## Decision Making

### solver_decide() - Variable Selection

**Location:** `src/solver.c:1527-1585`

**Algorithm:**
```c
bool solver_decide(Solver* s) {
    // 1. Select variable with highest activity (VSIDS)
    Var next_var = INVALID_VAR;

    while (next_var == INVALID_VAR) {
        if (heap_empty(&s->order)) {
            return false;  // All variables assigned
        }

        next_var = heap_extract_max(&s->order);

        if (vars[next_var].value != UNDEF) {
            next_var = INVALID_VAR;  // Already assigned
        }
    }

    // 2. Choose polarity (phase saving + random phase)
    bool phase;

    if (random_phase && rand() < random_phase_prob) {
        // Random phase (prevents stuck states)
        phase = rand() & 1;
    } else {
        // Saved phase from previous search
        phase = vars[next_var].polarity;
    }

    // 3. Make decision
    Lit decision_lit = lit_make(next_var, !phase);

    // 4. Increment decision level
    s->decision_level++;
    s->trail_lims[s->decision_level] = s->trail_size;

    // 5. Enqueue decision
    enqueue(decision_lit, INVALID_CREF);  // No reason

    s->stats.decisions++;
    return true;
}
```

**Phase selection strategies:**
1. **Phase saving** (default): Use last polarity from previous search
   - Preserves good partial assignments across restarts
   - Reduces thrashing

2. **Random phase** (1% default): Random polarity selection
   - Prevents infinite loops and stuck states
   - Essential for completeness on some instances

3. **Adaptive random boost**: Increase randomness when stuck
   - Detects stuck state (many conflicts at low decision levels)
   - Temporarily increases random_phase_prob to 20%

---

## Backtracking

### solver_backtrack() - Undo Assignments

**Location:** `src/solver.c:1502-1525`

**Algorithm:**
```c
void solver_backtrack(Solver* s, Level level) {
    if (s->decision_level <= level) {
        return;  // Already at or below target level
    }

    // Find trail position for target level
    uint32_t backtrack_pos = s->trail_lims[level];

    // Undo assignments from trail end to backtrack position
    for (uint32_t i = s->trail_size; i > backtrack_pos; i--) {
        Var v = var(s->trail[i-1].lit);

        // Save polarity for phase saving
        vars[v].polarity = (vars[v].value == TRUE);

        // Unassign variable
        vars[v].value = UNDEF;
        vars[v].level = 255;
        vars[v].reason = INVALID_CREF;

        // Re-insert into VSIDS heap
        if (vars[v].heap_pos == UINT32_MAX) {
            heap_insert(&s->order, v);
        }
    }

    // Update trail and decision level
    s->trail_size = backtrack_pos;
    s->qhead = backtrack_pos;
    s->decision_level = level;
}
```

**Complexity:** O(k) where k = number of assignments to undo

**Invariants maintained:**
- All assignments at levels > target are undone
- VSIDS heap contains exactly the unassigned variables
- Trail size matches decision level boundaries

---

## LBD Computation

### compute_lbd() - Clause Quality Metric

**Location:** `src/solver.c:1963-1985`

**Purpose:** Compute Literal Block Distance for learned clause

**Algorithm:**
```c
uint32_t compute_lbd(Solver* s, Lit* lits, uint32_t size) {
    static bool level_seen[256];  // One bit per decision level
    memset(level_seen, 0, sizeof(level_seen));

    uint32_t lbd = 0;

    for (uint32_t i = 0; i < size; i++) {
        Level level = vars[var(lits[i])].level;

        if (!level_seen[level]) {
            level_seen[level] = true;
            lbd++;
        }
    }

    return lbd;
}
```

**Interpretation:**
- **LBD = 1**: All literals from same decision level (very good)
- **LBD = 2**: "Glue" clause, spans 2 levels (excellent, never delete)
- **LBD ≤ 5**: Good quality, likely useful
- **LBD > 10**: Poor quality, candidate for deletion

**Example:**
```
Clause: (¬x1@1 ∨ ¬x2@1 ∨ ¬x3@2 ∨ ¬x4@3)
Levels: {1, 2, 3}
LBD: 3
```

---

## Performance Characteristics

### Time Complexity (per conflict)

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Propagation | O(1) amortized | Two-watched literals |
| Conflict analysis | O(graph size) | Trace implication graph |
| Clause minimization | O(clause size) | Recursive check |
| LBD computation | O(clause size) | Count distinct levels |
| Backtracking | O(assignments) | Undo trail entries |
| VSIDS update | O(log n) | Heap operations |

### Space Complexity

| Structure | Space | Notes |
|-----------|-------|-------|
| Variables | O(n) | VarInfo per variable |
| Trail | O(n) | At most n assignments |
| Clauses | O(m + l) | m original + l learned |
| Watch lists | O(2m + 2l) | Two watches per clause |
| VSIDS heap | O(n) | Binary heap |
| Analysis | O(n) | Seen flags, stack |

**Total:** O(n + m + l) where n=variables, m=clauses, l=learned clauses

The implementation achieves excellent cache locality through:
- Compact clause storage in arena
- Contiguous array-based data structures
- Watch list traversal without pointer chasing
