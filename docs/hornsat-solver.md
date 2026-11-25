# Horn-SAT Solver

A linear-time solver for Horn formulas using unit propagation.

## Overview

**Horn-SAT** is a restricted form of SAT where each clause has **at most one positive literal**. This restriction allows the problem to be solved in **linear time** O(n+m), unlike general SAT which is NP-complete.

**Implemented**: Complete Horn-SAT solver with unit propagation ✅

**Time Complexity**: O(n+m) where n = variables, m = clauses - linear in the size of the formula, using the classic counter-based algorithm of Dowling & Gallier (see [Why Linear Time?](#why-linear-time))
**Space Complexity**: O(n+m) for variable assignments, per-clause counters and occurrence lists
**Practical Performance**: Fast - no search or backtracking, always polynomial

## What is Horn-SAT?

Horn-SAT is a **polynomial-time solvable** special case of SAT with important practical applications.

### Definition

A **Horn clause** has **at most one positive literal**:

```
Valid Horn clauses:
  (x)           - 1 positive literal ✓
  (¬x ∨ y)      - 1 positive literal ✓
  (¬x ∨ ¬y ∨ z) - 1 positive literal ✓
  (¬x ∨ ¬y)     - 0 positive literals ✓

Not Horn:
  (x ∨ y)       - 2 positive literals ✗
  (x ∨ y ∨ z)   - 3 positive literals ✗
```

A **Horn formula** is a CNF where all clauses are Horn clauses.

### Why Horn-SAT Matters

Horn-SAT appears in many practical applications:

1. **Logic Programming** (Prolog, Datalog)
2. **Expert Systems** (rule-based reasoning)
3. **Type Inference** (programming languages)
4. **Database Queries** (Datalog queries)
5. **Forward Chaining** (inference engines)

### Implication Interpretation

Horn clauses naturally express implications:

```
Horn clause: (¬x ∨ ¬y ∨ z)
Meaning:     (x ∧ y) → z
In words:    "If x AND y, then z"

Horn clause: (¬x ∨ y)
Meaning:     x → y
In words:    "If x, then y"

Horn clause: (x)
Meaning:     x
In words:    "x is a fact"

Horn clause: (¬x ∨ ¬y)
Meaning:     ¬(x ∧ y)
In words:    "x and y cannot both be true"
```

## The Algorithm

### Horn-SAT Solver (Implemented)

The algorithm is remarkably simple. Read each clause `(¬b1 ∨ ... ∨ ¬bk ∨ h)` as the rule
`(b1 ∧ ... ∧ bk) → h`: the negated variables are the *body* and the positive variable (if any) is the
*head*. A rule "fires" once every variable in its body is True:

```
HornSAT(formula):
    // Start with all variables False
    assignment = {all variables → False}

    // Setup: one counter per clause, and for each variable the clauses
    // whose body mentions it
    for each clause c in formula:
        remaining[c] = number of body variables of c
        for each body variable b of c:
            add c to occurrences[b]
        if remaining[c] == 0:          // a fact (h) or the empty clause
            add c to ready

    // Forward chaining: fire every clause whose body is entirely True
    while ready is not empty:
        c = take a clause from ready

        // Body is True but there is no head to make True → UNSAT
        if c has no positive literal:
            return UNSAT

        h = head of c
        if assignment[h] is already True:
            continue

        set h to True
        for each clause d in occurrences[h]:
            remaining[d] -= 1
            if remaining[d] == 0:
                add d to ready

    // Nothing more fires: every clause is satisfied
    return assignment
```

### Key Insight: All-False Initialization

The algorithm starts with **all variables set to False**. Why?

- Horn clauses have **at most one positive literal**
- Setting a variable to True only helps **one clause** (the one with that positive literal)
- Setting it to False helps **all clauses** where it appears negatively
- Therefore, start conservative (all False) and only set True when forced

### Unit Propagation

When a clause has only one way to be satisfied (one unassigned positive literal), that literal **must** be True:

```
Example:
Formula: (x) ∧ (¬x ∨ y) ∧ (¬y ∨ z)
Initial: x=False, y=False, z=False

Step 1: Clause (x) is unit clause
        Only x can satisfy it → x = True

Step 2: With x=True, clause (¬x ∨ y) becomes (y)
        Only y can satisfy it → y = True

Step 3: With y=True, clause (¬y ∨ z) becomes (z)
        Only z can satisfy it → z = True

Result: {x=True, y=True, z=True} in O(n+m) time!
```

### Why Linear Time?

Each variable is set at most once (False → True), and each literal occurrence is touched a constant number of times:

- **Variables**: Each can transition False → True at most once
- **Clauses**: When a variable becomes True, only the clauses whose body mentions it are visited (via its occurrence list), and each visit just decrements a counter
- **Total**: O(n + m) where n = variables, m = clauses - more precisely, linear in the total number of literals in the formula

No backtracking needed!

**Implementation note**: this is the counter-based algorithm of Dowling & Gallier (1984), and it is what `HornSATSolver` implements. The more obvious approach - rescan every clause until nothing changes - is also correct, but needs up to n passes over the formula, i.e. O(n·m) in the worst case (for example, a long implication chain listed in reverse order). The counters and occurrence lists are what remove the rescanning.

## Usage

### Basic Usage

```python
from bsat import CNFExpression, solve_horn_sat, is_horn_formula

# Create a Horn formula
formula = "x & (~x | y) & (~y | z)"
cnf = CNFExpression.parse(formula)

# Check if it's Horn
if is_horn_formula(cnf):
    result = solve_horn_sat(cnf)

    if result:
        print(f"SAT: {result}")
        print(f"Verification: {cnf.evaluate(result)}")
    else:
        print("UNSAT")
```

### Using the Solver Class

```python
from bsat import HornSATSolver, CNFExpression

cnf = CNFExpression.parse("(a | ~b) & (~a | ~c) & (b)")

# Check if Horn (solver will raise ValueError if not)
solver = HornSATSolver(cnf)
solution = solver.solve()

if solution:
    print(f"Solution: {solution}")

    # Get statistics
    stats = solver.get_statistics()
    print(f"Variables: {stats['num_variables']}")
    print(f"Clauses: {stats['num_clauses']}")
    print(f"Unit propagations: {stats['num_unit_propagations']}")
else:
    print("UNSAT")
```

### Checking if Formula is Horn

```python
from bsat import is_horn_formula, CNFExpression

# This is Horn (at most 1 positive per clause)
horn = CNFExpression.parse("(x | ~y) & (~x | ~z)")
print(is_horn_formula(horn))  # True

# This is NOT Horn (2 positive literals)
not_horn = CNFExpression.parse("(x | y)")
print(is_horn_formula(not_horn))  # False
```

### Error Handling

```python
from bsat import HornSATSolver, CNFExpression

# Try to solve non-Horn formula
non_horn = CNFExpression.parse("(x | y | z)")

try:
    solver = HornSATSolver(non_horn)
except ValueError as e:
    print(f"Error: {e}")
    # Error: Formula is not Horn-SAT (contains clause with >1 positive literals)
```

## Examples

### Example 1: Logic Programming (Prolog-style)

```python
from bsat import CNFExpression, Clause, Literal, solve_horn_sat

# Prolog program:
#   likes_pizza(john).
#   likes_italian(X) :- likes_pizza(X).
#   happy(X) :- likes_italian(X).
#
# Query: happy(john)?

cnf = CNFExpression([
    # Fact: likes_pizza(john)
    Clause([Literal('likes_pizza_john')]),

    # Rule: likes_italian(john) :- likes_pizza(john)
    # Horn clause: (¬likes_pizza_john ∨ likes_italian_john)
    Clause([
        Literal('likes_pizza_john', negated=True),
        Literal('likes_italian_john')
    ]),

    # Rule: happy(john) :- likes_italian(john)
    # Horn clause: (¬likes_italian_john ∨ happy_john)
    Clause([
        Literal('likes_italian_john', negated=True),
        Literal('happy_john')
    ])
])

solution = solve_horn_sat(cnf)
if solution and solution['happy_john']:
    print("Yes, john is happy!")
```

### Example 2: Expert System

```python
from bsat import CNFExpression, Clause, Literal, solve_horn_sat

# Medical diagnosis expert system
# Rules:
#   has_fever(patient).
#   has_cough(patient).
#   has_flu(X) :- has_fever(X), has_cough(X).
#   needs_rest(X) :- has_flu(X).

cnf = CNFExpression([
    # Facts
    Clause([Literal('has_fever')]),
    Clause([Literal('has_cough')]),

    # Rule: has_flu :- has_fever ∧ has_cough
    # Horn: (¬has_fever ∨ ¬has_cough ∨ has_flu)
    Clause([
        Literal('has_fever', negated=True),
        Literal('has_cough', negated=True),
        Literal('has_flu')
    ]),

    # Rule: needs_rest :- has_flu
    # Horn: (¬has_flu ∨ needs_rest)
    Clause([
        Literal('has_flu', negated=True),
        Literal('needs_rest')
    ])
])

solution = solve_horn_sat(cnf)
if solution:
    print(f"Has flu: {solution['has_flu']}")
    print(f"Needs rest: {solution['needs_rest']}")
```

### Example 3: Type Inference

```python
from bsat import CNFExpression, Clause, Literal, HornSATSolver

# Type inference rules:
#   var_x : int        (fact)
#   expr_type : int :- var_x : int

cnf = CNFExpression([
    Clause([Literal('var_x_is_int')]),
    Clause([
        Literal('var_x_is_int', negated=True),
        Literal('expr_has_int_type')
    ])
])

solver = HornSATSolver(cnf)
solution = solver.solve()

if solution and solution['expr_has_int_type']:
    print("Type checking: PASS")
    stats = solver.get_statistics()
    print(f"Inference steps: {stats['num_unit_propagations']}")
```

### Example 4: Implication Chain

```python
from bsat import CNFExpression, solve_horn_sat

# Long implication chain: a → b → c → d → e
# Shows O(n) performance even for long chains

formula = "a & (~a | b) & (~b | c) & (~c | d) & (~d | e)"
cnf = CNFExpression.parse(formula)

solution = solve_horn_sat(cnf)
print(solution)
# {'a': True, 'b': True, 'c': True, 'd': True, 'e': True}
```

### Example 5: Unsatisfiable Formula

```python
from bsat import CNFExpression, solve_horn_sat

# Contradiction: x must be true, but x cannot be true
cnf = CNFExpression.parse("x & ~x")

result = solve_horn_sat(cnf)
print(result)  # None (UNSAT)
```

### Example 6: Database Query (Datalog)

```python
from bsat import CNFExpression, Clause, Literal, solve_horn_sat

# Datalog query:
#   parent(alice, bob).
#   parent(bob, charlie).
#   grandparent(X, Z) :- parent(X, Y), parent(Y, Z).
#
# Query: grandparent(alice, charlie)?

cnf = CNFExpression([
    Clause([Literal('parent_alice_bob')]),
    Clause([Literal('parent_bob_charlie')]),

    # grandparent(alice, charlie) :- parent(alice, bob) ∧ parent(bob, charlie)
    Clause([
        Literal('parent_alice_bob', negated=True),
        Literal('parent_bob_charlie', negated=True),
        Literal('grandparent_alice_charlie')
    ])
])

solution = solve_horn_sat(cnf)
if solution and solution['grandparent_alice_charlie']:
    print("Yes, alice is charlie's grandparent!")
```

### Example 7: Pure Negative Clauses

```python
from bsat import CNFExpression, solve_horn_sat

# All negative clauses: at most one variable can be true
formula = "(~x | ~y) & (~y | ~z) & (~x | ~z)"
cnf = CNFExpression.parse(formula)

solution = solve_horn_sat(cnf)
print(solution)
# {'x': False, 'y': False, 'z': False}
# All False satisfies all constraints!
```

## Performance Characteristics

### Time Complexity

**O(n + m)**, where:
- n = number of variables
- m = number of clauses

This is **linear time** - incredibly fast!

### Space Complexity

**O(n + m)** for the variable assignments, the per-clause counters and the occurrence lists

### Practical Performance

Horn-SAT scales to large instances. Approximate timings for `solve_horn_sat` on random Horn formulas (pure Python, one laptop - your numbers will differ):

| Variables | Clauses | Time |
|-----------|---------|------|
| 100       | 500     | <1ms |
| 1,000     | 5,000   | ~4ms |
| 10,000    | 50,000  | ~40ms |
| 100,000   | 500,000 | ~0.7s |

Clause order does not matter: an implication chain of 100,000 clauses solves in about 0.1s whether it is listed forwards or in reverse.

Compare to DPLL (exponential worst case)!

### Best Case: O(n+m)
- One pass over the clauses to set up the counters
- No backtracking ever needed

### Worst Case: O(n+m)
- Every variable becomes True once and every literal occurrence is visited once
- Horn-SAT is P-complete

### Why So Fast?

1. **No backtracking**: Each variable set at most once
2. **No branching**: Unit propagation determines all assignments
3. **Simple checks**: Each clause examined constant times
4. **No search**: Deterministic algorithm

## Types of Horn Clauses

### 1. Facts (Positive Unit Clauses)

```
(x)
```

**Meaning**: x is true
**Example**: `Clause([Literal('raining')])`

### 2. Negative Unit Clauses

```
(¬x)
```

**Meaning**: x must be false
**Example**: `Clause([Literal('sunny', negated=True)])`

### 3. Definite Clauses (Implications)

```
(¬x ∨ ¬y ∨ z)
```

**Meaning**: (x ∧ y) → z
**Example**: Logic programming rules
**Code**: `Clause([Literal('x', True), Literal('y', True), Literal('z')])`

### 4. Negative Clauses (Constraints)

```
(¬x ∨ ¬y)
```

**Meaning**: ¬(x ∧ y) - "x and y cannot both be true"
**Example**: Mutual exclusion constraints
**Code**: `Clause([Literal('x', True), Literal('y', True)])`

## Horn-SAT vs Other SAT Variants

| Property | Horn-SAT | 2SAT | 3SAT | General SAT |
|----------|----------|------|------|-------------|
| **Restriction** | ≤1 positive/clause | 2 literals/clause | 3 literals/clause | Any CNF |
| **Complexity** | O(n+m) | O(n+m) | NP-complete | NP-complete |
| **Algorithm** | Unit prop | SCC | DPLL/CDCL | DPLL/CDCL |
| **Scalability** | Millions | Millions | Hundreds | Thousands |
| **Applications** | Logic prog | Implications | Optimization | General |

## Applications

### 1. Logic Programming (Prolog, Datalog)

Prolog programs are Horn clauses:

```prolog
% Prolog facts and rules
parent(alice, bob).
ancestor(X, Y) :- parent(X, Y).
ancestor(X, Z) :- parent(X, Y), ancestor(Y, Z).
```

Convert to Horn-SAT to check queries.

### 2. Expert Systems

Rule-based reasoning:

```
IF fever AND cough THEN flu
IF flu THEN needs_rest
```

These are Horn clauses.

### 3. Type Inference

Type systems use Horn clauses:

```
x : int             (fact)
e : int :- x : int  (rule)
```

### 4. Database Queries

Datalog queries are Horn clauses:

```sql
employee(alice, engineering).
manager(X) :- employee(X, D), large_department(D).
```

### 5. Forward Chaining

Inference engines use Horn-SAT for forward chaining from facts to conclusions.

### 6. Configuration Problems

Product configuration with constraints:

```
has_feature_A → requires_component_X
has_feature_B → requires_component_Y
```

## Comparison with DPLL

| Aspect | Horn-SAT | DPLL |
|--------|----------|------|
| **Complexity** | O(n+m) | O(2ⁿ) |
| **Backtracking** | No | Yes |
| **Branching** | No | Yes |
| **Speed** | Always fast | Varies |
| **Applicability** | Horn only | Any CNF |
| **Guarantees** | Always polynomial | Exponential worst case |

**When to use each**:
- Use Horn-SAT if formula is Horn (guaranteed fast)
- Use DPLL for general SAT (necessary but slower)

## Algorithm Correctness

### Soundness

If Horn-SAT solver returns SAT, the assignment is correct:
- **Proof**: Algorithm verifies all clauses are satisfied before returning

### Completeness

If a solution exists, Horn-SAT solver finds it:
- **Proof**: Starting from all-False and only increasing variables to True explores all necessary assignments without backtracking

### Optimality

The solution minimizes the number of True variables:
- **Proof**: Algorithm only sets variables to True when forced (unit propagation)
- This gives the "minimal model" in logic programming

## Common Pitfalls

### 1. Assuming All Formulas are Horn

```python
# This is NOT Horn (2 positive literals)
cnf = CNFExpression.parse("(x | y)")

# This will raise ValueError
solver = HornSATSolver(cnf)  # ERROR!
```

**Solution**: Always check `is_horn_formula(cnf)` first

### 2. Misinterpreting Implications

```python
# x → y is NOT (x ∨ y)
# x → y is (¬x ∨ y)

# Wrong:
wrong = CNFExpression.parse("(x | y)")  # Not Horn!

# Correct:
correct = CNFExpression.parse("(~x | y)")  # Horn! ✓
```

### 3. Forgetting Pure Negative Clauses

```python
# Pure negative clauses are Horn!
# (¬x ∨ ¬y) has 0 positive literals ✓

cnf = CNFExpression.parse("(~x | ~y)")
print(is_horn_formula(cnf))  # True
```

## Debugging Tips

### Check if Formula is Horn

```python
from bsat import is_horn_formula

if not is_horn_formula(cnf):
    # Find non-Horn clauses
    for i, clause in enumerate(cnf.clauses):
        pos_count = sum(1 for lit in clause.literals if not lit.negated)
        if pos_count > 1:
            print(f"Clause {i} is not Horn: {clause}")
            print(f"  Positive literals: {pos_count}")
```

### Trace Unit Propagations

```python
solver = HornSATSolver(cnf)
solution = solver.solve()

stats = solver.get_statistics()
print(f"Unit propagations: {stats['num_unit_propagations']}")
# Should equal number of variables set to True
```

### Verify Solution

```python
result = solve_horn_sat(cnf)
if result:
    assert cnf.evaluate(result), "Solution doesn't satisfy formula!"
```

## Statistics

The solver tracks useful metrics:

```python
solver = HornSATSolver(cnf)
solution = solver.solve()

stats = solver.get_statistics()
# {
#     'num_variables': 10,
#     'num_clauses': 25,
#     'num_unit_propagations': 7
# }
```

**num_unit_propagations**: Number of variables set to True (indicates inference depth)

## Further Reading

### Classic Papers

- **Dowling & Gallier (1984)**: "Linear-time algorithms for testing the satisfiability of propositional Horn formulae"
  - Original polynomial-time algorithm for Horn-SAT
  - [Journal of Logic Programming, 1(3):267-284](https://www.sciencedirect.com/science/article/pii/0743106684900140)

- **Jones & Laaser (1976)**: "Complete problems for deterministic polynomial time"
  - Proves Horn-SAT is P-complete
  - Mathematical Systems Theory, 9(3):205-217

### Textbooks

- **"Handbook of Satisfiability"** (2021)
  - Chapter 2: Horn formulas and their applications
  - [IOS Press](https://www.iospress.com/catalog/books/handbook-of-satisfiability-2)

- **"Logic for Computer Science"** by Huth & Ryan
  - Chapter 2: Propositional logic and Horn clauses

- **"Foundations of Logic Programming"** by Lloyd (1987)
  - Definitive text on logic programming and Horn clauses

### Logic Programming

- **"Programming in Prolog"** by Clocksin & Mellish
  - Practical introduction to Horn clause logic programming

- **"What You Always Wanted to Know About Datalog"** by Ceri et al.
  - Survey of Datalog (database query language using Horn clauses)
  - [IEEE TKDE 1989](https://ieeexplore.ieee.org/document/43410)

### Online Resources

- [Horn Clause on Wikipedia](https://en.wikipedia.org/wiki/Horn_clause)
- [SWI-Prolog](https://www.swi-prolog.org/) - Popular Prolog implementation
- [Datalog Educational System](http://www.fdi.ucm.es/profesor/fernan/DES/) - Learn Datalog

### Complexity Theory

- **"Computational Complexity"** by Papadimitriou
  - Section on P-complete problems (Horn-SAT is P-complete)

## Next Steps

- Learn about [2SAT solving](2sat-solver.md) - another polynomial-time case
- Understand [DPLL solver](dpll-solver.md) for general SAT
- Explore [CNF expressions](cnf.md) and data structures
- Read about [theory and complexity](theory.md)
- Try the [examples and tutorials](examples.md)
- Compare [all solvers](solvers.md)

## Implementation Notes

**Current Implementation** (BSAT v0.1+):
- ✅ Counter-based forward chaining / unit propagation (Dowling & Gallier 1984), O(n+m)
- ✅ All-false initialization
- ✅ Horn formula validation
- ✅ Statistics tracking
- ✅ Comprehensive examples

**Future Enhancements**:
- ⏳ Optimized data structures for very large formulas
- ⏳ Incremental solving (add clauses after solving)
- ⏳ Explanation generation (why is variable True?)
- ⏳ Integration with Prolog/Datalog syntax

The current implementation is a complete (sound and terminating) Horn-SAT solver intended for learning and small-to-medium formulas.
