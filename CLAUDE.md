# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

BSAT is a Python package for learning and solving Boolean satisfiability (SAT) problems using Conjunctive Normal Form (CNF). It implements multiple SAT solvers ranging from educational (Davis-Putnam 1960) to modern industrial-strength algorithms (CDCL), along with specialized polynomial-time solvers for restricted SAT variants.

## Development Commands

### Package Installation
```bash
# Install in development mode (recommended for development)
pip install -e .

# Install with dev dependencies
pip install -e ".[dev]"
```

### Testing
```bash
# Run individual test modules
python tests/test_2sat.py
python tests/test_dpll.py
python tests/test_cdcl.py
python tests/test_hornsat.py
python tests/test_xorsat.py
python tests/test_walksat.py
python tests/test_schoening.py
python tests/test_preprocessing.py
python tests/test_reductions.py
python tests/test_dimacs.py
python tests/test_benchmarks.py
python tests/test_davis_putnam.py

# Run all tests with pytest (if installed)
pytest tests/

# Run with coverage
pytest --cov=src/bsat tests/
```

### Running Examples
```bash
# Solver examples
python examples/example.py
python examples/example_2sat.py
python examples/example_dpll.py
python examples/example_cdcl.py
python examples/example_hornsat.py
python examples/example_xorsat.py
python examples/example_walksat.py
python examples/example_schoening.py
python examples/example_preprocessing.py
python examples/example_enumerate_solutions.py
python examples/example_reductions.py
python examples/example_dimacs.py
python examples/example_davis_putnam.py

# Real-world problem encodings
python examples/encodings/graph_coloring.py
python examples/encodings/sudoku.py
python examples/encodings/n_queens.py

# Performance benchmarking
python examples/benchmark_comparison.py
```

### Visualization Server
```bash
# Install visualization server dependencies
pip install -r visualization_server/requirements.txt

# Run the visualization server (from bsat root directory)
cd visualization_server
python -m uvicorn backend.main:app --reload --port 8000

# Open browser to http://localhost:8000
```

### Code Quality
```bash
# Format code with black
black src/ tests/ examples/

# Check code style with flake8
flake8 src/ tests/ examples/

# Type checking with mypy
mypy src/
```

## Architecture

### Core Data Structures (`src/bsat/cnf.py`)

The package is built on three fundamental classes:

1. **Literal**: Represents a variable or its negation
   - `variable: str` - The variable name (e.g., 'x', 'y', 'p1')
   - `negated: bool` - Whether this literal is negated
   - Supports Unicode display: `¬x` for negated variables

2. **Clause**: Represents a disjunction (OR) of literals
   - `literals: List[Literal]` - The literals in this clause
   - Empty clause represents ⊥ (always false)
   - Displayed as `(x ∨ y ∨ z)` with Unicode

3. **CNFExpression**: Represents a conjunction (AND) of clauses
   - `clauses: List[Clause]` - The clauses in this CNF
   - Empty CNF represents ⊤ (always true)
   - Displayed as `(x ∨ y) ∧ (¬x ∨ z)` with Unicode

**Key Methods:**
- `CNFExpression.parse(formula: str)` - Parse from text (supports Unicode, ASCII, or English: `¬|~|!`, `∨||`, `∧|&|AND`)
- `evaluate(assignment: Dict[str, bool])` - Evaluate with variable assignments
- `get_variables()` - Extract all variables in expression
- `generate_truth_table()` - Generate complete truth table
- `to_json()/from_json()` - JSON serialization

### Solver Modules

Each solver is implemented as both a class (for advanced usage) and convenience functions:

1. **Davis-Putnam** (`davis_putnam.py`) - *Educational/Historical*
   - Original 1960 resolution-based algorithm
   - Class: `DavisPutnamSolver`
   - Function: `solve_davis_putnam(cnf)` or `get_davis_putnam_stats(cnf)`
   - Note: Exponential clause growth - mainly for historical interest

2. **2SAT** (`twosatsolver.py`) - *Polynomial Time*
   - O(n+m) algorithm using Strongly Connected Components (SCC)
   - Class: `TwoSATSolver`
   - Functions: `solve_2sat(cnf)`, `is_2sat(cnf)`
   - Only works if all clauses have exactly 2 literals
   - Implementation: Builds implication graph, finds SCCs using Kosaraju's algorithm

3. **DPLL** (`dpll.py`) - *General SAT*
   - Davis-Putnam-Logemann-Loveland (1962) backtracking algorithm
   - Class: `DPLLSolver`
   - Functions: `solve_sat(cnf)`, `find_all_sat_solutions(cnf)`, `count_sat_solutions(cnf)`
   - Optimizations: Unit propagation, pure literal elimination
   - Use as default for simple/educational SAT solving

