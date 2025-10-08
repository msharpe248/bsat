# BSAT - Boolean Satisfiability Package

A Python package for working with Boolean satisfiability (SAT) problems using Conjunctive Normal Form (CNF) expressions.

## Features

1. **CNF Data Structures**: Literal, Clause, and CNFExpression classes
2. **Pretty Printing**: Display expressions using standard logical notation (∧, ∨, ¬)
3. **Parsing**: Read expressions from strings using multiple notation styles
4. **JSON Support**: Serialize and deserialize expressions to/from JSON
5. **Truth Tables**: Generate and display complete truth tables
6. **Logical Equivalence**: Compare expressions using truth table comparison
7. **2SAT Solver**: Optimal O(n+m) algorithm using strongly connected components

## Installation

```bash
# Activate the virtual environment
source venv/bin/activate

# The package is ready to use locally
```

## Usage

### Creating Expressions Programmatically

```python
from cnf import CNFExpression, Clause, Literal

# Create (x ∨ y) ∧ (¬x ∨ z)
clause1 = Clause([Literal("x"), Literal("y")])
clause2 = Clause([Literal("x", negated=True), Literal("z")])
expr = CNFExpression([clause1, clause2])

print(expr)  # (x ∨ y) ∧ (¬x ∨ z)
```

### Parsing from Strings

```python
# Unicode symbols
expr = CNFExpression.parse("(a ∨ b) ∧ (¬a ∨ c)")

# ASCII symbols
expr = CNFExpression.parse("(a | b) & (~a | c)")

# Text notation
expr = CNFExpression.parse("(a OR b) AND (NOT a OR c)")
```

### Truth Tables

```python
expr = CNFExpression.parse("(x ∨ y) ∧ ¬x")
expr.print_truth_table()
```

Output:
```
----------------
x | y | Result
----------------
0 | 0 | 0
0 | 1 | 1
1 | 0 | 0
1 | 1 | 0
----------------
```

### Checking Logical Equivalence

```python
expr1 = CNFExpression.parse("(x ∨ y)")
expr2 = CNFExpression.parse("(y ∨ x)")

if expr1.is_equivalent(expr2):
    print("Expressions are logically equivalent!")
```

### JSON Serialization

```python
expr = CNFExpression.parse("(a ∨ ¬b) ∧ (b ∨ c)")

# To JSON
json_str = expr.to_json()

# From JSON
restored = CNFExpression.from_json(json_str)
```

### Solving 2SAT Problems

```python
from cnf import CNFExpression
from twosatsolver import solve_2sat, is_2sat_satisfiable, TwoSATSolver

# Create a 2SAT instance (all clauses must have exactly 2 literals)
expr = CNFExpression.parse("(x | y) & (~x | z) & (~y | z)")

# Check if satisfiable
if is_2sat_satisfiable(expr):
    # Get a solution
    solution = solve_2sat(expr)
    print(f"Solution: {solution}")
    print(f"Verification: {expr.evaluate(solution)}")
else:
    print("Unsatisfiable")

# Or use the solver class directly
solver = TwoSATSolver(expr)
solution = solver.solve()
```

## Running Examples

```bash
source venv/bin/activate
python example.py       # General CNF examples
python example_2sat.py  # 2SAT solver examples
python test_2sat.py     # Run 2SAT solver tests
```

## API Reference

### Literal

Represents a variable or its negation.

- `__init__(variable: str, negated: bool = False)`
- `__str__()`: Returns string representation (e.g., "x" or "¬x")
- `evaluate(assignment: Dict[str, bool])`: Evaluate with variable assignment
- `to_dict()` / `from_dict()`: JSON serialization

### Clause

Represents a disjunction of literals.

- `__init__(literals: List[Literal])`
- `__str__()`: Returns string representation (e.g., "(x ∨ y ∨ z)")
- `evaluate(assignment: Dict[str, bool])`: Evaluate with variable assignment
- `get_variables()`: Get all variables in the clause
- `to_dict()` / `from_dict()`: JSON serialization

### CNFExpression

Represents a CNF expression (conjunction of clauses).

- `__init__(clauses: List[Clause])`
- `__str__()`: Returns string representation
- `parse(expression: str)`: Parse from string notation
- `evaluate(assignment: Dict[str, bool])`: Evaluate with variable assignment
- `get_variables()`: Get all variables in the expression
- `generate_truth_table()`: Generate complete truth table
- `print_truth_table()`: Print formatted truth table
- `is_equivalent(other: CNFExpression)`: Check logical equivalence
- `to_json()` / `from_json()`: JSON serialization
- `to_dict()` / `from_dict()`: Dictionary conversion

### TwoSATSolver

Solves 2SAT problems using strongly connected components (Kosaraju's algorithm).

- `__init__(cnf: CNFExpression)`: Initialize with a 2SAT instance
- `is_satisfiable()`: Check if the formula is satisfiable
- `solve()`: Return a satisfying assignment or None
- Time complexity: O(n + m) where n = variables, m = clauses
- Space complexity: O(n + m)

### Helper Functions

- `solve_2sat(cnf: CNFExpression)`: Solve a 2SAT problem, returns assignment or None
- `is_2sat_satisfiable(cnf: CNFExpression)`: Check if a 2SAT problem is satisfiable

## Supported Notations

### Negation
- Unicode: `¬`
- ASCII: `~`, `!`
- Text: `NOT`

### Disjunction (OR)
- Unicode: `∨`
- ASCII: `|`
- Text: `OR`

### Conjunction (AND)
- Unicode: `∧`
- ASCII: `&`
- Text: `AND`

## License

MIT License
