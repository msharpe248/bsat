# BSAT - Boolean Satisfiability Package

A Python package for learning and solving Boolean satisfiability (SAT) problems using Conjunctive Normal Form (CNF).

## Features

✅ **CNF Data Structures**: Clean, Pythonic representation (Literal, Clause, CNFExpression)
✅ **2SAT Solver**: O(n+m) polynomial-time algorithm using strongly connected components
✅ **DPLL Solver**: Backtracking with unit propagation and pure literal elimination
✅ **Horn-SAT Solver**: O(n+m) polynomial-time solver for Horn formulas
✅ **Pretty Printing**: Unicode symbols (∧, ∨, ¬) for readable output
✅ **Multiple Input Formats**: Parse from text, JSON, or build programmatically
✅ **Truth Tables**: Generate and compare truth tables
🚧 **Coming Soon**: CDCL, WalkSAT, XOR-SAT

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
from bsat import CNFExpression, solve_sat, solve_2sat

# Parse a 3SAT formula
formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

# Solve it
result = solve_sat(cnf)  # Uses DPLL solver

if result:
    print(f"SAT: {result}")
    print(f"Verification: {cnf.evaluate(result)}")
else:
    print("UNSAT")

# For 2SAT, use the faster polynomial-time solver
formula_2sat = "(x | y) & (~x | z) & (y | ~z)"
cnf_2sat = CNFExpression.parse(formula_2sat)
result_2sat = solve_2sat(cnf_2sat)  # O(n+m) time!
```

## Examples

Run the example scripts:

```bash
python examples/example.py        # General CNF examples
python examples/example_2sat.py   # 2SAT solver examples
python examples/example_dpll.py   # DPLL solver examples
```

## Testing

Run the test suite:

```bash
python tests/test_2sat.py   # 2SAT tests
python tests/test_dpll.py   # DPLL tests
```

Or with pytest (if installed):

```bash
pytest tests/
```

## Documentation

📚 **[Complete Documentation](docs/README.md)**

- [Introduction to SAT](docs/introduction.md) - Start here if you're new!
- [CNF Data Structures](docs/cnf.md) - Understanding the API
- [2SAT Solver](docs/2sat-solver.md) - Polynomial-time algorithm
- [DPLL Solver](docs/dpll-solver.md) - General SAT solving
- [Examples & Tutorials](docs/examples.md) - Practical usage
- [Theory & References](docs/theory.md) - Papers and further reading

## License

MIT License - see LICENSE file for details.