4. **CDCL** (`cdcl.py`) - *Modern Industrial-Strength*
   - Conflict-Driven Clause Learning with VSIDS heuristic
   - Class: `CDCLSolver`
   - Functions: `solve_cdcl(cnf)`, `get_cdcl_stats(cnf)`
   - Returns detailed statistics (decisions, conflicts, learned clauses, restarts)
   - Best for structured/industrial problems with patterns

5. **Horn-SAT** (`hornsat.py`) - *Logic Programming*
   - O(n+m) polynomial-time solver
   - Class: `HornSATSolver`
   - Functions: `solve_horn_sat(cnf)`, `is_horn_formula(cnf)`
   - Only works if each clause has ≤1 positive literal
   - Implementation: Unit propagation on Horn clauses

6. **XOR-SAT** (`xorsat.py`) - *Cryptography/Coding Theory*
   - O(n³) polynomial-time solver using Gaussian elimination over GF(2)
   - Class: `XORSATSolver`
   - Functions: `solve_xorsat(cnf)`, `get_xorsat_stats(cnf)`
   - Only works for XOR constraints (odd parity)

7. **WalkSAT** (`walksat.py`) - *Fast Randomized*
   - Randomized local search (incomplete but often very fast)
   - Class: `WalkSATSolver`
   - Functions: `solve_walksat(cnf, noise=0.5, max_flips=10000, seed=None)`, `get_walksat_stats(cnf)`
   - Parameters: `noise` (randomness), `max_flips` (iteration limit), `seed` (reproducibility)
   - May fail to find solution even if one exists

8. **Schöning's Algorithm** (`schoening.py`) - *Provably Fast 3SAT*
   - Randomized k-SAT solver with O(1.334^n) expected runtime for 3SAT
   - Class: `SchoeningSolver`
   - Functions: `solve_schoening(cnf, seed=None)`, `get_schoening_stats(cnf)`
   - Incomplete but provably better than O(2^n) for 3SAT

### Advanced Features

9. **Preprocessing** (`preprocessing.py`)
   - Simplification techniques: unit propagation, subsumption, decomposition
   - Class: `SATPreprocessor`
   - Functions:
     - `preprocess_cnf(cnf)` - Simplify formula
     - `decompose_into_components(cnf)` - Find independent subproblems
     - `decompose_and_preprocess(cnf)` - Combined decomposition + simplification
   - Returns statistics on reductions achieved

10. **k-SAT to 3-SAT Reduction** (`reductions.py`)
    - Convert any k-SAT formula to equisatisfiable 3-SAT using auxiliary variables
    - Functions:
      - `reduce_to_3sat(cnf)` - Perform reduction
      - `extract_original_solution(solution, aux_map)` - Remove auxiliary variables
      - `solve_with_reduction(cnf)` - Automatically solve via reduction
      - `is_3sat(cnf)`, `get_max_clause_size(cnf)` - Check formula properties
    - Theory: Long clauses split into 3-literal clauses linked by new variables

11. **DIMACS Format** (`dimacs.py`)
    - Industry-standard file format for SAT competitions
    - Functions:
      - `parse_dimacs(text)` / `read_dimacs_file(path)` - Parse DIMACS CNF
      - `to_dimacs(cnf)` / `write_dimacs_file(cnf, path)` - Write DIMACS CNF
      - `parse_dimacs_solution(text)` / `solution_to_dimacs(solution)` - Solution I/O
    - Format: Variables are integers (1-indexed), negation is `-`, lines: `p cnf <vars> <clauses>`, `<lit1> <lit2> ... 0`

### Module Organization

