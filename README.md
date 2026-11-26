# BSAT - Boolean Satisfiability in Python

BSAT is a Python package for **learning** Boolean satisfiability (SAT). It pairs small, readable implementations of the classic SAT algorithms with documentation that explains the theory behind them: why 2SAT is easy and 3SAT is hard, how DPLL became CDCL, what Schaefer's dichotomy says, and how real problems are encoded as CNF.

The core package is pure Python with zero dependencies. Every solver is written to be read, stepped through, and modified, not to win benchmarks.

## What's Inside

### Solvers

| Solver | Function | Complete? | What it teaches |
|--------|----------|-----------|-----------------|
| **Davis-Putnam** (1960) | `solve_davis_putnam()` | ✅ | The original resolution-based procedure, and why it runs out of memory |
| **DPLL** (1962) | `solve_sat()` | ✅ | Backtracking search, unit propagation, pure literal elimination |
| **CDCL** | `solve_cdcl()` | ✅ | Conflict-driven clause learning, VSIDS, non-chronological backtracking, restarts |
| **Modern CDCL** | `solve_modern_cdcl()` | ✅ | What makes CDCL fast in practice: two-watched literals, clause minimization, phase saving, LBD clause deletion, Glucose restarts |
| **2SAT** | `solve_2sat()` | ✅ | Implication graphs and strongly connected components |
| **Horn-SAT** | `solve_horn_sat()` | ✅ | Forward chaining / unit propagation as a decision procedure |
| **XOR-SAT** | `solve_xorsat()` | ✅ | Gaussian elimination over GF(2) |
| **WalkSAT** | `solve_walksat()` | ❌ | Randomized local search |
| **Schöning** | `solve_schoening()` | ❌ | A random walk with a provable O(1.334ⁿ) bound for 3SAT |
| **P-bit** | `solve_pbit()` | ❌ | Gibbs sampling / simulated annealing, modeled on probabilistic-bit hardware |

"Complete" means the solver can prove unsatisfiability. The incomplete solvers return `None` when they give up, which does not mean the formula is UNSAT.

### Tools

- **CNF data structures**: `Literal`, `Clause`, `CNFExpression`, with Unicode pretty printing (∧, ∨, ¬)
- **Parsing and I/O**: text formulas, JSON, and the industry-standard DIMACS format
- **Truth tables**: generate and compare
- **Preprocessing**: unit propagation, subsumption, and decomposition into independent components
- **Simplification**: failed literal probing, blocked clause elimination, bounded variable elimination, and vivification, with solution reconstruction
- **Solution enumeration and counting**
- **k-SAT to 3-SAT reduction** with auxiliary variables
- **Problem encodings**: graph coloring, Sudoku, N-Queens
- **Benchmark instances** and a solver comparison script
- **Interactive visualizer**: watch the algorithms run step by step in a browser

## Installation

```bash
# From the repository root
pip install -e .

# With development dependencies (pytest, black, flake8, mypy)
pip install -e ".[dev]"
```

Requires Python 3.7+. No third-party runtime dependencies.

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

### 2SAT (Polynomial Time)

```python
from bsat import solve_2sat, CNFExpression

# Every clause has exactly 2 literals: solved via strongly connected components
cnf = CNFExpression.parse("(x | y) & (~x | z) & (y | ~z)")
result = solve_2sat(cnf)
```

### Horn-SAT (Logic Programming)

```python
from bsat import solve_horn_sat, is_horn_formula, CNFExpression

# Horn clauses have at most one positive literal
cnf = CNFExpression.parse("x & (~x | y) & (~y | z)")

if is_horn_formula(cnf):
    result = solve_horn_sat(cnf)
    print(f"Solution: {result}")
```

### XOR-SAT (Cryptography & Coding Theory)

```python
from bsat import solve_xorsat, CNFExpression, Clause, Literal

# Each clause is read as an XOR constraint: an odd number of its literals must be true
# x ⊕ y = 1 (they must differ)
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)])
])

result = solve_xorsat(cnf)  # Gaussian elimination over GF(2)
print(f"Solution: {result}")
```

### CDCL (Modern SAT Solving)

```python
from bsat import solve_cdcl, get_cdcl_stats, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y | ~z) & (x | ~y | z)")

result = solve_cdcl(cnf)
if result:
    print(f"Solution: {result}")

# Get detailed statistics
result, stats = get_cdcl_stats(cnf)
print(f"Decisions: {stats.decisions}")
print(f"Conflicts: {stats.conflicts}")
print(f"Learned clauses: {stats.learned_clauses}")
```

