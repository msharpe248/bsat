# Introduction to SAT

A beginner-friendly introduction to the Boolean Satisfiability Problem.

## What is a Boolean Formula?

A **Boolean formula** is a logical expression using:
- **Variables**: x, y, z, etc. (can be True or False)
- **Operators**:
  - NOT (¬): Negation, flips True↔False
  - AND (∧): True only if both operands are True
  - OR (∨): True if at least one operand is True

### Examples

```
x ∧ y          → True only if x=True AND y=True
x ∨ y          → True if x=True OR y=True (or both)
¬x             → True if x=False (NOT x)
(x ∨ y) ∧ ¬z   → True if (x OR y) AND (NOT z)
```

## The SAT Problem

**Question**: Given a Boolean formula, can we find an assignment of True/False to each variable that makes the entire formula True?

**Input**: A Boolean formula
**Output**:
- **SAT** (Satisfiable) + an assignment that works
- **UNSAT** (Unsatisfiable) if no assignment works

### Example 1: Satisfiable

**Formula**: `(x ∨ y) ∧ (¬x ∨ z)`

Let's try x=True, y=False, z=True:
- First clause: `(True ∨ False)` = True ✓
- Second clause: `(¬True ∨ True)` = `(False ∨ True)` = True ✓
- Result: **SAT** with assignment {x=True, y=False, z=True}

### Example 2: Unsatisfiable

**Formula**: `x ∧ ¬x`

This says "x is True AND x is False" - impossible!
- If x=True: First part is True, but ¬x is False → False overall
- If x=False: First part is False → False overall
- Result: **UNSAT** (no solution exists)

## Conjunctive Normal Form (CNF)

Most SAT solvers work with formulas in **Conjunctive Normal Form (CNF)**:

### Definition

A CNF formula is:
1. A **conjunction** (AND) of **clauses**
2. Each **clause** is a **disjunction** (OR) of **literals**
3. A **literal** is a variable or its negation

### Structure

```
CNF = Clause₁ ∧ Clause₂ ∧ ... ∧ Clauseₙ

where each Clause = (Literal₁ ∨ Literal₂ ∨ ... ∨ Literalₘ)
and each Literal = variable or ¬variable
```

### Example

```
(x ∨ y ∨ z) ∧ (¬x ∨ y) ∧ (¬y ∨ ¬z)
   Clause 1      Clause 2   Clause 3
```

This CNF has:
- 3 clauses
- 3 variables (x, y, z)
- 7 total literals

### Why CNF?

1. **Standardized format**: All SAT solvers use it
2. **Easy to check**: Just verify each clause has at least one True literal
3. **Any formula can be converted**: Though it may get larger
4. **Efficient algorithms**: Many optimizations work on CNF

## k-SAT Problems

A **k-SAT** problem is a SAT problem where every clause has exactly k literals.

### 2-SAT

Every clause has exactly 2 literals.

**Example**: `(x ∨ y) ∧ (¬x ∨ z) ∧ (y ∨ ¬z)`

**Special property**: 2-SAT can be solved in **polynomial time** O(n+m)!
- n = number of variables
- m = number of clauses

See [2SAT Solver](2sat-solver.md) for details.

### 3-SAT

Every clause has exactly 3 literals.

**Example**: `(x ∨ y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ ¬z)`

**Special property**: 3-SAT is **NP-complete**!
- This was the first NP-complete problem ever proven (Cook-Levin theorem, 1971)
- No known polynomial-time algorithm
- Worst-case requires exponential time (2ⁿ)
- BUT: Modern solvers are very practical for real-world instances

### General SAT

Clauses can have any number of literals (including 1).

**Example**: `x ∧ (y ∨ z) ∧ (x ∨ ¬y ∨ z ∨ w)`

Also NP-complete (since 3-SAT reduces to it).

## Complexity Landscape

| Problem | Complexity | Example Solver |
|---------|-----------|----------------|
| 1-SAT | O(n) | Trivial |
| 2-SAT | O(n+m) | SCC algorithm |
| 3-SAT | NP-complete | DPLL, CDCL |
| k-SAT (k≥3) | NP-complete | DPLL, CDCL |
| General SAT | NP-complete | DPLL, CDCL |

