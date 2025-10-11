# Davis-Putnam Algorithm (1960)

The original SAT solver - using resolution-based variable elimination.

## Overview

**Historical Significance**: This was the **FIRST** practical SAT solver algorithm, published by Martin Davis and Hilary Putnam in 1960.

**Why It Matters**: Understanding Davis-Putnam helps you understand:
- The origins of SAT solving
- Why DPLL was developed (1962)
- Modern techniques (resolution in CDCL, component caching)
- The exponential space problem

**Status**: Educational implementation only (< 30 variables)

**Replaced By**: DPLL (1962) due to exponential space requirements

---

## Table of Contents

1. [The Algorithm](#the-algorithm)
2. [Why It Doesn't Scale](#why-it-doesnt-scale)
3. [The Three Rules](#the-three-rules)
4. [Usage](#usage)
5. [Examples](#examples)
6. [Complexity Analysis](#complexity-analysis)
7. [Historical Context](#historical-context)
8. [Connection to Modern Techniques](#connection-to-modern-techniques)
9. [When (Not) to Use](#when-not-to-use)

---

## The Algorithm

Davis-Putnam solves SAT by **eliminating variables via resolution**.

### High-Level Idea

```
Input: CNF formula F with variables {x₁, x₂, ..., xₙ}

Repeat until no clauses remain:
    1. Apply one-literal rule (unit propagation)
    2. Apply affirmative-negative rule (pure literal elimination)
    3. Pick a variable x
    4. Resolve all clauses on x:
       - For each (x ∨ A) and (¬x ∨ B), create (A ∨ B)
    5. Remove all clauses containing x
    6. Add resolved clauses

If empty clause generated → UNSAT
If no clauses remain → SAT
```

### Resolution Rule

**Core idea**: Combine clauses to eliminate a variable

```
Given:
  (x ∨ a ∨ b)    ← clause with x
  (¬x ∨ c ∨ d)   ← clause with ¬x

Resolve on x:
  (a ∨ b ∨ c ∨ d)  ← x eliminated!
```

**General form**:
```
(x ∨ A) and (¬x ∨ B) → (A ∨ B)

where A and B are disjunctions of literals
```

### Complete Example

```
Formula: (x ∨ y) ∧ (x ∨ z) ∧ (¬x ∨ w)

Eliminate x via resolution:

Positive clauses (contain x):
  C₁: (x ∨ y)
  C₂: (x ∨ z)

Negative clauses (contain ¬x):
  C₃: (¬x ∨ w)

Resolve each positive with each negative:
  C₁ + C₃: (x ∨ y) + (¬x ∨ w) → (y ∨ w)
  C₂ + C₃: (x ∨ z) + (¬x ∨ w) → (z ∨ w)

New formula: (y ∨ w) ∧ (z ∨ w)

Continue with remaining variables...
```

---

## Why It Doesn't Scale

### The Exponential Space Problem 💥

**Problem**: Resolution creates n × m new clauses

```
If variable x appears in:
- n positive clauses (contain x)
- m negative clauses (contain ¬x)

Resolution creates: n × m new clauses (Cartesian product!)
```

### Concrete Example

```
Clauses:
(a ∨ x)
(b ∨ x)
(c ∨ x)
(d ∨ ¬x)
(e ∨ ¬x)

Positive: 3 clauses with x
Negative: 2 clauses with ¬x

Resolution on x creates: 3 × 2 = 6 new clauses:
  (a ∨ d), (a ∨ e)
  (b ∨ d), (b ∨ e)
  (c ∨ d), (c ∨ e)

Started with 5 clauses → Now have 6 clauses!
```

### Exponential Blowup

```
Worst case: Each variable appears in half the clauses

Start: 100 clauses, 50 variables
After eliminating 10 variables: 1,000 clauses
After eliminating 20 variables: 100,000 clauses
After eliminating 30 variables: 10,000,000 clauses
After eliminating 40 variables: 1,000,000,000 clauses (out of memory!)
```

**This is why DPLL was developed in 1962!**

---

## The Three Rules

From the original 1960 paper, Davis-Putnam uses three rules:

### Rule 1: One-Literal Rule

**Also known as**: Unit propagation, Boolean Constraint Propagation (BCP)

```
If formula contains unit clause (single literal):
  (x) or (¬x)

Then:
  - Assign x to satisfy the clause
  - Remove all clauses containing that literal
  - Remove negation from other clauses
```

**Example**:
```
Formula: (x) ∧ (x ∨ y) ∧ (¬x ∨ z)

Unit clause: (x) → assign x = True

After propagation:
  (x) satisfied → remove
  (x ∨ y) satisfied → remove
  (¬x ∨ z) → (z)

Result: (z)
```

### Rule 2: Affirmative-Negative Rule

**Also known as**: Pure literal elimination

```
If variable appears with only one polarity:
  - Only positive: x appears but never ¬x
  - Only negative: ¬x appears but never x

Then:
  - Assign to satisfy all occurrences
  - Remove all clauses containing that literal
```

**Example**:
```
Formula: (x ∨ y) ∧ (x ∨ z) ∧ (y ∨ w)

Variable x appears only positively (pure positive)
Variable w appears only positively (pure positive)

Assign: x = True, w = True
Remove: All three clauses satisfied

Result: Empty (SAT!)
```

### Rule 3: Resolution - Eliminating Atomic Formulas

**This is the main Davis-Putnam step**:

```
Pick a variable x
For each pair of clauses (C₁ ∨ x) and (C₂ ∨ ¬x):
  Create resolvent (C₁ ∨ C₂)

Remove all clauses containing x or ¬x
Add all resolvents
```

**This is where exponential blowup occurs!**

---

## Usage

### Basic Solving

```python
from bsat import solve_davis_putnam, CNFExpression

# Small formula (< 20 variables recommended)
formula = "(x | y) & (~x | z) & (y | ~z)"
cnf = CNFExpression.parse(formula)

result = solve_davis_putnam(cnf)

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### With Statistics

```python
from bsat import get_davis_putnam_stats, CNFExpression

formula = "(a | b) & (a | c) & (~a | d)"
cnf = CNFExpression.parse(formula)

result, stats = get_davis_putnam_stats(cnf)

print(f"Initial clauses: {stats.initial_clauses}")
print(f"Max clauses: {stats.max_clauses}")
print(f"Resolutions: {stats.resolutions_performed}")
print(f"Clause growth: {stats.max_clauses / stats.initial_clauses:.2f}x")
```

### Using the Solver Class

```python
from bsat import DavisPutnamSolver, CNFExpression

cnf = CNFExpression.parse("(x | y) & (~x | z)")
solver = DavisPutnamSolver(cnf)

result = solver.solve()
stats = solver.get_statistics()

print(f"Solution: {result}")
print(f"One-literal eliminations: {stats.one_literal_eliminations}")
print(f"Pure literal eliminations: {stats.pure_literal_eliminations}")
print(f"Variables eliminated: {stats.variables_eliminated}")
```

---

## Examples

### Example 1: Simple SAT

```python
from bsat import solve_davis_putnam, CNFExpression

cnf = CNFExpression.parse("(x | y) & (~x | z)")
result = solve_davis_putnam(cnf)

# Result: {'x': True, 'y': True, 'z': True}
```

### Example 2: Simple UNSAT

```python
from bsat import CNFExpression, Clause, Literal, solve_davis_putnam

# (x) ∧ (¬x) - impossible!
cnf = CNFExpression([
    Clause([Literal('x', False)]),
    Clause([Literal('x', True)])
])

result = solve_davis_putnam(cnf)
# Result: None (UNSAT)
```

### Example 3: Clause Growth

```python
from bsat import get_davis_putnam_stats, CNFExpression

# Formula that causes clause growth
formula = "(a | b) & (a | c) & (~a | d) & (~a | e)"
cnf = CNFExpression.parse(formula)

result, stats = get_davis_putnam_stats(cnf)

print(f"Started: {stats.initial_clauses} clauses")
print(f"Peak: {stats.max_clauses} clauses")
print(f"Resolutions: {stats.resolutions_performed}")

# Output:
# Started: 4 clauses
# Peak: 4 clauses (2×2 resolution)
# Resolutions: 4
```

### Example 4: Pure Literal Elimination

```python
from bsat import get_davis_putnam_stats, CNFExpression

# Variable 'z' appears only positively
formula = "(x | y) & (~x | y) & (y | z)"
cnf = CNFExpression.parse(formula)

result, stats = get_davis_putnam_stats(cnf)

print(f"Pure literal eliminations: {stats.pure_literal_eliminations}")
# At least 1 (for 'z')
```

### Example 5: Comparison with DPLL

```python
from bsat import (
    solve_davis_putnam, get_davis_putnam_stats,
    solve_sat, DPLLSolver,
    CNFExpression
)

formula = "(a | b | c) & (~a | b | ~c) & (a | ~b | c)"
cnf = CNFExpression.parse(formula)

# Davis-Putnam
dp_result, dp_stats = get_davis_putnam_stats(cnf)

# DPLL
dpll_solver = DPLLSolver(cnf)
dpll_result = dpll_solver.solve()
dpll_stats = dpll_solver.get_statistics()

print("Davis-Putnam:")
print(f"  Max clauses: {dp_stats.max_clauses}")
print(f"  Resolutions: {dp_stats.resolutions_performed}")

print("DPLL:")
print(f"  Decisions: {dpll_stats['num_decisions']}")
print(f"  Never stores more than original clause count")

# Both find same answer (SAT/UNSAT)
# But DPLL uses O(n) space, Davis-Putnam uses O(2^n)
```

---

## Complexity Analysis

### Time Complexity

**Worst case**: O(2ⁿ)
- Same as DPLL
- Must explore exponential search space
- Resolution doesn't avoid the fundamental NP-completeness

**Best case**: O(poly)
- Formula with only unit clauses
- Formula with only pure literals
- Independent components

### Space Complexity

**Worst case**: O(2ⁿ) - **This is the problem!**

```
Stored clauses can grow exponentially:
- Each resolution: n × m new clauses
- After k variables: potentially 2^k clauses
- With 50 variables: 2^50 ≈ 10^15 clauses (petabytes!)
```

**Best case**: O(m)
- No resolution needed
- Only unit propagation and pure literals
- Same as original clause count

### Comparison with DPLL

| Aspect | Davis-Putnam (1960) | DPLL (1962) |
|--------|---------------------|-------------|
| **Time** | O(2ⁿ) worst case | O(2ⁿ) worst case |
| **Space** | O(2ⁿ) worst case | O(n) recursion stack |
| **Approach** | Resolution | Backtracking |
| **Scalability** | < 30 variables | 100s-1000s variables |
| **Status** | Historical | Foundation of modern solvers |

**Key insight**: DPLL explores the same search space but doesn't store all clauses!

---

## Historical Context

### The 1960 Paper

**Title**: "A Computing Procedure for Quantification Theory"

**Authors**: Martin Davis and Hilary Putnam

**Contribution**: First practical SAT solver

**Impact**:
- Proved SAT could be solved algorithmically (not just theory)
- Inspired DPLL (1962)
- Foundation for modern SAT solving
- Davis won Turing Award (later, for other work)

### The Problem They Faced

```
1960: Memory was expensive and scarce
- 4 KB was typical computer memory
- Resolution generates many clauses
- 50 variables → Out of memory!
```

### The 1962 Solution: DPLL

**Title**: "A Machine Program for Theorem-Proving"

**Authors**: Martin Davis, George Logemann, Donald Loveland

**Key Innovation**:
```
Instead of storing all resolved clauses:
- Pick a variable
- Try assigning it True
- Simplify and recursively solve
- If fails, backtrack and try False

Space: O(n) recursion stack (not exponential!)
```

**This became the foundation of all modern SAT solvers!**

---

## Connection to Modern Techniques

Davis-Putnam's ideas live on in modern SAT solving:

### 1. Resolution in CDCL

**Modern CDCL solvers** (1996+) use resolution for conflict analysis:

```
When conflict occurs:
- Resolve clauses to find "real reason"
- Learn a new clause
- This is selective resolution (not exhaustive like Davis-Putnam)
```

**Davis-Putnam**: Resolve all pairs
**CDCL**: Resolve only to analyze conflicts

### 2. Component Decomposition

**Modern preprocessing** detects independent components:

```
Formula: (a ∨ b) ∧ (c ∨ d)
         └─comp 1─┘ └─comp 2─┘

Solve independently and combine
(Similar to Davis-Putnam's variable elimination idea)
```

BSAT has this:
```python
from bsat import decompose_into_components

components = decompose_into_components(cnf)
# Solve each separately
```

### 3. Model Counting (#SAT)

**sharpSAT** and other model counters use:
- Component caching (memoize subproblem results)
- Variable elimination (when safe)
- Resolution (selective)

**Connection**: Davis-Putnam's bottom-up combination, but memoized!

### 4. Bounded Variable Addition (BVA)

**Modern preprocessors** use controlled variable introduction:

```
Given: (a ∨ b ∨ c) ∧ (a ∨ b ∨ d)
Add: x ↔ (a ∨ b)  [auxiliary variable]
Result: (x ∨ c) ∧ (x ∨ d) ∧ (¬x ∨ a ∨ b) ∧ ...

Sometimes simpler for solver!
```

**Davis-Putnam eliminates variables via resolution**
**BVA adds variables via resolution**

---

## When (Not) to Use

### ✅ Good For:

1. **Educational purposes**
   - Understanding SAT solving history
   - Seeing why exponential space is problematic
   - Connecting to modern techniques

2. **Very small instances** (< 20 variables)
   - Fast enough
   - Space not an issue
   - Interesting to see clause growth

3. **Theoretical analysis**
   - Studying resolution complexity
   - Understanding proof complexity
   - Research on SAT hardness

### ❌ Not Good For:

1. **Production use**
   - Use DPLL or CDCL instead
   - 100-1000× better scalability
   - Industry-standard implementations

2. **Large formulas** (> 30 variables)
   - Exponential space blowup
   - Will exhaust memory
   - May crash with out-of-memory error

3. **Real-world problems**
   - Hardware verification
   - Planning
   - Scheduling
   - Use modern solvers!

### Recommendations

| Problem Size | Use This |
|-------------|----------|
| < 10 variables | Davis-Putnam OK (educational) |
| 10-100 variables | DPLL (`solve_sat()`) |
| 100-10,000 variables | CDCL (`solve_cdcl()`) |
| > 10,000 variables | Production solver (Kissat, CaDiCaL) |

---

## Performance Characteristics

### Clause Growth Examples

**Best case** (no growth):
```
Formula: x ∧ (x ∨ y) ∧ (x ∨ z)
Unit clause (x) → unit propagation only
No resolution needed
Clause count stays constant
```

**Moderate growth**:
```
Formula: (a ∨ b) ∧ (¬a ∨ c) ∧ (b ∨ d)
Resolve on 'a': 1 × 1 = 1 new clause
Started: 3 clauses → Peak: 3 clauses
```

**Exponential growth**:
```
Formula: n/2 clauses with x, n/2 clauses with ¬x
Resolve on 'x': (n/2) × (n/2) = n²/4 new clauses
Started: n clauses → Peak: n²/4 clauses
```

### Memory Usage

```python
# Estimate memory for Davis-Putnam
variables = 50
clauses_per_variable = 10

# Worst case after k eliminations
k = 25  # Halfway
max_clauses = (clauses_per_variable) ** k

# If each clause is 100 bytes:
memory_gb = max_clauses * 100 / (1024**3)

print(f"Estimated memory: {memory_gb:.1f} GB")
# Likely: Hundreds of GB or more (out of memory!)
```

**DPLL uses**: O(n) ≈ a few KB for 50 variables

---

## Further Reading

### Original Papers

- **Davis & Putnam (1960)**: "A Computing Procedure for Quantification Theory"
  - [ACM Digital Library](https://dl.acm.org/doi/10.1145/321033.321034)
  - The original paper (paywalled)

- **Davis, Logemann & Loveland (1962)**: "A Machine Program for Theorem-Proving"
  - [ACM Digital Library](https://dl.acm.org/doi/10.1145/368273.368557)
  - The DPLL paper that superseded Davis-Putnam

### Modern References

- **Handbook of Satisfiability (2021)**
  - Chapter on resolution and proof complexity
  - Historical perspectives

- **BSAT Documentation**
  - [DPLL Solver](dpll-solver.md) - The successor to Davis-Putnam
  - [CDCL Solver](cdcl-solver.md) - Modern approach using resolution
  - [History](history.md) - Full timeline from 1960 to present

---

## Summary

**Davis-Putnam (1960)**:
- ✅ First practical SAT solver
- ✅ Uses resolution to eliminate variables
- ❌ Exponential space (n×m clause blowup)
- ❌ Doesn't scale beyond ~30 variables
- ✅ Ideas live on in modern SAT solving

**Why study it?**:
- Historical significance
- Understand resolution and proof complexity
- Appreciate why DPLL was revolutionary
- Connect to modern techniques (CDCL, #SAT)

**When to use it?**:
- Educational purposes only
- Small instances (< 20 variables)
- Demonstrating exponential space problem

**What to use instead?**:
- `solve_sat()` - DPLL with O(n) space
- `solve_cdcl()` - Modern CDCL solver
- Production solvers - Kissat, CaDiCaL, Glucose

---

## See Also

- [DPLL Solver](dpll-solver.md) - The 1962 improvement
- [CDCL Solver](cdcl-solver.md) - Modern SAT solving (1996+)
- [History of SAT Solving](history.md) - From 1960 to present
- [Preprocessing](preprocessing.md) - Modern variable elimination
- [Theory & References](theory.md) - Academic foundations

---

*This implementation is educational - showing why the original Davis-Putnam algorithm doesn't scale and connecting to modern SAT solving techniques. Always use DPLL or CDCL for real problems!*