### Modern CDCL (What Makes It Fast)

```python
from bsat import CNFExpression, get_modern_cdcl_stats

cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (~b | c) & (~c | d) & (~d | ~a)")

# Same algorithm as CDCL, plus the engineering used by real solvers
result, stats = get_modern_cdcl_stats(cnf)
print(result)
print(stats)   # decisions, conflicts, clause visits, learned clause sizes, restarts, ...

# Several techniques have a switch, so you can measure what each contributes
result, stats = get_modern_cdcl_stats(cnf, minimize_learned=False, restart_strategy='luby')
```

### WalkSAT (Randomized Local Search)

```python
from bsat import solve_walksat, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y | ~z) & (x | ~y | z)")

result = solve_walksat(cnf, noise=0.5, max_flips=10000, seed=42)
if result:
    print(f"Solution found: {result}")
else:
    print("No solution found (but may still be SAT)")
```

### Schöning's Algorithm (Provably Fast 3SAT)

```python
from bsat import solve_schoening, get_schoening_stats, CNFExpression

cnf = CNFExpression.parse("(a | b | c) & (~a | b | ~c) & (a | ~b | c)")

result = solve_schoening(cnf, seed=42)
if result:
    print(f"Solution: {result}")
else:
    print("No solution found (algorithm is incomplete)")

result, stats = get_schoening_stats(cnf, seed=42)
print(f"Tries: {stats.tries}")
print(f"Total flips: {stats.total_flips}")
```

### P-Bit Solver (Probabilistic Computing)

```python
from bsat import solve_pbit, get_pbit_stats, CNFExpression

# Gibbs sampling with simulated annealing; energy = number of unsatisfied clauses
cnf = CNFExpression.parse("(a | b | c) & (~a | b | ~c) & (a | ~b | c)")

result = solve_pbit(cnf, seed=42)
if result:
    print(f"Solution: {result}")
else:
    print("No solution found (algorithm is incomplete)")

result, stats = get_pbit_stats(cnf, seed=42)
print(f"Sweeps: {stats.sweeps}")
print(f"Best energy: {stats.best_energy}")  # 0 means solved
```

### Preprocessing

```python
from bsat import decompose_and_preprocess, CNFExpression, solve_sat

cnf = CNFExpression.parse("a & (a | b) & (~a | c) & (d | e) & (f | g) & f")

# Simplify, then split into independent components
components, forced, stats = decompose_and_preprocess(cnf)

print(f"Reduced: {stats.original_clauses} → {stats.final_clauses} clauses")
print(f"Forced assignments: {forced}")
print(f"Independent components: {len(components)}")

# Solve whatever is left
solution = forced.copy()
for comp in components:
    sol = solve_sat(comp)
    if sol is None:  # one UNSAT component makes the whole formula UNSAT
        solution = None
        break
    solution.update(sol)
```

### Simplification

```python
from bsat import CNFExpression, simplify_cnf, solve_cdcl

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d | e) & (~d | ~e)")

# Probing, blocked clause elimination, variable elimination, vivification
result = simplify_cnf(cnf)
print(result.stats)

# Solve what is left, then map the solution back to the original formula
model = solve_cdcl(result.cnf)
full_model = result.extend_model(model)
print(cnf.evaluate(full_model))  # True
```

### Solution Enumeration

```python
from bsat import find_all_sat_solutions, count_sat_solutions, CNFExpression

cnf = CNFExpression.parse("(x | y)")

# Enumerate every distinct satisfying assignment
all_solutions = find_all_sat_solutions(cnf)
print(f"Found {len(all_solutions)} solutions:")  # 3
for sol in all_solutions:
    print(f"  {sol}")

print(f"Total solutions: {count_sat_solutions(cnf)}")

# Limit the search for formulas with many solutions
some = find_all_sat_solutions(cnf, max_solutions=100)
```

### k-SAT to 3-SAT Reduction

