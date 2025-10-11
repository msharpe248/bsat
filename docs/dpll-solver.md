# DPLL Solver

The classic [Davis-Putnam-Logemann-Loveland algorithm](terminology.md#dpll-davis-putnam-logemann-loveland) for solving general SAT and 3SAT problems.

> **Need terminology help?** See the [Terminology Reference](terminology.md) for definitions of SAT concepts.

## Overview

**DPLL** is a complete backtracking-based search algorithm for SAT. It works for any CNF formula (including 3SAT and beyond).

**Implemented**: DPLL with backtracking, unit propagation, pure literal elimination ✅
**Coming Soon**: CDCL (conflict-driven clause learning)

**Time Complexity**: O(2ⁿ) worst case (NP-complete problem)
**Space Complexity**: O(n) for recursion stack
**Practical Performance**: Often much better than worst case, especially with optimizations

## What is DPLL?

DPLL is a **complete** and **sound** SAT solver:
- **Complete**: If a solution exists, DPLL will find it
- **Sound**: If DPLL says SAT, there is a solution; if UNSAT, there isn't

The algorithm systematically explores the space of possible variable assignments using:
1. **[Branching](terminology.md#branching-decision-heuristic)**: Try assigning a variable True, then False
2. **[Backtracking](terminology.md#backtracking)**: Undo assignments when conflicts are detected
3. **Early termination**: Stop exploring branches that can't succeed

## The Algorithm

### DPLL with Optimizations (Implemented)

```
DPLL(formula, assignment):
    // Simplify formula based on current assignment
    simplified = simplify(formula, assignment)

    // Base case: empty clause (conflict)
    if simplified contains empty clause:
        return UNSAT

    // Base case: all clauses satisfied
    if simplified is empty:
        return assignment (SAT)

    // OPTIMIZATION 1: Unit Propagation (see terminology.md#unit-propagation)
    if exists unit clause (single literal):
        assign that literal to True
        return DPLL(simplified, assignment)

    // OPTIMIZATION 2: Pure Literal Elimination (see terminology.md#pure-literal-elimination)
    if exists pure literal (only one polarity):
        assign it to satisfy all clauses
        return DPLL(simplified, assignment)

    // Recursive case: choose a variable and branch
    variable = choose_variable(simplified)

    // Try variable = True
    assignment[variable] = True
    result = DPLL(simplified, assignment)
    if result != UNSAT:
        return result

    // Backtrack: try variable = False
    assignment[variable] = False
    result = DPLL(simplified, assignment)
    return result
```

### Key Ideas

**1. Unit Propagation (Implemented)**

When a clause has only one unassigned literal, that literal MUST be True:

```
Formula: (x ∨ y) ∧ (¬x ∨ z) ∧ (¬y)
Assign: y = False (from unit clause (¬y))

After simplification:
- (x ∨ False) = (x) → unit clause! → x = True
- (¬True ∨ z) = (z) → unit clause! → z = True

Result: {x=True, y=False, z=True} without any branching!
```

Unit propagation chains through implications, dramatically reducing search space.

**2. Pure Literal Elimination (Implemented)**

A variable is "pure" if it appears with only one polarity (only positive or only negative):

```
Formula: (x ∨ y) ∧ (x ∨ z) ∧ (¬w ∨ y)
- x appears only positively → pure positive
- w appears only negatively → pure negative

Assign: x = True, w = False
These satisfy all clauses containing x and w with no conflicts!
```

Pure literal elimination removes variables without branching.

**3. Clause Simplification**

When we assign a variable:
- **Remove satisfied clauses**: If any literal in a clause is True, the clause is satisfied
- **Remove false literals**: Literals that are False can be removed from clauses

Example:
```
Formula: (x ∨ y ∨ z) ∧ (¬x ∨ y)
Assign: x = True

Simplification:
- First clause: (True ∨ y ∨ z) = True → remove clause
- Second clause: (False ∨ y) = (y) → keep simplified clause

Result: (y)
```

**2. Early Conflict Detection**

If simplification produces an empty clause, we have a conflict:
```
Formula: (x ∨ y) ∧ (¬x ∨ ¬y)
Assign: x = True, y = True

After x = True: (y) ∧ (¬y)
After y = True: () ∧ () → empty clause! → UNSAT this branch
```

**3. Backtracking**

When a branch fails, undo the assignment and try the opposite value:
```
Try x = True  → UNSAT
Backtrack
Try x = False → SAT or continue search
```

## Usage

### Basic Usage

```python
from bsat import CNFExpression, solve_sat

# Create a 3SAT formula
formula = "(x | y | z) & (~x | y | z) & (x | ~y | ~z)"
cnf = CNFExpression.parse(formula)

# Solve it
result = solve_sat(cnf)

if result:
    print(f"SAT: {result}")
    print(f"Verification: {cnf.evaluate(result)}")
else:
    print("UNSAT")
```

### Using the Solver Class

```python
from bsat import DPLLSolver, CNFExpression

cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (~b | ~c)")

# Solve with optimizations enabled (default)
solver = DPLLSolver(cnf)
solution = solver.solve()

if solution:
    print(f"Solution: {solution}")

    # Get statistics
    stats = solver.get_statistics()
    print(f"Decisions: {stats['num_decisions']}")
    print(f"Unit propagations: {stats['num_unit_propagations']}")
    print(f"Pure literals: {stats['num_pure_literals']}")
else:
    print("UNSAT")
```

### Controlling Optimizations

```python
# Disable optimizations for comparison
solver_basic = DPLLSolver(cnf,
                          use_unit_propagation=False,
                          use_pure_literal=False)

# Enable only unit propagation
solver_unit = DPLLSolver(cnf,
                         use_unit_propagation=True,
                         use_pure_literal=False)

# Enable only pure literal elimination
solver_pure = DPLLSolver(cnf,
                         use_unit_propagation=False,
                         use_pure_literal=True)

# Both enabled (default)
solver_both = DPLLSolver(cnf)
```

### Solution Enumeration

The DPLL solver can find **all satisfying assignments**, not just one:

```python
from bsat import find_all_sat_solutions, count_sat_solutions, CNFExpression

# Find ALL solutions
formula = "(x | y)"
cnf = CNFExpression.parse(formula)

# Method 1: Using convenience functions
all_solutions = find_all_sat_solutions(cnf)
print(f"Found {len(all_solutions)} solutions")
for sol in all_solutions:
    print(f"  {sol}")

# Method 2: Using solver class
from bsat import DPLLSolver
solver = DPLLSolver(cnf)
solutions = solver.find_all_solutions()

# Method 3: Just count solutions (more efficient)
count = count_sat_solutions(cnf)
print(f"Total solutions: {count}")
```

**Limiting the Search**:

For formulas with many solutions (potentially exponential!), you can limit the search:

```python
# Find at most 100 solutions
solutions = find_all_sat_solutions(cnf, max_solutions=100)

# Or using solver directly
solver = DPLLSolver(cnf)
solutions = solver.find_all_solutions(max_solutions=100)
```

**Important Note on Optimizations**:

When optimizations are enabled (default), pure literal elimination may group some solutions together:

```python
# With optimizations (default)
solver1 = DPLLSolver(cnf)
solutions1 = solver1.find_all_solutions()
# May find fewer distinct solutions (some grouped)

# Without optimizations
solver2 = DPLLSolver(cnf,
                     use_unit_propagation=False,
                     use_pure_literal=False)
solutions2 = solver2.find_all_solutions()
# Finds ALL distinct solutions
```

**Why enumerate solutions?**

1. **Find alternatives**: Multiple valid configurations for planning/scheduling
2. **Model counting**: Count satisfying assignments (probabilistic reasoning)
3. **Verify completeness**: Ensure all cases are covered
4. **Generate test cases**: All valid inputs for a constraint system

**Performance Warning**:

Solution enumeration can be **exponential** in the number of variables:
- n variables → up to 2ⁿ solutions
- 10 variables → up to 1,024 solutions
- 20 variables → up to 1,048,576 solutions

Always use `max_solutions` for large formulas!

### Statistics

The solver tracks how many decision points were explored:

```python
solver = DPLLSolver(cnf)
result = solver.solve()

stats = solver.get_statistics()
print(stats)
# {
#   'num_variables': 5,
#   'num_clauses': 8,
#   'num_decisions': 12
# }
```

## Examples

### Example 1: Satisfiable 3SAT

```python
from bsat import CNFExpression, solve_sat

# (x ∨ y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ z)
cnf = CNFExpression.parse("(x | y | z) & (~x | y | ~z) & (x | ~y | z)")

result = solve_sat(cnf)
print(f"Result: {result}")
# Possible: {'x': True, 'y': True, 'z': True}

# Verify by hand:
# (True ∨ True ∨ True) = True ✓
# (False ∨ True ∨ False) = True ✓
# (True ∨ False ∨ True) = True ✓
```

### Example 2: Unsatisfiable Formula

```python
from bsat import CNFExpression, solve_sat

# (x) ∧ (¬x) - obviously unsatisfiable
cnf = CNFExpression.parse("x & ~x")

result = solve_sat(cnf)
print(f"Result: {result}")  # None
```

### Example 3: Larger 3SAT Instance

```python
from bsat import DPLLSolver, CNFExpression

# 10 variables, 20 clauses
formula = """
    (a | b | c) & (~a | d | e) & (b | ~c | f) &
    (~d | e | g) & (a | ~f | h) & (c | ~g | i) &
    (d | ~h | j) & (~b | f | ~i) & (e | g | ~j) &
    (~a | ~e | h) & (b | ~d | i) & (~c | f | ~j) &
    (a | ~g | j) & (~b | c | ~h) & (d | ~f | i) &
    (~e | g | ~a) & (f | ~h | b) & (~i | j | c) &
    (g | ~j | ~d) & (h | i | ~e)
"""

cnf = CNFExpression.parse(formula)
solver = DPLLSolver(cnf)
result = solver.solve()

if result:
    print("SAT")
    stats = solver.get_statistics()
    print(f"Explored {stats['num_decisions']} decision points")
else:
    print("UNSAT")
```

### Example 4: Pigeonhole Principle

The **pigeonhole principle**: You can't put n+1 pigeons into n holes with one pigeon per hole.

This creates an unsatisfiable CNF:

```python
from bsat import CNFExpression, Clause, Literal

# 3 pigeons, 2 holes
# Variables: p_i_j means "pigeon i is in hole j"

clauses = []

# Each pigeon must be in some hole
for pigeon in range(3):
    clauses.append(Clause([
        Literal(f'p{pigeon}_0'),
        Literal(f'p{pigeon}_1')
    ]))

# No two pigeons in same hole
for hole in range(2):
    for p1 in range(3):
        for p2 in range(p1 + 1, 3):
            clauses.append(Clause([
                Literal(f'p{p1}_{hole}', negated=True),
                Literal(f'p{p2}_{hole}', negated=True)
            ]))

cnf = CNFExpression(clauses)
result = solve_sat(cnf)
print(f"Result: {result}")  # None (UNSAT as expected)
```

### Example 5: Enumerating All Solutions

Find all satisfying assignments for a formula:

```python
from bsat import DPLLSolver, CNFExpression

# Scheduling problem: tasks a, b, c
# Constraint: if a is scheduled, b must be scheduled
formula = "(~a | b)"
cnf = CNFExpression.parse(formula)

# Find all valid schedules
solver = DPLLSolver(cnf,
                    use_unit_propagation=False,
                    use_pure_literal=False)
solutions = solver.find_all_solutions()

print(f"Found {len(solutions)} valid schedules:")
for i, sol in enumerate(solutions, 1):
    a_status = "schedule A" if sol['a'] else "skip A"
    b_status = "schedule B" if sol['b'] else "skip B"
    print(f"  Option {i}: {a_status}, {b_status}")

# Output:
# Found 3 valid schedules:
#   Option 1: skip A, skip B
#   Option 2: skip A, schedule B
#   Option 3: schedule A, schedule B
```

**Model Counting Example**:

```python
from bsat import count_sat_solutions, CNFExpression

formula = "(x | y) & (~x | z)"
cnf = CNFExpression.parse(formula)

# Count satisfying assignments
count = count_sat_solutions(cnf)
total_possible = 2 ** len(cnf.get_variables())

print(f"Satisfying assignments: {count}/{total_possible}")
print(f"Probability formula is satisfied: {count/total_possible:.2%}")

# Output:
# Satisfying assignments: 6/8
# Probability formula is satisfied: 75.00%
```

### Example 6: Graph 3-Coloring

Color a graph with 3 colors such that adjacent vertices have different colors:

```python
from bsat import CNFExpression, Clause, Literal

# Triangle graph: vertices 1, 2, 3 all connected
# Variables: v_i_c means "vertex i has color c" (c ∈ {0,1,2})

clauses = []

# Each vertex has at least one color
for v in [1, 2, 3]:
    clauses.append(Clause([
        Literal(f'v{v}_0'),
        Literal(f'v{v}_1'),
        Literal(f'v{v}_2')
    ]))

# Each vertex has at most one color (all pairs)
for v in [1, 2, 3]:
    for c1 in range(3):
        for c2 in range(c1 + 1, 3):
            clauses.append(Clause([
                Literal(f'v{v}_{c1}', negated=True),
                Literal(f'v{v}_{c2}', negated=True)
            ]))

# Adjacent vertices have different colors
edges = [(1, 2), (2, 3), (1, 3)]
for v1, v2 in edges:
    for color in range(3):
        clauses.append(Clause([
            Literal(f'v{v1}_{color}', negated=True),
            Literal(f'v{v2}_{color}', negated=True)
        ]))

cnf = CNFExpression(clauses)
result = solve_sat(cnf)

if result:
    print("3-colorable!")
    # Extract colors
    for v in [1, 2, 3]:
        for c in range(3):
            if result.get(f'v{v}_{c}'):
                print(f"  Vertex {v}: Color {c}")
else:
    print("Not 3-colorable")
```

## Performance Characteristics

### Best Case: O(n)
- Formula is trivially satisfiable (e.g., all positive unit clauses)
- No backtracking needed

### Average Case: Varies Widely
- Depends on formula structure
- Random 3SAT near phase transition: exponential
- Structured instances: often polynomial

### Worst Case: O(2ⁿ)
- Must explore all possible assignments
- Happens with adversarial formulas

### Phase Transition

Random 3SAT has a **satisfiability phase transition**:

```
clause/variable ratio = m/n

m/n < 4.26:  Usually SAT (easy to find solution)
m/n ≈ 4.26:  50% SAT, hardest to solve
m/n > 4.26:  Usually UNSAT (easy to find conflict)
```

The hardest instances are right at the threshold!

## Algorithm Evolution

DPLL has evolved over 60+ years:

### 1960: Davis-Putnam Algorithm
- Original algorithm
- Used resolution
- Exponential space

### 1962: DPLL (Davis-Logemann-Loveland)
- Added backtracking
- Linear space (recursion stack)
- **This is what we implement**

### 1980s-1990s: Optimizations
- Unit propagation
- Pure literal elimination
- Efficient data structures
- **Implemented in BSAT** ✅

### 1996+: CDCL (Conflict-Driven Clause Learning)
- Learn from conflicts
- Non-chronological backtracking
- Modern solvers (MiniSAT, CryptoMiniSat, etc.)
- **Planned for BSAT**

## Current Implementation

DPLL in BSAT includes:

✅ **Backtracking search**
✅ **Clause simplification**
✅ **Early conflict detection**
✅ **Unit propagation** - Forced assignments from unit clauses
✅ **Pure literal elimination** - Variables with single polarity
✅ **Solution enumeration** - Find all satisfying assignments, not just one
✅ **Statistics tracking** - Decisions, unit props, pure literals

Coming soon:
- ⏳ Conflict-driven clause learning (CDCL)
- ⏳ VSIDS variable ordering
- ⏳ Watched literals
- ⏳ Non-chronological backtracking

The optimizations provide significant performance improvements on many instances!

## Comparison with Other Solvers

| Solver | Time Complexity | Best For |
|--------|----------------|----------|
| **DPLL (basic)** | O(2ⁿ) | Small instances, educational |
| **DPLL + optimizations** | O(2ⁿ) but faster | Medium instances |
| **CDCL** | O(2ⁿ) but much faster | Large, structured instances |
| **2SAT (SCC)** | O(n+m) | 2SAT only |
| **WalkSAT** | Incomplete | Finding solutions quickly |

## When to Use DPLL

### Good For:
- ✅ Learning about SAT solving
- ✅ Small to medium instances (< 100 variables)
- ✅ Instances that need complete solver (not heuristic)
- ✅ When you need proof of unsatisfiability

### Not Good For:
- ❌ Very large instances (> 1000 variables)
- ❌ Hard random 3SAT at phase transition
- ❌ When only SAT/UNSAT answer needed (use 2SAT if possible)
- ❌ Real-time applications (unpredictable runtime)

## Common Pitfalls

### 1. Exponential Blowup

```python
# This might take a very long time!
n = 100
clauses = generate_hard_random_3sat(n, 4.26 * n)
result = solve_sat(clauses)  # Could run for hours
```

**Solution**: Start with small formulas, add timeouts for large ones.

### 2. Not Checking Satisfiability

```python
result = solve_sat(cnf)
# Don't assume result is not None without checking!

if result:
    # Use result
else:
    # Handle UNSAT case
```

### 3. Performance Expectations

```python
# Just because 2SAT is fast doesn't mean 3SAT is!
cnf_2sat = parse_2sat_formula()  # Solves in milliseconds
cnf_3sat = parse_3sat_formula()  # Might take minutes or hours
```

## Debugging Tips

### Trace Execution

```python
# Add your own tracing
class TracedDPLLSolver(DPLLSolver):
    def _dpll(self, assignment, clauses):
        print(f"Assignment: {assignment}")
        print(f"Remaining clauses: {len(clauses)}")
        return super()._dpll(assignment, clauses)
```

### Verify Solutions

```python
result = solve_sat(cnf)
if result:
    assert cnf.evaluate(result), "Solver returned wrong solution!"
```

### Check Formula Structure

```python
# Is it actually in CNF?
# Are clauses the right size?
for clause in cnf.clauses:
    print(f"Clause size: {len(clause.literals)}")
```

## Further Reading

### Classic Papers

- **Davis & Putnam (1960)**: "A Computing Procedure for Quantification Theory"
  - Original algorithm
  - [Link (paywalled)](https://dl.acm.org/doi/10.1145/321033.321034)

- **Davis, Logemann & Loveland (1962)**: "A Machine Program for Theorem-Proving"
  - DPLL algorithm
  - [Link (paywalled)](https://dl.acm.org/doi/10.1145/368273.368557)

### Modern Textbooks

- **"Handbook of Satisfiability"** (2021)
  - Chapter 4: DPLL and extensions
  - [IOS Press](https://www.iospress.com/catalog/books/handbook-of-satisfiability-2)

- **"Decision Procedures"** by Kroening & Strichman
  - Chapter 2: SAT solving

### Online Resources

- [DPLL on Wikipedia](https://en.wikipedia.org/wiki/DPLL_algorithm)
- [SAT Competitions](http://www.satcompetition.org/) - Benchmarks and solvers
- [MiniSat](http://minisat.se/) - Modern CDCL solver (C++)

### Interactive Tools

- [SAT Solver Visualizations](https://cse442-17f.github.io/Conflict-Driven-Clause-Learning/)
- [DPLL Simulator](https://www.lri.fr/~conchon/ENS/sat.html)

## Next Steps

- Learn about [2SAT solving](2sat-solver.md) for polynomial-time special case
- Understand [CNF expressions](cnf.md)
- Explore [advanced solvers](advanced-solvers.md) (CDCL, WalkSAT - coming soon)
- Read about [theory and complexity](theory.md)
- Try the [examples and tutorials](examples.md)

## Implementation Roadmap

**Current** (v0.1):
- ✅ Basic DPLL with backtracking
- ✅ Unit propagation
- ✅ Pure literal elimination
- ✅ Solution enumeration
- ✅ Model counting

**Next Release** (v0.2):
- ⏳ Better variable ordering (VSIDS)
- ⏳ Watched literals

**Future** (v0.3+):
- ⏳ CDCL (conflict-driven clause learning)
- ⏳ Non-chronological backtracking
- ⏳ Incremental solving

Stay tuned!