**Key insight**: The jump from 2-SAT to 3-SAT is where the problem becomes hard!

## Reducing k-SAT to 3-SAT

Any k-SAT problem (where k > 3) can be efficiently reduced to 3-SAT by introducing auxiliary variables. This is why **3-SAT is the canonical NP-complete problem**.

### Why This Matters

If we could solve 3-SAT in polynomial time, we could solve ALL k-SAT problems in polynomial time (and therefore P = NP). Conversely, 3-SAT captures the full difficulty of SAT.

### The Reduction Algorithm

To convert a clause with k literals (k > 3) to 3-SAT:

**Original clause**: `(l₁ ∨ l₂ ∨ l₃ ∨ l₄ ∨ ... ∨ lₖ)`

**Introduce k-3 new variables**: `x₁, x₂, ..., xₖ₋₃`

**Replace with k-2 clauses**:
```
(l₁ ∨ l₂ ∨ x₁) ∧
(¬x₁ ∨ l₃ ∨ x₂) ∧
(¬x₂ ∨ l₄ ∨ x₃) ∧
...
(¬xₖ₋₃ ∨ lₖ₋₁ ∨ lₖ)
```

### Example: 4-SAT to 3-SAT

**Original**: `(a ∨ b ∨ c ∨ d)`

**Converted**: `(a ∨ b ∨ x) ∧ (¬x ∨ c ∨ d)`

Where `x` is a new auxiliary variable.

**Why this works**:
- If original is satisfied by `a` or `b`: Set `x = True`, both clauses satisfied ✓
- If original is satisfied by `c` or `d`: Set `x = False`, both clauses satisfied ✓
- If original is unsatisfied: No assignment to `x` can satisfy both clauses ✗

### Example: 5-SAT to 3-SAT

**Original**: `(a ∨ b ∨ c ∨ d ∨ e)`

**Converted**:
```
(a ∨ b ∨ x₁) ∧
(¬x₁ ∨ c ∨ x₂) ∧
(¬x₂ ∨ d ∨ e)
```

**Chain of reasoning**:
- If `a` or `b` is true → set `x₁ = True` → first clause satisfied
- Then `¬x₁` forces `c` or `x₂` to be true
- This cascades down the chain
- Original satisfied iff converted formula satisfied

### Cost of the Reduction

For a single k-literal clause:
- **Variables added**: k - 3
- **Clauses created**: k - 2
- **Time**: O(k) (linear in clause size)

For a formula with m clauses of average size k:
- **Total variables**: Original n + approximately m(k-3)
- **Total clauses**: Approximately m(k-2)
- **Polynomial blowup**: The reduction is efficient!

### Why 3-SAT is Universal

This reduction shows:
1. **3-SAT is as hard as any k-SAT**: Any k-SAT can be solved by reducing to 3-SAT
2. **3-SAT is sufficient**: No need to study 4-SAT, 5-SAT, etc. separately
3. **Minimal NP-complete form**: Can't go lower (2-SAT is polynomial!)

### Using k-SAT to 3-SAT Reduction in BSAT

BSAT provides functions to perform k-SAT to 3-SAT reduction automatically:

```python
from bsat import reduce_to_3sat, solve_with_reduction, CNFExpression, Clause, Literal

# Create a 5-SAT clause
cnf = CNFExpression([
    Clause([
        Literal('a', False),
        Literal('b', False),
        Literal('c', False),
        Literal('d', False),
        Literal('e', False)
    ])
])

# Method 1: Just reduce to 3-SAT
reduced, aux_map, stats = reduce_to_3sat(cnf)
print(f"Original: {cnf}")
print(f"Reduced: {reduced}")
print(f"Auxiliary variables: {list(aux_map.keys())}")
print(f"Statistics: {stats}")

# Method 2: Reduce and solve in one step
solution, stats = solve_with_reduction(cnf)
if solution:
    print(f"Solution (original vars only): {solution}")
    print(f"Verifies original: {cnf.evaluate(solution)}")
```

