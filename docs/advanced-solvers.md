# Advanced Solvers

Coming soon: CDCL, WalkSAT, Horn-SAT, and XOR-SAT solvers.

## Status: Planned Features 🚧

This page describes solvers that are **planned but not yet implemented** in BSAT.

---

## CDCL (Conflict-Driven Clause Learning)

### Overview

**CDCL** is the algorithm behind modern industrial SAT solvers. It extends DPLL with:
1. **Conflict analysis**: Learn why conflicts occur
2. **Clause learning**: Add clauses to prevent repeating mistakes
3. **Non-chronological backtracking**: Jump back further when conflicts detected
4. **Intelligent restarts**: Periodically restart search with learned clauses

**Status**: 🚧 Planned for v0.3

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

## WalkSAT

### Overview

**WalkSAT** is a randomized local search algorithm:
1. Start with random assignment
2. While unsatisfied clauses exist:
   - Pick unsatisfied clause
   - Flip one variable in that clause
3. Repeat until solved or timeout

**Status**: 🚧 Planned for v0.4

### How It Works

```
1. Random initial assignment: {x=T, y=F, z=T}
2. Evaluate: Some clauses unsatisfied
3. Pick random unsatisfied clause: (¬x ∨ y ∨ ¬z)
4. Choose variable to flip:
   - With probability p: random variable from clause
   - With probability 1-p: variable that minimizes "breaks" (newly unsatisfied clauses)
5. Flip variable and repeat
```

### Characteristics

**Incomplete**: May not find solution even if one exists
**Probabilistic**: Different runs may give different results
**Fast**: Often finds solutions much faster than DPLL/CDCL

### When to Use

✅ **Good for**:
- Satisfiable instances (want any solution quickly)
- Very large formulas
- Optimization problems (MaxSAT)
- Real-time applications

❌ **Not good for**:
- Proving unsatisfiability
- Needing guaranteed solution
- Small instances (DPLL is fine)

### Variants

- **SKC (Selman-Kautz-Cohen)**: Original WalkSAT
- **Novelty**: Avoid flipping recently flipped variables
- **ProbSAT**: Probability distribution over variables

### Expected Performance

- **Random 3SAT (SAT)**: Often finds solution in seconds
- **Hard instances**: May not find solution
- **UNSAT**: Will never terminate (incomplete)

### Further Reading

