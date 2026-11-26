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
8. **More Solvers and Tools**: DPLL, CDCL, modern CDCL, Davis-Putnam, Horn-SAT, XOR-SAT, WalkSAT, Schöning, p-bit, preprocessing, simplification, k-SAT→3-SAT reduction, and DIMACS I/O (see [Modules](#modules))

This README covers the core CNF data structures and the 2SAT solver in detail. For the other solvers see the [top-level README](../../README.md) and [`docs/`](../../docs/).

## Modules

Everything listed here is re-exported from the package root (`from bsat import ...`).

| Module | Exports |
|--------|---------|
| `cnf.py` | `Literal`, `Clause`, `CNFExpression` |
| `twosatsolver.py` | `TwoSATSolver`, `solve_2sat`, `is_2sat_satisfiable`, `is_2sat` |
| `dpll.py` | `DPLLSolver`, `solve_sat`, `find_all_sat_solutions`, `count_sat_solutions` |
| `cdcl.py` | `CDCLSolver`, `solve_cdcl`, `get_cdcl_stats` |
| `cdcl_modern.py` | `ModernCDCLSolver`, `solve_modern_cdcl`, `get_modern_cdcl_stats`, `ModernCDCLStats` |
| `davis_putnam.py` | `DavisPutnamSolver`, `solve_davis_putnam`, `get_davis_putnam_stats`, `DavisPutnamStats` |
| `hornsat.py` | `HornSATSolver`, `solve_horn_sat`, `is_horn_formula` |
| `xorsat.py` | `XORSATSolver`, `solve_xorsat`, `get_xorsat_stats` |
| `walksat.py` | `WalkSATSolver`, `solve_walksat`, `get_walksat_stats` |
| `schoening.py` | `SchoeningSolver`, `solve_schoening`, `get_schoening_stats` |
| `pbit.py` | `PBitSolver`, `solve_pbit`, `get_pbit_stats`, `PBitStats` |
| `preprocessing.py` | `SATPreprocessor`, `preprocess_cnf`, `decompose_into_components`, `decompose_and_preprocess`, `PreprocessingResult`, `PreprocessingStats` |
| `simplification.py` | `Simplifier`, `simplify_cnf`, `solve_with_simplification`, `SimplificationResult`, `SimplificationStats` |
| `reductions.py` | `reduce_to_3sat`, `extract_original_solution`, `solve_with_reduction`, `is_3sat`, `get_max_clause_size`, `ReductionStats` |
| `dimacs.py` | `parse_dimacs`, `to_dimacs`, `read_dimacs_file`, `write_dimacs_file`, `parse_dimacs_solution`, `solution_to_dimacs`, `DIMACSParseError` |
| `c_solver.py` | `solve_c`, `get_c_solver_stats`, `CSATSolver`, `CSolverStats`, `is_c_solver_available`, `BSAT_SAT`, `BSAT_UNSAT`, `BSAT_UNKNOWN` |

`c_solver.py` is an optional `ctypes` binding to the native C solver (`libbsat`). It is the only module that needs anything beyond the standard library: build the shared library with `make -C competition/c shared` and check `is_c_solver_available()` before calling `solve_c`. See [`competition/c/README.md`](../../competition/c/README.md).

## Installation

From the repository root:

```bash
pip install -e .
```

## Usage

### Creating Expressions Programmatically

```python
from bsat import CNFExpression, Clause, Literal

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
--------------
x | y | Result
--------------
0 | 0 | 0
0 | 1 | 1
1 | 0 | 0
1 | 1 | 0
--------------
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
from bsat import CNFExpression, solve_2sat, is_2sat_satisfiable, TwoSATSolver

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

From the repository root (after `pip install -e .`):

```bash
python examples/example.py       # General CNF examples
python examples/example_2sat.py  # 2SAT solver examples
python tests/test_2sat.py        # Run 2SAT solver tests
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
- `to_json(indent: int = 2)` / `from_json(json_str: str)`: JSON serialization
- `to_dict()` / `from_dict()`: Dictionary conversion

### TwoSATSolver

Solves 2SAT problems using strongly connected components (Kosaraju's algorithm).

- `__init__(cnf: CNFExpression)`: Initialize with a 2SAT instance (raises `ValueError` if any clause does not have exactly 2 literals)
- `is_satisfiable()`: Check if the formula is satisfiable
- `solve()`: Return a satisfying assignment or None
- `get_all_solutions()`: Return all satisfying assignments
- Time complexity: O(n + m) where n = variables, m = clauses
- Space complexity: O(n + m)

### Helper Functions

- `solve_2sat(cnf: CNFExpression)`: Solve a 2SAT problem, returns assignment or None
- `is_2sat_satisfiable(cnf: CNFExpression)`: Check if a 2SAT problem is satisfiable
- `is_2sat(cnf: CNFExpression)`: Check whether every clause has exactly 2 literals

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
