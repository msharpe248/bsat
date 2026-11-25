# Solvers Overview

BSAT provides multiple SAT solving algorithms, each with different characteristics and use cases.

## Available Solvers

### 2SAT Solver ✅
**Status**: Implemented
**Algorithm**: Strongly Connected Components (SCC)
**Complexity**: O(n+m) - Polynomial time
**Use Case**: 2-literal clauses only

[Read more →](2sat-solver.md)

```python
from bsat import solve_2sat, CNFExpression

cnf = CNFExpression.parse("(x | y) & (~x | z)")
result = solve_2sat(cnf)
```

**Pros**:
- ✅ Guaranteed polynomial time
- ✅ Very fast even for large instances (the DFS is recursive, so very large dense instances can hit Python's recursion limit)
- ✅ Always finds solution if one exists

**Cons**:
- ❌ Only works for 2SAT (exactly 2 literals per clause)
- ❌ Can't handle general 3SAT

---

### DPLL Solver ✅
**Status**: Implemented with optimizations
**Algorithm**: Davis-Putnam-Logemann-Loveland with backtracking
**Optimizations**: Unit propagation, pure literal elimination
**Complexity**: O(2ⁿ) - Exponential worst case, but much faster in practice
**Use Case**: General SAT, 3SAT, any CNF formula

[Read more →](dpll-solver.md)

```python
from bsat import solve_sat, DPLLSolver, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
result = solve_sat(cnf)

# Or with control over optimizations
solver = DPLLSolver(cnf, use_unit_propagation=True, use_pure_literal=True)
result = solver.solve()
```

**Pros**:
- ✅ Complete and sound
- ✅ Works for any CNF formula
- ✅ Unit propagation reduces search space
- ✅ Pure literal elimination
- ✅ Statistics tracking
- ✅ Guaranteed to find solution if exists

**Cons**:
- ❌ Exponential worst case
- ❌ Can be slow on very hard instances
- ❌ Slower than CDCL on structured problems

---

### CDCL Solver ✅
**Status**: Implemented
**Algorithm**: Conflict-Driven Clause Learning with VSIDS
**Complexity**: O(2ⁿ) - Exponential worst case, much faster in practice
**Use Case**: Large instances, structured problems, industrial applications

[Read more →](cdcl-solver.md)

```python
from bsat import solve_cdcl, get_cdcl_stats, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
result = solve_cdcl(cnf)

# Get detailed statistics
solution, stats = get_cdcl_stats(cnf)
print(f"Decisions: {stats.decisions}")
print(f"Conflicts: {stats.conflicts}")
print(f"Learned clauses: {stats.learned_clauses}")
```

**Pros**:
- ✅ Modern industrial-strength algorithm
- ✅ Conflict-driven clause learning
- ✅ Non-chronological backtracking (backjumping)
- ✅ VSIDS variable ordering heuristic
- ✅ Restart strategy (Luby sequence)
- ✅ Much faster than DPLL on structured instances
- ✅ Detailed statistics tracking

**Cons**:
- ❌ More complex than DPLL
- ❌ Still exponential worst case
- ❌ May use more memory (learned clauses)

---

### WalkSAT Solver ✅
**Status**: Implemented
**Algorithm**: Randomized local search
**Complexity**: Incomplete (may not find solution even if exists)
**Use Case**: Finding solutions quickly on satisfiable instances

[Read more →](walksat-solver.md)

```python
from bsat import solve_walksat, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
result = solve_walksat(cnf, noise=0.5, max_flips=10000, seed=42)
```

**Pros**:
- ✅ Very fast on satisfiable instances
- ✅ Can handle very large formulas
- ✅ Configurable noise parameter
- ✅ Good for optimization problems

**Cons**:
- ❌ Incomplete (might not find solution)
- ❌ Can't prove unsatisfiability
- ❌ Randomized (non-deterministic)

---

### XOR-SAT Solver ✅
**Status**: Implemented
**Algorithm**: Gaussian elimination over GF(2)
**Complexity**: O(n³) - Polynomial time
**Use Case**: XOR clauses (parity constraints), cryptography

[Read more →](xorsat-solver.md)

```python
from bsat import solve_xorsat, CNFExpression

# Each clause is read as an XOR constraint: an odd number of its literals must be true
cnf = CNFExpression.parse("(x | y) & (~x | ~y)")  # x ⊕ y = 1 and ¬x ⊕ ¬y = 1 (the same constraint)
result = solve_xorsat(cnf)  # {'x': True, 'y': False}
```

**Pros**:
- ✅ Polynomial time O(n³)
- ✅ Always fast
- ✅ Important for cryptographic applications
- ✅ Used in coding theory

**Cons**:
- ❌ Only works for XOR-SAT formulas
- ❌ Special clause structure required

---

### Horn-SAT Solver ✅
**Status**: Implemented
**Algorithm**: Unit propagation with all-false initialization
**Complexity**: Polynomial time - O(n+m) for the optimal algorithm; this implementation's fixpoint loop is O(n·m) worst case
**Use Case**: Horn clauses (at most 1 positive literal per clause)

[Read more →](hornsat-solver.md)

```python
from bsat import solve_horn_sat, is_horn_formula, CNFExpression

# Check if formula is Horn
cnf = CNFExpression.parse("(x | ~y) & (~x | ~z)")
if is_horn_formula(cnf):
    result = solve_horn_sat(cnf)
```

**Pros**:
- ✅ Polynomial time, no search or backtracking
- ✅ Always fast
- ✅ Used in logic programming (Prolog, Datalog)
- ✅ Common in expert systems and rule-based reasoning

**Cons**:
- ❌ Only works for Horn formulas
- ❌ Limited expressiveness (at most 1 positive literal)

---

### Schöning's Algorithm ✅
**Status**: Implemented
**Algorithm**: Randomized random walk for k-SAT
**Complexity**: O(1.334^n) expected for 3SAT - Provably better than O(2^n)!
**Use Case**: Quick SAT checks, educational purposes, theoretical analysis

[Read more →](schoening-solver.md)

```python
from bsat import solve_schoening, get_schoening_stats, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
result = solve_schoening(cnf, seed=42)

# Get detailed statistics
solution, stats = get_schoening_stats(cnf, seed=42)
print(f"Tries: {stats.tries}")
print(f"Total flips: {stats.total_flips}")
```

**Pros**:
- ✅ Provably O(1.334^n) for 3SAT - beats O(2^n)
- ✅ Extremely simple algorithm
- ✅ Low memory usage
- ✅ Great for educational purposes
- ✅ Reproducible with seed parameter (within one Python process)
- ✅ Statistics tracking

**Cons**:
- ❌ Incomplete (might not find solution)
- ❌ Can't prove unsatisfiability
- ❌ Randomized (non-deterministic)
- ❌ Not as fast as CDCL on structured problems

---

### P-Bit Solver ✅
**Status**: Implemented
**Algorithm**: Annealed Gibbs sampling (heat-bath updates) modeled on probabilistic-bit hardware
**Complexity**: Incomplete (may not find solution even if exists)
**Use Case**: Learning about Boltzmann sampling / simulated annealing; any clause length

[Read more →](pbit-solver.md)

```python
from bsat import solve_pbit, get_pbit_stats, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
result = solve_pbit(cnf, sweeps=1000, restarts=10, seed=42)

solution, stats = get_pbit_stats(cnf, seed=42)
print(f"Sweeps: {stats.sweeps}, flips: {stats.flips}")
```

**Cons**:
- ❌ Incomplete, can't prove unsatisfiability

---

### Davis-Putnam Solver ✅
**Status**: Implemented (educational/historical)
**Algorithm**: The original 1960 resolution-based variable elimination
**Complexity**: Exponential time *and space*
**Use Case**: Understanding why DPLL replaced it; tiny formulas only

[Read more →](davis-putnam-solver.md)

```python
from bsat import solve_davis_putnam, get_davis_putnam_stats, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
result = solve_davis_putnam(cnf)
```

---

### Native C Solver (optional)
**Status**: Optional `ctypes` bindings (`solve_c`, `get_c_solver_stats`, `CSATSolver`) to the C competition solver in `competition/c/`
**Requires**: Building the shared library first with `make -C competition/c shared` (or setting `BSAT_LIB_PATH`)

```python
from bsat import solve_c, is_c_solver_available, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
if is_c_solver_available():
    result = solve_c(cnf)   # same Dict[str, bool] | None convention
```

The C solver is a separate project and is not covered by these docs - see [competition/c/README.md](../competition/c/README.md).

---

## Choosing a Solver

### Decision Tree

```
Is your formula 2SAT (all clauses have exactly 2 literals)?
├─ Yes → Use solve_2sat() ✅ (guaranteed fast, O(n+m))
└─ No
   ├─ Is it Horn-SAT (at most 1 positive literal per clause)?
   │  └─ Yes → Use solve_horn_sat() ✅ (guaranteed fast, O(n+m))
   ├─ Is it XOR-SAT (all clauses are XOR)?
   │  └─ Yes → Use solve_xorsat() ✅ (polynomial time, O(n³))
   ├─ Do you just need any solution quickly (don't need UNSAT proof)?
   │  ├─ Random 3SAT → Try solve_schoening() ✅ (provably O(1.334^n))
   │  └─ General → Try solve_walksat() ✅ (very fast but incomplete)
   └─ General case
      ├─ Small instance (< 50 variables)
      │  └─ Use solve_sat() ✅ (DPLL, will finish quickly)
      ├─ Medium instance (50-500 variables)
      │  └─ Use solve_cdcl() ✅ (modern, faster than DPLL)
      └─ Large instance (> 500 variables)
         └─ Use solve_cdcl() ✅ (or the optional native solve_c() if built)
```

### Comparison Table

| Solver | Complexity | Complete? | Best For | Status |
|--------|-----------|-----------|----------|--------|
| **2SAT** | O(n+m) | ✅ Yes | 2-literal clauses | ✅ Implemented |
| **Horn-SAT** | O(n+m) | ✅ Yes | Horn clauses | ✅ Implemented |
| **XOR-SAT** | O(n³) | ✅ Yes | XOR constraints | ✅ Implemented |
| **DPLL** | O(2ⁿ) | ✅ Yes | Small instances | ✅ Implemented |
| **CDCL** | O(2ⁿ)* | ✅ Yes | Large, structured | ✅ Implemented |
| **Schöning** | O(1.334ⁿ)† | ❌ No | Random 3SAT | ✅ Implemented |
| **WalkSAT** | Varies | ❌ No | Quick SAT answers | ✅ Implemented |
| **P-bit** | Varies | ❌ No | Learning annealing / Gibbs sampling | ✅ Implemented |
| **Davis-Putnam** | O(2ⁿ) time and space | ✅ Yes | Historical interest, tiny formulas | ✅ Implemented |

*Much faster in practice due to clause learning and intelligent backtracking
†Expected time for 3SAT - provably better than O(2ⁿ)!

## Usage Examples

### Manual Solver Selection

Choose the appropriate solver based on your formula structure:

```python
from bsat import (
    CNFExpression, solve_2sat, solve_horn_sat, solve_xorsat,
    solve_sat, solve_cdcl, solve_walksat, solve_schoening,
    is_2sat, is_horn_formula
)

cnf = CNFExpression.parse("(x | y) & (~x | z)")

# Check formula structure and pick solver
if is_2sat(cnf):
    result = solve_2sat(cnf)  # O(n+m) - very fast!
elif is_horn_formula(cnf):
    result = solve_horn_sat(cnf)  # O(n+m) - very fast!
elif len(cnf.get_variables()) < 50:
    result = solve_sat(cnf)  # DPLL - good for small instances
else:
    result = solve_cdcl(cnf)  # Modern solver - best for large instances
```

### Quick Solutions with Randomized Algorithms

When you just need a solution fast and don't need UNSAT proofs:

```python
from bsat import solve_schoening, solve_walksat, solve_cdcl

# For random 3SAT: Try Schöning's algorithm first (provably O(1.334^n))
result = solve_schoening(cnf, max_tries=1000, seed=42)

if not result:
    # Try WalkSAT (often faster in practice)
    result = solve_walksat(cnf, max_flips=10000, noise=0.5)

if not result:
    # Fall back to complete solver
    result = solve_cdcl(cnf)
```

## Performance Guidelines

These are rough guidelines for the pure-Python implementations in this package (the algorithms themselves scale much further in optimized solvers).

### 2SAT
- **Variables**: Up to ~100,000 on sparse instances
- **Clauses**: Up to ~100,000
- **Time**: Milliseconds to about a second
- **Note**: Linear time, but the recursive DFS can raise `RecursionError` on large instances with long implication chains

### Horn-SAT
- **Variables**: Up to ~100,000
- **Clauses**: Up to ~500,000
- **Time**: Milliseconds to seconds
- **Note**: Polynomial; the fixpoint loop is O(n·m) in the worst case

### DPLL with Optimizations
- **Variables**: Up to ~100-200
- **Clauses**: Up to ~500
- **Time**: Milliseconds to minutes
- **Note**: Unit propagation and pure literal elimination provide significant speedup

### XOR-SAT
- **Variables**: Up to ~1,000
- **Clauses**: Up to ~1,000
- **Time**: Milliseconds to seconds (around 10+ seconds for a dense 1,000-variable system)
- **Note**: Cubic complexity O(n³), but still polynomial

### CDCL
- **Variables**: Hundreds to a few thousand (instance-dependent)
- **Clauses**: Thousands
- **Time**: Milliseconds to minutes or more (instance-dependent)
- **Note**: Clause learning makes it much faster than DPLL in practice, but propagation scans every clause (no watched literals)

### Schöning's Algorithm
- **Variables**: Up to ~50-100 (3SAT)
- **Clauses**: Up to ~200-400
- **Time**: Milliseconds to seconds
- **Note**: Incomplete, but provably O(1.334^n) for 3SAT

### WalkSAT
- **Variables**: Hundreds to a few thousand
- **Clauses**: Thousands
- **Time**: Milliseconds to seconds (when it finds solutions)
- **Note**: Incomplete - may not find solution even if exists. Each flip rescans all clauses

## Benchmarking

Compare solver performance on your problems:

```bash
# Run benchmark comparison
python examples/benchmark_comparison.py

# Test on specific instances
python -m pytest tests/test_benchmarks.py -v
```

## Next Steps

- Learn about [2SAT algorithm](2sat-solver.md)
- Understand [DPLL solver](dpll-solver.md)
- Explore [CDCL solver](cdcl-solver.md)
- Read about [Horn-SAT solver](hornsat-solver.md)
- Learn [XOR-SAT solver](xorsat-solver.md)
- Discover [Schöning's algorithm](schoening-solver.md)
- Understand [WalkSAT solver](walksat-solver.md)
- Try the [P-bit solver](pbit-solver.md)
- Revisit history with the [Davis-Putnam solver](davis-putnam-solver.md)
- Compare solvers with [benchmarking](benchmarking.md)
- Try [examples and tutorials](examples.md)