```
src/bsat/
├── __init__.py          # Public API exports
├── cnf.py               # Core data structures (Literal, Clause, CNFExpression)
├── davis_putnam.py      # Historical Davis-Putnam solver (1960)
├── twosatsolver.py      # 2SAT polynomial-time solver
├── dpll.py              # DPLL general SAT solver
├── cdcl.py              # Modern CDCL solver
├── hornsat.py           # Horn-SAT polynomial solver
├── xorsat.py            # XOR-SAT Gaussian elimination solver
├── walksat.py           # WalkSAT randomized local search
├── schoening.py         # Schöning's randomized k-SAT solver
├── preprocessing.py     # SAT preprocessing and decomposition
├── reductions.py        # k-SAT to 3-SAT reduction
└── dimacs.py            # DIMACS format I/O

tests/
├── test_davis_putnam.py
├── test_2sat.py
├── test_dpll.py
├── test_cdcl.py
├── test_hornsat.py
├── test_xorsat.py
├── test_walksat.py
├── test_schoening.py
├── test_preprocessing.py
├── test_reductions.py
├── test_dimacs.py
└── test_benchmarks.py

examples/
├── example*.py                   # One example file per solver/feature
├── encodings/
│   ├── graph_coloring.py        # Graph coloring as SAT
│   ├── sudoku.py                # Sudoku solver via SAT
│   └── n_queens.py              # N-Queens problem as SAT
└── benchmark_comparison.py      # Solver performance comparison

visualization_server/
├── backend/
│   ├── main.py                  # FastAPI application
│   ├── solver_wrappers.py       # Instrumented solvers for visualization
│   ├── models.py                # Pydantic models
│   └── session_manager.py       # Session state management
└── frontend/
    ├── index.html               # Web UI
    └── static/                  # CSS, JS, D3.js visualizers
```

## Testing Guidelines

- Each solver has a dedicated test file: `tests/test_<solver>.py`
- Tests use standard Python `unittest` framework (can also run with `pytest`)
- Test structure: Create CNF → Solve → Verify solution
- Always verify solutions with `cnf.evaluate(solution)` for SAT results
- Include both SAT and UNSAT test cases
- Test edge cases: empty CNF, single clause, unit clauses, large formulas

## Adding New Solvers

1. Create new module `src/bsat/<name>.py`
2. Implement solver class with `solve()` method returning `Dict[str, bool] | None`
3. Add convenience function: `def solve_<name>(cnf: CNFExpression) -> Dict[str, bool] | None`
4. Optional: Add stats function returning tuple `(solution, stats_object)`
5. Export in `src/bsat/__init__.py`
6. Add tests in `tests/test_<name>.py`
7. Add example in `examples/example_<name>.py`
8. Update README.md solver comparison table

## Code Style

- Python 3.7+ (uses type hints but not extensively)
- Line length: 100 characters (black config)
- Naming: snake_case for functions/variables, PascalCase for classes
- Docstrings: Google style with Args/Returns sections
- Unicode symbols in output: ∧ (AND), ∨ (OR), ¬ (NOT), ⊤ (true), ⊥ (false)
- Solver functions return `Dict[str, bool] | None` (dict for SAT, None for UNSAT)

## Common Patterns

### Creating CNF Programmatically
```python
from bsat import Literal, Clause, CNFExpression

cnf = CNFExpression([
    Clause([Literal('x'), Literal('y')]),
    Clause([Literal('x', True), Literal('z')])  # True = negated
])
```

### Parsing from String
```python
from bsat import CNFExpression

cnf = CNFExpression.parse("(x | y) & (~x | z)")
```

### Solving and Verifying
```python
from bsat import solve_sat

result = solve_sat(cnf)
if result:
    assert cnf.evaluate(result), "Solution must satisfy CNF"
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### Getting Statistics
```python
from bsat import get_cdcl_stats

solution, stats = get_cdcl_stats(cnf)
print(f"Decisions: {stats.decisions}, Conflicts: {stats.conflicts}")
```

## Important Notes

- **No External Dependencies**: Core package has zero external dependencies (only stdlib)
- **Dev Dependencies**: pytest, black, flake8, mypy (optional, in `[dev]` extra)
- **Visualization Server**: Separate dependencies (FastAPI, uvicorn, websockets) in `visualization_server/requirements.txt`
- **Performance**: CDCL is fastest for structured problems; specialized solvers (2SAT, Horn-SAT, XOR-SAT) are polynomial when applicable
- **Solver Choice**: Always use specialized solver if formula matches (2SAT → O(n+m), Horn → O(n+m), XOR → O(n³)); otherwise use CDCL for large/structured or DPLL for simple cases
- **Variable Names**: Must be valid Python identifiers matching `[a-zA-Z_][a-zA-Z0-9_]*`
- **DIMACS Integration**: Use for importing benchmark instances or interfacing with other SAT solvers

## Documentation

Extensive documentation in `docs/`:
- `introduction.md` - SAT concepts and theory
- `terminology.md` - Comprehensive glossary
- `cnf.md` - API documentation for core data structures
- `*-solver.md` - Algorithm-specific documentation
- `theory.md`, `reading-list.md` - Academic references
- `problem-encodings.md` - Encoding problems as SAT
- `benchmarking.md` - Performance testing guide
