# BSAT - Boolean Satisfiability Package

A Python package for learning and solving Boolean satisfiability (SAT) problems using Conjunctive Normal Form (CNF).

## Features

### Production Solvers
✅ **CNF Data Structures**: Clean, Pythonic representation (Literal, Clause, CNFExpression)
✅ **Davis-Putnam Solver**: The original 1960 resolution-based algorithm (educational)
✅ **2SAT Solver**: O(n+m) polynomial-time algorithm using strongly connected components
✅ **DPLL Solver**: Backtracking with unit propagation and pure literal elimination
✅ **CDCL Solver**: Conflict-Driven Clause Learning with VSIDS heuristic and restarts
✅ **Horn-SAT Solver**: O(n+m) polynomial-time solver for Horn formulas
✅ **XOR-SAT Solver**: O(n³) polynomial-time solver using Gaussian elimination over GF(2)
✅ **WalkSAT Solver**: Randomized local search (incomplete but often very fast)
✅ **Schöning's Algorithm**: Randomized k-SAT solver with O(1.334^n) expected runtime for 3SAT

### Research Solvers 🔬
✅ **15 Advanced Research Solvers** - Novel algorithms achieving **100-2710× speedups**
- **Original Suite** (4): CGPM-SAT, CoBD-SAT, LA-CDCL, BB-CDCL
- **New Research Suite** (8): TPM-SAT, SSTA-SAT, VPL-SAT, CQP-SAT, MAB-SAT, CCG-SAT, HAS-SAT, CEGP-SAT
- **Bio-Inspired Suite** (3): MARKET-SAT, PHYSARUM-SAT, FOLD-SAT
📚 **[See research/README.md for complete documentation](research/README.md)**

### Advanced Features
✅ **SAT Preprocessing**: Simplification techniques (decomposition, unit propagation, subsumption)
✅ **Solution Enumeration**: Find all satisfying assignments, not just one
✅ **k-SAT to 3-SAT Reduction**: Convert any CNF to 3-SAT form using auxiliary variables
✅ **Pretty Printing**: Unicode symbols (∧, ∨, ¬) for readable output
✅ **Multiple Input Formats**: Parse from text, JSON, or build programmatically
✅ **DIMACS Format**: Full support for reading/writing industry-standard DIMACS CNF files
✅ **Truth Tables**: Generate and compare truth tables

### Validation & Benchmarking
✅ **Comprehensive Benchmarks**: 15 problems, 7 solvers, rigorous performance testing
✅ **Validation Framework**: 4-level validation (correctness, statistical, profiling, timing)
✅ **Performance Comparison**: Tools for comparing solver performance and analyzing results
✅ **Statistical Validation**: 95% confidence intervals, reproducibility testing
✅ **Interactive Visualization**: Web-based 3-SAT reduction visualizer

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

### Solution Enumeration

```python
from bsat import find_all_sat_solutions, count_sat_solutions, CNFExpression

# Find ALL satisfying assignments (not just one)
formula = "(x | y)"
cnf = CNFExpression.parse(formula)

# Find all solutions
all_solutions = find_all_sat_solutions(cnf)
print(f"Found {len(all_solutions)} solutions:")
for sol in all_solutions:
    print(f"  {sol}")

# Or just count them
count = count_sat_solutions(cnf)
print(f"Total solutions: {count}")

# Limit search for formulas with many solutions
solutions = find_all_sat_solutions(cnf, max_solutions=100)
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

## Research Solvers 🔬

The `research/` directory contains **15 advanced SAT solvers** exploring novel algorithmic approaches beyond traditional techniques. These implementations demonstrate cutting-edge research in SAT solving with significant performance improvements on specific problem classes.

### Three Research Suites

**Original Research Suite** (4 solvers) - **100-2710× speedups**
- **CGPM-SAT**: Conflict graph pattern mining using PageRank and betweenness centrality
- **CoBD-SAT**: Community-based decomposition with Louvain algorithm
- **LA-CDCL**: Lookahead-enhanced CDCL with adaptive frequency
- **BB-CDCL**: Backbone detection using statistical sampling

**New Research Suite** (8 solvers) - Novel and educational algorithms
- **TPM-SAT**: Temporal pattern mining from conflict history (⭐⭐ Novel)
- **SSTA-SAT**: Solution space topology analysis and clustering (⭐⭐ Novel)
- **VPL-SAT**: Variable phase learning from conflicts (⭐ Partially Novel)
- **CQP-SAT**: Glucose LBD clause quality prediction (📚 Educational)
- **MAB-SAT**: Multi-armed bandit variable selection (📚 Educational)
- **CCG-SAT**: Conflict causality graph for restarts (⭐ Partially Novel)
- **HAS-SAT**: Hierarchical abstraction-refinement (📚 Educational)
- **CEGP-SAT**: Clause evolution with genetic programming (🧪 Experimental)

**Bio-Inspired Suite** (3 solvers) - Groundbreaking paradigms
- **MARKET-SAT**: Economic auction theory and Walrasian equilibrium (🌟🌟 Groundbreaking)
- **PHYSARUM-SAT**: Slime mold network flow optimization (🌟🌟 Groundbreaking)
- **FOLD-SAT**: Protein folding energy minimization (🌟🌟 Groundbreaking)

### Quick Example

```python
# Original research suite
from research.cgpm_sat import CGPMSolver
from research.cobd_sat import CoBDSATSolver

