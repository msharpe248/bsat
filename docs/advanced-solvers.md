# Advanced Solvers

Specialized SAT solvers: Horn-SAT, XOR-SAT, WalkSAT, and CDCL.

## Implemented Solvers

### Horn-SAT ✅

**Status**: ✅ Implemented

### XOR-SAT ✅

**Status**: ✅ Implemented

### WalkSAT ✅

**Status**: ✅ Implemented

### CDCL ✅

**Status**: ✅ Implemented

### Also in the package

These solvers have their own pages and are not covered further here:

- **[Schöning's algorithm](schoening-solver.md)** (`solve_schoening`) - randomized k-SAT with a provable O(1.334ⁿ) bound for 3SAT
- **[P-bit solver](pbit-solver.md)** (`solve_pbit`) - annealed Gibbs sampling modeled on probabilistic-bit hardware
- **[Davis-Putnam](davis-putnam-solver.md)** (`solve_davis_putnam`) - the historical 1960 resolution algorithm
- **Native C solver** (`solve_c`, `CSATSolver` in `bsat.c_solver`) - optional `ctypes` bindings to the C competition solver. Requires building the shared library first (`make -C competition/c shared`); check with `is_c_solver_available()`. See [competition/c/README.md](../competition/c/README.md).

---

## CDCL (Conflict-Driven Clause Learning)

### Overview

**CDCL** is the algorithm behind modern industrial SAT solvers. It extends DPLL with:
1. **Conflict analysis**: Learn why conflicts occur
2. **Clause learning**: Add clauses to prevent repeating mistakes
3. **Non-chronological backtracking**: Jump back further when conflicts detected
4. **Intelligent restarts**: Periodically restart search with learned clauses
5. **VSIDS heuristic**: Conflict-driven variable selection

**Status**: ✅ Implemented

**[📖 Full CDCL Documentation](cdcl-solver.md)**

### How It Works

```
CDCL extends DPLL:

DPLL:
  Pick variable → Assign → Simplify → Recurse
                              ↓
                           Conflict? → Backtrack one level

CDCL:
  Pick variable → Assign → Propagate → Conflict?
                                          ↓
                                      Analyze conflict
                                          ↓
                                      Learn new clause
                                          ↓
                                      Backtrack (non-chronologically)
```

### Key Components

#### 1. Unit Propagation (BCP)
When a clause has only one unassigned literal, that literal must be true:

```
Current: x = True
Clause: (¬x ∨ y) becomes (False ∨ y) = (y)
Therefore: y must be True (forced assignment)
```

#### 2. Conflict Analysis
When conflict occurs, analyze the implication graph to find the reason:

```
Conflict: () (empty clause)
Why? Trace back through forced assignments
Result: Learn clause that prevents this conflict in future
```

#### 3. Clause Learning
Add new clauses during search:

```
Initial formula: (x ∨ y) ∧ (¬x ∨ z) ∧ (¬y ∨ ¬z)
During search, learn: (x ∨ ¬z)
New formula: ... ∧ (x ∨ ¬z)
```

#### 4. VSIDS Heuristic
**Variable State Independent Decaying Sum**: Choose variables involved in recent conflicts

- Variables get scores based on conflict involvement
- Scores decay over time
- Pick highest-scoring unassigned variable

### Expected Performance

- **Small instances**: Similar to DPLL
- **Medium instances**: 10-100x faster than basic DPLL
- **Large instances**: Can solve problems DPLL can't touch
- **Structured problems**: Especially effective (e.g., hardware verification)

### Further Reading

- [Handbook of Satisfiability, Chapter 4](https://www.iospress.com/catalog/books/handbook-of-satisfiability-2)
- [GRASP paper (1996)](https://doi.org/10.1109/ICCAD.1996.569607) - First CDCL
- [Chaff paper (2001)](https://dl.acm.org/doi/10.1145/378239.379017) - VSIDS + watched literals
- [MiniSat paper (2003)](http://minisat.se/Papers.html) - Clean implementation

---

## WalkSAT ✅

### Overview

**WalkSAT** is a randomized local search algorithm that's incomplete but often very fast for SAT instances.

**Status**: ✅ Implemented

**[📖 Full WalkSAT Documentation](walksat-solver.md)**

### Quick Start

```python
from bsat import solve_walksat, CNFExpression

formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

result = solve_walksat(cnf, noise=0.5, max_flips=10000, seed=42)
if result:
    print(f"Solution found: {result}")
else:
    print("No solution found (but may still be SAT)")
```

### Key Features

- **Incomplete but fast**: Often finds solutions much quicker than DPLL/CDCL
- **Randomized**: Different runs give different results
- **Configurable**: Tune noise parameter and restart strategy
- **Good for large SAT instances**: Scales to problems DPLL can't handle

### Characteristics

**Incomplete**: May not find solution even if one exists
**Probabilistic**: Different runs may give different results
**Fast**: Often finds solutions in seconds for large formulas

### When to Use

✅ **Good for**:
- Large satisfiable instances (thousands of variables)
- When you just need any solution quickly
- Real-time applications with time constraints
- Generating multiple different solutions

❌ **Not good for**:
- Proving unsatisfiability (use DPLL or CDCL instead)
- When you need guaranteed solutions
- Small instances (DPLL is fast enough)

### Learn More

See the **[complete WalkSAT documentation](walksat-solver.md)** for:
- Detailed algorithm explanation
- Parameter tuning guide
- Performance characteristics
- Comparison with complete solvers
- Practical examples

---

## Horn-SAT ✅

### Overview

**Horn clauses** have at most one positive literal:
```
(¬x ∨ ¬y ∨ z)  ✓ Horn clause (one positive: z)
(¬x ∨ ¬y)      ✓ Horn clause (zero positive)
(x ∨ ¬y ∨ ¬z)  ✓ Horn clause (one positive: x)
(x ∨ y ∨ ¬z)   ✗ Not Horn (two positive)
```

**Status**: ✅ Implemented

### Algorithm

Horn-SAT can be solved in **linear time** O(n+m) using unit propagation (the implementation here is the counter-based algorithm of Dowling & Gallier - see [Horn-SAT Solver](hornsat-solver.md)):

```
1. Set all variables to False initially
2. Repeat:
   - Find unit clause (single unassigned literal that could satisfy)
   - If positive literal (x): set x = True
   - Propagate implications
3. If empty clause: UNSAT
   If all clauses satisfied: SAT
```

### Usage

```python
from bsat import solve_horn_sat, is_horn_formula, CNFExpression

# Create Horn formula
cnf = CNFExpression.parse("x & (~x | y) & (~y | z)")

# Check if it's Horn
if is_horn_formula(cnf):
    result = solve_horn_sat(cnf)
    if result:
        print(f"SAT: {result}")
    else:
        print("UNSAT")
else:
    print("Not a Horn formula")
```

### Why It's Fast

Unit propagation on Horn clauses always makes progress:
- Each iteration either satisfies a clause or forces a variable
- No backtracking needed!
- Linear time complexity O(n+m): a per-clause counter means each literal is visited only once

### Applications

✅ **Logic programming**: Prolog, Datalog
✅ **Expert systems**: Rule-based reasoning
✅ **Type inference**: Programming language compilers
✅ **Database queries**: Conjunctive queries

### Example

```python
# Logic program: likes_pizza(john) → likes_italian(john) → happy(john)
cnf = CNFExpression([
    Clause([Literal('likes_pizza_john')]),
    Clause([Literal('likes_pizza_john', True), Literal('likes_italian_john')]),
    Clause([Literal('likes_italian_john', True), Literal('happy_john')])
])

result = solve_horn_sat(cnf)
# {'likes_pizza_john': True, 'likes_italian_john': True, 'happy_john': True}
```

### Performance

- **Time**: O(n+m) - Linear in formula size
- **Space**: O(n+m) - Variable assignments, clause counters and occurrence lists
- **Practical**: Linear, no search - around 40ms for 10,000 variables / 50,000 clauses in pure Python

### Further Reading

- [Horn clauses on Wikipedia](https://en.wikipedia.org/wiki/Horn_clause)
- [Dowling & Gallier (1984): "Linear-time algorithms for testing the satisfiability of propositional Horn formulae"](https://www.sciencedirect.com/science/article/pii/0743106684900140)

---

## XOR-SAT ✅

### Overview

**XOR-SAT** deals with XOR (exclusive-or) constraints where clauses are satisfied when an odd number of literals are true.

**Status**: ✅ Implemented

**[📖 Full XOR-SAT Documentation](xorsat-solver.md)**

### Quick Start

```python
from bsat import solve_xorsat, CNFExpression, Clause, Literal

# Create XOR formula: x ⊕ y = 1 (they must differ)
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)])
])

result = solve_xorsat(cnf)
if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### Key Features

- **Polynomial time**: O(n³) using Gaussian elimination over GF(2)
- **Complete solver**: Always finds solution if one exists
- **Efficient**: Much better than encoding XOR as CNF

### Applications

✅ **Cryptography**: Linear cryptanalysis, breaking ciphers
✅ **Coding theory**: Parity checks, error correction
✅ **Secret sharing**: XOR-based schemes
✅ **Hardware verification**: Equivalence checking

### Learn More

See the **[complete XOR-SAT documentation](xorsat-solver.md)** for:
- Detailed algorithm explanation
- Comprehensive examples
- Performance analysis
- Cryptography and coding theory applications

---

## Comparison Table

| Solver | Complexity | Complete | Use Case | Status |
|--------|-----------|----------|----------|--------|
| **Horn-SAT** | O(n+m) | ✅ Yes | Logic programming | ✅ Done |
| **XOR-SAT** | O(n³) | ✅ Yes | Cryptography, coding theory | ✅ Done |
| **WalkSAT** | Varies | ❌ No | Large SAT instances (fast) | ✅ Done |
| **CDCL** | O(2ⁿ)* | ✅ Yes | Large structured SAT, industrial | ✅ Done |

*Much faster in practice due to clause learning

---

## Implementation Checklist

### Horn-SAT ✅
- [x] Horn formula detection
- [x] Linear-time unit propagation (per-clause counters)
- [x] All-false initialization
- [x] Statistics tracking

### XOR-SAT ✅
- [x] Gaussian elimination over GF(2)
- [x] Augmented matrix construction
- [x] Contradiction detection
- [x] Back substitution
- [x] Statistics tracking

### WalkSAT ✅
- [x] Basic WalkSAT algorithm
- [x] Configurable noise parameter
- [x] Multi-restart support
- [x] Break count calculation
- [x] Statistics tracking

### CDCL ✅
- [x] Unit propagation (BCP)
- [x] Conflict analysis with 1UIP
- [x] Clause learning
- [x] VSIDS heuristic
- [x] Non-chronological backtracking
- [x] Luby restart strategy
- [x] Clause deletion

---

## Next Steps

- Try the [current solvers](solvers.md)
- Read about [DPLL](dpll-solver.md) and [2SAT](2sat-solver.md)
- Learn from [examples](examples.md)
- Explore [theory and references](theory.md)
