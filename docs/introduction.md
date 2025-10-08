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

## NP-Completeness

### What does NP-complete mean?

1. **NP**: Can verify a solution quickly (polynomial time)
   - Given an assignment, we can check if it works in O(n×m) time

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

- [Malik & Zhang (2009): "Boolean Satisfiability: From Theoretical Hardness to Practical Success"](https://www.cs.princeton.edu/~sharad/p394-malik.pdf)

## Next Steps

Now that you understand the basics:

1. Learn about [CNF data structures](cnf.md) in this package
2. Try the [2SAT solver](2sat-solver.md) for polynomial-time solving
3. Explore the [DPLL solver](dpll-solver.md) for general SAT
4. Work through [examples and tutorials](examples.md)

Ready to dive in? Let's start coding!