# New research suite
from research.tpm_sat import TPMSATSolver
from research.ssta_sat import SSTASATSolver

# Bio-inspired suite
from research.market_sat import MARKETSATSolver
from research.physarum_sat import PHYSARUMSATSolver
from research.fold_sat import FOLDSATSolver

cnf = CNFExpression.parse("(a | b | c) & (~a | b | ~c) & ...")

# Use any research solver
solver = CGPMSolver(cnf, graph_weight=0.5)
result = solver.solve()
stats = solver.get_statistics()
```

### Documentation & Benchmarks

📚 **[Complete Research Documentation](research/README.md)** - Detailed algorithm descriptions, usage examples, and theoretical foundations

📊 **[Benchmark Results](research/BENCHMARKS.md)** - Comprehensive performance analysis across all 15 solvers

🔬 **[Algorithm Showcase](research/ALGORITHM_SHOWCASE.md)** - In-depth algorithm descriptions and design motivation

✅ **[Validation Guide](research/benchmarks/VALIDATION_GUIDE.md)** - How to reproduce and validate performance claims

## Validation Framework

All performance claims are validated using a **4-level validation framework**:

### 1. Correctness Verification ✅

Verifies that all SAT solutions actually satisfy the formula:

```bash
cd research/benchmarks
python validate_correctness.py
```

- Every SAT solution verified against all clauses
- UNSAT results cross-checked with independent solvers
- Catches bugs and incorrect implementations

### 2. Statistical Validation ✅

Proves speedups are reproducible with statistical rigor:

```bash
python statistical_benchmark.py --runs 10 --output results.csv
```

- **10 runs per solver** (configurable)
- **Mean, median, standard deviation**
- **95% confidence intervals**
- **Coefficient of variation** (stability metric)

### 3. Profiling Analysis ✅

Validates algorithmic improvements (not measurement artifacts):

```bash
python profile_solvers.py --output profile_report.md
```

- Function-level time breakdown
- Memory usage tracking
- Hotspot identification
- Proves genuine algorithmic differences

### 4. One-Command Validation ✅

Run all validations with a single script:

```bash
cd research/benchmarks
./reproduce_validation.sh        # Full validation
./reproduce_validation.sh --quick  # Quick mode (5 runs)
```

Generates comprehensive validation report in `validation_results/`.

**See:** `research/benchmarks/VALIDATION_GUIDE.md` for complete documentation.

## Interactive Visualization 🎨

Web-based visualizer for understanding k-SAT to 3-SAT reduction:

```bash
cd visualization_server
python -m uvicorn backend.main:app --reload --port 8000
```

Then open: `http://localhost:8000`

**Features:**
- Interactive formula editor
- Step-by-step reduction visualization
- Variable mapping and clause transformation
- Real-time validation

## Solver Comparison

Choose the right solver for your problem:

### Production Solvers

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

### Research Solvers

📚 **See [research/README.md](research/README.md) for complete documentation of all 15 research solvers**

**Original Research Suite** - Proven speedups on large problems (25+ vars):

| Solver | Best Speedup | Complete | Best For |
|--------|--------------|----------|----------|
| **CGPM-SAT** 🏆 | **2710×** | ✅ Yes | Large random SAT, structured problems (graph-based heuristics) |
| **CoBD-SAT** | **1612×** | ✅ Yes | Modular/decomposable problems (high modularity Q > 0.3) |
| **LA-CDCL** | **529×** | ✅ Yes | Hard random SAT near phase transition (lookahead prevents bad decisions) |
| **BB-CDCL** | 93% backbone | ✅ Yes | Problems with backbone variables (>30% forced assignments) |

**New Research Suite** - 8 novel/educational algorithms exploring pattern mining, topology analysis, phase learning, and more

**Bio-Inspired Suite** - 3 groundbreaking solvers using economic auction theory, slime mold networks, and protein folding

**Quick decision guide:**

**For small problems (< 15 vars):**
- All clauses have 2 literals? → Use `solve_2sat()`
- Clauses are implications (≤1 positive literal)? → Use `solve_horn_sat()`
- XOR/parity constraints? → Use `solve_xorsat()`
- Simple/small problem? → Use `solve_sat()` (DPLL) or `solve_cdcl()`