- [Selman et al. (1994): "Noise Strategies for Improving Local Search"](https://www.cs.cornell.edu/selman/papers/pdf/94.aaai.walksat.pdf)
- [Hoos (2002): "An Adaptive Noise Mechanism for WalkSAT"](https://www.cs.ubc.ca/~hoos/Publ/HoosAAAI02.pdf)

---

## Horn-SAT

### Overview

**Horn clauses** have at most one positive literal:
```
(¬x ∨ ¬y ∨ z)  ✓ Horn clause (one positive: z)
(¬x ∨ ¬y)      ✓ Horn clause (zero positive)
(x ∨ ¬y ∨ ¬z)  ✓ Horn clause (one positive: x)
(x ∨ y ∨ ¬z)   ✗ Not Horn (two positive)
```

**Status**: 🚧 Planned for v0.5

### Algorithm

Horn-SAT can be solved in **linear time** O(n+m) using unit propagation:

```
1. Set all variables to False initially
2. Repeat:
   - Find unit clause (single literal)
   - If positive literal (x): set x = True
   - If negative literal (¬x): already False, clause satisfied
   - Simplify formula
3. If empty clause: UNSAT
   If no clauses left: SAT
```

### Why It's Fast

Unit propagation on Horn clauses always makes progress:
- Each iteration either satisfies a clause or forces a variable
- No backtracking needed!
- Linear time complexity

### Applications

✅ **Logic programming**: Prolog, Datalog
✅ **Expert systems**: Rule-based reasoning
✅ **Type inference**: Programming language compilers
✅ **Database queries**: Conjunctive queries

### Example

```
Formula: x ∧ (¬x ∨ y) ∧ (¬y ∨ z)

Step 1: x is unit clause → x = True
Step 2: (¬True ∨ y) = (y) is unit → y = True
Step 3: (¬True ∨ z) = (z) is unit → z = True
Result: SAT with {x=T, y=T, z=T}
```

### Further Reading

- [Horn clauses on Wikipedia](https://en.wikipedia.org/wiki/Horn_clause)
- [Dowling & Gallier (1984): "Linear-time algorithms for testing the satisfiability of propositional Horn formulae"](https://www.sciencedirect.com/science/article/pii/0743106684900140)

---

## XOR-SAT

### Overview

**XOR-SAT** deals with XOR (exclusive-or) constraints:
```
x ⊕ y ⊕ z = 1    (odd number of variables must be True)
x ⊕ y = 0        (variables must have same value)
```

Can encode as CNF but more efficient to solve directly.

**Status**: 🚧 Planned for v0.5

### Algorithm

Use **Gaussian elimination** over GF(2) (binary field):

```
System of XOR equations:
  x ⊕ y ⊕ z = 1
  x ⊕ y     = 0
  y ⊕ z     = 1

Convert to matrix (mod 2):
  [1 1 1 | 1]
  [1 1 0 | 0]
  [0 1 1 | 1]

Gaussian elimination:
  [1 0 0 | 0]
  [0 1 0 | 0]
  [0 0 1 | 1]

Solution: x=0, y=0, z=1
```

### Complexity

- **Solvable**: O(n³) using Gaussian elimination
- **Polynomial time**: Tractable for large systems
- **Better than CNF encoding**: More efficient

### Applications

✅ **Cryptography**: Breaking encryption schemes
✅ **Coding theory**: Error correction codes
✅ **Boolean circuits**: Circuit optimization
✅ **Hardware verification**: Equivalence checking

### XOR vs CNF

**XOR clause**: x ⊕ y ⊕ z = 1

**CNF encoding** (expensive!):
```
(x ∨ y ∨ z) ∧ (¬x ∨ ¬y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ ¬z)
```

1 XOR clause → 4 CNF clauses with 3 literals each

**Direct XOR solving is much more efficient!**

### Further Reading

- [Courtois & Bard (2007): "Algebraic Cryptanalysis of the Data Encryption Standard"](https://www.iacr.org/archive/crypto2007/46220377/46220377.pdf)
- [Soos et al. (2009): "Extending SAT Solvers to Cryptographic Problems"](https://link.springer.com/chapter/10.1007/978-3-642-02777-2_24)

---

## Comparison Table

| Solver | Complexity | Complete | Use Case | Status |
|--------|-----------|----------|----------|--------|
| **CDCL** | O(2ⁿ)* | ✅ Yes | Large structured SAT | 🚧 Planned |
| **WalkSAT** | Varies | ❌ No | Quick solutions | 🚧 Planned |
| **Horn-SAT** | O(n+m) | ✅ Yes | Logic programming | 🚧 Planned |
| **XOR-SAT** | O(n³) | ✅ Yes | Cryptography | 🚧 Planned |

*Much faster in practice

---

## Implementation Roadmap

### Version 0.3: CDCL
- [ ] Unit propagation (BCP)
- [ ] Conflict analysis
- [ ] Clause learning
- [ ] VSIDS heuristic
- [ ] Watched literals
- [ ] Non-chronological backtracking

### Version 0.4: WalkSAT
- [ ] Basic WalkSAT
- [ ] Novelty variant
- [ ] Configurable noise parameter
- [ ] Multi-restart support

### Version 0.5: Special Cases
- [ ] Horn-SAT solver
- [ ] XOR-SAT solver (Gaussian elimination)
- [ ] Automatic solver selection based on formula type

---

## Want to Help?

These solvers are planned but not yet implemented. Contributions welcome!

1. **Fork the repository**: [github.com/msharpe248/bsat](https://github.com/msharpe248/bsat)
2. **Choose a solver**: Pick one you want to implement
3. **Read the papers**: Understand the algorithm
4. **Implement**: Add to `src/bsat/`
5. **Test**: Add comprehensive tests
6. **Submit PR**: We'll review and merge!

See the existing solvers for code style and structure:
- `src/bsat/twosatsolver.py` - 2SAT implementation
- `src/bsat/dpll.py` - DPLL implementation

---

## Next Steps

- Try the [current solvers](solvers.md)
- Read about [DPLL](dpll-solver.md) and [2SAT](2sat-solver.md)
- Learn from [examples](examples.md)
- Explore [theory and references](theory.md)

Stay tuned for these advanced features! 🚀