```python
from bsat import reduce_to_3sat, solve_with_reduction, CNFExpression, Clause, Literal

# One 5-literal clause
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

## Choosing a Solver

| Problem Type | Solver | Complexity | Complete | Use When |
|-------------|---------|-----------|----------|----------|
| **2SAT** | `solve_2sat()` | O(n+m) (linear time) | ✅ | Every clause has exactly 2 literals |
| **Horn-SAT** | `solve_horn_sat()` | O(n+m) (linear time) | ✅ | At most 1 positive literal per clause |
| **XOR-SAT** | `solve_xorsat()` | O(n³) | ✅ | Parity constraints |
| **General SAT** | `solve_sat()` | O(2ⁿ)* | ✅ | Small formulas; reading how DPLL works |
| **General SAT** | `solve_cdcl()` | O(2ⁿ)* | ✅ | Reading how clause learning works |
| **General SAT** | `solve_modern_cdcl()` | O(2ⁿ)* | ✅ | Larger or structured formulas; the fastest pure-Python solver here |
| **3SAT** | `solve_schoening()` | O(1.334ⁿ)† | ❌ | Studying randomized algorithms |
| **General SAT** | `solve_walksat()` | No guarantee | ❌ | Satisfiable instances where a quick answer is enough |
| **General SAT** | `solve_pbit()` | No guarantee | ❌ | Exploring annealing / Boltzmann sampling |

*Exponential in the worst case; CDCL is usually far faster in practice because it learns from conflicts.
†Expected time for 3SAT.

Rules of thumb:

- All clauses have exactly 2 literals → `solve_2sat()`
- Every clause has at most one positive literal → `solve_horn_sat()`
- The problem is a system of parity constraints → `solve_xorsat()`
- Anything else → `solve_modern_cdcl()`; read `solve_sat()` (DPLL) and then `solve_cdcl()` first if you want to see how the algorithm is built up

These are pure-Python teaching implementations. They are comfortable on formulas with tens to a few hundred variables; for anything larger, see [Beyond the Core Package](#beyond-the-core-package).

## Documentation

The [`docs/`](docs/README.md) directory is the heart of the project. A suggested reading order:

### Start Here
- [Introduction to SAT](docs/introduction.md) - What SAT is and why it matters
- [Terminology Reference](docs/terminology.md) - Glossary of SAT concepts
- [History of SAT Solving](docs/history.md) - From Davis-Putnam to modern solvers
- [CNF Data Structures](docs/cnf.md) - The package API

### Theory
- [Schaefer's Dichotomy Theorem](docs/schaefer-dichotomy.md) - Which SAT variants are in P and which are NP-complete
- [Constraint Types](docs/constraint-types.md) - A taxonomy of Boolean constraints
- [3SAT Variants](docs/3sat-variants.md) - NAE-3SAT, 1-in-3-SAT, random and structured instances
- [k-SAT to 3-SAT Reduction](docs/introduction.md#reducing-k-sat-to-3-sat)
- [Theory & References](docs/theory.md) and the [Reading List](docs/reading-list.md)

### Algorithms
- [Solver Overview](docs/solvers.md)
- [Davis-Putnam](docs/davis-putnam-solver.md) · [DPLL](docs/dpll-solver.md) · [CDCL](docs/cdcl-solver.md)
- [Modern CDCL](docs/modern-cdcl.md), one technique at a time: [two-watched literals](docs/two-watched-literals.md) · [conflict analysis and clause minimization](docs/conflict-analysis.md) · [VSIDS](docs/vsids.md) · [phase saving](docs/phase-saving.md) · [LBD and clause deletion](docs/lbd-clause-management.md) · [restarts](docs/restarts.md)
- [2SAT](docs/2sat-solver.md) · [Horn-SAT](docs/hornsat-solver.md) · [XOR-SAT](docs/xorsat-solver.md)
- [WalkSAT](docs/walksat-solver.md) · [Schöning](docs/schoening-solver.md) · [P-bit](docs/pbit-solver.md)
- [Preprocessing](docs/preprocessing.md) · [Simplification](docs/simplification.md)

### Practice
- [Examples & Tutorials](docs/examples.md)
- [Problem Encodings](docs/problem-encodings.md) - Graph coloring, Sudoku, N-Queens, and more
- [DIMACS Format](docs/dimacs.md)
- [Benchmarking](docs/benchmarking.md)

### Going Further
- [Modern Competition Solvers](docs/competition_solvers.md) - How state-of-the-art solvers work
- [What a Competition Solver Requires](docs/competition_requirements.md) - The gap between a teaching solver and a competitive one

## Examples

```bash
# Solvers
python examples/example.py                      # CNF basics
python examples/example_davis_putnam.py
python examples/example_dpll.py
python examples/example_cdcl.py
python examples/example_cdcl_modern.py         # each modern technique, switched on and off
python examples/example_2sat.py
python examples/example_hornsat.py
python examples/example_xorsat.py
python examples/example_walksat.py
python examples/example_schoening.py
python examples/example_pbit.py

