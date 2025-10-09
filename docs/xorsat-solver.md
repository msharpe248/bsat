# XOR-SAT Solver

A polynomial-time algorithm for solving XOR-SAT problems using Gaussian elimination over GF(2).

## Overview

**XOR-SAT** is a variant of SAT where clauses are XOR (exclusive-or) constraints rather than OR constraints. Unlike general 3SAT (which is NP-complete), XOR-SAT can be solved in **polynomial time**!

**Time Complexity**: O(n³)
- n = number of variables

**Space Complexity**: O(n²)
- For the augmented matrix

## What is XOR-SAT?

An XOR-SAT formula has clauses where literals are combined with XOR (⊕):

```
(x ⊕ y) ∧ (y ⊕ z ⊕ w) ∧ (¬x ⊕ z)
```

Each clause is satisfied when an **odd number** of its literals are true.

### XOR vs OR

**Regular SAT (OR)**:
- `(x ∨ y)` is true when **at least one** of x, y is true
- Satisfied by: {x=T, y=F}, {x=F, y=T}, {x=T, y=T}

**XOR-SAT**:
- `(x ⊕ y)` is true when **exactly one** of x, y is true
- Satisfied by: {x=T, y=F}, {x=F, y=T}
- NOT satisfied by: {x=T, y=T} or {x=F, y=F}

### Why XOR-SAT Matters

XOR clauses appear naturally in:
- **Cryptography**: Linear cryptanalysis, breaking ciphers
- **Error correction**: Parity check codes, Hamming codes
- **Hardware verification**: Equivalence checking
- **Secret sharing**: XOR-based threshold schemes
- **Linear algebra**: Systems over GF(2)

## Why is XOR-SAT Polynomial?

XOR-SAT formulas are systems of **linear equations over GF(2)** (the binary field).

**Key insight**: Each XOR clause is a linear equation modulo 2:
- `x ⊕ y ⊕ z = 1` is the same as `x + y + z = 1 (mod 2)`

We can use **Gaussian elimination** to solve these systems in O(n³) time!

## The Algorithm

### Step 1: Convert to Linear System

Each XOR clause becomes a linear equation over GF(2).

**Example**:
```
x ⊕ y = 1
y ⊕ z = 1
x ⊕ z = ?
```

Represents the system:
```
x + y = 1 (mod 2)
y + z = 1 (mod 2)
```

### Step 2: Handle Negations

Negated literals are handled via the identity: `¬x = 1 ⊕ x`

**Example**: `x ⊕ ¬y ⊕ z = 1`
- Expand: `x ⊕ (1 ⊕ y) ⊕ z = 1`
- Simplify: `x ⊕ y ⊕ z = 0` (the negation moves to the RHS)

### Step 3: Build Augmented Matrix

Convert the system to matrix form `[A|b]`:
- Each row = one XOR clause
- Each column = one variable
- Entry is 1 if variable appears in clause
- Last column = parity bit (constant term)

**Example**:
```
x ⊕ y ⊕ z = 1    →    [1 1 1 | 1]
x ⊕ y     = 0    →    [1 1 0 | 0]
y ⊕ z     = 1    →    [0 1 1 | 1]
```

### Step 4: Gaussian Elimination

Perform row reduction over GF(2):
- All arithmetic is mod 2
- Addition is XOR: 1+1=0, 1+0=1, 0+0=0
- Multiplication is AND: 1×1=1, otherwise 0

**Goal**: Reduce to row echelon form

```
[1 1 1 | 1]        [1 0 0 | 0]
[1 1 0 | 0]   →    [0 1 0 | 0]
[0 1 1 | 1]        [0 0 1 | 1]
```

### Step 5: Check for Contradictions

A contradiction looks like: `[0 0 0 | 1]`
- This means: 0 = 1 (impossible!)
- Formula is UNSAT

### Step 6: Back Substitution

Extract solution from reduced matrix:
- **Pivot variables**: Variables with leading 1 in their row
- **Free variables**: Variables with no pivot row (assign False)

From the example above:
```
[1 0 0 | 0]  →  x = 0
[0 1 0 | 0]  →  y = 0
[0 0 1 | 1]  →  z = 1
```

Solution: `{x: False, y: False, z: True}`

## Usage

### Basic Usage

```python
from bsat import CNFExpression, Clause, Literal, solve_xorsat

# Create XOR formula: x ⊕ y = 1
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)])
])

# Solve it
result = solve_xorsat(cnf)

if result:
    print(f"SAT: {result}")
    # SAT: {'x': True, 'y': False}
else:
    print("UNSAT")
```

