# CNF Expressions

Understanding the data structures for representing Boolean formulas in Conjunctive Normal Form.

## Core Data Structures

The BSAT package provides three main classes to represent CNF formulas:

```
CNFExpression
    ├── Clause (multiple)
    │   └── Literal (multiple)
    └── Clause
        └── Literal
```

### Literal

A **literal** is the smallest unit: a variable or its negation.

```python
from bsat import Literal

# Positive literal (x)
x = Literal('x', negated=False)
x = Literal('x')  # negated defaults to False

# Negative literal (¬x)
not_x = Literal('x', negated=True)

print(x)       # Output: x
print(not_x)   # Output: ¬x
```

**Properties**:
- `variable`: The variable name (string)
- `negated`: Boolean indicating if it's negated

**Methods**:
- `evaluate(assignment)`: Evaluate given a variable assignment

```python
x = Literal('x')
not_x = Literal('x', negated=True)

print(x.evaluate({'x': True}))      # True
print(not_x.evaluate({'x': True}))  # False
```

### Clause

A **clause** is a disjunction (OR) of literals.

```python
from bsat import Literal, Clause

# Create clause (x ∨ y ∨ z)
clause = Clause([
    Literal('x'),
    Literal('y'),
    Literal('z')
])

print(clause)  # Output: (x ∨ y ∨ z)
```

**Properties**:
- `literals`: List of Literal objects

**Methods**:
- `evaluate(assignment)`: Returns True if at least one literal is True
- `get_variables()`: Returns set of all variables in the clause

```python
clause = Clause([Literal('x'), Literal('y', negated=True)])

# Clause is True if x=True OR y=False
print(clause.evaluate({'x': True, 'y': True}))   # True (x is True)
print(clause.evaluate({'x': False, 'y': False})) # True (¬y is True)
print(clause.evaluate({'x': False, 'y': True}))  # False (both literals False)

print(clause.get_variables())  # {'x', 'y'}
```

### CNFExpression

A **CNF expression** is a conjunction (AND) of clauses.

```python
from bsat import CNFExpression, Clause, Literal

# Create CNF: (x ∨ y) ∧ (¬x ∨ z)
cnf = CNFExpression([
    Clause([Literal('x'), Literal('y')]),
    Clause([Literal('x', negated=True), Literal('z')])
])

print(cnf)  # Output: (x ∨ y) ∧ (¬x ∨ z)
```

**Properties**:
- `clauses`: List of Clause objects

**Methods**:
- `evaluate(assignment)`: Returns True if ALL clauses are True
- `get_variables()`: Returns set of all variables
- `generate_truth_table()`: Generate all possible assignments
- `print_truth_table()`: Print formatted truth table
- `is_equivalent(other)`: Check logical equivalence with another CNF

## Creating CNF Expressions

There are three ways to create CNF expressions:

### 1. Programmatic Construction

Build from Literal, Clause, and CNFExpression objects:

```python
from bsat import Literal, Clause, CNFExpression

# (x ∨ y ∨ z) ∧ (¬x ∨ y) ∧ (x ∨ ¬z)
cnf = CNFExpression([
    Clause([Literal('x'), Literal('y'), Literal('z')]),
    Clause([Literal('x', True), Literal('y')]),
    Clause([Literal('x'), Literal('z', True)])
])
```

**Pros**: Type-safe, explicit, good for generated formulas
**Cons**: Verbose for manual creation

### 2. Parsing from Strings

Parse from human-readable text:

```python
from bsat import CNFExpression

# Multiple notation styles supported
cnf1 = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
cnf2 = CNFExpression.parse("(x ∨ y ∨ z) ∧ (¬x ∨ y) ∧ (x ∨ ¬z)")
cnf3 = CNFExpression.parse("(x OR y OR z) AND (NOT x OR y) AND (x OR NOT z)")

# All three create the same CNF
assert cnf1 == cnf2 == cnf3
```

**Supported Notation**:

| Operator | Symbols | Example |
|----------|---------|---------|
| NOT | `¬`, `~`, `!`, `NOT` | `¬x`, `~x`, `!x`, `NOT x` |
| OR | `∨`, `\|`, `OR` | `x ∨ y`, `x \| y`, `x OR y` |
| AND | `∧`, `&`, `AND` | `p ∧ q`, `p & q`, `p AND q` |

**Pros**: Readable, concise, good for examples
**Cons**: No compile-time checking

### 3. JSON Serialization

Load from/save to JSON:

```python
from bsat import CNFExpression

# Create a CNF
cnf = CNFExpression.parse("(x | y) & (~x | z)")

# Serialize to JSON
json_str = cnf.to_json()
print(json_str)
# {
#   "clauses": [
#     {"literals": [{"variable": "x", "negated": false}, {"variable": "y", "negated": false}]},
#     {"literals": [{"variable": "x", "negated": true}, {"variable": "z", "negated": false}]}
#   ]
# }

# Deserialize from JSON
cnf2 = CNFExpression.from_json(json_str)
assert cnf == cnf2
```

**Pros**: Portable, language-independent, good for storage
**Cons**: Verbose, harder to read

## Evaluating CNF Expressions

### Direct Evaluation

```python
cnf = CNFExpression.parse("(x | y) & (~x | z)")

# Check a specific assignment
assignment = {'x': True, 'y': False, 'z': True}
result = cnf.evaluate(assignment)
print(f"Formula is {result} for {assignment}")
# Output: Formula is True for {'x': True, 'y': False, 'z': True}
```

### Truth Tables

Generate and display all possible assignments:

```python
cnf = CNFExpression.parse("(x | y) & (~x | y)")

# Print formatted truth table
cnf.print_truth_table()
```

