# 3SAT Variants: A Comprehensive Guide

## Table of Contents
1. [Introduction: Why 3SAT is Special](#introduction-why-3sat-is-special)
2. [Standard 3SAT](#standard-3sat)
3. [Monotone Variants](#monotone-variants)
4. [Semantic Variants](#semantic-variants)
5. [Random 3SAT](#random-3sat)
6. [Structured 3SAT](#structured-3sat)
7. [Solver Selection Guide](#solver-selection-guide)

---

## Introduction: Why 3SAT is Special

### The P/NP-Complete Boundary

**3SAT occupies a critical position in complexity theory:**

```
1SAT (unit clauses)  →  Trivial, O(n)          ✅ Polynomial
2SAT (binary clauses) →  O(n+m) via SCC        ✅ Polynomial
─────────────────────────────────────────────────────────────
3SAT (ternary clauses) →  NP-complete          ❌ Exponential
4SAT, 5SAT, k-SAT     →  NP-complete          ❌ Exponential
```

**The jump from 2 to 3 literals per clause:**
- Causes exponential complexity blowup
- No known polynomial-time algorithm
- First NP-complete SAT variant

### Historical Significance

**Cook-Levin Theorem (1971):**
- **Stephen Cook** proved SAT is NP-complete
- **Leonid Levin** independently proved same result
- Launched entire field of NP-completeness theory

**Karp's 21 Problems (1972):**
- **Richard Karp** showed 21 problems NP-complete
- Included 3SAT (called "3-Satisfiability")
- Established 3SAT as canonical NP-complete problem

### Why Study 3SAT Variants?

1. **Theoretical:** Understanding complexity boundaries
2. **Practical:** Many real-world problems reduce to 3SAT variants
3. **Algorithmic:** Different variants need different solving strategies
4. **Educational:** Illustrates subtle complexity distinctions

**Key insight:** Small syntactic changes can dramatically affect complexity!

---

## Standard 3SAT

### Definition

**3SAT** (also called 3-CNF-SAT or 3-Satisfiability):
- Boolean formula in Conjunctive Normal Form (CNF)
- Every clause has **exactly 3 literals**
- Decide if there exists a satisfying assignment

### Formal Definition

```
Input: CNF formula φ where each clause has exactly 3 literals
Output: YES if φ is satisfiable, NO otherwise

Example:
φ = (x₁ ∨ x₂ ∨ x₃) ∧ (¬x₁ ∨ x₂ ∨ ¬x₃) ∧ (x₁ ∨ ¬x₂ ∨ x₃)
```

### Properties

**Complexity:** NP-complete
- No known polynomial-time algorithm
- Widely believed that none exists (if P ≠ NP)

**Satisfiability:** Not always SAT or UNSAT
- Some instances SAT, some UNSAT
- Determining which is NP-complete!

**Universal:** Every NP problem reduces to 3SAT
- This makes 3SAT a "universal" NP-complete problem
- Solving 3SAT efficiently would solve ALL NP problems efficiently

### BSAT Implementation

✅ **Multiple solvers available:**

```python
from bsat import solve_sat, solve_cdcl, solve_schoening, CNFExpression

# Example 3SAT instance
formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

# Option 1: DPLL (good for small instances)
result1 = solve_sat(cnf)

# Option 2: CDCL (best for large/structured instances)
result2 = solve_cdcl(cnf)

# Option 3: Schöning's algorithm (randomized, O(1.334^n) for 3SAT)
result3 = solve_schoening(cnf, seed=42)

if result1:
    print(f"SAT: {result1}")
    assert cnf.evaluate(result1)
else:
    print("UNSAT")
```

### Phase Transition

**Random 3SAT exhibits sharp phase transition:**

```
Clause-to-variable ratio: r = m/n

r < 4.2:      Almost surely SAT (easy to find solution)
r ≈ 4.267:    Critical ratio (hardest instances)
r > 4.3:      Almost surely UNSAT (easy to prove impossibility)
```

**Discovery:** Mitchell, Selman & Levesque (1992)

**Implications:**
- Hardest instances at phase transition
- Solvers tuned for specific regions
- Benchmark suites target critical region

**Visual:**
```
Difficulty
    │     ╱╲
    │    ╱  ╲
    │   ╱    ╲
    │  ╱      ╲___
    │ ╱            ╲___
    │╱                  ╲___
    └────┬────┬────┬────────→ r = m/n
       3.0  4.2  4.5  5.0

        SAT  |HARD| UNSAT
        easy |    | easy
```

---

## Monotone Variants

### Overview

**Monotone** means all literals have the same polarity (all positive or all negative).

**Surprising fact:** Even monotone 3SAT variants are NP-complete!

### Positive 3SAT (All Positive Literals)

#### Definition

**Positive 3SAT**: Every literal is positive (unnegated).

#### Examples

✅ **Positive 3SAT formulas:**
```
(x ∨ y ∨ z)
(x ∨ w ∨ v) ∧ (y ∨ z ∨ w)
(a ∨ b ∨ c) ∧ (b ∨ c ∨ d) ∧ (a ∨ c ∨ d)
```

❌ **Not positive 3SAT:**
```
(¬x ∨ y ∨ z)           # Has negated literal
(x ∨ y)                # Only 2 literals (not 3SAT)
(x ∨ y ∨ z ∨ w)        # 4 literals (not 3SAT)
```

#### Properties

**Always SAT:** Set all variables to True
- Every positive clause satisfied by all-true assignment
- So satisfiability decision is trivial!

**But still interesting:**
- **Minimum satisfying assignment:** Fewest variables set to True?
- **Counting solutions:** How many satisfying assignments?
- **Maximum unsatisfying assignment:** Most variables True while staying UNSAT?

**Complexity:** NP-complete for optimization version

**Why NP-complete?**
- Not all clauses can be satisfied by choosing fewer than all variables
- Finding minimal solution is hard
- Despite being 1-valid (Schaefer case), width=3 makes optimization hard

#### Example: Minimum Positive 3SAT

```python
# Find minimum number of True variables
formula = "(x | y | z) & (y | z | w)"

# All-true works:  {x: T, y: T, z: T, w: T}  (4 true)
# But can we do better?
# Try: {x: F, y: T, z: T, w: F}             (2 true)
#   (x | y | z) = (F | T | T) = T  ✅
#   (y | z | w) = (T | T | F) = T  ✅
# Success! Only 2 variables needed.

# Finding this minimum is NP-complete!
```

#### BSAT Support

✅ **Works with general solvers:**
```python
formula = "(x | y | z) & (y | z | w)"
cnf = CNFExpression.parse(formula)

# Find ANY solution (easy)
result = solve_sat(cnf)  # Returns any satisfying assignment

# Find MINIMUM solution (hard - requires optimization)
# Not directly supported, would need custom optimization
```

---

### Negative 3SAT (All Negative Literals)

#### Definition

**Negative 3SAT**: Every literal is negative (negated).

#### Examples

```
(¬x ∨ ¬y ∨ ¬z)
(¬x ∨ ¬w ∨ ¬v) ∧ (¬y ∨ ¬z ∨ ¬w)
(¬a ∨ ¬b ∨ ¬c) ∧ (¬b ∨ ¬c ∨ ¬d)
```

#### Properties

**Always SAT:** Set all variables to False
- Dual to Positive 3SAT
- Every negative clause satisfied by all-false assignment

**Interpretation:** AtMost-2 constraints
- `(¬x ∨ ¬y ∨ ¬z)` means "at most 2 of {x,y,z} can be True"
- Forbids all three being simultaneously True

**Also NP-complete** for optimization (maximum satisfying assignment)

#### BSAT Support

✅ **Works with general solvers** (same as Positive 3SAT)

---

### Mixed Monotone 3SAT

#### Definition

Each clause is **pure** (all positive OR all negative), but formula mixes both types.

#### Examples

```
(x ∨ y ∨ z) ∧ (¬a ∨ ¬b ∨ ¬c) ∧ (p ∨ q ∨ r)
      ↑              ↑              ↑
  Positive      Negative       Positive
```

#### Properties

**Not necessarily SAT:**
- Unlike pure positive or pure negative
- Some instances SAT, some UNSAT

**Complexity:** NP-complete for satisfiability decision

**Why hard?**
- Positive clauses "push" variables toward True
- Negative clauses "push" variables toward False
- Finding consistent assignment is hard

#### Example

```python
# Mixed monotone 3SAT
formula = """
(x | y | z) &      # All positive
(~a | ~b | ~c) &   # All negative
(x | a | z)        # All positive
"""

# Satisfying assignment needs balance:
# - x, y, z should be True (for positive clauses)
# - a, b, c should be False (for negative clauses)
# Solution: {x: T, y: T, z: T, a: F, b: F, c: F}
```

---

## Semantic Variants

These variants impose **additional constraints** beyond standard 3SAT.

### NAE-3SAT (Not-All-Equal 3SAT)

#### Definition

**NAE-3SAT**: In each clause, not all literals can have the same truth value.

**Constraint:** Forbids {all True} AND {all False}

#### Examples

```
NAE(x, y, z): Not all three can be True, not all three can be False

Valid assignments to (x, y, z):
✅ (T, T, F)  # Not all equal
✅ (T, F, T)
✅ (T, F, F)
✅ (F, T, T)
✅ (F, T, F)
✅ (F, F, T)

Invalid:
❌ (T, T, T)  # All equal (all True)
❌ (F, F, F)  # All equal (all False)
```

#### CNF Encoding

```
NAE(x, y, z) encodes as 2 clauses:

(x ∨ y ∨ z)         # At least one True (forbids all-False)
(¬x ∨ ¬y ∨ ¬z)      # At least one False (forbids all-True)
```

**Encoding size:** 2 clauses per NAE constraint

#### Properties

**Complexity:** NP-complete

**Self-complementary property:**
- If assignment α satisfies NAE-3SAT instance
- Then ¬α (complement) also satisfies it
- This follows from symmetry of "not all equal"

**Example of self-complementary:**
```
NAE(x, y, z)

Solution 1: {x: T, y: T, z: F}  ✅
Solution 2: {x: F, y: F, z: T}  ✅  (complement of Solution 1)
```

**More solutions than 3SAT:**
- NAE has 6 valid assignments per clause (out of 8 total)
- Standard 3SAT clause has 7 valid assignments (out of 8)
- But NAE is stricter overall (requires both clauses in encoding)

#### Applications

**Hypergraph 2-Coloring:**
```
Given hypergraph H = (V, E) where each hyperedge |e| = 3
Question: Can we 2-color vertices so each hyperedge has both colors?

Reduction to NAE-3SAT:
- Each vertex → Boolean variable
- Each hyperedge {u, v, w} → NAE(u, v, w)
- 2-coloring exists ↔ NAE-3SAT instance is SAT
```

**Graph Theory:**
```
# Ensuring diversity in selections
# Each triple must have mixed True/False values
```

#### BSAT Usage

❌ **No native NAE support**

**Workaround:** Encode as CNF

```python
def encode_nae_3sat(clauses_vars):
    """
    Encode NAE-3SAT as standard CNF

    Args:
        clauses_vars: List of triples [(x,y,z), (a,b,c), ...]

    Returns:
        CNF formula encoding NAE constraints
    """
    cnf_clauses = []

    for (v1, v2, v3) in clauses_vars:
        # At least one True
        clause1 = Clause([
            Literal(v1, False),
            Literal(v2, False),
            Literal(v3, False)
        ])

        # At least one False
        clause2 = Clause([
            Literal(v1, True),
            Literal(v2, True),
            Literal(v3, True)
        ])

        cnf_clauses.extend([clause1, clause2])

    return CNFExpression(cnf_clauses)


# Usage
nae_clauses = [('x', 'y', 'z'), ('a', 'b', 'c')]
cnf = encode_nae_3sat(nae_clauses)

result = solve_cdcl(cnf)
if result:
    print(f"NAE-3SAT solution: {result}")
    # Verify NAE property
    for (v1, v2, v3) in nae_clauses:
        vals = [result[v1], result[v2], result[v3]]
        assert not (all(vals) or not any(vals)), "NAE violation!"
```

---

### 1-in-3-SAT (Exactly-One 3SAT)

#### Definition

**1-in-3-SAT**: In each clause, exactly one literal must be True.

**Stricter than NAE:** Allows only 1 True (not 0, not 2, not 3).

#### Examples

```
1-in-3(x, y, z): Exactly one of {x, y, z} is True

Valid assignments:
✅ (T, F, F)  # Exactly one True
✅ (F, T, F)
✅ (F, F, T)

Invalid:
❌ (F, F, F)  # Zero true
❌ (T, T, F)  # Two true
❌ (T, F, T)  # Two true
❌ (T, T, T)  # Three true
```

#### CNF Encoding

```
1-in-3(x, y, z) = AtLeast-1(x,y,z) ∧ AtMost-1(x,y,z)

(x ∨ y ∨ z) ∧           # AtLeast-1
(¬x ∨ ¬y) ∧             # AtMost-1 (pairwise forbid)
(¬x ∨ ¬z) ∧
(¬y ∨ ¬z)

Total: 4 clauses per 1-in-3 constraint
```

**Encoding size:** 4 clauses per 1-in-3 constraint (double NAE)

#### Properties

**Complexity:** NP-complete

**Even Positive 1-in-3-SAT is NP-complete!**
- All positive literals: 1-in-3(x, y, z) with no negations
- Still NP-complete (Schaefer 1978)
- Shows that width + monotonicity together don't guarantee tractability

**Historical significance:**
- Part of Karp's 21 NP-complete problems (1972)
- Used in many reductions

**Comparison to NAE:**
```
1-in-3 is stricter:

NAE(x, y, z):     6 valid assignments (not all-same)
1-in-3(x, y, z):  3 valid assignments (exactly-one)

NAE allows (T, T, F)   ✅
1-in-3 forbids this    ❌
```

#### Applications

**Exact Cover:**
```
Given universe U and collection S of subsets
Find subcollection S' ⊆ S where each element covered exactly once

Reduction to 1-in-3-SAT:
- Each subset Sᵢ → Boolean variable xᵢ
- For each element e in U: 1-in-3(xᵢ, xⱼ, xₖ) where e ∈ Sᵢ ∩ Sⱼ ∩ Sₖ
```

**Partitioning:**
```
# Divide items into groups where exactly one property holds per group
```

#### BSAT Usage

❌ **No native 1-in-3 support**

**Workaround:** Encode with AtLeast + AtMost

```python
def encode_1_in_3_sat(clauses_vars):
    """
    Encode 1-in-3-SAT as standard CNF

    Each (x,y,z) becomes:
      (x ∨ y ∨ z) ∧ (¬x ∨ ¬y) ∧ (¬x ∨ ¬z) ∧ (¬y ∨ ¬z)
    """
    cnf_clauses = []

    for (v1, v2, v3) in clauses_vars:
        # AtLeast-1
        clause_atleast = Clause([
            Literal(v1, False),
            Literal(v2, False),
            Literal(v3, False)
        ])
        cnf_clauses.append(clause_atleast)

        # AtMost-1 (all pairs)
        pairs = [(v1,v2), (v1,v3), (v2,v3)]
        for (va, vb) in pairs:
            clause_pair = Clause([
                Literal(va, True),
                Literal(vb, True)
            ])
            cnf_clauses.append(clause_pair)

    return CNFExpression(cnf_clauses)


# Usage
one_in_three_clauses = [('x', 'y', 'z')]
cnf = encode_1_in_3_sat(one_in_three_clauses)

# Result: 4 clauses
# (x ∨ y ∨ z) ∧ (¬x ∨ ¬y) ∧ (¬x ∨ ¬z) ∧ (¬y ∨ ¬z)
```

---

## Random 3SAT

### Uniform Random Generation

#### How to Generate Random 3SAT

**Algorithm:**
```
Input: n variables, m clauses
Output: Random 3SAT instance

For each of m clauses:
  1. Choose 3 distinct variables uniformly at random
  2. For each variable, negate with probability 0.5
  3. Add clause to formula
```

**Python implementation:**
```python
import random

def generate_random_3sat(n_vars, n_clauses, seed=None):
    """
    Generate uniform random 3SAT instance

    Args:
        n_vars: Number of variables
        n_clauses: Number of clauses
        seed: Random seed for reproducibility

    Returns:
        CNF formula
    """
    if seed is not None:
        random.seed(seed)

    clauses = []
    for _ in range(n_clauses):
        # Choose 3 distinct variables
        vars_in_clause = random.sample(range(1, n_vars + 1), 3)

        # Create literals (negate with p=0.5)
        literals = []
        for v in vars_in_clause:
            negated = random.choice([True, False])
            literals.append(Literal(f'x{v}', negated))

        clauses.append(Clause(literals))

    return CNFExpression(clauses)


# Usage
random_3sat = generate_random_3sat(n_vars=20, n_clauses=85, seed=42)
# Clause-to-variable ratio: r = 85/20 = 4.25 (near phase transition!)

result = solve_cdcl(random_3sat)
```

### Phase Transition Phenomenon

#### The Critical Ratio

**Definition:** r = m/n (clauses per variable)

**Empirical observation:**
```
r < 4.0:      >99% SAT, easy to find solution
r ≈ 4.267:    ~50% SAT, hardest instances
r > 4.5:      >99% UNSAT, easy to prove impossible
```

**Sharp transition:**
```
Probability of SAT
    │ 1.0 ┤─────────╲
    │     │          ╲
    │ 0.5 ┤           ●───  Critical point
    │     │             ╲
    │ 0.0 ┤              ───────
    └─────┴──┬──┬──┬──┬──────→ r = m/n
           3  4  4.27 5  6

    UNSAT region increasingly sharp as n → ∞
```

#### Why This Happens

**Intuition:**
- **Too few clauses (r < 4):** Many solutions exist, easy to find
- **Critical ratio (r ≈ 4.267):** Few solutions, hard to find
- **Too many clauses (r > 4.5):** Over-constrained, contradictions easy to derive

**Connection to statistical physics:**
- Similar to phase transitions in physical systems
- "Order parameter" = satisfiability
- Critical exponents studied

**Hardness peak:**
```
Search time (log scale)
    │         ╱╲
    │        ╱  ╲
    │       ╱    ╲
    │      ╱      ╲
    │     ╱        ╲____
    │____╱              ╲____
    └─────┬──┬──┬──┬──────→ r
        3  4  4.27 5  6

    Peak at phase transition
```

#### k-SAT Critical Ratios

**As k increases, critical ratio increases:**

| Width k | Critical Ratio r* | Approximate Formula |
|---------|-------------------|---------------------|
| 2 | 1.0 | Exact |
| 3 | 4.267 | Empirical |
| 4 | 9.931 | Empirical |
| 5 | 21.117 | Empirical |
| k | ~2^k ln(2) | Asymptotic |

**Observation:** Larger k → more literals → easier to satisfy

#### Research Applications

**Algorithm testing:**
- Test solvers on hardest instances (at phase transition)
- Compare performance across easy/hard/UNSAT regions

**Theoretical study:**
- Understanding P vs NP boundary
- Analyzing algorithm behavior

**Benchmarking:**
- Competition benchmarks often near phase transition
- Tests solver robustness

### Planted Solution 3SAT

#### Definition

**Planted solution**: Random formula **guaranteed to be SAT** with known solution.

#### Generation Algorithm

```
Input: n variables, m clauses, solution α
Output: Random 3SAT guaranteed satisfied by α

1. Generate random satisfying assignment α
2. For each of m clauses:
   a. Choose 3 distinct variables uniformly
   b. Negate each to ensure clause satisfied by α
   c. Add clause to formula
```

**Key property:** Solution α is "planted" (hidden) in the formula.

#### Python Implementation

```python
def generate_planted_3sat(n_vars, n_clauses, seed=None):
    """
    Generate random 3SAT with planted solution

    Returns:
        (cnf, solution) where solution satisfies cnf
    """
    if seed is not None:
        random.seed(seed)

    # Step 1: Generate random satisfying assignment
    solution = {f'x{i}': random.choice([True, False])
                for i in range(1, n_vars + 1)}

    clauses = []
    for _ in range(n_clauses):
        # Choose 3 distinct variables
        vars_in_clause = random.sample(range(1, n_vars + 1), 3)

        # Create clause satisfied by solution
        literals = []
        for v in vars_in_clause:
            var_name = f'x{v}'
            # Negate if needed to satisfy
            var_val = solution[var_name]
            # Want at least one literal true in clause
            # Randomly assign polarities but ensure clause is satisfied

        # Simpler: Just ensure at least one literal is satisfied
        # Pick one variable to be satisfied
        satisfied_var = random.choice(vars_in_clause)
        for v in vars_in_clause:
            var_name = f'x{v}'
            if v == satisfied_var:
                # This literal must be true under solution
                negated = not solution[var_name]
            else:
                # Random polarity
                negated = random.choice([True, False])
            literals.append(Literal(var_name, negated))

        clauses.append(Clause(literals))

    return CNFExpression(clauses), solution
```

#### Applications

**Testing solvers:**
- Verify solver finds correct solution
- Compare against ground truth

**Machine learning:**
- Training data with labels (solution known)
- Supervised learning for SAT solvers

**Difficulty control:**
- Can tune hardness by controlling clause structure
- Useful for gradual difficulty benchmarks

---

## Structured 3SAT

### Community Structure

#### Definition

**Community structure**: Variables grouped into communities with more intra-community constraints than inter-community.

#### Properties

**Modularity Q measure:**
```
Q = (fraction of edges within communities) - (expected fraction in random graph)

Q = 0:    No community structure
Q > 0.3:  Strong community structure
Q ≈ 1:    Perfect modularity
```

**Example:**
```
Community 1: {x₁, x₂, x₃}
Community 2: {y₁, y₂, y₃}

Clauses:
  Within C1: (x₁ ∨ x₂ ∨ x₃), (x₁ ∨ ¬x₂ ∨ x₃), ...  [many]
  Within C2: (y₁ ∨ y₂ ∨ y₃), (y₁ ∨ ¬y₂ ∨ y₃), ...  [many]
  Between:   (x₁ ∨ y₁ ∨ x₂), (x₂ ∨ y₂ ∨ x₃)       [few]

High modularity → strong community structure
```

#### Why It Matters

**Solver performance:**
- CDCL exploits structure through learned clauses
- Modern solvers much faster on structured instances
- Random instances have Q ≈ 0 (no structure)

**BSAT research solvers:**
```python
from research.cobd_sat import CoBDSATSolver

# CoBD-SAT exploits community structure
solver = CoBDSATSolver(cnf, min_communities=2)
result = solver.solve()
stats = solver.get_statistics()

print(f"Modularity Q: {stats['modularity']}")
print(f"Communities found: {stats['num_communities']}")
print(f"Used decomposition: {stats['used_decomposition']}")

# Performance: 1612× speedup on high-modularity instances!
```

#### Generation

```python
def generate_community_3sat(communities, intra_clauses, inter_clauses):
    """
    Generate 3SAT with community structure

    Args:
        communities: List of variable lists [[x1,x2], [y1,y2], ...]
        intra_clauses: Clauses per community
        inter_clauses: Clauses between communities
    """
    clauses = []

    # Intra-community clauses
    for community in communities:
        for _ in range(intra_clauses):
            # Sample 3 variables from this community
            vars_in_clause = random.sample(community, min(3, len(community)))
            # ... create clause ...

    # Inter-community clauses
    for _ in range(inter_clauses):
        # Sample variables from different communities
        comm1, comm2 = random.sample(communities, 2)
        v1 = random.choice(comm1)
        v2 = random.choice(comm2)
        v3 = random.choice(random.choice(communities))
        # ... create clause ...

    return CNFExpression(clauses)
```

### Modular 3SAT

#### Definition

**Modular**: Formula decomposes into independent or weakly-connected modules.

#### Structure

```
CNF = Module₁ ∧ Module₂ ∧ ... ∧ Moduleₖ ∧ BridgeClauses

Where:
- Each module is relatively independent
- Bridge clauses connect modules
- Few bridge clauses → high modularity
```

#### Solving Strategy

**Divide and conquer:**
```
1. Decompose formula into modules
2. Solve each module independently
3. Combine solutions
4. Resolve bridge clause constraints
```

**BSAT preprocessing:**
```python
from bsat import decompose_into_components, solve_sat

components, forced, stats = decompose_into_components(cnf)

print(f"Found {len(components)} independent components")
print(f"Forced assignments: {forced}")

# Solve each component separately
solution = forced.copy()
for comp in components:
    sol = solve_sat(comp)
    if sol:
        solution.update(sol)
    else:
        return None  # UNSAT

return solution
```

**Performance advantage:**
- Exponential speedup if modules truly independent
- Solve n/k variables k times: O((2^(n/k))^k) = O(2^n)
- But constants much better!

---

## Solver Selection Guide

### Decision Tree

```
I have a 3SAT instance. Which solver?
│
├─ Is it small (< 15 variables)?
│  └─ YES → Use solve_sat() (DPLL is sufficient)
│
├─ Is it random 3SAT?
│  ├─ YES, at phase transition (r ≈ 4.27)
│  │  └─ Use solve_schoening() (provably O(1.334^n))
│  └─ YES, away from phase transition
│     └─ Use solve_cdcl() (efficient on easy/hard regions)
│
├─ Does it have structure (community, modular)?
│  └─ YES → Use solve_cdcl() (learns structure)
│     └─ Consider research.cobd_sat for high modularity
│
├─ Is it NAE-3SAT or 1-in-3-SAT?
│  └─ Encode as CNF, then use solve_cdcl()
│
└─ Default
   └─ Use solve_cdcl() (best general-purpose solver)
```

### Performance Guidelines

| Instance Type | Variables | Best Solver | Expected Time |
|--------------|-----------|-------------|---------------|
| Small | < 15 | DPLL | < 1 second |
| Random (easy) | 20-100 | CDCL | < 10 seconds |
| Random (hard) | 20-100 | Schöning's | Minutes |
| Structured | 100-1000 | CDCL | Seconds to minutes |
| Community | 100-1000 | CoBD-SAT | Faster than CDCL |

### BSAT Examples

```python
from bsat import solve_sat, solve_cdcl, solve_schoening, CNFExpression

# Example 1: Small instance
small_3sat = "(x | y | z) & (~x | y | ~z)"
cnf = CNFExpression.parse(small_3sat)
result = solve_sat(cnf)  # DPLL is fine

# Example 2: Random hard instance
import random
random_3sat = generate_random_3sat(n_vars=30, n_clauses=128, seed=42)
result = solve_schoening(random_3sat, seed=42)  # Randomized algorithm

# Example 3: Structured instance (from file)
from bsat import read_dimacs_file
structured_cnf = read_dimacs_file("industrial_benchmark.cnf")
result = solve_cdcl(structured_cnf)  # CDCL excels on structure

# Example 4: Community structure
from research.cobd_sat import CoBDSATSolver
solver = CoBDSATSolver(structured_cnf)
result = solver.solve()
```

---

## Further Reading

### Foundational Papers

1. **Cook, S. A. (1971).** "The complexity of theorem-proving procedures." *STOC.*
2. **Karp, R. M. (1972).** "Reducibility among combinatorial problems."
3. **Mitchell, D., Selman, B., & Levesque, H. (1992).** "Hard and easy distributions of SAT problems." *AAAI.*
4. **Monasson, R., et al. (1999).** "Determining computational complexity from characteristic 'phase transitions'." *Nature.*

### Related BSAT Documentation

- **`schaefer-dichotomy.md`** - Why 3SAT is NP-complete
- **`constraint-types.md`** - Classification of SAT variants
- **`cdcl-solver.md`** - Modern CDCL techniques
- **`dpll-solver.md`** - Basic backtracking solver

---

*This comprehensive guide covers all major 3SAT variants with practical BSAT usage examples.*