### Using the Solver Class

```python
from bsat import XORSATSolver, CNFExpression

cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)]),
    Clause([Literal('y', False), Literal('z', False)])
])

solver = XORSATSolver(cnf)
result = solver.solve()

print(f"Solution: {result}")
print(f"Statistics: {solver.stats}")
```

### Getting Statistics

```python
from bsat import get_xorsat_stats

stats = get_xorsat_stats(cnf)

print(f"Satisfiable: {stats['satisfiable']}")
print(f"Solution: {stats['solution']}")
print(f"Gaussian elimination steps: {stats['stats']['gaussian_elimination_steps']}")
print(f"Matrix rank: {stats['stats']['matrix_rank']}")
```

## Examples

### Example 1: Simple XOR Equation

```python
# x ⊕ y = 1 (x and y must differ)
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)])
])

result = solve_xorsat(cnf)
# Result: {'x': True, 'y': False}
# (or {'x': False, 'y': True} depending on implementation)
```

### Example 2: System of Equations

```python
# x ⊕ y = 1
# y ⊕ z = 1
# Therefore: x ⊕ z = 0 (x and z must be equal)
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)]),
    Clause([Literal('y', False), Literal('z', False)])
])

result = solve_xorsat(cnf)
# Result: {'x': False, 'y': True, 'z': False}
# Verify: x ⊕ y = 0 ⊕ 1 = 1 ✓
#         y ⊕ z = 1 ⊕ 0 = 1 ✓
```

### Example 3: Unsatisfiable System

```python
# Contradictory system:
# x ⊕ y = 1
# y ⊕ z = 1
# x ⊕ z = 1
# But (x⊕y) ⊕ (y⊕z) = x⊕z = 0, not 1!
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)]),
    Clause([Literal('y', False), Literal('z', False)]),
    Clause([Literal('x', False), Literal('z', False)])
])

result = solve_xorsat(cnf)
# Result: None (UNSAT)
```

### Example 4: Parity Check Code

```python
# Parity bit p for data bits d₁, d₂, d₃
# p ⊕ d₁ ⊕ d₂ ⊕ d₃ = 0 (even parity)
# Given: d₁=1, d₂=0, d₃=0, find p

# p ⊕ d₁ ⊕ d₂ ⊕ d₃ = 0 is encoded as ¬p ⊕ ¬d₁ ⊕ ¬d₂ ⊕ ¬d₃ = 1
cnf = CNFExpression([
    Clause([
        Literal('p', True),
        Literal('d1', True),
        Literal('d2', True),
        Literal('d3', True)
    ]),
    Clause([Literal('d1', False)]),  # d₁ = 1
    Clause([Literal('d2', True)]),   # d₂ = 0
    Clause([Literal('d3', True)])    # d₃ = 0
])

result = solve_xorsat(cnf)
# Result: p = True (because 1 ⊕ 1 ⊕ 0 ⊕ 0 = 0)
```

### Example 5: Secret Sharing

```python
# Secret s split into shares: s = s₁ ⊕ s₂ ⊕ s₃
# Given: s₁=1, s₂=1, s₃=0, reconstruct s

cnf = CNFExpression([
    # s ⊕ s₁ ⊕ s₂ ⊕ s₃ = 0
    Clause([
        Literal('s', True),
        Literal('s1', True),
        Literal('s2', True),
        Literal('s3', True)
    ]),
    Clause([Literal('s1', False)]),  # s₁ = 1
    Clause([Literal('s2', False)]),  # s₂ = 1
    Clause([Literal('s3', True)])    # s₃ = 0
])

result = solve_xorsat(cnf)
# Result: s = False (because 1 ⊕ 1 ⊕ 0 = 0)
```

## XOR-SAT vs CNF Encoding

### Converting XOR to CNF is Expensive!

A single XOR clause requires multiple CNF clauses:

**XOR**: `x ⊕ y ⊕ z = 1`

**CNF encoding**:
```
(x ∨ y ∨ z) ∧ (¬x ∨ ¬y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ ¬z)
```

**Cost**: 1 XOR clause → 4 CNF clauses (exponential blowup!)

For n variables:
- 1 XOR clause → 2^(n-1) CNF clauses

**Direct XOR solving is much more efficient!**

## Performance Characteristics

### Complexity Analysis

- **Best case**: O(n²) when matrix is already diagonal
- **Average case**: O(n³) for Gaussian elimination
- **Worst case**: O(n³)

### Practical Performance