**For large problems (25+ vars):**
- Random 3SAT or structured SAT? → Use `CGPMSolver` 🏆 (2710× speedup!)
- Modular/decomposable structure? → Use `CoBDSATSolver` (1612× speedup)
- Hard instances near phase transition? → Use `LACDCLSolver` (529× speedup)
- Known backbone variables? → Use `BBCDCLSolver` (93% detection)
- Exploring novel algorithms? → See [research/README.md](research/README.md) for 11 more solvers
- Not sure? → Try `CGPMSolver` first (best overall performance)

**For educational/theoretical:**
- Random 3SAT analysis? → Try `solve_schoening()` (provably O(1.334^n))
- Want fast incomplete solver? → Try `solve_walksat()`
- Explore novel paradigms? → Try bio-inspired solvers (MARKET-SAT, PHYSARUM-SAT, FOLD-SAT)

## Examples

Run the example scripts:

```bash
# Solver examples
python examples/example.py                  # General CNF examples
python examples/example_davis_putnam.py     # Davis-Putnam (1960) historical solver
python examples/example_2sat.py             # 2SAT solver examples
python examples/example_dpll.py             # DPLL solver examples
python examples/example_cdcl.py             # CDCL solver examples
python examples/example_hornsat.py    # Horn-SAT solver examples
python examples/example_xorsat.py     # XOR-SAT solver examples
python examples/example_walksat.py    # WalkSAT solver examples
python examples/example_schoening.py       # Schöning's algorithm examples
python examples/example_preprocessing.py   # SAT preprocessing examples
python examples/example_enumerate_solutions.py # Solution enumeration examples
python examples/example_reductions.py      # k-SAT to 3-SAT reduction examples
python examples/example_dimacs.py       # DIMACS format examples

# Real-world problem encodings
python examples/encodings/graph_coloring.py  # Graph coloring problems
python examples/encodings/sudoku.py          # Sudoku solver
python examples/encodings/n_queens.py        # N-Queens problem

# Performance benchmarking
python examples/benchmark_comparison.py      # Compare solver performance
```

## Research & Advanced Usage

The `research/` directory contains **15 advanced SAT solvers** organized into three suites, plus comprehensive benchmarking and validation tools:

### Explore Research Solvers

```bash
# View all 15 research solvers
cat research/README.md

# Interactive demo and algorithm showcase
python research/ALGORITHM_SHOWCASE.py

# Run examples for each solver
python research/cgpm_sat/example.py        # Original research suite
python research/tpm_sat/example.py         # New research suite
python research/market_sat/example.py      # Bio-inspired suite
```

### Run Benchmarks

```bash
cd research/benchmarks

# Full benchmark suite (all 15 research solvers)
python run_full_benchmark.py
cat benchmark_results_full.md

# Focused benchmarks
python run_simple_benchmark.py
python run_focused_benchmark.py
```

### Validate Performance Claims

```bash
cd research/benchmarks

# One-command validation (all 4 validation levels)
./reproduce_validation.sh                   # Full validation
./reproduce_validation.sh --quick           # Quick mode (5 runs)

# Individual validation levels
python validate_correctness.py              # Level 1: Correctness
python statistical_benchmark.py --runs 10   # Level 2: Statistical
python profile_solvers.py                   # Level 3: Profiling
```

**Key Research Documentation:**
- 📚 **[research/README.md](research/README.md)** - Complete documentation of all 15 research solvers
- 🔬 **[research/ALGORITHM_SHOWCASE.md](research/ALGORITHM_SHOWCASE.md)** - Algorithm descriptions and design motivation
- 📊 **[research/BENCHMARKS.md](research/BENCHMARKS.md)** - Comprehensive benchmark results and rankings
- ✅ **[research/benchmarks/VALIDATION_GUIDE.md](research/benchmarks/VALIDATION_GUIDE.md)** - How to reproduce and validate performance claims

## Testing

Run the test suite:

```bash
python tests/test_davis_putnam.py  # Davis-Putnam tests
python tests/test_2sat.py          # 2SAT tests
python tests/test_dpll.py          # DPLL tests
python tests/test_cdcl.py          # CDCL tests
python tests/test_hornsat.py       # Horn-SAT tests
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

### Core Documentation
- [Introduction to SAT](docs/introduction.md) - Start here if you're new!
- [Terminology Reference](docs/terminology.md) - Comprehensive glossary of SAT concepts
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

### Research Documentation (15 Advanced Solvers)
- [Research Overview](research/README.md) - **Complete documentation of all 15 research solvers** 🔬
- [Algorithm Showcase](research/ALGORITHM_SHOWCASE.md) - Detailed algorithm descriptions and design motivation
- [Benchmark Results](research/BENCHMARKS.md) - **100-2710× speedup results** across all solvers 🏆
- [Validation Guide](research/benchmarks/VALIDATION_GUIDE.md) - How to reproduce and validate performance claims

## License

MIT License - see LICENSE file for details.
