# CDCL Solver

Conflict-Driven Clause Learning (CDCL) is the algorithm behind modern industrial SAT solvers. This document explains the CDCL implementation in BSAT.

## Table of Contents

1. [Overview](#overview)
2. [Algorithm](#algorithm)
3. [Quick Start](#quick-start)
4. [Key Components](#key-components)
5. [Parameters](#parameters)
6. [Performance](#performance)
7. [Examples](#examples)
8. [Comparison with DPLL](#comparison-with-dpll)

---

## Overview

**CDCL** extends the classic DPLL algorithm with powerful learning techniques that make modern SAT solvers orders of magnitude faster on real-world instances.

### What Makes CDCL Special?

- **Clause Learning**: When conflicts occur, CDCL analyzes why and learns new clauses to prevent repeating the same mistakes
- **Non-chronological Backtracking**: Can jump back multiple decision levels at once
- **VSIDS Heuristic**: Intelligent variable selection that focuses on variables involved in recent conflicts
- **Restart Strategy**: Periodically restarts search while keeping learned clauses

### Status

✅ **Implemented** (Educational version)

**Note**: This is an educational implementation prioritizing clarity over performance. For production use, consider industrial-strength solvers like MiniSat, CryptoMiniSat, Glucose, or CaDiCaL.

---

## Algorithm

### High-Level Flow

```
1. Initialize: Set all variables as unassigned, decision level = 0

2. Loop:
   a. Unit Propagation (BCP):
      - Find unit clauses (only one unassigned literal)
      - Assign those literals to satisfy the clauses
      - Repeat until no more unit clauses

   b. If conflict during propagation:
      - Analyze conflict (find 1UIP)
      - Learn new clause
      - Backjump to appropriate level
      - Add learned clause
      - Check for restart
      - Continue

   c. If all clauses satisfied:
      - Return SAT with current assignment

   d. If all variables assigned but not all clauses satisfied:
      - Return UNSAT

   e. Decision:
      - Pick unassigned variable (using VSIDS)
      - Increment decision level
      - Assign variable
      - Go to step 2a

3. If conflict at decision level 0:
   - Return UNSAT
```

### Key Differences from DPLL

| Feature | DPLL | CDCL |
|---------|------|------|
| **Backtracking** | Chronological (one level) | Non-chronological (multiple levels) |
| **Learning** | None | Learns clauses from conflicts |
| **Heuristic** | Various | VSIDS (conflict-driven) |
| **Restarts** | No | Yes (with learned clauses) |
| **Performance** | Exponential on hard instances | Much better on structured problems |

---

## Quick Start

### Basic Usage

```python
from bsat import CNFExpression, solve_cdcl

# Parse a 3-SAT formula
formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

# Solve using CDCL
result = solve_cdcl(cnf)

if result:
    print(f"SAT: {result}")
    print(f"Verifies: {cnf.evaluate(result)}")
else:
    print("UNSAT")
```

### With Statistics

```python
from bsat import get_cdcl_stats

result, stats = get_cdcl_stats(cnf)

print(f"Result: {result}")
print(f"Decisions: {stats.decisions}")
print(f"Propagations: {stats.propagations}")
print(f"Conflicts: {stats.conflicts}")
print(f"Learned clauses: {stats.learned_clauses}")
print(f"Restarts: {stats.restarts}")
```

### Using the Solver Class

```python
from bsat import CDCLSolver

solver = CDCLSolver(
    cnf,
    vsids_decay=0.95,           # VSIDS decay factor
    restart_base=100,            # Restart interval
    learned_clause_limit=10000   # Max learned clauses
)

result = solver.solve(max_conflicts=1000000)
stats = solver.get_stats()
```

---

## Key Components

### 1. Unit Propagation (BCP)

Unit propagation finds clauses with only one unassigned literal and forces that literal to be true.

```
Example:
  Clause: (x ∨ y ∨ z)
  Current: x = False, y = False, z = ?

  After propagation: z must be True (forced)
```

**In BSAT**: Our implementation scans all clauses to find unit clauses. Industrial solvers use the "two-watched literals" optimization for O(1) amortized propagation.

### 2. Conflict Analysis

When a conflict occurs (all literals in a clause are false), CDCL analyzes the implication graph to find the **First Unique Implication Point (1UIP)**.

```
Conflict Graph:
  Decision x=T
    → propagate y=T (from clause c1)
    → propagate z=T (from clause c2)
    → CONFLICT in clause c3

Analyze:
  - Why did this conflict happen?
  - Learn: (¬x ∨ other_lits)
  - This prevents repeating the same mistake
```

**1UIP**: The first variable on the current decision level that, when flipped, would have prevented the conflict.

### 3. Clause Learning

After analyzing a conflict, CDCL adds a new clause that captures why the conflict occurred.

**Benefits**:
- Prunes the search space
- Prevents exploring the same failing paths
- Can lead to more unit propagations

**Clause Deletion**: To prevent unbounded memory growth, old learned clauses are periodically deleted.

### 4. VSIDS Heuristic

**Variable State Independent Decaying Sum** - a dynamic variable ordering heuristic.

**How it works**:
1. Each variable has a score (initially 0)
2. When a variable appears in a conflict, bump its score
3. Periodically decay all scores
4. Pick the unassigned variable with highest score

**Why it works**: Variables involved in recent conflicts are likely to be involved in future conflicts.

```python
# In BSAT
vsids_decay = 0.95  # Typical range: 0.9 - 0.99
```

### 5. Non-Chronological Backtracking (Backjumping)

Instead of backtracking one level at a time, CDCL jumps directly to the decision level where the learned clause becomes unit.

```
Example:
  Decision levels: 0 → 1 → 2 → 3 → 4 → 5 (conflict)
  Learned clause becomes unit at level 2

  DPLL: 5 → 4 → 3 → 2 (four steps)
  CDCL: 5 → 2 (one jump)
```

### 6. Restart Strategy

CDCL periodically restarts the search from decision level 0, keeping all learned clauses.

**Why restart?**
- Escape from bad decision sequences
- Better leverage of learned clauses
- Proven to improve performance empirically

**BSAT uses Luby sequence**: 1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, ...

---

## Parameters

### `vsids_decay` (float, default=0.95)

Controls how quickly VSIDS scores decay.

- **Higher (0.99)**: Longer memory, focuses on recent conflicts
- **Lower (0.90)**: Shorter memory, more exploratory
- **Typical**: 0.95

```python
result = solve_cdcl(cnf, vsids_decay=0.95)
```

### `max_conflicts` (int, default=1000000)

Maximum number of conflicts before giving up.

```python
result = solve_cdcl(cnf, max_conflicts=10000)
```

### `restart_base` (int, default=100)

Base interval for Luby restart sequence.

```python
solver = CDCLSolver(cnf, restart_base=100)
```

### `learned_clause_limit` (int, default=10000)

Maximum number of learned clauses to keep. When exceeded, half are deleted.

```python
solver = CDCLSolver(cnf, learned_clause_limit=5000)
```

---

## Performance

### Complexity

- **Worst-case**: O(2ⁿ) - exponential (same as DPLL)
- **Practical**: Much better on structured instances due to learning

### When CDCL Excels

✅ **Good for**:
- Structured SAT instances (hardware verification, planning)
- Instances with hidden structure
- Problems where conflicts reveal useful information

❌ **Not ideal for**:
- Random unsatisfiable instances near the phase transition
- Instances requiring exhaustive search
- Very simple instances (overhead of learning not worth it)

### Performance Characteristics

```
                DPLL    CDCL
Simple SAT:     Fast    Slightly slower (overhead)
Hard SAT:       Slow    Much faster (learning helps)
UNSAT:          Slow    Faster (learns conflicts)
```

### Statistics

Monitor performance with statistics:

```python
result, stats = get_cdcl_stats(cnf)

print(stats.decisions)        # Number of decisions made
print(stats.propagations)     # Unit propagations
print(stats.conflicts)        # Conflicts encountered
print(stats.learned_clauses)  # Clauses learned
print(stats.restarts)         # Restarts performed
print(stats.backjumps)        # Non-chronological backjumps
```

---

## Examples

### Example 1: Basic Solving

```python
from bsat import CNFExpression, solve_cdcl

cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (~b | c)")
result = solve_cdcl(cnf)

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### Example 2: Detecting UNSAT

```python
# Unsatisfiable formula
cnf = CNFExpression.parse("x & ~x")
result, stats = get_cdcl_stats(cnf)

print(f"Result: {result}")  # None
print(f"Conflicts: {stats.conflicts}")
```

### Example 3: Chain of Implications

```python
from bsat import Clause, Literal

# x → y → z → w
cnf = CNFExpression([
    Clause([Literal('x', False)]),           # x
    Clause([Literal('x', True), Literal('y', False)]),   # x → y
    Clause([Literal('y', True), Literal('z', False)]),   # y → z
    Clause([Literal('z', True), Literal('w', False)])    # z → w
])

result, stats = get_cdcl_stats(cnf)
print(f"Solution: {result}")
print(f"Propagations: {stats.propagations}")  # All via unit propagation!
```

### Example 4: Tuning VSIDS

```python
# Try different VSIDS decay factors
for decay in [0.90, 0.95, 0.99]:
    result = solve_cdcl(cnf, vsids_decay=decay)
    print(f"Decay {decay}: {result}")
```

See [examples/example_cdcl.py](../examples/example_cdcl.py) for more examples.

---

## Comparison with DPLL

### Performance Comparison

```python
from bsat import solve_sat, solve_cdcl, get_cdcl_stats

formula = "(x | y | z) & (~x | y) & (~y | z) & (~z)"
cnf = CNFExpression.parse(formula)

# DPLL
result_dpll = solve_sat(cnf)
print(f"DPLL: {result_dpll}")

# CDCL
result_cdcl, stats = get_cdcl_stats(cnf)
print(f"CDCL: {result_cdcl}")
print(f"Decisions: {stats.decisions}")
print(f"Conflicts: {stats.conflicts}")
print(f"Learned clauses: {stats.learned_clauses}")
```

### When to Use Each

| Use DPLL When | Use CDCL When |
|--------------|---------------|
| Simple formulas | Complex structured formulas |
| Small instances | Large instances |
| Need simplicity | Need performance |
| Learning overhead too high | Structure can be exploited |

---

## Implementation Notes

### Educational Implementation

This CDCL implementation prioritizes **clarity and correctness** over performance:

**Simplifications**:
- ✅ Simple unit propagation (scans all clauses)
- ✅ Basic 1UIP conflict analysis
- ✅ No watched literals optimization
- ✅ Simple clause deletion strategy

**What's Missing from Industrial Solvers**:
- Two-watched literals (constant-time propagation)
- Advanced clause deletion heuristics (LBD/activity-based)
- Phase saving
- Variable elimination preprocessing
- Parallel solving
- Proof generation

### Industrial-Strength Alternatives

For production use, consider:

- **[MiniSat](http://minisat.se/)**: Clean reference implementation
- **[CryptoMiniSat](https://github.com/msoos/cryptominisat)**: XOR reasoning, Gaussian elimination
- **[Glucose](https://www.labri.fr/perso/lsimon/glucose/)**: LBD-based learning
- **[CaDiCaL](https://github.com/arminbiere/cadical)**: Modern competition winner
- **[Kissat](https://github.com/arminbiere/kissat)**: Latest from Armin Biere

---

## References

### Key Papers

**GRASP** (1996) - First CDCL solver:
- Marques-Silva & Sakallah. "GRASP: A Search Algorithm for Propositional Satisfiability"

**Chaff** (2001) - VSIDS + watched literals:
- Moskewicz et al. "Chaff: Engineering an Efficient SAT Solver"

**MiniSat** (2003) - Clean implementation:
- Eén & Sörensson. "An Extensible SAT-solver"

### Further Reading

- [Handbook of Satisfiability, Chapter 4](https://www.iospress.com/catalog/books/handbook-of-satisfiability-2) - Comprehensive CDCL coverage
- [SAT Solvers: A Tutorial](https://www.cs.utexas.edu/~marques/papers/SATSolversTutorial.pdf) - Excellent modern tutorial
- [BSAT Reading List](reading-list.md) - Full bibliography

---

## Next Steps

- Try [examples/example_cdcl.py](../examples/example_cdcl.py)
- Compare with [DPLL solver](dpll-solver.md)
- Read about [theory and complexity](introduction.md)
- Explore [advanced solvers](advanced-solvers.md)

---

**Note**: This is an educational implementation. The real power of CDCL is seen in industrial solvers with optimizations like watched literals, LBD-based clause deletion, and advanced preprocessing.