| Variables | Typical Time |
|-----------|--------------|
| n < 100   | < 1 ms       |
| n < 1000  | < 100 ms     |
| n < 10000 | < 10 s       |
| n > 10000 | May be slow  |

### Comparison with Other Solvers

| Problem Type | Best Solver | Complexity |
|--------------|-------------|------------|
| XOR-SAT      | XOR-SAT     | O(n³)      |
| 2SAT         | 2SAT        | O(n+m)     |
| Horn-SAT     | Horn-SAT    | O(n+m)     |
| 3SAT         | DPLL/CDCL   | O(2ⁿ)      |

**Use the specialized solver for best performance!**

## Applications in Cryptography

### Linear Cryptanalysis

Breaking block ciphers by approximating non-linear operations with XOR equations:

```python
# Simplified example: recovering key bits
# Plaintext P and ciphertext C related by key K
# P ⊕ K = C
# Given P and C, solve for K

cnf = CNFExpression([
    Clause([Literal('p1', False), Literal('k1', False), Literal('c1', True)]),
    Clause([Literal('p2', False), Literal('k2', False), Literal('c2', True)]),
    # ... more equations
])

key = solve_xorsat(cnf)
```

### Hash Function Analysis

Finding collisions in hash functions based on XOR operations.

## Applications in Coding Theory

### Hamming Codes

Error detection and correction:

```python
# Hamming(7,4) code: 4 data bits, 3 parity bits
# Parity equations:
# p₁ = d₁ ⊕ d₂ ⊕ d₄
# p₂ = d₁ ⊕ d₃ ⊕ d₄
# p₃ = d₂ ⊕ d₃ ⊕ d₄

# Given received bits, check parity to detect errors
```

### LDPC Codes

Low-Density Parity-Check codes use sparse XOR constraints for efficient error correction in modern communications.

## When to Use XOR-SAT

✅ **Use XOR-SAT when**:
- Formula contains XOR clauses
- Working with parity constraints
- Solving linear systems over GF(2)
- Cryptanalysis problems
- Error correction codes

❌ **Don't use XOR-SAT when**:
- Formula has OR clauses (use DPLL, 2SAT, or Horn-SAT)
- Need to encode XOR as CNF (very inefficient!)

## Advanced Topics

### Free Variables

When the system is underconstrained, some variables are "free":

```python
# Only one equation, two variables
# x ⊕ y = 1
# Solution: x can be anything, y = 1 ⊕ x

# Our implementation assigns free variables to False
# Result: {x: False, y: True}
```

### Matrix Rank

The rank tells us the degrees of freedom:
- Rank = n: Unique solution (fully constrained)
- Rank < n: Multiple solutions (n - rank free variables)
- Rank inconsistent: No solution (UNSAT)

### Optimization

For very large systems:
- **Sparse matrices**: Use sparse matrix representations
- **Iterative methods**: Conjugate gradient for huge systems
- **Preprocessing**: Eliminate obvious variables first

## Further Reading

### Academic Papers

1. **[Schaefer (1978)](https://dl.acm.org/doi/10.1145/800133.804350)**: "The complexity of satisfiability problems"
   - Proves XOR-SAT is in P (polynomial time)

2. **[Courtois & Bard (2007)](https://www.iacr.org/archive/crypto2007/46220377/46220377.pdf)**: "Algebraic Cryptanalysis of DES"
   - XOR-SAT for breaking encryption

3. **[Soos et al. (2009)](https://link.springer.com/chapter/10.1007/978-3-642-02777-2_24)**: "Extending SAT Solvers to Cryptographic Problems"
   - Integrating XOR solving in SAT solvers

### Books

- **Handbook of Satisfiability** (2021): Chapter on XOR constraints
- **Introduction to Linear Algebra** (Strang): GF(2) arithmetic
- **Applied Cryptography** (Schneier): Cryptanalysis techniques

### Online Resources

- [Wikipedia: Boolean Satisfiability Problem](https://en.wikipedia.org/wiki/Boolean_satisfiability_problem)
- [XOR-SAT on Wikipedia](https://en.wikipedia.org/wiki/Schaefer%27s_dichotomy_theorem)
- [Gaussian Elimination](https://en.wikipedia.org/wiki/Gaussian_elimination)

## Next Steps

- Try the [examples](examples.md) to see XOR-SAT in action
- Compare with [other solvers](solvers.md)
- Read about [applications](advanced-solvers.md)
- Explore the [theory](theory.md) behind SAT

---

**Remember**: XOR-SAT is polynomial-time but still O(n³). For small problems it's very fast, but for very large systems (n > 10,000), consider optimized linear algebra libraries or sparse matrix techniques.
