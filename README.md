# BSAT - Boolean Satisfiability Package

A Python package for learning and solving Boolean satisfiability (SAT) problems using Conjunctive Normal Form (CNF).

## Features

✅ **CNF Data Structures**: Clean, Pythonic representation (Literal, Clause, CNFExpression)
✅ **2SAT Solver**: O(n+m) polynomial-time algorithm using strongly connected components
✅ **DPLL Solver**: Backtracking with unit propagation and pure literal elimination
✅ **CDCL Solver**: Conflict-Driven Clause Learning with VSIDS heuristic and restarts
✅ **Horn-SAT Solver**: O(n+m) polynomial-time solver for Horn formulas
✅ **XOR-SAT Solver**: O(n³) polynomial-time solver using Gaussian elimination over GF(2)
✅ **WalkSAT Solver**: Randomized local search (incomplete but often very fast)
✅ **Schöning's Algorithm**: Randomized k-SAT solver with O(1.334^n) expected runtime for 3SAT
✅ **SAT Preprocessing**: Simplification techniques (decomposition, unit propagation, subsumption)
✅ **k-SAT to 3-SAT Reduction**: Convert any CNF to 3-SAT form using auxiliary variables
✅ **Pretty Printing**: Unicode symbols (∧, ∨, ¬) for readable output
✅ **Multiple Input Formats**: Parse from text, JSON, or build programmatically
✅ **DIMACS Format**: Full support for reading/writing industry-standard DIMACS CNF files
✅ **Truth Tables**: Generate and compare truth tables
✅ **Comprehensive Benchmarks**: Test suite with random 3SAT, pigeon-hole, phase transition instances
✅ **Performance Comparison**: Tools for comparing solver performance and analyzing results

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

### CDCL (Modern SAT Solving)

```python
from bsat import solve_cdcl, get_cdcl_stats, CNFExpression

# CDCL uses conflict-driven clause learning
formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

result = solve_cdcl(cnf)
if result:
    print(f"Solution: {result}")

# Get detailed statistics
result, stats = get_cdcl_stats(cnf)
print(f"Decisions: {stats.decisions}")
print(f"Conflicts: {stats.conflicts}")
print(f"Learned clauses: {stats.learned_clauses}")
```

### Schöning's Algorithm (Provably Fast 3SAT)

```python
from bsat import solve_schoening, get_schoening_stats, CNFExpression

# Schöning's randomized algorithm - O(1.334^n) for 3SAT!
formula = "(a | b | c) & (~a | b | ~c) & (a | ~b | c)"
cnf = CNFExpression.parse(formula)

result = solve_schoening(cnf, seed=42)
if result:
    print(f"Solution: {result}")
else:
    print("No solution found (algorithm is incomplete)")

# Get detailed statistics
result, stats = get_schoening_stats(cnf, seed=42)
print(f"Tries: {stats.tries}")
print(f"Total flips: {stats.total_flips}")
```

### SAT Preprocessing (Simplification)

