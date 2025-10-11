# 2SAT Solver

A [polynomial-time](terminology.md#polynomial-time) algorithm for solving [2SAT](terminology.md#2-sat) problems using [strongly connected components](terminology.md#scc-strongly-connected-components).

> **Need terminology help?** See the [Terminology Reference](terminology.md) for definitions of SAT concepts.

## Overview

**2SAT** is a special case of SAT where every clause has exactly 2 literals. Unlike general 3SAT (which is NP-complete), 2SAT can be solved in **polynomial time**!

**Time Complexity**: O(n + m)
- n = number of variables
- m = number of clauses

**Space Complexity**: O(n + m)

## What is 2SAT?

A 2SAT formula has clauses with exactly 2 literals:

```
(x ∨ y) ∧ (¬x ∨ z) ∧ (y ∨ ¬z) ∧ (¬y ∨ ¬z)
```

Each clause: `(literal₁ ∨ literal₂)`

## Why is 2SAT Polynomial?

The key insight: A 2-literal clause `(a ∨ b)` is equivalent to two **implications**:
- `¬a → b` (if a is false, then b must be true)
- `¬b → a` (if b is false, then a must be true)

We can build an **implication graph** and use graph algorithms to solve it!

## The Algorithm

### Step 1: Build Implication Graph

For each clause `(a ∨ b)`, add two edges:
1. `¬a → b`
2. `¬b → a`

**Example**:

Formula: `(x ∨ y) ∧ (¬x ∨ ¬y)`

Implications:
- `(x ∨ y)` → `¬x → y` and `¬y → x`
- `(¬x ∨ ¬y)` → `x → ¬y` and `y → ¬x`

Graph:
```
x → ¬y → x   (cycle!)
y → ¬x → y   (cycle!)
```

### Step 2: Find Strongly Connected Components (SCCs)

Use **Tarjan's** or **Kosaraju's** algorithm to find SCCs in O(n + m) time.

**Strongly Connected Component**: A set of nodes where every node can reach every other node.

### Step 3: Check for Conflicts

The formula is **unsatisfiable** if and only if:
- There exists a variable x where x and ¬x are in the same SCC

Why? If x and ¬x are in the same SCC:
- x → ¬x (x being true implies x is false)
- ¬x → x (x being false implies x is true)
- Contradiction!

### Step 4: Extract Solution

If satisfiable, assign values based on topological order of SCCs:
- Variables in "later" SCCs are assigned True
- Variables in "earlier" SCCs are assigned False

This works because of the implication structure!

## Usage

### Basic Usage

```python
from bsat import CNFExpression, solve_2sat

# Create a 2SAT formula
formula = "(x | y) & (~x | z) & (y | ~z)"
cnf = CNFExpression.parse(formula)

# Solve it
result = solve_2sat(cnf)

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### Using the Solver Class

```python
from bsat import TwoSATSolver, CNFExpression

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

solver = TwoSATSolver(cnf)
solution = solver.solve()

if solution:
    print(f"Solution: {solution}")
    print(f"Verification: {cnf.evaluate(solution)}")
else:
    print("No solution exists")
```

### Checking if Formula is 2SAT

```python
from bsat import is_2sat_satisfiable, CNFExpression

# Valid 2SAT
cnf1 = CNFExpression.parse("(x | y) & (~x | z)")
print(is_2sat_satisfiable(cnf1))  # True or False

# Not 2SAT (has 3-literal clause)
cnf2 = CNFExpression.parse("(x | y | z) & (~x | y)")
# Will check anyway but might not be a true 2SAT formula
```

## Examples

### Example 1: Satisfiable 2SAT

```python
from bsat import CNFExpression, solve_2sat

# (x ∨ y) ∧ (¬x ∨ z) ∧ (¬y ∨ z)
cnf = CNFExpression.parse("(x | y) & (~x | z) & (~y | z)")

result = solve_2sat(cnf)
print(f"Result: {result}")
# Possible output: {'x': False, 'y': True, 'z': True}

# Verify
print(f"Verification: {cnf.evaluate(result)}")  # True
```

**Implication Graph**:
```
¬x → y    ¬y → x
x → z     ¬z → ¬x
y → z     ¬z → ¬y
```

**SCCs**: No conflicts (x and ¬x in different SCCs)
**Result**: SAT

### Example 2: Unsatisfiable 2SAT

```python
from bsat import CNFExpression, solve_2sat

# (x ∨ y) ∧ (¬x ∨ ¬y) ∧ (x ∨ ¬y) ∧ (¬x ∨ y)
cnf = CNFExpression.parse("(x | y) & (~x | ~y) & (x | ~y) & (~x | y)")

result = solve_2sat(cnf)
print(f"Result: {result}")  # None (UNSAT)
```

**Analysis**:
- `(x ∨ y)` and `(¬x ∨ ¬y)` → exactly one of x, y is true
- `(x ∨ ¬y)` → if y then x
- `(¬x ∨ y)` → if x then y

This creates a cycle: `x → y → x`, but also `¬x` and `¬y` are forced.
Result: UNSAT

### Example 3: Constraint Encoding

Encode "exactly one of a, b is true":

```python
from bsat import CNFExpression, solve_2sat

# At least one: (a ∨ b)
# At most one: (¬a ∨ ¬b)
cnf = CNFExpression.parse("(a | b) & (~a | ~b)")

result = solve_2sat(cnf)
print(f"Solution: {result}")
# Possible: {'a': True, 'b': False} or {'a': False, 'b': True}
```

### Example 4: Graph Coloring

Color a graph with 2 colors such that no adjacent vertices have the same color:

```python
from bsat import CNFExpression, solve_2sat

# Graph edges: (1,2), (2,3), (3,1)
# Variables: x_i means "vertex i is color 1" (otherwise color 2)

# Edge (1,2): vertices must have different colors
# (¬x₁ ∨ ¬x₂) ∧ (x₁ ∨ x₂)
# Meaning: not both color 1, not both color 2

formula = """
    (~v1 | ~v2) & (v1 | v2) &
    (~v2 | ~v3) & (v2 | v3) &
    (~v3 | ~v1) & (v3 | v1)
"""

cnf = CNFExpression.parse(formula)
result = solve_2sat(cnf)

if result:
    print("2-colorable!")
    for vertex, color in sorted(result.items()):
        color_name = "Color1" if color else "Color2"
        print(f"  {vertex}: {color_name}")
else:
    print("Not 2-colorable (graph contains odd cycle)")
```

For a triangle (3-cycle), this is UNSAT - can't 2-color an odd cycle!

## Implementation Details

### Strongly Connected Components

The `TwoSATSolver` uses **Kosaraju's algorithm** for finding SCCs:

1. **First DFS**: Order vertices by finish time
2. **Transpose Graph**: Reverse all edges
3. **Second DFS**: Process in reverse finish order

**Time**: O(n + m) - two depth-first searches
**Space**: O(n + m) - adjacency lists and stacks

### Graph Construction

```python
# Pseudo-code for building implication graph
for each clause (a ∨ b):
    add_edge(¬a, b)  # If a is false, b must be true
    add_edge(¬b, a)  # If b is false, a must be true
```

### Variable Assignment

After finding SCCs, we assign values based on **topological ordering**:

```python
# Pseudo-code
for each variable x:
    scc_x = which_scc[x]
    scc_not_x = which_scc[¬x]

    if scc_x == scc_not_x:
        return UNSAT  # Conflict!

    # Later SCC in topological order → assign True
    assignment[x] = (scc_x > scc_not_x)
```

This ensures all implications are satisfied!

## Performance Characteristics

### Best Case: O(n)
- Simple chain of implications
- Each clause processed once

### Average Case: O(n + m)
- Linear in formula size
- Very fast in practice

### Worst Case: O(n + m)
- Still polynomial!
- Much faster than 3SAT's exponential worst case

### Space Usage: O(n + m)
- Adjacency lists for graph
- Arrays for SCC computation

## Comparison with 3SAT

| Property | 2SAT | 3SAT |
|----------|------|------|
| **Complexity** | P (polynomial) | NP-complete |
| **Algorithm** | SCC | DPLL, CDCL, etc. |
| **Time** | O(n+m) | O(2ⁿ) worst case |
| **Practical** | Always fast | Can be very slow |
| **Certainty** | Always solves quickly | May time out |

**Key Takeaway**: 2SAT is fundamentally easier than 3SAT!

## When to Use 2SAT

### Good For:
- ✅ Binary constraints (if-then, equivalence, XOR)
- ✅ Graph problems (2-coloring, matching)
- ✅ Planning with binary choices
- ✅ Large instances (millions of clauses)

### Not Good For:
- ❌ Problems requiring 3+ literals per constraint
- ❌ Encoding "at least k" constraints (needs 3SAT)
- ❌ Complex Boolean functions

Often: Try to encode as 2SAT first, fall back to 3SAT if needed.

## Common Pitfalls

### 1. Not Actually 2SAT

```python
# This has a 3-literal clause!
cnf = CNFExpression.parse("(x | y) & (a | b | c)")

# The solver may still try to solve it, but it's not a true 2SAT
```

**Solution**: Check your formula has only 2-literal clauses.

### 2. Unit Clauses

```python
# (x) is a 1-literal clause
cnf = CNFExpression.parse("x & (y | z)")

# This is fine! Unit clauses are even easier than 2SAT
# The solver handles them correctly
```

### 3. Expecting a Specific Solution

```python
result = solve_2sat(cnf)
# result might be any of multiple valid solutions!
```

**Note**: 2SAT solver returns *one* solution, but there may be many.

## Advanced Topics

### Counting Solutions

2SAT can have exponentially many solutions:

```python
# Independent clauses: 2^n solutions
cnf = CNFExpression.parse("(x | y) & (a | b)")
# Solutions: all combinations where at least one per clause is true
```

Counting all solutions is #P-complete (still hard!).

### All Solutions

To find all solutions, use:
```python
# Exponential time! Only for small formulas
all_solutions = []
for assignment, result in cnf.generate_truth_table():
    if result:
        all_solutions.append(assignment)
```

### Maximum Satisfiability (MaxSAT)

If formula is UNSAT, which clauses should we remove to make it SAT?

This becomes NP-hard even for 2SAT!

## Further Reading

### Academic Papers

- **Aspvall, Plass, Tarjan (1979)**: "A Linear-Time Algorithm for Testing the Truth of Certain Quantified Boolean Formulas"
  - Original 2SAT polynomial-time algorithm
  - [Link to paper](https://epubs.siam.org/doi/10.1137/0208032)

- **Even, Itai, Shamir (1976)**: "On the Complexity of Timetable and Multicommodity Flow Problems"
  - Early work on 2SAT

### Textbooks

- **"Introduction to Algorithms"** (CLRS) - Chapter on Strongly Connected Components
- **"The Art of Computer Programming"** (Knuth, Vol 4B) - Section on 2SAT

### Online Resources

- [2SAT on Wikipedia](https://en.wikipedia.org/wiki/2-satisfiability)
- [SCC Algorithms](https://en.wikipedia.org/wiki/Strongly_connected_component)
- [Codeforces Tutorial on 2SAT](https://codeforces.com/blog/entry/16205)

### Related Algorithms

- **Tarjan's SCC Algorithm**: O(n+m) with one DFS
- **Kosaraju's SCC Algorithm**: O(n+m) with two DFS (used in this package)
- **Path-based SCC**: Alternative approach

## Next Steps

- Try the [DPLL solver](dpll-solver.md) for general 3SAT
- Learn about [CNF expressions](cnf.md)
- Work through [examples and tutorials](examples.md)
- Explore [theory and references](theory.md)
