# Feature Testing Guide for CDCL SAT Solvers

## The Challenge: Testing Features vs. Testing Correctness

**Testing correctness** (SAT/UNSAT) is straightforward:
- Provide any DIMACS file
- Check if answer matches expected result
- Validate solution (for SAT)

**Testing features** requires **BOTH**:
1. **Right DIMACS files** - Instances that actually trigger the feature
2. **Instrumentation** - Statistics/logs to verify feature activated

## Why the Right Instance Matters

### Example: Testing Clause Learning

**❌ TOO SIMPLE** (`simple_unsat_3.cnf`):
```dimacs
p cnf 2 3
1 2 0
-1 0
-2 0
```

**Why it doesn't work**:
- Solved entirely by unit propagation
- No search decisions needed
- No conflicts → No clause learning
- Statistics show: `conflicts=0, learned_clauses=0`

**✅ BETTER** (3-SAT with no unit clauses):
```dimacs
p cnf 10 30
1 2 3 0
-1 4 5 0
-2 -4 6 0
...
```

**Why it works**:
- Requires search (no unit clauses)
- Decisions lead to conflicts
- Conflicts trigger clause learning
- Statistics show: `conflicts>0, learned_clauses>0`

## Feature Testing Matrix

| Feature | Requires | Good Instance Properties | Example Files | Stats to Check |
|---------|----------|-------------------------|---------------|----------------|
| **Unit Propagation** | Unit clauses | Contains many unit clauses or binary clauses | `unit_propagation.cnf` | `propagations > 0` |
| **Clause Learning** | Conflicts during search | No unit clauses, requires backtracking | 3-SAT random instances | `conflicts > 0`<br>`learned_clauses > 0` |
| **VSIDS** | Multiple search decisions | No unit propagation chain | Loosely constrained SAT | `decisions > 0` |
| **Restarts** | Long search | Hard SAT/UNSAT instances | `random3sat_v*.cnf` | `restarts > 0` |
| **LBD Calculation** | Clause learning | Conflicts with varied decision levels | Structured instances | `max_lbd > 0` |
| **Database Reduction** | Many learned clauses | Long-running instances | Large 3-SAT | `deleted_clauses > 0` |
| **BCE Preprocessing** | Blocked clauses | Clauses where literal blocks resolution | Specific structure | `blocked_clauses > 0` |
| **Subsumption** | Subsumed clauses | Redundant clauses (A ⊂ B) | Horn formulas | `subsumed_clauses > 0` |
| **Minimization** | Learned clauses | Conflicts with redundant literals | Any instance with conflicts | `minimized_literals > 0` |
| **Phase Saving** | Restarts | Multiple restarts | Hard SAT instances | Check across restarts |
| **Chronological BT** | Non-chronological opportunities | Deep backtracking | Structured SAT | Compare BT levels |

## How to Test Each Feature

### 1. Unit Propagation
**Test Strategy**: Create chain of implications

**Good Instance**:
```dimacs
p cnf 5 5
1 0           # x1 = true
-1 2 0        # x1 → x2
-2 3 0        # x2 → x3
-3 4 0        # x3 → x4
-4 5 0        # x4 → x5
```

**Check**: `propagations >= 5`, `decisions == 0`

**Why**: Each unit clause forces an assignment, triggering propagation chain.

### 2. Clause Learning (CDCL Core)
**Test Strategy**: Instance requiring search with conflicts

**Good Instance**:
```dimacs
p cnf 10 40
# Random 3-SAT with no unit clauses
# k-SAT where k >= 3 typically requires learning
1 2 3 0
-1 4 5 0
-2 -4 6 0
...
```

**Check**: `conflicts > 0`, `learned_clauses > 0`

**Why**: No unit propagation solves it; search must explore and learn from conflicts.

**Bad Instance** (too simple):
```dimacs
p cnf 2 3
1 2 0
-1 0      # Unit clause!
-2 0      # Unit clause!
```
→ Solved by unit propagation, `conflicts=0`

### 3. VSIDS Heuristic
**Test Strategy**: Instance requiring variable decisions

**Good Instance**:
```dimacs
p cnf 20 40
# Multiple independent clauses, no obvious order
1 2 3 0
4 5 6 0
7 8 9 0
...
```

