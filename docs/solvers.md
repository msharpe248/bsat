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
**Status**: Basic implementation complete
**Algorithm**: Davis-Putnam-Logemann-Loveland with backtracking
**Complexity**: O(2ⁿ) - Exponential worst case
**Use Case**: General SAT, 3SAT, any CNF formula

[Read more →](dpll-solver.md)

```python
from bsat import solve_sat, CNFExpression

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
result = solve_sat(cnf)
```

**Pros**:
- ✅ Complete and sound
- ✅ Works for any CNF formula
- ✅ Simple and understandable
- ✅ Guaranteed to find solution if exists

**Cons**:
- ❌ Exponential worst case
- ❌ Can be slow on hard instances
- ❌ No optimizations yet (unit propagation, etc.)

**Coming Soon**:
- ⏳ Unit propagation
- ⏳ Pure literal elimination
- ⏳ Better variable ordering heuristics

---

## Coming Soon

### DPLL with Optimizations 🚧
**Status**: Planned
**Additions**: Unit propagation, pure literal elimination
**Expected**: Significantly faster on many instances

Unit propagation and pure literal elimination are classical optimizations that can dramatically reduce the search space.

---

### CDCL Solver 🚧
**Status**: Planned
**Algorithm**: Conflict-Driven Clause Learning
**Complexity**: O(2ⁿ) but much faster in practice

Modern industrial-strength SAT solver algorithm used in tools like MiniSat, Glucose, and CryptoMiniSat.

[Read more →](advanced-solvers.md)

**Planned Features**:
- ⏳ Conflict clause learning
- ⏳ Non-chronological backtracking
- ⏳ VSIDS variable ordering
- ⏳ Clause database management

---

### WalkSAT 🚧
**Status**: Planned
**Algorithm**: Randomized local search
**Complexity**: Incomplete (may not find solution even if exists)
**Use Case**: Finding solutions quickly on satisfiable instances

[Read more →](advanced-solvers.md)

**Pros**:
- ✅ Very fast on satisfiable instances
- ✅ Can handle very large formulas
- ✅ Good for optimization problems

**Cons**:
- ❌ Incomplete (might not find solution)
- ❌ Can't prove unsatisfiability
- ❌ Randomized (non-deterministic)

---

### Horn-SAT Solver 🚧
**Status**: Planned
**Algorithm**: Unit propagation
**Complexity**: O(n+m) - Polynomial time
**Use Case**: Horn clauses (at most one positive literal per clause)

[Read more →](advanced-solvers.md)

Horn-SAT is a special case that can be solved in linear time and is widely used in logic programming and expert systems.

---

### XOR-SAT Solver 🚧
**Status**: Planned
**Algorithm**: Gaussian elimination
**Complexity**: O(n³) - Polynomial time
**Use Case**: XOR clauses (parity constraints)

[Read more →](advanced-solvers.md)

XOR-SAT solves systems of XOR equations and is important for cryptographic applications.

---

## Choosing a Solver

### Decision Tree

```
Is your formula 2SAT (all clauses have exactly 2 literals)?
├─ Yes → Use 2SAT Solver ✅ (guaranteed fast)
└─ No
   ├─ Is it Horn-SAT (at most 1 positive literal per clause)?
   │  └─ Yes → Use Horn-SAT Solver 🚧 (coming soon)
   ├─ Is it XOR-SAT (all clauses are XOR)?
   │  └─ Yes → Use XOR-SAT Solver 🚧 (coming soon)
   ├─ Do you just need any solution (don't care about optimality)?
   │  └─ Yes → Try WalkSAT 🚧 (coming soon) or DPLL ✅
   └─ General case
      ├─ Small instance (< 50 variables)
      │  └─ Use DPLL ✅ (will finish quickly)
      ├─ Medium instance (50-500 variables)
      │  └─ Use DPLL ✅ (might take time) or wait for CDCL 🚧
      └─ Large instance (> 500 variables)
         └─ Wait for CDCL 🚧 or use external solver (MiniSat)
```

### Comparison Table

| Solver | Complexity | Complete? | Best For | Status |
|--------|-----------|-----------|----------|--------|
| **2SAT** | O(n+m) | ✅ Yes | 2-literal clauses | ✅ Done |
| **Horn-SAT** | O(n+m) | ✅ Yes | Horn clauses | 🚧 Planned |
| **XOR-SAT** | O(n³) | ✅ Yes | XOR constraints | 🚧 Planned |
| **DPLL (basic)** | O(2ⁿ) | ✅ Yes | Small general instances | ✅ Done |
| **DPLL + opts** | O(2ⁿ) | ✅ Yes | Medium instances | 🚧 Planned |
| **CDCL** | O(2ⁿ)* | ✅ Yes | Large, structured | 🚧 Planned |
| **WalkSAT** | Varies | ❌ No | Quick SAT answers | 🚧 Planned |

*Much faster in practice due to learning

## Usage Examples

### Automatic Solver Selection

In the future, BSAT will automatically choose the best solver:

```python
# Future API (not yet implemented)
from bsat import auto_solve

cnf = CNFExpression.parse("...")
result = auto_solve(cnf)  # Picks best solver automatically
```

### Manual Solver Selection

Current API requires manual selection:

```python
from bsat import CNFExpression, solve_2sat, solve_sat

cnf = CNFExpression.parse("(x | y) & (~x | z)")

# Check if it's 2SAT
is_2sat = all(len(clause.literals) == 2 for clause in cnf.clauses)

if is_2sat:
    result = solve_2sat(cnf)  # Fast!
else:
    result = solve_sat(cnf)   # DPLL
```

## Performance Guidelines

### 2SAT
- **Variables**: Up to millions
- **Clauses**: Up to millions
- **Time**: Milliseconds to seconds

### DPLL (Basic)
- **Variables**: Up to ~50 (practical limit)
- **Clauses**: Up to ~200 (practical limit)
- **Time**: Milliseconds to minutes

### DPLL with Optimizations (Planned)
- **Variables**: Up to ~100-200
- **Clauses**: Up to ~1000
- **Time**: Seconds to minutes

### CDCL (Planned)
- **Variables**: Up to thousands
- **Clauses**: Up to millions
- **Time**: Seconds to hours (instance-dependent)

## Benchmarking

Run benchmarks to compare solvers:

```bash
# Coming soon
python -m bsat.benchmark --formula my_formula.cnf --all-solvers
```

## Contributing

Want to help implement these solvers? See the [development roadmap](https://github.com/msharpe248/bsat) and contribute!

## Next Steps

- Learn about [2SAT algorithm](2sat-solver.md)
- Understand [DPLL solver](dpll-solver.md)
- Read about [future solvers](advanced-solvers.md)
- Try [examples and tutorials](examples.md)
