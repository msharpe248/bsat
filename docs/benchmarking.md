# Benchmarking and Performance Testing

This guide covers how to use BSAT's benchmarking utilities to evaluate and compare SAT solver performance.

## Table of Contents

1. [Overview](#overview)
2. [Benchmark Instances](#benchmark-instances)
3. [Performance Comparison](#performance-comparison)
4. [Running the Test Suite](#running-the-test-suite)
5. [Creating Custom Benchmarks](#creating-custom-benchmarks)
6. [Interpreting Results](#interpreting-results)

---

## Overview

BSAT includes comprehensive benchmarking tools for:

- **Testing solver correctness** on known SAT/UNSAT instances
- **Comparing performance** across different solvers
- **Analyzing algorithmic behavior** (decisions, conflicts, learned clauses)
- **Understanding complexity** through problem scaling

---

## Benchmark Instances

### Standard Benchmark Libraries

For rigorous benchmarking and comparison with other solvers, consider using established benchmark libraries:

- [**SATLIB**](https://www.cs.ubc.ca/~hoos/SATLIB/index-ubc.html) - Comprehensive benchmark library
  - Uniform random 3-SAT instances at various clause/variable ratios
  - Graph coloring problems (flat, morphed)
  - Blocksworld planning problems
  - Quasigroup completion problems
  - Real-world industrial instances
  - Includes known SAT/UNSAT status for validation

SATLIB instances are in DIMACS format and can be loaded directly:
```python
from bsat import read_dimacs_file

# Download from SATLIB and load
cnf = read_dimacs_file("uf50-218.cnf")  # Uniform random 3-SAT, 50 vars, 218 clauses
result = solve_sat(cnf)
```

### Pre-defined Benchmarks

The `tests/benchmarks.py` module provides a collection of benchmark instances:

```python
from tests.benchmarks import get_benchmark, list_benchmarks

# List all available benchmarks
benchmarks = list_benchmarks()
print(benchmarks)

# Get a specific benchmark
cnf = get_benchmark("phase_transition_20")
```

### Available Benchmarks

| Category | Name | Type | Description |
|----------|------|------|-------------|
| **Easy SAT** | `easy_sat_1` | SAT | 10 vars, 20 clauses |
| | `easy_sat_2` | SAT | 15 vars, 30 clauses |
| **Easy UNSAT** | `easy_unsat_1` | UNSAT | Over-constrained random |
| | `easy_unsat_2` | UNSAT | Pigeon-hole 4→3 |
| **Medium** | `medium_sat` | SAT | 30 vars, 100 clauses |
| | `medium_unsat` | UNSAT | 30 vars, 150 clauses |
| **Hard (Phase Transition)** | `phase_transition_20` | ? | 20 vars at ratio 4.26 |
| | `phase_transition_30` | ? | 30 vars at ratio 4.26 |
| **Structured** | `pigeon_hole_5` | UNSAT | 5 pigeons, 4 holes |
| | `pigeon_hole_6` | UNSAT | 6 pigeons, 5 holes |
| **Graph Coloring** | `graph_coloring_sat` | SAT | 8 vertices, 4 colors |
| | `graph_coloring_unsat` | UNSAT | 8 vertices, 2 colors |
| **XOR Chains** | `xor_chain_sat` | SAT | XOR chain = True |
| | `xor_chain_unsat` | UNSAT | XOR chain = False |

### Benchmark Statistics

```python
from tests.benchmarks import benchmark_stats

cnf = get_benchmark("phase_transition_20")
stats = benchmark_stats(cnf)

print(f"Variables: {stats['num_variables']}")
print(f"Clauses: {stats['num_clauses']}")
print(f"Ratio: {stats['ratio']:.2f}")
print(f"Avg clause size: {stats['avg_clause_size']:.1f}")
```

---

## Performance Comparison

### Basic Comparison

Compare solvers on a single benchmark:

```python
from examples.benchmark_comparison import compare_solvers, print_comparison

# Compare DPLL and CDCL
results = compare_solvers("phase_transition_20", solvers=["DPLL", "CDCL"])
print_comparison(results)
```

Output:
```
======================================================================
Benchmark: phase_transition_20
======================================================================
Variables: 20
Clauses: 85
Ratio: 4.26

Results:
----------------------------------------------------------------------

DPLL:
  Result: SAT
  Time: 0.0234s

CDCL:
  Result: SAT
  Time: 0.0087s
  Decisions: 12
  Conflicts: 5
  Propagations: 156
  Learned clauses: 5
```

### Running Multiple Benchmarks

```python
from examples.benchmark_comparison import run_benchmark_suite, print_summary_table

benchmarks = ["easy_sat_1", "medium_sat", "phase_transition_20"]
results = run_benchmark_suite(benchmarks, solvers=["DPLL", "CDCL", "WalkSAT"])

print_summary_table(results)
```

### Detailed CDCL Statistics

Get detailed information about CDCL's decision-making:

```python
from examples.benchmark_comparison import run_cdcl_with_stats

result = run_cdcl_with_stats(cnf, "my_benchmark")

print(f"Time: {result.time_seconds:.4f}s")
print(f"Decisions: {result.decisions}")
print(f"Conflicts: {result.conflicts}")
print(f"Propagations: {result.propagations}")
print(f"Learned clauses: {result.learned_clauses}")
```

---

## Running the Test Suite

### Test Benchmark Generation

```bash
python tests/test_benchmarks.py TestBenchmarkGeneration -v
```

Tests random generation, pigeon-hole, graph coloring, and XOR chains.

### Test Solver Correctness

```bash
python tests/test_benchmarks.py TestDPLLOnBenchmarks -v
python tests/test_benchmarks.py TestCDCLOnBenchmarks -v
python tests/test_benchmarks.py TestWalkSATOnBenchmarks -v
```

### Test Solver Agreement

Verify all complete solvers agree on SAT/UNSAT:

```bash
python tests/test_benchmarks.py TestBenchmarkConsistency -v
```

### Run Full Comparison Examples

```bash
python examples/benchmark_comparison.py
```

This runs 6 comprehensive examples:
1. Single benchmark comparison
2. DPLL vs CDCL on multiple benchmarks
3. Including WalkSAT
4. Scaling analysis (10, 20, 30, 40 variables)
5. Pigeon-hole UNSAT proofs
6. Detailed CDCL statistics

---

## Creating Custom Benchmarks

### Random 3SAT

```python
from tests.benchmarks import random_3sat

# Random 3SAT with 50 variables, 200 clauses
cnf = random_3sat(num_vars=50, num_clauses=200, seed=42)
```

### Phase Transition Instances

The hardest random 3SAT instances occur at clause-to-variable ratio ≈ 4.26:

```python
from tests.benchmarks import phase_transition_3sat

# Generate instance at phase transition
cnf = phase_transition_3sat(num_vars=30, ratio=4.26, seed=42)
```

### Pigeon-Hole Principle

Classic UNSAT instance (n+1 pigeons into n holes):

```python
from tests.benchmarks import pigeon_hole

# 7 pigeons, 6 holes (always UNSAT)
cnf = pigeon_hole(num_pigeons=7)
```

### Graph Coloring

```python
from tests.benchmarks import graph_coloring_hard

# Random graph with 10 vertices, 3 colors, 50% edge density
cnf = graph_coloring_hard(
    num_vertices=10,
    num_colors=3,
    density=0.5,
    seed=42
)
```

### XOR Chains

Easy for XOR-SAT solver, hard for DPLL/CDCL:

```python
from tests.benchmarks import xor_chain

# x1 ⊕ x2 ⊕ ... ⊕ x20 = True
cnf = xor_chain(length=20, value=True)
```

---

## Interpreting Results

### Time Complexity

Different solvers have different theoretical complexities:

| Solver | Worst Case | Typical Performance |
|--------|-----------|-------------------|
| **2SAT** | O(n+m) | Linear - very fast |
| **Horn-SAT** | O(n+m) | Linear - very fast |
| **XOR-SAT** | O(n³) | Cubic - fast for XOR |
| **DPLL** | O(2ⁿ) | Exponential - slow on hard instances |
| **CDCL** | O(2ⁿ) | Much faster than DPLL in practice |
| **WalkSAT** | Varies | Fast but incomplete |

### CDCL Metrics

Understanding CDCL statistics:

- **Decisions**: Number of variable assignments made by the solver
  - *Lower is better* - indicates efficient search

- **Conflicts**: Number of conflicts encountered
  - Shows how many times backtracking occurred
  - More conflicts → learned more clauses

- **Propagations**: Number of unit propagations
  - High propagation/decision ratio is good
  - Indicates effective constraint propagation

- **Learned Clauses**: Clauses added during solving
  - Help prune future search space
  - Too many can slow down solver

### Example Analysis

```
CDCL on pigeon_hole_6:
  Time: 0.0234s
  Decisions: 15
  Conflicts: 8
  Propagations: 203
  Learned clauses: 8
```

**Interpretation**:
- Only 15 decisions needed (efficient search)
- 8 conflicts led to 8 learned clauses
- High propagations (203) show effective BCP
- Fast UNSAT proof despite exponential worst-case

### Scaling Behavior

Test how solvers scale:

```python
from tests.benchmarks import random_3sat
import time

sizes = [10, 20, 30, 40, 50]
for n in sizes:
    cnf = random_3sat(n, int(n * 4.26), seed=42)

    start = time.time()
    solve_cdcl(cnf)
    elapsed = time.time() - start

    print(f"n={n}: {elapsed:.4f}s")
```

**Expected behavior**:
- 2SAT: Linear scaling
- DPLL: Exponential on hard instances
- CDCL: Much better than DPLL, but still exponential worst-case

---

## Performance Tips

### Choosing the Right Solver

1. **Check structure first**:
   ```python
   if is_2sat(cnf):
       use solve_2sat()  # O(n+m)
   elif is_horn_formula(cnf):
       use solve_horn_sat()  # O(n+m)
   ```

2. **For general SAT**:
   - Small (< 50 vars): `solve_sat()` (DPLL) is fine
   - Medium (50-500 vars): `solve_cdcl()` recommended
   - Large (> 500 vars): `solve_cdcl()` or try `solve_walksat()`

3. **For XOR constraints**: Use `solve_xorsat()` (O(n³))

### Benchmarking Best Practices

1. **Use seeds** for reproducible random instances
2. **Run multiple trials** and average results
3. **Include warmup** (first run may be slower)
4. **Test on diverse instances** (SAT, UNSAT, structured, random)
5. **Monitor memory** for large instances
6. **Set timeouts** for potentially hard instances

---

## Example Workflow

Complete benchmarking workflow:

```python
from tests.benchmarks import (
    get_benchmark, random_3sat, phase_transition_3sat
)
from examples.benchmark_comparison import (
    compare_solvers, print_comparison, run_benchmark_suite
)

# 1. Test on known benchmarks
results = compare_solvers("phase_transition_20", ["DPLL", "CDCL"])
print_comparison(results)

# 2. Create custom benchmark
my_cnf = random_3sat(25, 100, seed=42)

# 3. Compare solvers
from examples.benchmark_comparison import run_solver
dpll_result = run_solver(solve_sat, my_cnf, "DPLL", "custom")
cdcl_result = run_solver(solve_cdcl, my_cnf, "CDCL", "custom")

print(f"DPLL: {dpll_result.time_seconds:.4f}s")
print(f"CDCL: {cdcl_result.time_seconds:.4f}s")

# 4. Run full suite
benchmarks = ["easy_sat_1", "medium_sat", "pigeon_hole_5"]
all_results = run_benchmark_suite(benchmarks, ["DPLL", "CDCL"])
```

---

## Further Reading

- [DPLL Solver](dpll-solver.md) - Understanding DPLL performance
- [CDCL Solver](cdcl-solver.md) - CDCL optimizations and statistics
- [WalkSAT Solver](walksat-solver.md) - When to use incomplete search
- [Theory & Complexity](theory.md) - Complexity analysis

---

## Quick Reference

```python
# Import utilities
from tests.benchmarks import get_benchmark, random_3sat, benchmark_stats
from examples.benchmark_comparison import compare_solvers, print_comparison

# Get benchmark
cnf = get_benchmark("phase_transition_20")

# Generate custom
cnf = random_3sat(30, 120, seed=42)

# Get stats
stats = benchmark_stats(cnf)

# Compare solvers
results = compare_solvers("benchmark_name", ["DPLL", "CDCL"])
print_comparison(results)

# Run full suite
from examples.benchmark_comparison import run_benchmark_suite
all_results = run_benchmark_suite(
    benchmarks=["easy_sat_1", "medium_sat"],
    solvers=["DPLL", "CDCL"]
)
```