**Check**: `decisions > 5`

**Why**: No unit propagation path; solver must use heuristic to pick variables.

### 4. Restarts
**Test Strategy**: Hard instance requiring long search

**Good Instance**:
- Random 3-SAT near phase transition (4.26 clauses/variable)
- Medium-size UNSAT instances (100+ variables)
- Cryptographic instances

**Example**: `dataset/medium_tests/medium_suite/easy_3sat_v012_c0050.cnf`

**Check**: `restarts > 0`

**Why**: Long search triggers restart policy (geometric or Glucose).

**Bad Instance**: Easy instances solved quickly won't restart.

### 5. LBD (Literal Block Distance)
**Test Strategy**: Conflicts with clauses spanning multiple decision levels

**Good Instance**:
- Structured SAT (graph coloring, pigeonhole)
- Industrial instances with dependencies

**Check**: `max_lbd > 2` (good), `max_lbd > 5` (complex)

**Why**: LBD measures decision level spread; structured problems have varied LBDs.

### 6. Clause Database Reduction
**Test Strategy**: Generate many learned clauses

**Good Instance**:
- Long-running SAT instances
- Large random 3-SAT

**Check**: `deleted_clauses > 0`, `learned_clauses` decreases over time

**Why**: Database fills up (default: >10000 clauses) and triggers LBD-based reduction.

**Note**: Needs long enough search to reach threshold.

### 7. BCE (Blocked Clause Elimination)
**Test Strategy**: Formulas with clauses that block resolution

**Good Instance**:
```dimacs
p cnf 4 5
1 2 0
1 3 0
1 4 0
-1 2 3 4 0
2 3 4 0      # Blocked by literal 1 in other clauses
```

**Check**: `blocked_clauses > 0`

**Why**: Clause `(2 3 4)` is blocked on literal 2 by `(1 2)`.

**Note**: BCE runs during preprocessing; check before search starts.

### 8. On-the-Fly Subsumption
**Test Strategy**: Learned clauses that subsume original clauses

**Good Instance**:
```dimacs
p cnf 5 10
1 2 3 0
1 2 3 4 0    # Subsumed by (1 2 3)
1 2 3 5 0    # Also subsumed
...
```

**Check**: `subsumed_clauses > 0`

**Why**: Smaller learned clauses may subsume larger original clauses.

**Note**: Happens during clause learning; check stats after conflicts.

### 9. Recursive Clause Minimization
**Test Strategy**: Conflicts generating clauses with redundant literals

**Good Instance**: Any instance with conflicts

**Check**: `minimized_literals > 0`, typically 3-10% reduction

**Why**: Learned clauses often contain redundant literals that can be removed.

**Good Instances**: Industrial benchmarks (cryptography, verification)

### 10. Phase Saving
**Test Strategy**: Multiple restarts to observe polarity memory

**Good Instance**: Hard SAT requiring restarts

**Check**: Solutions found faster in later restarts (indirect measure)

**Why**: Saved polarities help solver return to promising areas.

**Testing Method**: Compare decision count before vs. after restarts.

## Creating Custom Test Fixtures

### For Clause Learning: Pigeonhole Principle
```python
# Encode: n+1 pigeons into n holes (UNSAT, requires learning)
def pigeonhole(n_pigeons, n_holes):
    clauses = []

    # Each pigeon in at least one hole
    for p in range(n_pigeons):
        clauses.append([p * n_holes + h for h in range(n_holes)])

    # No two pigeons in same hole
    for h in range(n_holes):
        for p1 in range(n_pigeons):
            for p2 in range(p1+1, n_pigeons):
                clauses.append([-(p1 * n_holes + h), -(p2 * n_holes + h)])

    return clauses

# pigeonhole(4, 3) creates good learning test (4 pigeons, 3 holes)
```

**Why it works**: Forces exponential search, lots of conflicts and learning.

### For VSIDS: Random k-SAT
```python
import random

def random_ksat(n_vars, n_clauses, k=3, seed=42):
    random.seed(seed)
    clauses = []
    for _ in range(n_clauses):
        clause = random.sample(range(1, n_vars+1), k)
        clause = [v if random.random() < 0.5 else -v for v in clause]
        clauses.append(clause)
    return clauses
```