Output:
```
------------------
x | y | Result
------------------
0 | 0 | 0
0 | 1 | 1
1 | 0 | 0
1 | 1 | 1
------------------
```

### Programmatic Truth Table Access

```python
cnf = CNFExpression.parse("(x | y)")

for assignment, result in cnf.generate_truth_table():
    print(f"{assignment} → {result}")

# Output:
# {'x': False, 'y': False} → False
# {'x': False, 'y': True} → True
# {'x': True, 'y': False} → True
# {'x': True, 'y': True} → True
```

## Checking Equivalence

Two formulas are **logically equivalent** if they have the same truth table:

```python
cnf1 = CNFExpression.parse("(x | y) & (~x | y)")
cnf2 = CNFExpression.parse("y | (~y & x & ~x)")  # Different structure, same logic?

if cnf1.is_equivalent(cnf2):
    print("Formulas are equivalent!")
else:
    print("Formulas are different")
```

**Note**: This uses truth table comparison, which is exponential in the number of variables. Use only for small formulas (≤ 20 variables).

## Working with Variables

```python
cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (a | b)")

# Get all variables
variables = cnf.get_variables()
print(variables)  # {'a', 'b', 'x', 'y', 'z'}

# Count variables and clauses
print(f"Variables: {len(cnf.get_variables())}")  # 5
print(f"Clauses: {len(cnf.clauses)}")             # 3
```

## Special Cases

### Empty Clause (⊥)

An empty clause is **always False** (unsatisfiable):

```python
empty_clause = Clause([])
print(empty_clause)  # Output: ⊥
print(empty_clause.evaluate({'x': True}))  # False (always)
```

If a CNF contains an empty clause, the entire formula is unsatisfiable.

### Empty CNF (⊤)

An empty CNF (no clauses) is **always True** (trivially satisfiable):

```python
empty_cnf = CNFExpression([])
print(empty_cnf)  # Output: ⊤
print(empty_cnf.evaluate({}))  # True (always)
```

### Unit Clause

A clause with exactly one literal:

```python
unit = Clause([Literal('x')])
print(unit)  # Output: x (not (x), just x)
```

Unit clauses are important in SAT solving - they force a variable assignment.

## Performance Tips

### 1. Avoid Repeated Truth Tables

```python
# Bad: Generates truth table multiple times
for i in range(100):
    cnf.print_truth_table()  # O(2^n) each time!

# Good: Generate once, reuse
table = cnf.generate_truth_table()
for assignment, result in table:
    process(assignment, result)
```

### 2. Use Direct Evaluation

```python
# Bad: Check all assignments to find one
for assignment, result in cnf.generate_truth_table():
    if result:
        return assignment  # O(2^n)

# Good: Use a SAT solver
from bsat import solve_sat
result = solve_sat(cnf)  # Much faster for large formulas
```

### 3. Equality vs Equivalence

```python
# Fast: Structural equality (O(n×m))
cnf1 == cnf2  # Are they the same object?

# Slow: Logical equivalence (O(2^n × m))
cnf1.is_equivalent(cnf2)  # Do they have the same truth table?
```

## Examples

### Example 1: Building a Formula

```python
from bsat import Literal, Clause, CNFExpression

# Build (a ∨ b ∨ c) ∧ (¬a ∨ ¬b) ∧ (¬b ∨ ¬c) ∧ (¬a ∨ ¬c)
# This encodes "at most one of a, b, c is true"

cnf = CNFExpression([
    Clause([Literal('a'), Literal('b'), Literal('c')]),      # At least one
    Clause([Literal('a', True), Literal('b', True)]),        # Not both a and b
    Clause([Literal('b', True), Literal('c', True)]),        # Not both b and c
    Clause([Literal('a', True), Literal('c', True)])         # Not both a and c
])

# Test it
print(cnf.evaluate({'a': True, 'b': False, 'c': False}))  # True (only a)
print(cnf.evaluate({'a': True, 'b': True, 'c': False}))   # False (both a and b)
```

### Example 2: Converting Notations

```python
# Start with ASCII notation (easy to type)
formula = "(p1 | p2 | p3) & (~p1 | ~p2) & (~p2 | ~p3) & (~p1 | ~p3)"
cnf = CNFExpression.parse(formula)

# Display with Unicode (pretty)
print(cnf)
# (p1 ∨ p2 ∨ p3) ∧ (¬p1 ∨ ¬p2) ∧ (¬p2 ∨ ¬p3) ∧ (¬p1 ∨ ¬p3)

# Save to JSON (portable)
with open('formula.json', 'w') as f:
    f.write(cnf.to_json())
```

### Example 3: Analyzing a Formula

```python
cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (~y | z) & (~z | x)")

# Basic stats
print(f"Variables: {len(cnf.get_variables())}")
print(f"Clauses: {len(cnf.clauses)}")
print(f"Literals: {sum(len(c.literals) for c in cnf.clauses)}")

# Check satisfiability
from bsat import solve_sat
solution = solve_sat(cnf)

if solution:
    print(f"SAT: {solution}")
    print(f"Verification: {cnf.evaluate(solution)}")
else:
    print("UNSAT")
```

## Further Reading

- [DIMACS CNF Format](http://www.satcompetition.org/2009/format-benchmarks2009.html) - Standard file format for SAT instances
- [Introduction to SAT](introduction.md) - Understanding SAT problems
- [DPLL Solver](dpll-solver.md) - How to solve CNF formulas

## Next Steps

- Learn about [2SAT solving](2sat-solver.md)
- Try the [DPLL algorithm](dpll-solver.md)
- Work through [examples and tutorials](examples.md)