**Output**:
```
Original: (a ∨ b ∨ c ∨ d ∨ e)
Reduced: (a ∨ b ∨ _aux0) ∧ (¬_aux0 ∨ c ∨ _aux1) ∧ (¬_aux1 ∨ d ∨ e)
Auxiliary variables: ['_aux0', '_aux1']
```

**Key functions**:
- `reduce_to_3sat(cnf)`: Convert any CNF to 3-SAT form
- `solve_with_reduction(cnf)`: Reduce, solve, and extract original solution
- `is_3sat(cnf)`: Check if formula is already in 3-SAT
- `get_max_clause_size(cnf)`: Find largest clause
- `extract_original_solution(solution, aux_map)`: Remove auxiliary variables

See [examples/example_reductions.py](../examples/example_reductions.py) for more usage examples.

## Why 3-SAT Cannot Be Reduced to 2-SAT

Unlike the reduction from k-SAT to 3-SAT, **there is no polynomial-time reduction from 3-SAT to 2-SAT** (unless P = NP).

### The Fundamental Difference

**2-SAT is in P** (polynomial time), but **3-SAT is NP-complete**. If we could reduce 3-SAT to 2-SAT in polynomial time, we could solve 3-SAT in polynomial time, which would prove P = NP!

### Why the Previous Trick Doesn't Work

The auxiliary variable technique that works for k-SAT → 3-SAT fails for 3-SAT → 2-SAT.

**Attempt**: Convert `(a ∨ b ∨ c)` to 2-SAT

**Naive try**: `(a ∨ b) ∧ (¬? ∨ c)` — What goes in the `?`?

We can't create a simple chain because:
- 2-SAT clauses are **implications**: `(a ∨ b)` means `¬a → b` and `¬b → a`
- These implications form a directed graph
- 2-SAT is polynomial because we can find strongly connected components
- 3-SAT breaks this structure!

### What Goes Wrong?

Consider `(x ∨ y ∨ z)` — all three variables must not be simultaneously false.

**In 2-SAT**, we can only express pairwise implications:
- `(x ∨ y)` means: "If ¬x then y" AND "If ¬y then x"
- But this doesn't capture the three-way constraint!

**The problem**: `(x ∨ y) ∧ (x ∨ z) ∧ (y ∨ z)` is **not equivalent** to `(x ∨ y ∨ z)`:

| x | y | z | (x ∨ y ∨ z) | (x ∨ y) ∧ (x ∨ z) ∧ (y ∨ z) |
|---|---|---|-------------|------------------------------|
| F | F | F | **F** | (F ∧ F ∧ F) = **F** | ✓ Same
| F | F | T | **T** | (F ∧ T ∧ T) = **F** | ✗ Different!
| F | T | F | **T** | (T ∧ F ∧ T) = **F** | ✗ Different!
| F | T | T | **T** | (T ∧ T ∧ T) = **T** | ✓ Same
| T | F | F | **T** | (T ∧ T ∧ F) = **F** | ✗ Different!
| T | F | T | **T** | (T ∧ T ∧ T) = **T** | ✓ Same
| T | T | F | **T** | (T ∧ T ∧ T) = **T** | ✓ Same
| T | T | T | **T** | (T ∧ T ∧ T) = **T** | ✓ Same

They are **NOT** the same! The 2-SAT conjunction is more restrictive — it requires at least **two** of the three variables to be true, while the original 3-SAT clause only requires at least **one**.

### The Real Problem: Exponential Blowup

To correctly encode a 3-SAT formula in 2-SAT (if possible), you'd need to enumerate all possible ways variables could satisfy the clause, leading to **exponentially many** 2-SAT clauses.

For example, `(a ∨ b ∨ c)` is satisfied by 7 out of 8 possible assignments. To express "at least one of these 7 assignments" using only 2-SAT constraints would require encoding the entire truth table — exponential in the number of variables.

### Structural Difference

**2-SAT graph structure**:
- Each clause creates edges in an implication graph
- Cycles in this graph represent equivalences
- Solvable by finding SCCs (polynomial time)

