# Examples & Tutorials

Practical examples for learning SAT solving with BSAT.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Basic Examples](#basic-examples)
3. [Encoding Problems](#encoding-problems)
4. [Real-World Applications](#real-world-applications)
5. [Performance Tips](#performance-tips)

## Getting Started

### Installation

```bash
pip install bsat
```

### Your First SAT Problem

```python
from bsat import CNFExpression, solve_sat

# Create a simple formula
formula = "(x | y) & (~x | z)"
cnf = CNFExpression.parse(formula)

# Solve it
result = solve_sat(cnf)

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

## Basic Examples

### Example 1: Understanding CNF

```python
from bsat import Literal, Clause, CNFExpression

# Method 1: Build programmatically
x = Literal('x')
y = Literal('y')
not_x = Literal('x', negated=True)

clause1 = Clause([x, y])           # (x ∨ y)
clause2 = Clause([not_x, y])       # (¬x ∨ y)

cnf = CNFExpression([clause1, clause2])
print(cnf)  # (x ∨ y) ∧ (¬x ∨ y)

# Method 2: Parse from string
cnf2 = CNFExpression.parse("(x | y) & (~x | y)")
assert cnf == cnf2
```

### Example 2: Checking Solutions

```python
from bsat import CNFExpression

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

# Try different assignments
test_cases = [
    {'a': True, 'b': False, 'c': True},
    {'a': False, 'b': True, 'c': False},
    {'a': True, 'b': True, 'c': False}
]

for assignment in test_cases:
    result = cnf.evaluate(assignment)
    print(f"{assignment} → {result}")
```

### Example 3: Truth Tables

```python
from bsat import CNFExpression

# Small formula
cnf = CNFExpression.parse("(x | y) & (~x | y)")

print("Truth table:")
cnf.print_truth_table()

# Output:
# ------------------
# x | y | Result
# ------------------
# 0 | 0 | 0
# 0 | 1 | 1
# 1 | 0 | 0
# 1 | 1 | 1
# ------------------
```

### Example 4: 2SAT vs 3SAT

```python
from bsat import CNFExpression, solve_2sat, solve_sat

# 2SAT: Fast (polynomial time)
cnf_2sat = CNFExpression.parse("(x | y) & (~x | z) & (y | ~z)")
result = solve_2sat(cnf_2sat)
print(f"2SAT result: {result}")

# 3SAT: Slower (NP-complete)
cnf_3sat = CNFExpression.parse("(x | y | z) & (~x | y | ~z) & (x | ~y | z)")
result = solve_sat(cnf_3sat)
print(f"3SAT result: {result}")
```

## Encoding Problems

### Example 5: At Most One (AMO) Constraint

Encode "at most one of {x, y, z} is True":

```python
from bsat import CNFExpression, solve_sat

# AMO: no two can be True simultaneously
# (~x ∨ ~y) ∧ (~x ∨ ~z) ∧ (~y ∨ ~z)
amo = CNFExpression.parse("(~x | ~y) & (~x | ~z) & (~y | ~z)")

# Test cases
print("Testing AMO constraint:")
tests = [
    ({'x': True, 'y': False, 'z': False}, True),   # One is true
    ({'x': False, 'y': False, 'z': False}, True),  # None is true
    ({'x': True, 'y': True, 'z': False}, False)    # Two are true (violates AMO)
]

for assignment, expected in tests:
    result = amo.evaluate(assignment)
    status = "✓" if result == expected else "✗"
    print(f"  {status} {assignment} → {result}")
```

### Example 6: Exactly One (EXO) Constraint

Encode "exactly one of {x, y, z} is True":

```python
from bsat import CNFExpression, solve_sat

# EXO = (at least one) AND (at most one)
# At least one: (x ∨ y ∨ z)
# At most one: (~x ∨ ~y) ∧ (~x ∨ ~z) ∧ (~y ∨ ~z)

exo = CNFExpression.parse("""
    (x | y | z) &
    (~x | ~y) & (~x | ~z) & (~y | ~z)
""")

# Find a solution
result = solve_sat(exo)
print(f"Solution: {result}")

# Verify exactly one is True
num_true = sum(1 for v in result.values() if v)
print(f"Number of True variables: {num_true}")  # Should be 1
```

### Example 7: If-Then Constraint

Encode "if x then y" (x → y):

```python
from bsat import CNFExpression, solve_sat

# x → y is equivalent to (~x ∨ y)
# "If x is True, then y must be True"

implies = CNFExpression.parse("~x | y")

# Test cases
tests = [
    ({'x': False, 'y': False}, True),  # x false, implication trivially true
    ({'x': False, 'y': True}, True),   # x false, implication trivially true
    ({'x': True, 'y': True}, True),    # x true, y true: satisfied
    ({'x': True, 'y': False}, False)   # x true, y false: violated!
]

print("Testing x → y:")
for assignment, expected in tests:
    result = implies.evaluate(assignment)
    status = "✓" if result == expected else "✗"
    print(f"  {status} {assignment} → {result}")
```

### Example 8: Equivalence Constraint

Encode "x ↔ y" (x if and only if y):

```python
from bsat import CNFExpression, solve_sat

# x ↔ y means (x → y) AND (y → x)
# = (~x ∨ y) ∧ (~y ∨ x)

equivalence = CNFExpression.parse("(~x | y) & (~y | x)")

# Test: x and y must have same value
tests = [
    ({'x': False, 'y': False}, True),   # Both false ✓
    ({'x': True, 'y': True}, True),     # Both true ✓
    ({'x': True, 'y': False}, False),   # Different ✗
    ({'x': False, 'y': True}, False)    # Different ✗
]

print("Testing x ↔ y:")
for assignment, expected in tests:
    result = equivalence.evaluate(assignment)
    status = "✓" if result == expected else "✗"
    print(f"  {status} {assignment} → {result}")
```

## Real-World Applications

### Example 9: Graph Coloring

Color a graph with k colors such that no adjacent vertices share a color:

```python
from bsat import CNFExpression, Clause, Literal, solve_sat

def graph_k_coloring(edges, num_vertices, k):
    """
    Encode graph k-coloring as SAT.

    Args:
        edges: List of (u, v) tuples representing edges
        num_vertices: Number of vertices (numbered 0 to num_vertices-1)
        k: Number of colors
    """
    clauses = []

    # Each vertex has at least one color
    for v in range(num_vertices):
        clause = Clause([Literal(f'v{v}_c{c}') for c in range(k)])
        clauses.append(clause)

    # Each vertex has at most one color
    for v in range(num_vertices):
        for c1 in range(k):
            for c2 in range(c1 + 1, k):
                clauses.append(Clause([
                    Literal(f'v{v}_c{c1}', negated=True),
                    Literal(f'v{v}_c{c2}', negated=True)
                ]))

    # Adjacent vertices have different colors
    for u, v in edges:
        for c in range(k):
            clauses.append(Clause([
                Literal(f'v{u}_c{c}', negated=True),
                Literal(f'v{v}_c{c}', negated=True)
            ]))

    return CNFExpression(clauses)

# Example: 3-color a square (4 vertices, 4 edges)
edges = [(0, 1), (1, 2), (2, 3), (3, 0)]
cnf = graph_k_coloring(edges, num_vertices=4, k=3)

result = solve_sat(cnf)
if result:
    print("3-colorable! Coloring:")
    for v in range(4):
        for c in range(3):
            if result.get(f'v{v}_c{c}'):
                print(f"  Vertex {v}: Color {c}")
else:
    print("Not 3-colorable")
```

### Example 10: Sudoku

Encode and solve a Sudoku puzzle:

```python
from bsat import CNFExpression, Clause, Literal, solve_sat

def sudoku_to_sat(puzzle):
    """
    Encode 4x4 Sudoku as SAT.
    puzzle: 4x4 array, 0 for empty cells, 1-4 for given values
    """
    clauses = []

    # Variable: s_r_c_v means "cell (r,c) has value v"
    # r, c, v all in range(4)

    # Each cell has at least one value
    for r in range(4):
        for c in range(4):
            clauses.append(Clause([
                Literal(f's_{r}_{c}_{v}') for v in range(1, 5)
            ]))

    # Each cell has at most one value
    for r in range(4):
        for c in range(4):
            for v1 in range(1, 5):
                for v2 in range(v1 + 1, 5):
                    clauses.append(Clause([
                        Literal(f's_{r}_{c}_{v1}', negated=True),
                        Literal(f's_{r}_{c}_{v2}', negated=True)
                    ]))

    # Each row has each value at most once
    for r in range(4):
        for v in range(1, 5):
            for c1 in range(4):
                for c2 in range(c1 + 1, 4):
                    clauses.append(Clause([
                        Literal(f's_{r}_{c1}_{v}', negated=True),
                        Literal(f's_{r}_{c2}_{v}', negated=True)
                    ]))

    # Each column has each value at most once
    for c in range(4):
        for v in range(1, 5):
            for r1 in range(4):
                for r2 in range(r1 + 1, 4):
                    clauses.append(Clause([
                        Literal(f's_{r1}_{c}_{v}', negated=True),
                        Literal(f's_{r2}_{c}_{v}', negated=True)
                    ]))

    # 2x2 blocks constraint (simplified for 4x4 Sudoku)
    for block_r in range(2):
        for block_c in range(2):
            for v in range(1, 5):
                positions = [
                    (block_r * 2 + dr, block_c * 2 + dc)
                    for dr in range(2) for dc in range(2)
                ]
                for i in range(len(positions)):
                    for j in range(i + 1, len(positions)):
                        r1, c1 = positions[i]
                        r2, c2 = positions[j]
                        clauses.append(Clause([
                            Literal(f's_{r1}_{c1}_{v}', negated=True),
                            Literal(f's_{r2}_{c2}_{v}', negated=True)
                        ]))

    # Given values (unit clauses)
    for r in range(4):
        for c in range(4):
            if puzzle[r][c] != 0:
                v = puzzle[r][c]
                clauses.append(Clause([Literal(f's_{r}_{c}_{v}')]))

    return CNFExpression(clauses)

# Example 4x4 Sudoku puzzle
puzzle = [
    [1, 0, 0, 4],
    [0, 0, 0, 0],
    [0, 0, 0, 0],
    [4, 0, 0, 1]
]

cnf = sudoku_to_sat(puzzle)
result = solve_sat(cnf)

if result:
    print("Sudoku solution:")
    for r in range(4):
        row = []
        for c in range(4):
            for v in range(1, 5):
                if result.get(f's_{r}_{c}_{v}'):
                    row.append(str(v))
        print(" ".join(row))
else:
    print("No solution")
```

### Example 11: Scheduling

Schedule tasks with constraints:

```python
from bsat import CNFExpression, Clause, Literal, solve_sat

def scheduling_to_sat(tasks, num_time_slots, constraints):
    """
    Schedule tasks into time slots.

    Args:
        tasks: List of task names
        num_time_slots: Number of available time slots
        constraints: List of (task1, task2, relation) where relation is:
                     'before': task1 must be before task2
                     'different': task1 and task2 must be in different slots
                     'same': task1 and task2 must be in the same slot
    """
    clauses = []

    # Variable: t_task_slot means "task is assigned to slot"

    # Each task assigned to at least one slot
    for task in tasks:
        clauses.append(Clause([
            Literal(f't_{task}_{slot}') for slot in range(num_time_slots)
        ]))

    # Each task assigned to at most one slot
    for task in tasks:
        for s1 in range(num_time_slots):
            for s2 in range(s1 + 1, num_time_slots):
                clauses.append(Clause([
                    Literal(f't_{task}_{s1}', negated=True),
                    Literal(f't_{task}_{s2}', negated=True)
                ]))

    # Constraints
    for task1, task2, relation in constraints:
        if relation == 'different':
            # Can't be in same slot
            for slot in range(num_time_slots):
                clauses.append(Clause([
                    Literal(f't_{task1}_{slot}', negated=True),
                    Literal(f't_{task2}_{slot}', negated=True)
                ]))

    return CNFExpression(clauses)

# Example: Schedule 3 tasks into 2 time slots
tasks = ['A', 'B', 'C']
constraints = [
    ('A', 'B', 'different'),  # A and B can't be at same time
]

cnf = scheduling_to_sat(tasks, num_time_slots=2, constraints=constraints)
result = solve_sat(cnf)

if result:
    print("Schedule:")
    for slot in range(2):
        tasks_in_slot = [
            task for task in tasks
            if result.get(f't_{task}_{slot}')
        ]
        print(f"  Slot {slot}: {', '.join(tasks_in_slot)}")
else:
    print("No valid schedule")
```

## Performance Tips

### Tip 1: Start Small

```python
# Bad: Jump straight to large problem
cnf = generate_huge_formula(1000_variables)
solve_sat(cnf)  # Might never finish

# Good: Start small, scale up
for n in [10, 20, 50, 100]:
    cnf = generate_formula(n)
    result = solve_sat(cnf)
    print(f"n={n}: {'SAT' if result else 'UNSAT'}")
```

### Tip 2: Use 2SAT When Possible

```python
# If your problem is 2SAT, use the fast solver!
from bsat import solve_2sat

cnf = CNFExpression.parse("(x | y) & (~x | z)")
result = solve_2sat(cnf)  # O(n+m) instead of O(2^n)
```

### Tip 3: Check Formula Size

```python
cnf = your_encoding()

print(f"Variables: {len(cnf.get_variables())}")
print(f"Clauses: {len(cnf.clauses)}")
print(f"Literals: {sum(len(c.literals) for c in cnf.clauses)}")

# If variables > 100, expect potential slowness
# If clauses > 1000, definitely expect slowness
```

### Tip 4: Simplify Before Solving

```python
# Remove duplicate clauses
unique_clauses = list(set(cnf.clauses))
cnf_simplified = CNFExpression(unique_clauses)

# Remove tautology clauses (e.g., x ∨ ¬x)
non_tautology = [
    clause for clause in cnf.clauses
    if not is_tautology(clause)
]
```

### Tip 5: Save and Load Formulas

```python
# Save formula to JSON for reuse
cnf = create_large_formula()
with open('formula.json', 'w') as f:
    f.write(cnf.to_json())

# Load later
with open('formula.json', 'r') as f:
    cnf = CNFExpression.from_json(f.read())
```

## Running the Examples

All examples from this documentation are available in the `examples/` directory:

```bash
# Run DPLL examples
python examples/example_dpll.py

# Run 2SAT examples
python examples/example_2sat.py

# Run basic CNF examples
python examples/example.py
```

## Next Steps

- Dive deeper into [DPLL solver](dpll-solver.md)
- Understand [2SAT algorithm](2sat-solver.md)
- Read about [CNF data structures](cnf.md)
- Explore [theory and references](theory.md)

Happy solving! 🎉