```python
from bsat import decompose_and_preprocess, CNFExpression, solve_sat

# Complex formula with structure
formula = "a & (a | b) & (~a | c) & (d | e) & (f | g) & f"
cnf = CNFExpression.parse(formula)

# Decompose into components and preprocess
components, forced, stats = decompose_and_preprocess(cnf)

print(f"Reduced: {stats.original_clauses} → {stats.final_clauses} clauses")
print(f"Forced assignments: {forced}")
print(f"Independent components: {len(components)}")

# Solve remaining components
solution = forced.copy()
for comp in components:
    sol = solve_sat(comp)
    if sol:
        solution.update(sol)
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
| **Modern SAT** | `solve_cdcl()` | O(2ⁿ)* | ✅ Yes | Structured problems, industrial instances |
| **Random 3SAT** | `solve_schoening()` | O(1.334ⁿ)† | ❌ No | Random 3SAT, theoretical analysis |
| **Fast SAT** | `solve_walksat()` | Varies | ❌ No | Large SAT instances where speed > completeness |

*Exponential worst-case, but CDCL much faster in practice due to learning
†Expected time for 3SAT - provably better than O(2ⁿ)!

**Quick decision guide:**
- All clauses have 2 literals? → Use `solve_2sat()`
- Clauses are implications (≤1 positive literal)? → Use `solve_horn_sat()`
- XOR/parity constraints? → Use `solve_xorsat()`
- Random 3SAT instance? → Try `solve_schoening()` (provably O(1.334^n))
- Structured/industrial problem? → Use `solve_cdcl()` (modern, learns from conflicts)
- Simple/small problem? → Use `solve_sat()` (DPLL, simpler)
- Want fast solutions for large SAT instances? → Try `solve_walksat()`

## Examples

Run the example scripts:

```bash
# Solver examples
python examples/example.py            # General CNF examples
python examples/example_2sat.py       # 2SAT solver examples
python examples/example_dpll.py       # DPLL solver examples
python examples/example_cdcl.py       # CDCL solver examples
python examples/example_hornsat.py    # Horn-SAT solver examples
python examples/example_xorsat.py     # XOR-SAT solver examples
python examples/example_walksat.py    # WalkSAT solver examples
python examples/example_schoening.py    # Schöning's algorithm examples
python examples/example_preprocessing.py # SAT preprocessing examples
python examples/example_reductions.py   # k-SAT to 3-SAT reduction examples
python examples/example_dimacs.py       # DIMACS format examples

# Real-world problem encodings
python examples/encodings/graph_coloring.py  # Graph coloring problems
python examples/encodings/sudoku.py          # Sudoku solver
python examples/encodings/n_queens.py        # N-Queens problem

# Performance benchmarking
python examples/benchmark_comparison.py      # Compare solver performance
```

## Testing

Run the test suite:

```bash
python tests/test_2sat.py        # 2SAT tests
python tests/test_dpll.py        # DPLL tests
python tests/test_cdcl.py        # CDCL tests
python tests/test_hornsat.py     # Horn-SAT tests
python tests/test_xorsat.py      # XOR-SAT tests
python tests/test_walksat.py     # WalkSAT tests
python tests/test_schoening.py     # Schöning's algorithm tests
python tests/test_preprocessing.py # Preprocessing tests
python tests/test_reductions.py    # k-SAT reduction tests
python tests/test_dimacs.py        # DIMACS format tests
python tests/test_benchmarks.py  # Benchmark suite tests
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
- [CDCL Solver](docs/cdcl-solver.md) - Modern SAT solving with conflict-driven learning
- [Horn-SAT Solver](docs/advanced-solvers.md#horn-sat) - Polynomial-time Horn formula solver
- [XOR-SAT Solver](docs/xorsat-solver.md) - Polynomial-time XOR solver via Gaussian elimination
- [WalkSAT Solver](docs/walksat-solver.md) - Randomized local search (incomplete but fast)
- [Schöning's Algorithm](docs/schoening-solver.md) - Provably O(1.334^n) randomized 3SAT solver
- [SAT Preprocessing](docs/preprocessing.md) - Simplification and decomposition techniques
- [k-SAT to 3-SAT Reduction](docs/introduction.md#reducing-k-sat-to-3-sat) - Theory and implementation
- [DIMACS Format](docs/dimacs.md) - Industry-standard file format for SAT solvers
- [Problem Encodings](docs/problem-encodings.md) - Graph coloring, Sudoku, N-Queens, and more
- [Benchmarking & Performance](docs/benchmarking.md) - Testing and comparing solvers
- [Examples & Tutorials](docs/examples.md) - Practical usage
- [Theory & References](docs/theory.md) - Papers and further reading
- [Reading List](docs/reading-list.md) - Comprehensive bibliography of books, papers, and resources

## License

MIT License - see LICENSE file for details.
