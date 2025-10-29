# SAT Constraint Types: A Comprehensive Taxonomy

## Table of Contents
1. [Introduction](#introduction)
2. [Classification by Polarity](#classification-by-polarity)
3. [Classification by Width](#classification-by-width)
4. [Classification by Semantics](#classification-by-semantics)
5. [Complexity Summary](#complexity-summary)
6. [Practical Examples](#practical-examples)

---

## Introduction

### Why Constraint Classification Matters

Understanding constraint types is crucial for:
- **Solver selection:** Choose the right algorithm (P vs exponential runtime!)
- **Problem encoding:** Model problems efficiently
- **Theoretical insight:** Understand the P/NP-complete boundary
- **Performance optimization:** Recognize tractable subproblems

### Three Dimensions of Classification

We classify Boolean constraints along three axes:

1. **Polarity:** How many positive vs negative literals?
   - All positive, all negative, mixed, Horn, dual-Horn

2. **Width:** How many literals per clause?
   - Unit (1), binary (2), ternary (3), k-literal

3. **Semantics:** What does the constraint mean?
   - OR (disjunction), XOR (parity), cardinality, NAE, etc.

---

## Classification by Polarity

### Overview

**Polarity** refers to whether literals are positive (x) or negative (¬x).

```
Polarity Spectrum:
  All Negative ←──── Mixed ────→ All Positive
    (0-valid)                      (1-valid)
                 ↓
            Horn (≤1 pos)
            Dual-Horn (≤1 neg)
```

---

### Positive Clauses (All Unnegated)

#### Definition

**Positive clauses** contain only positive (unnegated) literals.

#### Examples

```
(x)                    # Single positive literal
(x ∨ y)                # Two positive literals
(x ∨ y ∨ z)            # Three positive literals
(a ∨ b ∨ c ∨ d ∨ e)    # k positive literals
```

#### Properties

- **Satisfiability:** Always SAT (set all variables to True)
- **Tractability:** O(1) if ALL clauses are positive (1-valid case)
- **Relationship:** Subset of 1-valid formulas

#### Complete Positive Formula

```python
# All clauses positive
formula = "(x | y) & (y | z) & (x | z)"
cnf = CNFExpression.parse(formula)

# Trivial solution
solution = {var: True for var in cnf.get_variables()}
# Result: {'x': True, 'y': True, 'z': True}

assert cnf.evaluate(solution)  # Always True for positive formulas
```

#### Applications

- **Coverage problems:** Ensure all options are covered
- **AtLeast-k constraints:** At least k variables must be true
- **Positive monotone logic:** "More is better" scenarios
- **Optimization:** Finding minimal satisfying assignments

#### Warning: Not Always Tractable!

```python
# Positive 3SAT is NP-complete despite all literals being positive!
positive_3sat = "(x | y | z) & (x | w | v) & (y | z | w)"

# Why? Width = 3, not covered by Schaefer's 1-valid case
# 1-valid guarantees SAT but doesn't solve minimum assignment problem
```

---

### Negative Clauses (All Negated)

#### Definition

**Negative clauses** contain only negative (negated) literals.

#### Examples

```
(¬x)                    # Single negative literal
(¬x ∨ ¬y)               # Two negative literals (AtMost-1 constraint)
(¬x ∨ ¬y ∨ ¬z)          # Three negative literals (AtMost-2)
(¬a ∨ ¬b ∨ ¬c ∨ ¬d)     # k negative literals (AtMost-(k-1))
```

#### Properties

- **Satisfiability:** Always SAT (set all variables to False)
- **Tractability:** O(1) if ALL clauses are negative (0-valid case)
- **Interpretation:** AtMost constraints (forbid too many True values)

#### Complete Negative Formula

```python
# All clauses negative
formula = "(~x | ~y) & (~y | ~z) & (~x | ~z)"
cnf = CNFExpression.parse(formula)

# Trivial solution
solution = {var: False for var in cnf.get_variables()}
# Result: {'x': False, 'y': False, 'z': False}

assert cnf.evaluate(solution)  # Always True for negative formulas
```

#### Applications

- **AtMost-k constraints:** At most k variables can be true
- **Conflict avoidance:** Prevent simultaneous activation
- **Resource limits:** Can't allocate more than k resources
- **Mutex (mutual exclusion):** Only one of many can be active

#### Example: AtMost-1 Constraint

```python
# AtMost-1(x, y, z): At most one of {x, y, z} can be true
# Encoding: All pairwise conflicts
formula = "(~x | ~y) & (~x | ~z) & (~y | ~z)"
cnf = CNFExpression.parse(formula)

# Valid solutions:
# {x: False, y: False, z: False}  # None true (OK)
# {x: True, y: False, z: False}   # One true (OK)
# {x: False, y: True, z: False}   # One true (OK)
# {x: False, y: False, z: True}   # One true (OK)

# Invalid:
# {x: True, y: True, z: False}    # Two true (violates AtMost-1)
```

---

### Horn Clauses (≤1 Positive Literal)

#### Definition

**Horn clauses** have at most one positive literal (any number of negative literals allowed).

#### Standard Forms

```
(x)                   # Fact (unit clause)
(¬x ∨ y)              # Definite clause: x → y
(¬x ∨ ¬y ∨ z)         # Definite clause: (x ∧ y) → z
(¬x ∨ ¬y ∨ ¬z)        # Goal clause (all negative, also Horn!)
```

#### Logical Interpretation

Horn clauses naturally express implications:

```
(¬x ∨ y)         ≡  (x → y)        "If x then y"
(¬x ∨ ¬y ∨ z)    ≡  (x ∧ y → z)    "If x and y then z"
(¬x ∨ ¬y ∨ ¬z)   ≡  ¬(x ∧ y ∧ z)   "Not all of x, y, z"
(x)              ≡  (⊤ → x)        "x is a fact"
```

#### Three Types of Horn Clauses

1. **Facts** (unit clauses): `(x)`
   - Known to be true
   - Starting point for inference

2. **Definite clauses**: `(¬x₁ ∨ ... ∨ ¬xₖ ∨ y)`
   - Rules: "If x₁, ..., xₖ all true, then y is true"
   - Forward chaining propagates through these

3. **Goal clauses**: `(¬x₁ ∨ ... ∨ ¬xₖ)`
   - Constraints: "Not all of x₁, ..., xₖ can be true"
   - Used to express goals or integrity constraints

#### BSAT Implementation

✅ **Implemented:** `src/bsat/hornsat.py`

```python
from bsat import solve_horn_sat, is_horn_formula, CNFExpression

# Example: Logic program with rules
program = """
    (human) &
    (~human | mortal) &
    (~mortal | will_die)
"""

# Parse and check
cnf = CNFExpression.parse(program)
print(f"Is Horn? {is_horn_formula(cnf)}")  # True

# Solve with forward chaining
result = solve_horn_sat(cnf)
print(f"Solution: {result}")
# Output: {'human': True, 'mortal': True, 'will_die': True}

# Explanation:
#   human = True (fact)
#   human → mortal, so mortal = True (propagate)
#   mortal → will_die, so will_die = True (propagate)
```

#### Algorithm: Unit Propagation (Forward Chaining)

```
1. Start with all variables False
2. Set all unit clauses (facts) to True
3. Repeat until no changes:
   a. For each clause (¬x₁ ∨ ... ∨ ¬xₖ ∨ y):
      If all xᵢ are True, set y = True
4. Check if any clause is violated (UNSAT) or all satisfied (SAT)
```

**Complexity:** O(n + m) where n = variables, m = clauses

#### Applications

**Logic Programming:**
```python
# Prolog-style rules
family_rules = """
(parent_of_john) &
(~parent_of_john | ancestor_of_john) &
(~ancestor_of_john | related_to_john)
"""
```

**Type Inference:**
```python
# Hindley-Milner type system
type_rules = """
(~expr_is_int | ~requires_bool | type_error) &
(variable_x_is_int) &
(~variable_x_is_int | expr_is_int)
"""
```

**Expert Systems:**
```python
# Medical diagnosis
diagnosis_rules = """
(has_fever) &
(~has_fever | has_infection) &
(~has_infection | needs_treatment)
"""
```

---

### Dual-Horn Clauses (≤1 Negative Literal)

#### Definition

**Dual-Horn clauses** (also called anti-Horn or reverse Horn) have at most one negative literal (any number of positive literals allowed).

#### Standard Forms

```
(¬x)                  # Negative unit clause
(x ∨ ¬y)              # Reverse implication
(x ∨ y ∨ ¬z)          # Reverse definite clause
(x ∨ y ∨ z)           # All positive (also dual-Horn!)
```

#### Logical Interpretation

Dual-Horn clauses express reverse implications:

```
(x ∨ ¬y)        ≡  (y → x)         "To achieve y, need x"
(x ∨ y ∨ ¬z)    ≡  (z → x ∨ y)     "If z, then x or y"
(¬x)            ≡  (x → ⊥)         "x is false"
```

#### Relationship to Horn

Dual-Horn is the **complement** of Horn:

```
Horn clause:       (¬x ∨ ¬y ∨ z)
Negate literals:   (x ∨ y ∨ ¬z)    ← This is dual-Horn!

Dual-Horn clause:  (x ∨ y ∨ ¬z)
Negate literals:   (¬x ∨ ¬y ∨ z)   ← This is Horn!
```

#### Algorithm: Transform to Horn

```python
def solve_dual_horn(cnf):
    """
    Solve dual-Horn by transforming to Horn

    Steps:
    1. Negate all literals (dual-Horn → Horn)
    2. Solve as Horn-SAT
    3. Negate solution back
    """
    # Step 1: Negate formula
    negated_cnf = negate_all_literals(cnf)

    # Step 2: Solve as Horn
    horn_solution = solve_horn_sat(negated_cnf)

    if horn_solution is None:
        return None  # UNSAT

    # Step 3: Negate solution
    return {var: not val for var, val in horn_solution.items()}
```

**Complexity:** O(n + m)

#### BSAT Status

❌ **Not implemented** (but algorithmically equivalent to Horn via negation)

#### Applications

- **Backward chaining:** Reasoning from goals to requirements
- **Goal-directed search:** Planning systems
- **Reverse implications:** "What's needed to achieve X?"
- **Minimal models:** Finding smallest satisfying assignments

#### Example: Goal-Directed Reasoning

```python
# Dual-Horn: backward chaining from goal
goal_rules = """
(~goal_achieved) &              # Goal must be achieved
(task_a | ~goal_achieved) &     # Need task_a for goal
(task_b | task_c | ~task_a) &   # Need task_b OR task_c for task_a
"""

# Reasoning backwards:
#   goal_achieved needed → task_a needed
#   task_a needed → task_b OR task_c needed
```

---

### Mixed Polarity (General Case)

#### Definition

**Mixed polarity** clauses contain both positive and negative literals with no restrictions.

#### Examples

```
(x ∨ ¬y)               # One positive, one negative (2SAT)
(x ∨ y ∨ ¬z)           # Two positive, one negative (3SAT)
(¬x ∨ y ∨ z)           # Two positive, one negative (Horn!)
(x ∨ ¬y ∨ ¬z)          # One positive, two negative (dual-Horn!)
(x ∨ y ∨ z ∨ ¬w)       # Mixed 4SAT
```

#### Classification

Mixed polarity formulas can still fall into tractable cases:

- **(x ∨ ¬y)** - Mixed but width 2 → 2SAT (tractable) ✅
- **(¬x ∨ ¬y ∨ z)** - Mixed but Horn → tractable ✅
- **(x ∨ y ∨ ¬z)** - Mixed but dual-Horn → tractable ✅
- **(x ∨ y ∨ z)** - Mixed 3SAT → NP-complete ❌

**Key insight:** Polarity alone doesn't determine complexity—must consider width and structure!

---

## Classification by Width

### Overview

**Width** = number of literals per clause.

```
Width Hierarchy:
  Unit (1) → Binary (2) → Ternary (3) → k-literal

  Tractable ←─────┤P/NP Boundary├─────→ NP-complete
                  │              │
            Width ≤ 2      Width ≥ 3
```

---

### Unit Clauses (Width 1)

#### Definition

**Unit clauses** contain exactly one literal.

#### Examples

```
(x)         # Positive unit clause
(¬y)        # Negative unit clause
```

#### Properties

- **Satisfiability:** Trivial—just assign the forced value
- **Complexity:** O(n) to check all unit clauses
- **Usage:** Facts, preprocessing, forced assignments

#### Unit Propagation

Unit clauses **force** variable assignments:

```python
(x)         → x must be True
(¬y)        → y must be False
```

After assignment, simplify remaining clauses:

```python
Original: (x) ∧ (x ∨ y ∨ z) ∧ (¬x ∨ w)

After x = True:
  (x ∨ y ∨ z) → satisfied (removed)
  (¬x ∨ w) → (w) (new unit clause!)

Result: (w)  → w must be True
```

#### BSAT Usage

All BSAT solvers use unit propagation as preprocessing:

```python
from bsat import solve_sat, CNFExpression

formula = "(x) & (~x | y) & (~y | z)"
cnf = CNFExpression.parse(formula)

result = solve_sat(cnf)
# Unit propagation:
#   x = True (unit clause)
#   x = True → y = True (from ¬x ∨ y)
#   y = True → z = True (from ¬y ∨ z)
# Result: {'x': True, 'y': True, 'z': True}
```

---

### Binary Clauses (Width 2) - The P/NP Boundary

#### Definition

**Binary clauses** contain exactly 2 literals (2SAT).

#### Examples

```
(x ∨ y)         # Both positive
(¬x ∨ z)        # Mixed polarity
(¬x ∨ ¬y)       # Both negative
```

#### The Critical Boundary

**2SAT is in P, 3SAT is NP-complete!**

This is one of the most striking results in complexity theory:
- Adding just **one** more literal per clause causes exponential blowup

#### Implication Graph

Every 2SAT clause can be viewed as implications:

```
(x ∨ y)  ≡  {¬x → y, ¬y → x}    "If not x, then y; if not y, then x"
(¬x ∨ z) ≡  {x → z, ¬z → ¬x}    "If x, then z; if not z, then not x"
```

Build directed graph and find Strongly Connected Components (SCCs).

#### BSAT Implementation

✅ **Implemented:** `src/bsat/twosatsolver.py`

```python
from bsat import solve_2sat, is_2sat, CNFExpression

# 2SAT example
formula = "(x | y) & (~x | z) & (~y | ~z)"
cnf = CNFExpression.parse(formula)

print(f"Is 2SAT? {is_2sat(cnf)}")  # True

result = solve_2sat(cnf)
print(f"Solution: {result}")
# Possible output: {'x': False, 'y': False, 'z': False}
```

**Complexity:** O(n + m) using SCC algorithm (Kosaraju or Tarjan)

#### Why 2SAT is Tractable

**Key insight:** Transitivity of implications works in linear time!

```
If x → y and y → z, then x → z
Build transitive closure via SCC
Check for contradictions: If x → ¬x (x and ¬x in same SCC), UNSAT
```

#### Applications

- **Type inference:** Constraint-based type systems
- **Circuit verification:** 2-input gates
- **Scheduling:** Binary dependencies
- **Resource allocation:** Pairwise conflicts

---

### Ternary Clauses (Width 3) - 3SAT

#### Definition

**Ternary clauses** (3SAT) contain exactly 3 literals.

#### Examples

```
(x ∨ y ∨ z)          # All positive
(¬x ∨ y ∨ ¬z)        # Mixed polarity
(¬x ∨ ¬y ∨ ¬z)       # All negative
```

#### The First NP-Complete Case

**3SAT is NP-complete** (Cook-Levin theorem, 1971)

Properties:
- No known polynomial-time algorithm
- No known implication structure like 2SAT
- Phase transition at clause/variable ratio ≈ 4.267

#### BSAT Implementation

✅ **General solvers:**
- `solve_sat()` - DPLL with backtracking
- `solve_cdcl()` - Modern CDCL with learning
- `solve_schoening()` - Randomized O(1.334^n) for 3SAT

```python
from bsat import solve_cdcl, CNFExpression

formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

result = solve_cdcl(cnf)
if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

**Complexity:** Exponential worst-case, but CDCL is efficient on many practical instances

#### Phase Transition

Random 3SAT has sharp phase transition:

```
r = m/n (clause-to-variable ratio)

r < 4.2:  Almost surely SAT (easy to solve)
r ≈ 4.267: Critical ratio (hardest instances)
r > 4.3:  Almost surely UNSAT (easy to prove)
```

---

### k-SAT (Width k ≥ 4)

#### Definition

**k-SAT** formulas have clauses with exactly k literals.

#### Examples

```
4-SAT: (x ∨ y ∨ z ∨ w)
5-SAT: (a ∨ b ∨ c ∨ d ∨ e)
k-SAT: k literals per clause
```

#### Complexity

**All k-SAT for k ≥ 3 are NP-complete**

Empirical observation:
- Larger k → easier to satisfy (more literals = more ways to satisfy)
- But worst-case complexity remains exponential

#### Critical Ratios

As k increases, critical ratio increases:

```
k     Critical ratio (r*)
2     1.0
3     4.267
4     9.931
5     21.117
k     ≈ 2^k ln(2)
```

#### Reduction to 3SAT

Any k-SAT can be reduced to 3SAT:

✅ **BSAT Implementation:** `src/bsat/reductions.py`

```python
from bsat import reduce_to_3sat, CNFExpression, Clause, Literal

# 5-SAT clause
clause_5sat = Clause([
    Literal('a'), Literal('b'), Literal('c'),
    Literal('d'), Literal('e')
])
cnf_5sat = CNFExpression([clause_5sat])

# Reduce to 3SAT (introduces auxiliary variables)
cnf_3sat, aux_map, stats = reduce_to_3sat(cnf_5sat)

print(f"Original clauses: {stats['original_clauses']}")
print(f"Reduced clauses: {stats['reduced_clauses']}")
print(f"Auxiliary variables: {stats['auxiliary_vars_added']}")

# Original: 1 clause with 5 literals
# Reduced: 3 clauses with ≤ 3 literals each
```

**Reduction technique:**
```
(a ∨ b ∨ c ∨ d ∨ e)

Becomes:
(a ∨ b ∨ x₁) ∧
(¬x₁ ∨ c ∨ x₂) ∧
(¬x₂ ∨ d ∨ e)

Where x₁, x₂ are fresh auxiliary variables
```

---

## Classification by Semantics

### Overview

Beyond syntax (polarity, width), we classify by **meaning**:
- What does the constraint express?
- OR, XOR, cardinality, NAE, etc.

---

### Affine Constraints (XOR-SAT)

#### Definition

**Affine constraints** are linear equations over the finite field GF(2).

#### Mathematical Form

```
x₁ ⊕ x₂ ⊕ ... ⊕ xₙ = b    (where b ∈ {0, 1})
```

**Semantics:**
- ⊕ is XOR (exclusive-OR)
- b = 0: Even number of variables must be True
- b = 1: Odd number of variables must be True

#### Examples

```
x ⊕ y = 1         # Exactly one of {x, y} is True
x ⊕ y = 0         # x and y have the same value
x ⊕ y ⊕ z = 1     # Odd number of {x, y, z} are True
x ⊕ y ⊕ z = 0     # Even number of {x, y, z} are True
```

#### CNF Encoding (Exponential Blowup!)

```
x ⊕ y = 1   →   (x ∨ y) ∧ (¬x ∨ ¬y)                    [2 clauses]

x ⊕ y ⊕ z = 1 → (x ∨ y ∨ z) ∧ (x ∨ ¬y ∨ ¬z) ∧           [4 clauses]
                 (¬x ∨ y ∨ ¬z) ∧ (¬x ∨ ¬y ∨ z)

k variables  →  2^(k-1) CNF clauses
```

**Problem:** Encoding XOR as CNF is inefficient!

#### BSAT Implementation

✅ **Implemented:** `src/bsat/xorsat.py`

```python
from bsat import solve_xorsat

# Note: BSAT expects CNF encoding of XOR
# x ⊕ y = 1 encoded as (x ∨ y) ∧ (¬x ∨ ¬y)

cnf_xor = CNFExpression([
    Clause([Literal('x', False), Literal('y', False)]),   # x ∨ y
    Clause([Literal('x', True), Literal('y', True)])      # ¬x ∨ ¬y
])

result = solve_xorsat(cnf_xor)
# Result: Either {x: True, y: False} or {x: False, y: True}
```

**Complexity:** O(n³) using Gaussian elimination over GF(2)

#### Applications

**Cryptanalysis:**
```
# Linear approximations in block ciphers
p₁ ⊕ p₂ ⊕ k₁ = c₁    # Plaintext + key = ciphertext
p₂ ⊕ p₃ ⊕ k₂ = c₂
# Solve for key bits k₁, k₂, ...
```

**Error Correction:**
```
# Hamming code parity checks
bit₁ ⊕ bit₂ ⊕ bit₃ = parity₁
bit₁ ⊕ bit₄ ⊕ bit₅ = parity₂
# Detect and correct errors
```

---

### Cardinality Constraints

#### Definition

**Cardinality constraints** specify how many variables must be true.

#### Types

```
AtLeast-k(x₁, ..., xₙ):  At least k of the n variables are True
AtMost-k(x₁, ..., xₙ):   At most k of the n variables are True
Exactly-k(x₁, ..., xₙ):  Exactly k of the n variables are True
```

#### Examples

**AtMost-1 (Mutex):**
```
AtMost-1(x, y, z)

CNF encoding (all pairwise conflicts):
(¬x ∨ ¬y) ∧ (¬x ∨ ¬z) ∧ (¬y ∨ ¬z)

Interpretation: At most one can be true (mutual exclusion)
```

**AtLeast-1 (Regular OR):**
```
AtLeast-1(x, y, z)

CNF encoding:
(x ∨ y ∨ z)

Interpretation: Standard disjunction
```

**Exactly-1:**
```
Exactly-1(x, y, z) = AtLeast-1(x, y, z) ∧ AtMost-1(x, y, z)

CNF encoding:
(x ∨ y ∨ z) ∧ (¬x ∨ ¬y) ∧ (¬x ∨ ¬z) ∧ (¬y ∨ ¬z)

[1 clause for AtLeast + 3 clauses for AtMost = 4 clauses total]
```

#### General Encoding

**AtMost-k via pairwise method:**
- Choose (k+1) variables from n
- Add clause forbidding all (k+1) to be simultaneously true
- Number of clauses: C(n, k+1)

**Example: AtMost-2(w, x, y, z)**
```
Forbid all triples:
(¬w ∨ ¬x ∨ ¬y) ∧
(¬w ∨ ¬x ∨ ¬z) ∧
(¬w ∨ ¬y ∨ ¬z) ∧
(¬x ∨ ¬y ∨ ¬z)

Number of clauses: C(4, 3) = 4
```

#### BSAT Status

❌ **No native cardinality support**

Workaround: Encode as CNF manually

```python
def encode_atmost_1(vars):
    """Encode AtMost-1 constraint as CNF"""
    clauses = []
    for i in range(len(vars)):
        for j in range(i+1, len(vars)):
            # Add clause: ¬vars[i] ∨ ¬vars[j]
            clause = Clause([
                Literal(vars[i], True),
                Literal(vars[j], True)
            ])
            clauses.append(clause)
    return CNFExpression(clauses)

# Usage
atmost1_cnf = encode_atmost_1(['x', 'y', 'z'])
# Result: (¬x ∨ ¬y) ∧ (¬x ∨ ¬z) ∧ (¬y ∨ ¬z)
```

#### Applications

- **Resource allocation:** At most k resources can be assigned
- **Scheduling:** At most 1 task per time slot
- **One-hot encoding:** Exactly 1 of n options
- **Sorting networks:** Counting True values

---

### Not-All-Equal (NAE) Constraints

#### Definition

**NAE constraints** require that not all literals have the same truth value.

#### Semantics

```
NAE(x, y, z): Forbids {all True} and {all False}

Valid assignments:
✅ {x: T, y: T, z: F}  # Not all equal
✅ {x: T, y: F, z: F}
✅ {x: F, y: T, z: F}
...

Invalid:
❌ {x: T, y: T, z: T}  # All equal (all True)
❌ {x: F, y: F, z: F}  # All equal (all False)
```

#### CNF Encoding

```
NAE(x, y, z) encodes as:
(x ∨ y ∨ z) ∧ (¬x ∨ ¬y ∨ ¬z)

First clause:  At least one is True (forbids all-False)
Second clause: At least one is False (forbids all-True)
```

#### Properties

**Self-Complementary:**
- If assignment α satisfies NAE formula
- Then ¬α (flipping all values) also satisfies it

**Example:**
```
NAE(x, y, z)

If {x: T, y: T, z: F} satisfies NAE
Then {x: F, y: F, z: T} also satisfies NAE
```

#### Complexity

**NAE-3SAT is NP-complete**

Even though NAE has nice symmetry, it's still hard!

#### BSAT Status

❌ **No native NAE support**

Workaround: Encode as CNF

```python
def encode_nae_constraint(vars):
    """Encode NAE constraint as CNF"""
    # Clause 1: At least one True
    pos_clause = Clause([Literal(v, False) for v in vars])

    # Clause 2: At least one False
    neg_clause = Clause([Literal(v, True) for v in vars])

    return CNFExpression([pos_clause, neg_clause])

# Usage
nae_cnf = encode_nae_constraint(['x', 'y', 'z'])
# Result: (x ∨ y ∨ z) ∧ (¬x ∨ ¬y ∨ ¬z)
```

#### Applications

- **Graph coloring:** NAE on edges ensures endpoints have different colors
- **Hypergraph 2-coloring:** NAE constraints on hyperedges
- **Diversity:** Ensure varied assignments
- **Symmetry:** Problems with solution symmetry

---

### 1-in-k Constraints (Exactly-One)

#### Definition

**1-in-k constraints** require exactly one of k variables to be true.

#### Semantics

```
1-in-3(x, y, z): Exactly one of {x, y, z} is True

Valid:
✅ {x: T, y: F, z: F}
✅ {x: F, y: T, z: F}
✅ {x: F, y: F, z: T}

Invalid:
❌ {x: F, y: F, z: F}  # Zero true
❌ {x: T, y: T, z: F}  # Two true
❌ {x: T, y: T, z: T}  # Three true
```

#### CNF Encoding

```
1-in-3(x, y, z) = AtLeast-1(x,y,z) ∧ AtMost-1(x,y,z)

CNF:
(x ∨ y ∨ z) ∧           # AtLeast-1
(¬x ∨ ¬y) ∧             # AtMost-1 (pairwise)
(¬x ∨ ¬z) ∧
(¬y ∨ ¬z)

Total: 4 clauses for 3 variables
```

#### Complexity

**1-in-3-SAT is NP-complete**

Even **positive 1-in-3-SAT** (no negations) is NP-complete!

This is surprising: Even monotone + exactly-one is hard.

#### Relationship to NAE

```
1-in-3 is stricter than NAE:

NAE(x, y, z): 6 valid assignments (not all-same)
1-in-3(x, y, z): 3 valid assignments (exactly-one)

NAE allows {x: T, y: T, z: F}  ✅
1-in-3 forbids this            ❌
```

#### BSAT Status

❌ **No native 1-in-k support**

Workaround: Encode using AtLeast + AtMost

```python
def encode_1_in_k(vars):
    """Encode exactly-one (1-in-k) as CNF"""
    clauses = []

    # AtLeast-1: (x₁ ∨ x₂ ∨ ... ∨ xₖ)
    at_least_clause = Clause([Literal(v, False) for v in vars])
    clauses.append(at_least_clause)

    # AtMost-1: All pairwise conflicts
    for i in range(len(vars)):
        for j in range(i+1, len(vars)):
            clause = Clause([
                Literal(vars[i], True),
                Literal(vars[j], True)
            ])
            clauses.append(clause)

    return CNFExpression(clauses)

# Usage
one_in_three = encode_1_in_k(['x', 'y', 'z'])
# Result: (x ∨ y ∨ z) ∧ (¬x ∨ ¬y) ∧ (¬x ∨ ¬z) ∧ (¬y ∨ ¬z)
```

#### Applications

- **Exact cover:** Choose exactly one option from each set
- **Partitioning:** Assign each item to exactly one partition
- **One-hot encoding:** Select exactly one choice
- **Sudoku:** Exactly one number per cell

---

## Complexity Summary

### Complete Classification Table

| Constraint Type | Width | Polarity | Semantics | Complexity | BSAT Status |
|----------------|-------|----------|-----------|------------|-------------|
| **0-Valid** | Any | All negative | All-false works | O(1) | ❌ Not implemented |
| **1-Valid** | Any | All positive | All-true works | O(1) | ❌ Not implemented |
| **2SAT** | ≤2 | Any | OR clauses | O(n+m) | ✅ `twosatsolver.py` |
| **Horn** | Any | ≤1 positive | Implications | O(n+m) | ✅ `hornsat.py` |
| **Dual-Horn** | Any | ≤1 negative | Reverse impl. | O(n+m) | ❌ Not implemented |
| **XOR-SAT** | Any | Any | XOR/parity | O(n³) | ✅ `xorsat.py` |
| **3SAT** | 3 | Any | OR clauses | NP-complete | ✅ General solvers |
| **k-SAT (k≥3)** | k | Any | OR clauses | NP-complete | ✅ General solvers |
| **NAE-3SAT** | 3 | Any | Not-all-equal | NP-complete | ❌ (encode as CNF) |
| **1-in-3-SAT** | 3 | Any | Exactly-one | NP-complete | ❌ (encode as CNF) |
| **AtMost-k** | Any | Negative | Cardinality | Varies | ❌ (encode as CNF) |
| **AtLeast-k** | Any | Positive | Cardinality | Varies | ❌ (encode as CNF) |

### Schaefer's Six Cases (Tractable)

1. **0-Valid:** O(1) - set all False
2. **1-Valid:** O(1) - set all True
3. **Bijunctive (2SAT):** O(n+m) - SCC algorithm
4. **Horn:** O(n+m) - forward chaining
5. **Dual-Horn:** O(n+m) - backward chaining
6. **Affine (XOR):** O(n³) - Gaussian elimination

**Everything else:** NP-complete

---

## Practical Examples

### Example 1: Automatic Classification

```python
def classify_and_recommend(cnf):
    """
    Classify CNF and recommend solver

    Returns:
        (constraint_type, solver, complexity)
    """
    # Check width
    max_width = max(len(clause.literals) for clause in cnf.clauses)

    # Check polarity
    all_positive = all(
        all(not lit.negated for lit in clause.literals)
        for clause in cnf.clauses
    )
    all_negative = all(
        all(lit.negated for lit in clause.literals)
        for clause in cnf.clauses
    )

    # Classification
    if all_negative:
        return ("0-valid", "trivial_false", "O(1)")

    if all_positive:
        return ("1-valid", "trivial_true", "O(1)")

    if max_width <= 2:
        return ("2SAT", "solve_2sat", "O(n+m)")

    if is_horn_formula(cnf):
        return ("Horn-SAT", "solve_horn_sat", "O(n+m)")

    # TODO: Add dual-Horn check
    # TODO: Add XOR-SAT check (harder from CNF)

    # Default: General SAT
    return ("General SAT", "solve_cdcl", "Exponential")


# Usage
formula = "(~x | ~y | z) & (~z | w)"
cnf = CNFExpression.parse(formula)

constraint_type, solver, complexity = classify_and_recommend(cnf)
print(f"Type: {constraint_type}")
print(f"Recommended solver: {solver}")
print(f"Complexity: {complexity}")
```

### Example 2: Encoding Sudoku Constraints

```python
def encode_sudoku_cell_constraint(cell_vars):
    """
    Each Sudoku cell has exactly one value (1-9)

    Uses Exactly-1 (1-in-9) encoding
    """
    # cell_vars = ['cell_1', 'cell_2', ..., 'cell_9']
    # Exactly one must be true

    return encode_1_in_k(cell_vars)


def encode_sudoku_row_constraint(row_vars):
    """
    Each number appears exactly once in row

    For each digit d:
        Exactly one cell in row has digit d
    """
    constraints = []
    for digit in range(1, 10):
        # Get all cells that could have this digit
        cells_with_digit = [f"row_cell_{i}_digit_{digit}"
                           for i in range(1, 10)]
        constraints.append(encode_1_in_k(cells_with_digit))

    return constraints
```

### Example 3: Graph Coloring with NAE

```python
def encode_graph_3coloring_as_nae(edges):
    """
    3-coloring using NAE constraints

    For each edge (u, v):
        u and v must have different colors
        Encode with NAE
    """
    constraints = []

    for (u, v) in edges:
        # For each color c:
        for color in ['red', 'green', 'blue']:
            # NAE(u_color, v_color) ensures they're not both that color
            u_var = f"{u}_{color}"
            v_var = f"{v}_{color}"

            # Actually, better encoding:
            # If u is color C, then v cannot be color C
            # (¬u_C ∨ ¬v_C) for all colors C

            clause = Clause([
                Literal(u_var, True),
                Literal(v_var, True)
            ])
            constraints.append(clause)

    # Also need: Each vertex has exactly one color
    # ... (encode with 1-in-3 constraints)

    return CNFExpression(constraints)
```

---

## Further Reading

### Related BSAT Documentation

- **[Schaefer's Dichotomy](schaefer-dichotomy.md)** - Complete theory of Schaefer's 6 tractable cases
- **[3SAT Variants](3sat-variants.md)** - Deep dive into NAE-3SAT, 1-in-3-SAT, etc.
- **[2SAT Solver](2sat-solver.md)** - 2SAT implementation details
- **[Horn-SAT Solver](advanced-solvers.md#horn-sat)** - Horn-SAT algorithm
- **[XOR-SAT Solver](xorsat-solver.md)** - XOR-SAT Gaussian elimination
- **[CDCL Solver](cdcl-solver.md)** - Modern solver for general SAT

### Papers

- Schaefer (1978) - Original dichotomy theorem
- Aspvall et al. (1979) - 2SAT in linear time
- Dowling & Gallier (1984) - Horn-SAT algorithms

---

*This documentation provides comprehensive coverage of SAT constraint types with practical BSAT examples.*