**3-SAT breaks this**:
- Three-way constraints can't be decomposed into pairwise implications
- The implication graph structure becomes insufficient
- Requires search/backtracking (exponential time)

### The Complexity Cliff

```
2-SAT: Polynomial (O(n+m))
       ↓
3-SAT: NP-complete (O(2ⁿ) worst case)
       ↓
k-SAT: NP-complete (reducible to 3-SAT)
```

The jump from 2 to 3 literals is where the **easy/hard boundary** lies. This is one of the most beautiful results in computational complexity theory!

### Why This Matters

This is why:
1. **2-SAT has a special polynomial algorithm** (SCC-based)
2. **3-SAT requires exponential-time solvers** (DPLL, CDCL, WalkSAT)
3. **The boundary is sharp**: Adding one literal per clause changes tractable → intractable

## Why Not Just Use a Truth Table?

The obvious approach to SAT is to try all possible assignments and check if any works. This is called the **brute force** or **truth table** approach.

### How Truth Tables Work

For n variables, there are 2ⁿ possible assignments:

**Example with 3 variables (x, y, z)**:

| x | y | z | (x ∨ y) ∧ (¬x ∨ z) | Result |
|---|---|---|-------------------|--------|
| F | F | F | (F ∨ F) ∧ (T ∨ F) = F ∧ T = F | ❌ |
| F | F | T | (F ∨ F) ∧ (T ∨ T) = F ∧ T = F | ❌ |
| F | T | F | (F ∨ T) ∧ (T ∨ F) = T ∧ T = T | ✓ SAT! |
| F | T | T | (F ∨ T) ∧ (T ∨ T) = T ∧ T = T | ✓ SAT! |
| T | F | F | (T ∨ F) ∧ (F ∨ F) = T ∧ F = F | ❌ |
| T | F | T | (T ∨ F) ∧ (F ∨ T) = T ∧ T = T | ✓ SAT! |
| T | T | F | (T ∨ T) ∧ (F ∨ F) = T ∧ F = F | ❌ |
| T | T | T | (T ∨ T) ∧ (F ∨ T) = T ∧ T = T | ✓ SAT! |

With 3 variables: 8 rows to check. Easy!

### The Exponential Explosion

The problem is that truth tables grow **exponentially**:

| Variables | Rows | Time at 1 billion/sec | Storage (1 bit/row) |
|-----------|------|----------------------|---------------------|
| 10 | 1,024 | < 1 microsecond | 128 bytes |
| 20 | 1,048,576 | 1 millisecond | 128 KB |
| 30 | 1,073,741,824 | 1 second | 128 MB |
| 40 | 1,099,511,627,776 | 18 minutes | 128 GB |
| 50 | 1,125,899,906,842,624 | 13 days | 128 TB |
| 60 | 1,152,921,504,606,846,976 | 36 years | 128 PB |
| 100 | 2¹⁰⁰ | **40 billion years** | 128 million TB |

**Real-world SAT problems often have hundreds or thousands of variables!**

### Why This Matters

Hardware verification problems might have:
- **1,000 variables** → 2¹⁰⁰⁰ rows (more atoms than in the universe!)
- **10,000 variables** → Completely infeasible

Even with the fastest computers imaginable, you couldn't enumerate all possibilities before the heat death of the universe.

### The Key Insight

This is why we need **smart algorithms** that:
1. **Prune the search space**: Don't check every possibility
2. **Learn from failures**: Avoid repeating mistakes
3. **Exploit structure**: Use problem-specific properties
4. **Make intelligent choices**: Pick good variable assignments

Modern SAT solvers can solve problems with **millions of variables** that would take 2¹⁰⁰⁰'⁰⁰⁰ rows in a truth table!

### Example: Where Solvers Shine

**Problem**: 1,000 variables, 4,000 clauses

**Brute force (truth table)**:
- Rows to check: 2¹⁰⁰⁰ ≈ 10³⁰¹
- Time: Longer than age of universe × 10²⁸⁰

**Modern SAT solver (CDCL)**:
- Time: Often < 1 second
- How? By exploring only a tiny fraction of the search space