# Tools
python examples/example_preprocessing.py
python examples/example_simplification.py
python examples/example_enumerate_solutions.py
python examples/example_reductions.py
python examples/example_dimacs.py

# Problem encodings
python examples/encodings/graph_coloring.py
python examples/encodings/sudoku.py
python examples/encodings/n_queens.py

# Solver comparison
python examples/benchmark_comparison.py
```

## Interactive Visualization

A browser-based visualizer animates the algorithms step by step: DPLL, CDCL, Davis-Putnam, 2SAT, Horn-SAT, WalkSAT, and the k-SAT to 3-SAT reduction.

```bash
pip install -r visualization_server/requirements.txt
cd visualization_server
python -m uvicorn backend.main:app --reload --port 8000
```

Then open `http://localhost:8000`. See [visualization_server/README.md](visualization_server/README.md).

## Testing

```bash
python tests/test_davis_putnam.py
python tests/test_dpll.py
python tests/test_cdcl.py
python tests/test_cdcl_modern.py
python tests/test_2sat.py
python tests/test_hornsat.py
python tests/test_xorsat.py
python tests/test_walksat.py
python tests/test_schoening.py
python tests/test_pbit.py
python tests/test_preprocessing.py
python tests/test_simplification.py
python tests/test_reductions.py
python tests/test_dimacs.py
python tests/test_benchmarks.py
python tests/test_regressions.py   # regression tests for previously fixed bugs
python tests/test_c_solver.py      # optional; skipped unless the C library is built
```

Or with pytest:

```bash
pytest tests/
```

## Beyond the Core Package

Two side projects live in this repository. Each has its own documentation and is independent of the core package.

### Research Solvers (`research/`)

Fifteen experimental solvers that explore ideas beyond the textbook algorithms: conflict-graph pattern mining, community-based decomposition, lookahead, backbone detection, bandit-based branching, and bio-inspired approaches such as auctions, slime-mold networks, and protein folding. They are exploratory Python prototypes for studying an idea, not general-purpose solvers: on current code the package's own `solve_cdcl()` is faster than them on nearly every benchmark instance, and several fall back to CDCL internally.

```bash
python research/examples/example_cgpm.py     # also example_cobd.py, example_la_cdcl.py, example_bb_cdcl.py
python research/tpm_sat/example.py           # each remaining solver has its own example.py
```

See [research/README.md](research/README.md) for the solvers. [research/BENCHMARKS.md](research/BENCHMARKS.md) records the original benchmark runs; note its status section, since the large speedups reported there were measured against an early CDCL baseline that had bugs and do not reproduce today.

### Competition Solver (`competition/`)

`competition/c/` contains a separate, standalone CDCL solver written in C11. It is meant to be competitive and educational at once: it implements what the [competition-solver documents](docs/competition_solvers.md) describe, such as inprocessing, proof logging, and cache-conscious data structures, and its design decisions and experiments are written up alongside the code. The core techniques it shares with `solve_modern_cdcl()` are easiest to learn from the Python version first. It is also where to turn when a formula is too large for the Python solvers.

If its shared library is built (`make -C competition/c shared`), the Python package can call it directly:

```python
from bsat import CNFExpression, solve_c, is_c_solver_available

cnf = CNFExpression.parse("(x | y) & (~x | z) & (~y | ~z)")
if is_c_solver_available():
    print(solve_c(cnf))
```

Everything else about it, including features, usage, benchmarks, and the embedding API, is documented in [competition/c/README.md](competition/c/README.md). [competition/README.md](competition/README.md) gives an overview of the directory.

## Repository Layout

```
src/bsat/              The Python package
docs/                  Theory, algorithm, and API documentation
examples/              Runnable examples and problem encodings
tests/                 Unit tests
visualization_server/  Browser-based algorithm visualizer
research/              Experimental research solvers
competition/           Standalone C competition solver
dataset/               Benchmark instances
```

## License

MIT License - see [LICENSE](LICENSE) for details.