**Parameters**:
- SAT: `4.0 * n_vars` clauses (under-constrained)
- Phase transition: `4.26 * n_vars` clauses (hardest)
- UNSAT: `4.5 * n_vars` clauses (over-constrained)

### For Restarts: Graph Coloring
```python
def graph_coloring(n_vertices, edges, n_colors):
    # Variable: x_v_c (vertex v has color c)
    clauses = []

    # Each vertex has at least one color
    for v in range(n_vertices):
        clauses.append([v * n_colors + c for c in range(n_colors)])

    # Each vertex has at most one color
    for v in range(n_vertices):
        for c1 in range(n_colors):
            for c2 in range(c1+1, n_colors):
                clauses.append([-(v * n_colors + c1), -(v * n_colors + c2)])

    # Adjacent vertices have different colors
    for (u, v) in edges:
        for c in range(n_colors):
            clauses.append([-(u * n_colors + c), -(v * n_colors + c)])

    return clauses
```

**Why it works**: Structured problem, medium-hard, triggers restarts.

## Using Existing Benchmarks

### SAT Competition Instances
```
dataset/
├── simple_tests/simple_suite/
│   └── random3sat_v*.cnf        # Good for: learning, VSIDS, restarts
├── medium_tests/medium_suite/
│   └── easy_3sat_v*.cnf         # Good for: all features
└── sat_competition2025/
    ├── random/                   # Good for: learning, restarts
    ├── crafted/                  # Good for: specific features
    └── industrial/               # Good for: all advanced features
```

### Recommendations by Feature

**For Basic Features** (unit prop, decisions):
- `simple_tests/` - Fast, good for CI

**For Clause Learning**:
- `random3sat_v10_c43.cnf` and larger
- Any pigeonhole instance

**For Restarts/LBD/Database Reduction**:
- `medium_tests/easy_3sat_v014_c0058.cnf` and harder
- Industrial instances

**For BCE/Subsumption**:
- Crafted instances with structure
- CNF encodings of real problems (planning, verification)

## Implementation Example: Feature Tests

```c
void test_clause_learning() {
    Solver* s = solver_new();

    // Load instance known to cause conflicts
    dimacs_parse_file(s, "path/to/hard_instance.cnf");

    solver_solve(s);

    // Verify feature activated
    assert(s->stats.conflicts > 0);
    assert(s->stats.learned_clauses > 0);

    printf("✅ Clause learning: %llu conflicts, %llu learned\n",
           s->stats.conflicts, s->stats.learned_clauses);

    solver_free(s);
}
```

## Key Takeaways

1. **Not all DIMACS files are equal** - Simple instances may bypass advanced features

2. **Unit propagation is powerful** - Many "test" instances are solved without search

3. **Statistics are essential** - Without instrumentation, you can't verify features work

4. **Choose instances deliberately**:
   - Too simple → Features never activate
   - Too hard → Tests timeout
   - **Just right → Features activate in <1 second**

5. **Test incrementally**:
   - Start with simple: Does it solve correctly?
   - Add medium: Do basic features activate?
   - Try hard: Do advanced features activate?

6. **Use existing benchmarks** - SAT competitions have well-characterized instances

7. **Create custom fixtures** - For targeted feature testing (pigeonhole, graph coloring, etc.)

## Testing Strategy Summary

| Goal | Instance Type | Expected Stats |
|------|---------------|----------------|
| **Smoke test** | Trivial SAT/UNSAT | Correctness only |
| **Unit propagation** | Horn formulas, unit chains | `propagations > 0` |
| **Basic CDCL** | Small random 3-SAT | `conflicts > 0`, `learned > 0` |
| **Full CDCL** | Medium random 3-SAT | All stats > 0 |
| **Advanced features** | Industrial/crafted | LBD, reduction, subsumption |
| **Performance** | Competition instances | Solve time, memory |

## Further Reading

- SAT Competition benchmarks: https://satcompetition.github.io/
- Random k-SAT generators: SAT competitions provide standard generators
- Structured encodings: Graph problems, planning, verification instances

Last updated: 2025-10-21