This is why SAT solving research matters!

## NP-Completeness

### What does NP-complete mean?

1. **NP**: Can verify a solution quickly (polynomial time)
   - Given an assignment, we can check if it works in O(n×m) time
   - Checking is easy, but finding is hard!

2. **NP-hard**: At least as hard as every problem in NP
   - If we could solve SAT quickly, we could solve ALL NP problems quickly

3. **NP-complete**: Both NP and NP-hard
   - SAT is the "hardest" problem in a class of thousands of important problems

### P vs NP

**The Million Dollar Question**: Does P = NP?

- **P**: Problems solvable in polynomial time
- **NP**: Problems where solutions are verifiable in polynomial time
- **Unknown**: Is P = NP? (Clay Mathematics Institute Millennium Prize)

If you find a polynomial-time algorithm for 3-SAT, you've proven P = NP and won $1 million!

**Why truth tables don't help**: They're exponential (O(2ⁿ)), not polynomial.

## Practical SAT Solving

Despite being NP-complete, modern SAT solvers are remarkably effective:

### Why Solvers Work Well

1. **Real-world structure**: Most practical instances aren't worst-case
2. **Clever algorithms**: CDCL, clause learning, intelligent backtracking
3. **Efficient implementations**: Cache-friendly data structures, bit operations
4. **Hardware improvements**: Modern CPUs are fast!

### What Makes Instances Hard?

- **Phase transitions**: Random 3-SAT with clause/variable ratio ≈ 4.26 is hardest
- **Cryptographic instances**: Designed to be hard
- **Random instances near satisfiability threshold**: Maximum uncertainty

### What Makes Instances Easy?

- **Special structure**: Horn clauses, 2-SAT sub-problems
- **Few variables**: Brute force works up to ~30-50 variables
- **Sparse clauses**: Many solutions exist
- **Dense clauses**: Quick contradictions

## Applications

SAT solving is used in many real-world applications:

### 1. Hardware Verification
- Verify CPU designs are correct
- Check that circuits implement specifications
- Find bugs in chip designs

### 2. Software Verification
- Prove program correctness
- Find security vulnerabilities
- Test generation

### 3. Planning and Scheduling
- Airline scheduling
- Resource allocation
- Automated planning in AI

### 4. Cryptography
- Breaking encryption schemes
- Finding hash collisions
- Analyzing cryptographic protocols

### 5. Bioinformatics
- Protein folding
- Haplotype assembly
- Gene regulatory networks

### 6. Formal Methods
- Automated theorem proving
- Model checking
- Program synthesis

## Further Reading

### Beginner-Friendly

- [Wikipedia: Boolean satisfiability problem](https://en.wikipedia.org/wiki/Boolean_satisfiability_problem)
- [SAT/SMT by Example](https://sat-smt.codes/) - Practical examples
- [Introduction to SAT Solving](http://www.cs.cornell.edu/gomes/pdf/2008_gomes_knowledge_satisfiability.pdf)

### Textbooks

- **"Handbook of Satisfiability"** (2009, 2021) - Comprehensive reference
  - [Online version](https://www.iospress.com/catalog/books/handbook-of-satisfiability-2)
- **"The Art of Computer Programming, Vol 4B"** by Donald Knuth - Satisfiability

### Classic Papers

- **Cook (1971)**: "The Complexity of Theorem-Proving Procedures" - First NP-complete proof
- **Davis-Putnam (1960)**: "A Computing Procedure for Quantification Theory"
- **Davis-Logemann-Loveland (1962)**: "A Machine Program for Theorem-Proving"

### Modern Surveys

- [Malik & Zhang (2009): "Boolean Satisfiability: From Theoretical Hardness to Practical Success"](https://dl.acm.org/doi/10.1145/1536616.1536637)

## Next Steps

Now that you understand the basics:

1. Learn about [CNF data structures](cnf.md) in this package
2. Try the [2SAT solver](2sat-solver.md) for polynomial-time solving
3. Explore the [DPLL solver](dpll-solver.md) for general SAT
4. Work through [examples and tutorials](examples.md)

Ready to dive in? Let's start coding!
