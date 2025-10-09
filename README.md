# BSAT - Boolean Satisfiability Package

A Python package for learning and solving Boolean satisfiability (SAT) problems using Conjunctive Normal Form (CNF).

## Features

✅ **CNF Data Structures**: Clean, Pythonic representation (Literal, Clause, CNFExpression)
✅ **2SAT Solver**: O(n+m) polynomial-time algorithm using strongly connected components
✅ **DPLL Solver**: Backtracking with unit propagation and pure literal elimination
✅ **Horn-SAT Solver**: O(n+m) polynomial-time solver for Horn formulas
✅ **XOR-SAT Solver**: O(n³) polynomial-time solver using Gaussian elimination over GF(2)
✅ **WalkSAT Solver**: Randomized local search (incomplete but often very fast)
✅ **k-SAT to 3-SAT Reduction**: Convert any CNF to 3-SAT form using auxiliary variables
✅ **Pretty Printing**: Unicode symbols (∧, ∨, ¬) for readable output
✅ **Multiple Input Formats**: Parse from text, JSON, or build programmatically
✅ **Truth Tables**: Generate and compare truth tables
🚧 **Coming Soon**: CDCL

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

### General SAT (DPLL)

```python
from bsat import CNFExpression, solve_sat

# Parse a 3SAT formula
formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

# Solve it using DPLL
result = solve_sat(cnf)

if result:
    print(f"SAT: {result}")
    print(f"Verification: {cnf.evaluate(result)}")
else:
    print("UNSAT")
```

### 2SAT (Polynomial-time)

```python
from bsat import solve_2sat, CNFExpression

# 2SAT is solved in O(n+m) time using SCC
formula_2sat = "(x | y) & (~x | z) & (y | ~z)"
cnf_2sat = CNFExpression.parse(formula_2sat)
result = solve_2sat(cnf_2sat)  # O(n+m) time!
```

### Horn-SAT (Logic Programming)

```python
from bsat import solve_horn_sat, is_horn_formula, CNFExpression

# Horn clauses have at most one positive literal
formula = "x & (~x | y) & (~y | z)"
cnf = CNFExpression.parse(formula)

if is_horn_formula(cnf):
    result = solve_horn_sat(cnf)  # O(n+m) time!
    print(f"Solution: {result}")
```

### XOR-SAT (Cryptography & Coding Theory)

```python
from bsat import solve_xorsat, CNFExpression, Clause, Literal

# XOR clauses: odd number of literals must be true
# x ⊕ y = 1 (they must differ)
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)])
])

result = solve_xorsat(cnf)  # O(n³) using Gaussian elimination
print(f"Solution: {result}")
```

### WalkSAT (Fast Randomized Search)

```python
from bsat import solve_walksat, CNFExpression

# Randomized local search - incomplete but often very fast
formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

result = solve_walksat(cnf, noise=0.5, max_flips=10000, seed=42)
if result:
    print(f"Solution found: {result}")
else:
    print("No solution found (but may still be SAT)")
```

### k-SAT to 3-SAT Reduction

```python
from bsat import reduce_to_3sat, solve_with_reduction, CNFExpression, Clause, Literal

# 5-SAT formula
cnf = CNFExpression([
    Clause([Literal('a'), Literal('b'), Literal('c'), Literal('d'), Literal('e')])
])

# Reduce to 3-SAT (introduces auxiliary variables)
reduced, aux_map, stats = reduce_to_3sat(cnf)
print(f"Reduced: {reduced}")  # All clauses now have ≤ 3 literals

# Or solve directly with automatic reduction
solution, stats = solve_with_reduction(cnf)
print(f"Solution: {solution}")  # Only original variables
```

## Solver Comparison

Choose the right solver for your problem:

| Problem Type | Solver | Complexity | Complete | Use When |
|-------------|---------|-----------|----------|----------|
| **2SAT** | `solve_2sat()` | O(n+m) | ✅ Yes | Every clause has exactly 2 literals |
| **Horn-SAT** | `solve_horn_sat()` | O(n+m) | ✅ Yes | At most 1 positive literal per clause (logic programming) |
| **XOR-SAT** | `solve_xorsat()` | O(n³) | ✅ Yes | XOR constraints (cryptography, coding theory) |
| **General SAT** | `solve_sat()` | O(2ⁿ)* | ✅ Yes | Any CNF formula (uses DPLL) |
| **Fast SAT** | `solve_walksat()` | Varies | ❌ No | Large SAT instances where speed > completeness |

*Exponential worst-case, but often practical with optimizations

**Quick decision guide:**
- All clauses have 2 literals? → Use `solve_2sat()`
- Clauses are implications (≤1 positive literal)? → Use `solve_horn_sat()`
- XOR/parity constraints? → Use `solve_xorsat()`
- Need guaranteed solution? → Use `solve_sat()` (DPLL)
- Want fast solutions for large SAT instances? → Try `solve_walksat()`

## Examples

Run the example scripts:

```bash
python examples/example.py            # General CNF examples
python examples/example_2sat.py       # 2SAT solver examples
python examples/example_dpll.py       # DPLL solver examples
python examples/example_hornsat.py    # Horn-SAT solver examples
python examples/example_xorsat.py     # XOR-SAT solver examples
python examples/example_walksat.py    # WalkSAT solver examples
python examples/example_reductions.py # k-SAT to 3-SAT reduction examples
```

## Testing

Run the test suite:

```bash
python tests/test_2sat.py        # 2SAT tests
python tests/test_dpll.py        # DPLL tests
python tests/test_hornsat.py     # Horn-SAT tests
python tests/test_xorsat.py      # XOR-SAT tests
python tests/test_walksat.py     # WalkSAT tests
python tests/test_reductions.py  # k-SAT reduction tests
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
- [DPLL Solver](docs/dpll-solver.md) - General SAT solving with backtracking
- [Horn-SAT Solver](docs/advanced-solvers.md#horn-sat) - Polynomial-time Horn formula solver
- [XOR-SAT Solver](docs/xorsat-solver.md) - Polynomial-time XOR solver via Gaussian elimination
- [WalkSAT Solver](docs/walksat-solver.md) - Randomized local search (incomplete but fast)
- [k-SAT to 3-SAT Reduction](docs/introduction.md#reducing-k-sat-to-3-sat) - Theory and implementation
- [Examples & Tutorials](docs/examples.md) - Practical usage
- [Theory & References](docs/theory.md) - Papers and further reading
- [Reading List](docs/reading-list.md) - Comprehensive bibliography of books, papers, and resources

## License

MIT License - see LICENSE file for details.
