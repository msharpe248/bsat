# SAT Terminology Reference

This page provides comprehensive explanations of key terms used throughout the BSAT documentation. If you're new to SAT solving, this is a great place to look up unfamiliar concepts.

## Table of Contents

1. [Basic Concepts](#basic-concepts)
2. [Core Techniques](#core-techniques)
3. [Solver Concepts](#solver-concepts)
4. [Problem Types](#problem-types)
5. [Complexity & Theory](#complexity--theory)
6. [Algorithm-Specific Terms](#algorithm-specific-terms)

---

## Basic Concepts

### Boolean Variable
A variable that can take on one of two values: **true** or **false**. In SAT problems, we're trying to find truth assignments for a set of Boolean variables.

**Example**: `x`, `y`, `z` are Boolean variables.

### Literal
A Boolean variable or its negation.

- **Positive literal**: The variable itself (e.g., `x`)
- **Negative literal**: The negation of a variable (e.g., `¬x` or `~x`)

**Example**: If `x` is a variable, then `x` and `¬x` are both literals.

**In BSAT**:
```python
from bsat import Literal
positive_lit = Literal('x', negated=False)  # x
negative_lit = Literal('x', negated=True)   # ¬x
```

### Clause
A **disjunction** (OR) of literals. A clause is satisfied if **at least one** of its literals is true.

**Example**: `(x ∨ y ∨ ¬z)` - This clause is satisfied if `x` is true, OR `y` is true, OR `z` is false.

**Types of clauses**:
- **Unit clause**: Contains exactly one literal (e.g., `x`)
- **Binary clause**: Contains exactly two literals (e.g., `x ∨ y`)
- **k-clause**: Contains exactly k literals

**In BSAT**:
```python
from bsat import Clause, Literal
clause = Clause([
    Literal('x', False),
    Literal('y', False),
    Literal('z', True)
])  # (x ∨ y ∨ ¬z)
```

### CNF (Conjunctive Normal Form)
A **conjunction** (AND) of clauses. A CNF formula is satisfied if **all** of its clauses are satisfied.

**Example**: `(x ∨ y) ∧ (¬x ∨ z) ∧ (y ∨ ¬z)`

This is CNF because it's an AND of clauses, where each clause is an OR of literals.

**Why CNF?**
- Standard form for SAT solvers
- Easy to check and manipulate
- Many real-world problems naturally encode to CNF

**In BSAT**:
```python
from bsat import CNFExpression
cnf = CNFExpression.parse("(x | y) & (~x | z) & (y | ~z)")
```

### Assignment
A mapping from variables to Boolean values (true/false).

**Example**: `{x: true, y: false, z: true}`

**Types**:
- **Partial assignment**: Some variables are unassigned
- **Complete assignment**: All variables have values
- **Satisfying assignment**: Makes the formula true

### Satisfiability (SAT)
A formula is **satisfiable** if there exists at least one assignment that makes it true.

**Example**:
- `(x ∨ y) ∧ (¬x ∨ z)` is **SAT** (e.g., `x=true, y=true, z=true` works)
- `(x) ∧ (¬x)` is **UNSAT** (no assignment can satisfy both clauses)

---

## Core Techniques

### Unit Propagation (Boolean Constraint Propagation / BCP)
When a clause contains only **one literal** (a unit clause), that literal **must be true** for the formula to be satisfiable.

**Process**:
1. Find a unit clause (e.g., `(x)`)
2. Set the variable to satisfy it (e.g., `x = true`)
3. Remove all clauses containing that literal (they're now satisfied)
4. Remove the negation of that literal from remaining clauses
5. Repeat until no unit clauses remain

**Example**:
```
Formula: (x) ∧ (¬x ∨ y) ∧ (¬y ∨ z)

Step 1: (x) is a unit clause → x = true
Step 2: Remove (x), simplify (¬x ∨ y) to (y)
Result: (y) ∧ (¬y ∨ z)

Step 3: (y) is a unit clause → y = true
Step 4: Remove (y), simplify (¬y ∨ z) to (z)
Result: (z)

Step 5: (z) is a unit clause → z = true
Final: x=true, y=true, z=true (SAT)
```

**Why it's important**:
- Infers forced variable assignments
- Prunes the search space
- Essential for modern SAT solvers
- Used in DPLL, CDCL, Davis-Putnam, Horn-SAT

**Complexity**: O(n) per propagation step

### Pure Literal Elimination
A literal is **pure** if it appears with only one polarity (all positive or all negative) throughout the formula.

**Process**:
1. Find a variable that appears only positively or only negatively
2. Set it to satisfy all its occurrences
3. Remove all clauses containing that literal (they're now satisfied)

**Example**:
```
Formula: (x ∨ y) ∧ (x ∨ z) ∧ (¬y ∨ w)

Observation: x appears only positively (never ¬x)
Action: Set x = true
Result: Clauses (x ∨ y) and (x ∨ z) are satisfied and removed
Simplified: (¬y ∨ w)
```

**Why it's important**:
- Simplifies formulas without search
- Never causes conflicts
- Used in DPLL, Davis-Putnam, preprocessing

**Historical note**: Called the "affirmative-negative rule" in the original Davis-Putnam paper (1960).

### Resolution
A proof technique that combines two clauses to produce a new clause by eliminating a variable.

**Rule**: If you have `(A ∨ x)` and `(B ∨ ¬x)`, you can derive `(A ∨ B)`

**Example**:
```
Clause 1: (x ∨ y)
Clause 2: (¬x ∨ z)
Resolution on x: (y ∨ z)
```

**Explanation**: Since either `x` is true (making clause 1 require `y ∨ z`) or `x` is false (making clause 2 require `y ∨ z`), we know `y ∨ z` must hold.

**Uses**:
- Davis-Putnam algorithm (1960): Eliminates variables via resolution
- CDCL: Conflict clause learning via resolution
- Proof generation: Resolution proofs show UNSAT

**Problem**: Can generate exponentially many clauses!
- If x appears in n positive clauses and m negative clauses
- Resolution creates n × m new clauses (Cartesian product)
- This is why Davis-Putnam (1960) was replaced by DPLL (1962)

### Subsumption
A clause C₁ **subsumes** clause C₂ if C₁ ⊆ C₂ (C₁'s literals are a subset of C₂'s literals).

**Key insight**: If C₁ subsumes C₂, then C₂ is redundant and can be removed.

**Example**:
```
Clause 1: (x ∨ y)
Clause 2: (x ∨ y ∨ z)

Clause 1 subsumes Clause 2 because:
- If (x ∨ y) is true, then (x ∨ y ∨ z) is automatically true
- We can remove Clause 2
```

**Why it's important**:
- Reduces formula size without changing satisfiability
- Preprocessing technique in modern solvers
- Improves solver performance

**In BSAT**: Used in the preprocessing module.

### Simplification
General term for techniques that reduce formula complexity:
- Unit propagation
- Pure literal elimination
- Subsumption
- Tautology removal (e.g., `x ∨ ¬x` is always true)
- Duplicate literal removal

**Goal**: Make the formula smaller and easier to solve.

---

## Solver Concepts

### Backtracking
When a solver makes a decision that leads to a conflict, it **backtracks** by:
1. Undoing the most recent decision
2. Trying the opposite value
3. If both values fail, backtracking further

**Analogy**: Like exploring a maze - when you hit a dead end, you go back to the last intersection and try a different path.

**Example**:
```
Decision: x = true
  Propagate: ...
  Decision: y = true
    Propagate: ...
    Conflict! (empty clause found)
  Backtrack: Try y = false
    Propagate: ...
    Success! Found solution

If y = false also fails:
  Backtrack further: Try x = false
```

**Used in**: DPLL, CDCL (with non-chronological backtracking)

### Decision Level
The depth of the decision stack in a backtracking solver.

- **Level 0**: No decisions yet (only unit propagation)
- **Level 1**: After first decision
- **Level 2**: After second decision
- etc.

**Why it matters**:
- CDCL uses decision levels for conflict analysis
- Determines how far to backtrack
- Tracks which assignments are decisions vs. implications

**Example**:
```
Level 0: (Propagate forced assignments)
  x = true (unit clause)

Level 1: (First decision)
  y = true [DECISION]
  z = true (implied by unit propagation)

Level 2: (Second decision)
  w = false [DECISION]
  v = false (implied)
  CONFLICT!
```

### Implication Graph
A directed acyclic graph (DAG) used in CDCL that tracks:
- **Nodes**: Variable assignments
- **Edges**: Implication relationships (which clauses caused which assignments)

**Purpose**: Analyze conflicts to learn why they occurred.

**Example**:
```
Decision: x = true (level 1)
Clause (¬x ∨ y) implies: y = true
Clause (¬y ∨ z) implies: z = true
Clause (¬z ∨ w) implies: w = true
Conflict clause: (¬w ∨ ¬x)

Implication chain: x → y → z → w → conflict
```

**Used in**: CDCL for learning conflict clauses

### Conflict
A situation where the current partial assignment makes the formula false (produces an empty clause).

**Types**:
- **Direct conflict**: Unit propagation produces an empty clause
- **Implied conflict**: Further propagation reveals contradiction

**Example**:
```
Clauses: (x ∨ y) ∧ (¬x) ∧ (¬y)
Propagate: x = false (from ¬x)
Propagate: y = false (from ¬y)
Check (x ∨ y): false ∨ false = false → CONFLICT!
```

**Response**:
- **DPLL**: Backtrack and try different assignment
- **CDCL**: Analyze conflict, learn clause, backtrack smartly

### Learned Clause (Conflict Clause)
In CDCL, when a conflict occurs, the solver analyzes the implication graph to derive a new clause that:
1. Explains why the conflict happened
2. Prevents the same conflict in the future
3. Is implied by the existing clauses (so doesn't change satisfiability)

**Example**:
```
Original clauses: (x ∨ y) ∧ (¬x ∨ z) ∧ (¬y ∨ ¬z)

Decision: x = true
Propagate: z = true (from ¬x ∨ z)
Decision: y = true
Propagate: Conflict from (¬y ∨ ¬z)

Conflict analysis: "When x is true, we can't have both y and z true"
Learned clause: (¬x ∨ ¬y)  ← Prevents this path in future
```

**Why powerful**: The solver "learns" from mistakes and doesn't repeat them!

### Restart
Modern SAT solvers periodically **restart** the search:
1. Keep all learned clauses
2. Clear the decision stack
3. Start fresh with a new search path

**Why restart?**
- Escape local minima
- Benefit from learned clauses without being stuck in wrong area
- Heavy-tailed behavior: Some search paths are much harder than others

**Strategies**:
- Luby sequence (used in BSAT's CDCL)
- Geometric (double the interval each time)
- Fixed interval

**Used in**: CDCL, modern industrial solvers

### Branching (Decision Heuristic)
How the solver chooses which variable to assign next.

**Common heuristics**:
- **Random**: Pick any unassigned variable (simple, used in DPLL)
- **VSIDS (Variable State Independent Decaying Sum)**: Track which variables appear in recent conflicts (used in CDCL)
- **MOMS (Maximum Occurrences in clauses of Minimum Size)**: Prefer variables in small clauses
- **Jeroslow-Wang**: Weight variables by clause length

**Why it matters**: Good branching can exponentially reduce search time!

**In BSAT**:
- DPLL: Random/first unassigned
- CDCL: VSIDS heuristic

### VSIDS (Variable State Independent Decaying Sum)
A branching heuristic used in CDCL that tracks variable "activity":

1. Each variable has a score
2. When a conflict occurs, increment scores for variables in the conflict clause
3. Periodically decay all scores (multiply by a constant < 1)
4. Branch on the variable with the highest score

**Intuition**: Variables involved in recent conflicts are likely important - focus on them!

**Why "decaying"?** Old conflicts become less relevant over time.

**Performance**: VSIDS is one of the key innovations that makes CDCL so powerful on industrial instances.

---

## Problem Types

### 2-SAT
SAT problems where **every clause has exactly 2 literals**.

**Example**: `(x ∨ y) ∧ (¬x ∨ z) ∧ (y ∨ ¬z)`

**Special property**: Can be solved in **polynomial time** O(n+m) using strongly connected components (SCC) in an implication graph!

**Why polynomial?**
- `(x ∨ y)` means "if ¬x then y" and "if ¬y then x"
- Build implication graph, find SCCs
- If x and ¬x are in the same SCC → UNSAT
- Otherwise → SAT (and we can construct solution)

**In BSAT**: See [2SAT Solver](2sat-solver.md)

### 3-SAT
SAT problems where **every clause has exactly 3 literals**.

**Example**: `(x ∨ y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ z)`

**Special properties**:
- **NP-complete** (hardest SAT variant to approximate)
- First problem proven NP-complete (Cook-Levin theorem, 1971)
- Used in theoretical complexity analysis
- Random 3-SAT exhibits **phase transition** behavior

**In BSAT**: Solved with DPLL or CDCL

### k-SAT
SAT problems where **every clause has exactly k literals**.

**Complexity**:
- **1-SAT**: Trivial (O(n))
- **2-SAT**: Polynomial (O(n+m))
- **3-SAT and above**: NP-complete

**k-SAT to 3-SAT reduction**: Any k-SAT can be converted to 3-SAT by introducing auxiliary variables.

**Example** (reducing 4-clause to 3-clauses):
```
Original: (a ∨ b ∨ c ∨ d)
Reduced:  (a ∨ b ∨ aux₁) ∧ (¬aux₁ ∨ c ∨ d)
```

**In BSAT**: See [k-SAT to 3-SAT Reduction](introduction.md#reducing-k-sat-to-3-sat)

### Horn-SAT
SAT problems where **every clause has at most one positive literal**.

**Example**: `(x) ∧ (¬x ∨ y) ∧ (¬y ∨ z) ∧ (¬z)`

**Alternative view**: Horn clauses are implications
- `(¬x ∨ y)` means "x → y" (if x then y)
- `(¬x ∨ ¬y ∨ z)` means "x ∧ y → z"

**Special property**: Can be solved in **polynomial time** O(n+m) using unit propagation!

**Applications**:
- Logic programming (Prolog)
- Rule-based systems
- Forward chaining in AI

**In BSAT**: See [Horn-SAT Solver](hornsat-solver.md)

### XOR-SAT
SAT problems where clauses represent XOR (exclusive OR) constraints.

**Example**: `x ⊕ y ⊕ z = 1` (odd number of variables must be true)

**Encoding as CNF**: XOR can be encoded as multiple clauses
- `x ⊕ y = 1` becomes `(x ∨ y) ∧ (¬x ∨ ¬y)`

**Special property**: Can be solved in **polynomial time** O(n³) using **Gaussian elimination** over GF(2) (the field with two elements)!

**Applications**:
- Cryptography (stream ciphers, hash functions)
- Coding theory (error-correcting codes)
- Circuit equivalence checking

**In BSAT**: See [XOR-SAT Solver](xorsat-solver.md)

### #SAT (Model Counting)
Counting the number of satisfying assignments, not just finding one.

**Example**:
```
Formula: (x ∨ y)
Satisfying assignments:
  1. {x: true, y: true}
  2. {x: true, y: false}
  3. {x: false, y: true}
#SAT = 3
```

**Complexity**: #P-complete (even harder than NP-complete!)

**Techniques**:
- Exhaustive enumeration (exponential)
- Component caching (used in modern counters)
- Knowledge compilation (d-DNNF)

**In BSAT**: See `count_sat_solutions()` function

---

## Complexity & Theory

### NP-Complete
A complexity class for decision problems (yes/no answers) that are:
1. **In NP**: Solutions can be verified in polynomial time
2. **NP-hard**: At least as hard as every problem in NP

**SAT is NP-complete**: This means if you can solve SAT efficiently, you can solve ANY problem in NP efficiently!

**Significance**: Cook-Levin theorem (1971) proved SAT is NP-complete, launching computational complexity theory.

**Practical impact**: We don't know if P=NP, so we expect SAT requires exponential time in the worst case. But modern solvers are amazingly effective on many real-world instances!

### Polynomial Time
An algorithm runs in polynomial time if its worst-case time complexity is O(n^k) for some constant k.

**Examples**:
- O(n): Linear time (2-SAT, Horn-SAT)
- O(n²): Quadratic time
- O(n³): Cubic time (XOR-SAT via Gaussian elimination)

**Why important**: Polynomial time = "tractable" = can solve large instances

**Contrast with exponential**: O(2^n), O(1.5^n) - grow much faster!

### Exponential Time
An algorithm runs in exponential time if its complexity is O(c^n) where c > 1.

**Examples**:
- O(2^n): Brute force (try all assignments)
- O(1.334^n): Schöning's algorithm for 3-SAT
- O(1.5^n): PPSZ algorithm for k-SAT

**Reality**: Most general SAT solvers are exponential in worst case, but often much better in practice due to:
- Problem structure
- Learned clauses (CDCL)
- Good heuristics

### Phase Transition
In random k-SAT, there's a sharp transition from "almost always SAT" to "almost always UNSAT" as the clause-to-variable ratio increases.

**For random 3-SAT**:
- Ratio < 4.2: Almost always SAT (easy to solve)
- Ratio ≈ 4.26: **Phase transition** (hardest instances!)
- Ratio > 4.3: Almost always UNSAT (easy to prove)

**Graph**:
```
Probability of SAT
    |
1.0 |█████████╲
    |          ╲
0.5 |           ●━━━ ← Phase transition (hardest!)
    |             ╲
0.0 |              ╲█████████
    +----+----+----+----+----+
         3    4    5    6    7  (Clause-to-variable ratio)
```

**Why it matters**: Benchmark instances are often generated near the phase transition to test solver performance on hard instances.

**In BSAT**: See benchmarking examples

### Clause-to-Variable Ratio
The ratio of the number of clauses (m) to the number of variables (n).

**Ratio = m/n**

**Significance**:
- Low ratio: Under-constrained (likely SAT)
- High ratio: Over-constrained (likely UNSAT)
- Critical ratio: Phase transition (hardest)

**For random 3-SAT**: Critical ratio ≈ 4.26

**Example**:
- 100 variables, 426 clauses → ratio = 4.26 (hard!)
- 100 variables, 300 clauses → ratio = 3.0 (easy SAT)
- 100 variables, 600 clauses → ratio = 6.0 (easy UNSAT)

---

## Algorithm-Specific Terms

### DPLL (Davis-Putnam-Logemann-Loveland)
The classic backtracking SAT algorithm (1962) that combines:
1. Unit propagation
2. Pure literal elimination
3. Branching (pick a variable)
4. Recursive search with backtracking

**Key innovation over Davis-Putnam (1960)**: Uses backtracking instead of resolution, requiring only O(n) space instead of exponential space.

**In BSAT**: See [DPLL Solver](dpll-solver.md)

### CDCL (Conflict-Driven Clause Learning)
Modern SAT solving algorithm (1990s) that extends DPLL with:
1. **Conflict analysis**: Analyze why conflicts occur
2. **Clause learning**: Add learned clauses to prevent future conflicts
3. **Non-chronological backtracking**: Jump back multiple levels
4. **Restarts**: Periodically restart search while keeping learned clauses
5. **VSIDS**: Adaptive branching heuristic

**Key innovation**: Learn from mistakes! Clauses learned from conflicts guide future search.

**Performance**: CDCL solvers can handle millions of variables in industrial instances.

**In BSAT**: See [CDCL Solver](cdcl-solver.md)

### SCC (Strongly Connected Components)
In graph theory, a strongly connected component is a maximal set of vertices where every vertex can reach every other vertex.

**Used in 2-SAT**:
- Build implication graph from clauses
- Find SCCs using Tarjan's or Kosaraju's algorithm
- If x and ¬x are in same SCC → UNSAT
- Otherwise → SAT (construct solution from SCC ordering)

**Complexity**: O(n+m) using Tarjan's algorithm

**In BSAT**: Used in 2SAT solver

### Gaussian Elimination
An algorithm from linear algebra for solving systems of linear equations.

**Used in XOR-SAT**:
- Treat XOR clauses as linear equations over GF(2)
- Use row reduction to solve
- O(n³) time complexity

**Example**:
```
Clauses:     Linear system (mod 2):
x ⊕ y = 1    x + y = 1
y ⊕ z = 0    y + z = 0
x ⊕ z = 1    x + z = 1

Solve via Gaussian elimination → x=1, y=0, z=0
```

**In BSAT**: Used in XOR-SAT solver

### WalkSAT (Local Search)
A randomized local search algorithm:
1. Start with a random assignment
2. While unsatisfied clauses exist:
   - Pick an unsatisfied clause
   - With probability p: flip a random variable in it (exploration)
   - With probability 1-p: flip the variable that satisfies the most clauses (exploitation)
3. Repeat for max_flips iterations

**Properties**:
- **Incomplete**: May not find solution even if one exists
- **Fast**: Often very fast on satisfiable instances
- **Randomized**: Different runs may give different results

**In BSAT**: See [WalkSAT Solver](walksat-solver.md)

### Schöning's Algorithm
A randomized algorithm for k-SAT with provably better complexity than brute force.

**Algorithm**:
1. Start with random assignment
2. While unsatisfied clauses exist (up to 3n flips):
   - Pick an unsatisfied clause
   - Flip a random variable in it
3. If still unsatisfied, restart with new random assignment
4. Repeat for O(1.334^n) tries

**Complexity**: Expected O(1.334^n) for 3-SAT (better than O(2^n) brute force!)

**Key insight**: Random walk on satisfying assignments has good probability of reaching a solution.

**In BSAT**: See [Schöning's Solver](schoening-solver.md)

### BVA (Bounded Variable Addition)
A preprocessing technique that adds auxiliary variables to simplify formulas.

**Connection to Davis-Putnam**: The exponential clause blowup in Davis-Putnam resolution is related to why BVA can help - sometimes adding variables makes problems easier!

---

## Quick Reference Table

| Term | Category | Complexity | Description |
|------|----------|------------|-------------|
| Unit Propagation | Technique | O(n) | Forced assignments from unit clauses |
| Pure Literal | Technique | O(n) | Variable with one polarity |
| Resolution | Technique | Varies | Combine clauses to eliminate variable |
| Subsumption | Technique | O(n²) | Remove redundant clauses |
| Backtracking | Solver Concept | — | Undo decisions when conflicts occur |
| Conflict | Solver Concept | — | Empty clause under current assignment |
| Learned Clause | CDCL | — | Clause derived from conflict analysis |
| 2-SAT | Problem Type | O(n+m) | Two literals per clause (polynomial!) |
| 3-SAT | Problem Type | O(2^n) | Three literals per clause (NP-complete) |
| Horn-SAT | Problem Type | O(n+m) | ≤1 positive literal per clause (polynomial!) |
| XOR-SAT | Problem Type | O(n³) | XOR constraints (polynomial via Gaussian elimination!) |
| DPLL | Algorithm | O(2^n) | Classic backtracking with unit propagation |
| CDCL | Algorithm | O(2^n)* | Modern solver with clause learning (*often much faster) |
| WalkSAT | Algorithm | Varies | Randomized local search (incomplete but fast) |
| Schöning | Algorithm | O(1.334^n) | Randomized 3-SAT with proven complexity |

---

## See Also

- [Introduction to SAT](introduction.md) - Start here if you're new
- [CNF Expressions](cnf.md) - Understanding the data structures
- [Solvers Overview](solvers.md) - Which solver to use when
- [DPLL Solver](dpll-solver.md) - Classic backtracking algorithm
- [CDCL Solver](cdcl-solver.md) - Modern clause learning
- [2SAT Solver](2sat-solver.md) - Polynomial-time algorithm
- [Horn-SAT Solver](hornsat-solver.md) - Logic programming
- [Theory & References](theory.md) - Academic papers and further reading
