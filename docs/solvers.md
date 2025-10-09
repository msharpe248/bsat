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
- ✅ Very fast even for large instances
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

# XOR constraints (even parity)
cnf = CNFExpression.parse("(x | y) & (~x | ~y)")  # x ⊕ y = 0
result = solve_xorsat(cnf)
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
**Complexity**: O(n+m) - Polynomial time
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
- ✅ Polynomial time O(n+m)
- ✅ Always fast
- ✅ Used in logic programming (Prolog, Datalog)
- ✅ Common in expert systems and rule-based reasoning

**Cons**:
- ❌ Only works for Horn formulas
- ❌ Limited expressiveness (at most 1 positive literal)

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
   │  └─ Yes → Try solve_walksat() ✅ (very fast but incomplete)
   └─ General case
      ├─ Small instance (< 50 variables)
      │  └─ Use solve_sat() ✅ (DPLL, will finish quickly)
      ├─ Medium instance (50-500 variables)
      │  └─ Use solve_cdcl() ✅ (modern, faster than DPLL)
      └─ Large instance (> 500 variables)
         └─ Use solve_cdcl() ✅ (industrial-strength)
```

### Comparison Table

| Solver | Complexity | Complete? | Best For | Status |
|--------|-----------|-----------|----------|--------|
| **2SAT** | O(n+m) | ✅ Yes | 2-literal clauses | ✅ Implemented |
| **Horn-SAT** | O(n+m) | ✅ Yes | Horn clauses | ✅ Implemented |
| **XOR-SAT** | O(n³) | ✅ Yes | XOR constraints | ✅ Implemented |
| **DPLL** | O(2ⁿ) | ✅ Yes | Small instances | ✅ Implemented |
| **CDCL** | O(2ⁿ)* | ✅ Yes | Large, structured | ✅ Implemented |
| **WalkSAT** | Varies | ❌ No | Quick SAT answers | ✅ Implemented |

*Much faster in practice due to clause learning and intelligent backtracking

## Usage Examples

### Manual Solver Selection

Choose the appropriate solver based on your formula structure:

```python
from bsat import (
    CNFExpression, solve_2sat, solve_horn_sat, solve_xorsat,
    solve_sat, solve_cdcl, solve_walksat,
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

### Quick Solutions with WalkSAT

When you just need a solution fast and don't need UNSAT proofs:

```python
from bsat import solve_walksat

# Try to find solution quickly
result = solve_walksat(cnf, max_flips=10000, noise=0.5)

if result:
    print(f"Found solution: {result}")
else:
    # WalkSAT didn't find one, try complete solver
    result = solve_cdcl(cnf)
```

## Performance Guidelines

### 2SAT
- **Variables**: Up to millions
- **Clauses**: Up to millions
- **Time**: Milliseconds to seconds

### Horn-SAT
- **Variables**: Up to millions
- **Clauses**: Up to millions
- **Time**: Milliseconds to seconds
- **Note**: Linear time O(n+m), very efficient

### DPLL with Optimizations
- **Variables**: Up to ~100-200
- **Clauses**: Up to ~500
- **Time**: Milliseconds to minutes
- **Note**: Unit propagation and pure literal elimination provide significant speedup

### XOR-SAT
- **Variables**: Up to thousands
- **Clauses**: Up to thousands
- **Time**: Milliseconds to seconds
- **Note**: Cubic complexity O(n³), but still polynomial

### CDCL
- **Variables**: Up to thousands
- **Clauses**: Up to millions
- **Time**: Seconds to hours (instance-dependent)
- **Note**: Clause learning makes it much faster than DPLL in practice

### WalkSAT
- **Variables**: Up to millions
- **Clauses**: Up to millions
- **Time**: Seconds (when it finds solutions)
- **Note**: Incomplete - may not find solution even if exists

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
- Understand [WalkSAT solver](walksat-solver.md)
- Compare solvers with [benchmarking](benchmarking.md)
- Try [examples and tutorials](examples.md)
