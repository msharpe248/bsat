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

### From Source

```bash
# Clone or navigate to the repository
cd bsat

# Install in development mode
pip install -e .

# Or install normally
pip install .
```

### With Development Dependencies

```bash
pip install -e ".[dev]"
```

## Quick Start

```python
from bsat import CNFExpression, solve_2sat

# Parse a CNF expression
expr = CNFExpression.parse("(x | y) & (~x | z)")

# Print it
print(expr)  # (x ∨ y) ∧ (¬x ∨ z)

# Generate truth table
expr.print_truth_table()

# Solve 2SAT problems
expr_2sat = CNFExpression.parse("(a | b) & (~a | c) & (~b | c)")
solution = solve_2sat(expr_2sat)
print(f"Solution: {solution}")
```

## Examples

Run the example scripts:

```bash
python examples/example.py       # General CNF examples
python examples/example_2sat.py  # 2SAT solver examples
```

## Testing

Run the test suite:

```bash
python tests/test_2sat.py
```

Or with pytest (if installed):

```bash
pytest tests/
```

## Documentation

See `src/bsat/README.md` for detailed API documentation and usage examples.

## License

MIT License - see LICENSE file for details.
