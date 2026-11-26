# SAT Preprocessing and Simplification

Transform complex SAT problems into simpler ones before solving. Preprocessing can reduce problem size by 50-90% and dramatically speed up solving.

## Table of Contents

1. [Overview](#overview)
2. [Why Preprocessing Matters](#why-preprocessing-matters)
3. [Techniques](#techniques)
4. [Usage](#usage)
5. [Performance](#performance)
6. [Examples](#examples)
7. [Best Practices](#best-practices)

---

## Overview

**SAT Preprocessing** applies simplification techniques to reduce formula complexity before solving:

- **Connected Component Decomposition** - Split independent subproblems
- **Unit Propagation** - Propagate forced assignments
- **Pure Literal Elimination** - Remove variables with single polarity
- **Clause Subsumption** - Remove redundant clauses
- **Self-Subsumption** - Simplify clauses through resolution

### Key Benefits

✅ **Reduced Problem Size** - Often 50-90% fewer clauses and variables
✅ **Faster Solving** - Smaller problems solve exponentially faster
✅ **Parallel Opportunities** - Independent components can be solved in parallel
✅ **Early Detection** - Find trivial SAT/UNSAT before expensive solving
✅ **Forced Assignments** - Discover variables that must have specific values

---

## Why Preprocessing Matters

### The Impact

Consider this formula:
```
a & (a | b | c) & (~a | d) & (d | e) & (f | g) & (f | h)
```

**Without preprocessing:**
- 6 clauses, 8 variables
- Solver must search 2^8 = 256 possibilities

**With preprocessing:**
- 0 clauses (completely solved!)
- Forced assignments: {a: True, d: True, f: True, ...}
- No search needed!

### When Preprocessing Shines

**Best for:**
- Formulas with structure (not completely random)
- Problems with unit clauses or implications
- Formulas with independent subproblems
- Large instances (thousands of clauses)

**Examples:**
- Hardware verification (lots of implications)
- Planning problems (many independent subgoals)
- Configuration problems (hierarchical constraints)

---

## Techniques

### 1. Connected Component Decomposition

**Idea**: Split formula into independent subproblems.

**How it works**: Two clauses are in the same component if they share a variable. Components can be solved separately.

**Example**:
```python
# Original: one problem
(a | b) & (c | d)

# After decomposition: two independent problems
Component 1: (a | b)
Component 2: (c | d)

# Can solve in parallel!
```

**Benefits**:
- ✅ Parallel solving
- ✅ Smaller subproblems (exponentially faster)
- ✅ If one component is UNSAT, entire formula is UNSAT

**Implementation**:
```python
from bsat import decompose_into_components, CNFExpression

cnf = CNFExpression.parse("(a | b) & (c | d)")
components = decompose_into_components(cnf)
# Returns: [CNFExpression((a | b)), CNFExpression((c | d))]
```

---

### 2. Unit Propagation

**Idea**: Propagate assignments from unit clauses (clauses with single literal).

**How it works**:
1. Find unit clause: `(a)`
2. Assign: `a = True`
3. Remove all clauses containing `a`
4. Remove `~a` from remaining clauses
5. Repeat if new unit clauses appear

**Example**:
```python
# Original
a & (a | b | c) & (~a | d)

# Step 1: a is unit → a = True
# Step 2: (a | b | c) satisfied, remove
# Step 3: (~a | d) becomes (d)
# Step 4: d is unit → d = True

# Result: completely solved!
# Assignments: {a: True, d: True}
```

**Benefits**:
- ✅ Discovers forced assignments
- ✅ Chains through implications
- ✅ Can prove UNSAT if conflict found
- ✅ Polynomial time O(clauses × literals)

**When it triggers**:
- Unit clauses in formula
- After other techniques create unit clauses
- Implication chains: `a → b → c → d`

---

### 3. Pure Literal Elimination

**Idea**: Remove variables appearing with only one polarity (always positive or always negative).

**How it works**:
1. Find variable appearing only positive or only negative
2. Assign it to satisfy all its occurrences
3. Remove all clauses containing it

**Example**:
```python
# Original: a appears only positive (pure)
(a | b) & (a | c) & (~b | ~c)

# Step 1: a is pure positive → a = True
# Step 2: First two clauses satisfied, remove
# Result: (~b | ~c)
```

**Benefits**:
- ✅ Safe simplification (preserves satisfiability)
- ✅ Reduces problem size
- ✅ Often creates more unit clauses

**Polarity Table**:
| Variable | Occurrences | Pure? | Assignment |
|----------|-------------|-------|------------|
| a | a, a | ✅ Pure positive | True |
| b | b, ~b | ❌ Mixed | - |
| c | ~c, ~c | ✅ Pure negative | False |

---

### 4. Clause Subsumption

**Idea**: Remove clauses that are "subsumed" by smaller clauses.

**Definition**: Clause C subsumes clause D if C ⊆ D (every literal in C is in D).

**How it works**:
1. If C ⊆ D, then D is redundant
2. Whenever C is satisfied, D is also satisfied
3. Remove D safely

**Example**:
```python
# (a) subsumes (a | b) and (a | b | c)
(a) & (a | b) & (a | b | c)

# After subsumption:
(a)  # Other clauses removed
```

**Why it's safe**:
- Any assignment satisfying (a) also satisfies (a | b)
- The smaller clause is "stronger" (harder to satisfy)
- Removing the weaker clause doesn't change satisfiability

**Benefits**:
- ✅ Reduces clause count
- ✅ Removes redundancy
- ✅ Often chains with other techniques

---

### 5. Self-Subsumption Resolution

**Idea**: Simplify clauses through resolution and subsumption.

**How it works**:
If `(a | C)` and `(~a | D)` exist, and `C ⊆ D`, then replace `(~a | D)` with `D`.

**Example**:
```python
# Original
(a | b) & (~a | b | c)

# C = {b}, D = {b, c}
# C ⊆ D, so simplify (~a | b | c) to (b | c)

# Result:
(a | b) & (b | c)
```

**Benefits**:
- ✅ Reduces clause size
- ✅ May create unit clauses
- ✅ Advanced technique for further reduction

---

## Usage

### Basic Usage

```python
from bsat import preprocess_cnf, CNFExpression

cnf = CNFExpression.parse("a & (a | b) & (~a | c)")
result = preprocess_cnf(cnf)

print(f"Simplified: {result.simplified}")
print(f"Forced: {result.assignments}")
print(f"Stats: {result.stats}")
```

### Decompose and Preprocess (Recommended)

```python
from bsat import decompose_and_preprocess

cnf = CNFExpression.parse("(a | b) & a & (c | d) & c")
components, assignments, stats = decompose_and_preprocess(cnf)

print(f"Components: {len(components)}")
print(f"Forced: {assignments}")
print(f"Reduction: {stats}")
```

`components` contains only the components that still need solving (here none, so `Components: 0`).

> **UNSAT components**: if preprocessing proves a component UNSAT (e.g. `a & ~a`), that component is returned as a formula containing only the empty clause (it prints as `⊥`), which every solver reports as UNSAT. So the pattern is: solve each returned component, and if any of them is UNSAT the whole formula is UNSAT (see the workflow below).

### Selective Techniques

```python
from bsat import SATPreprocessor

preprocessor = SATPreprocessor(cnf)
result = preprocessor.preprocess(
    unit_propagation=True,
    pure_literal=True,
    subsumption=True,
    self_subsumption=False  # Disable this one
)
```

### Complete Workflow

```python
from bsat import CNFExpression, decompose_and_preprocess, solve_sat

# 1. Parse formula
cnf = CNFExpression.parse("(a | b) & a & (c | d)")

# 2. Decompose and preprocess
components, forced, stats = decompose_and_preprocess(cnf)

print(f"Reduced {stats.original_clauses} → {stats.final_clauses} clauses")

# 3. Solve remaining components
all_solutions = forced.copy()

for component in components:
    solution = solve_sat(component)
    if solution is None:
        # One UNSAT component makes the whole formula UNSAT
        print("UNSAT!")
        all_solutions = None
        break
    all_solutions.update(solution)

# 4. Verify
if all_solutions is not None:
    print(f"Solution: {all_solutions}")
    print(f"Valid: {cnf.evaluate(all_solutions)}")
```

---

## Performance

### Complexity

| Technique | Time Complexity | Space |
|-----------|----------------|-------|
| Component Decomposition | O(n + m) | O(n + m) |
| Unit Propagation | O(m × l) | O(n + m) |
| Pure Literal | O(m × l) | O(n) |
| Subsumption | O(m² × l) | O(m) |
| Self-Subsumption | O(m² × l²) | O(m) |

Where:
- n = variables
- m = clauses
- l = average clause length

### Typical Impact

The reduction and speedup figures below are indicative of well-structured instances (unit clauses, implications, independent parts); they are not measurements of this package. Uniform random 3-SAT typically sees little or no reduction. The preprocessing times are approximate measurements of this pure-Python implementation with all techniques enabled - subsumption and self-subsumption compare clause pairs, so time grows quadratically with the number of clauses.

**Small formulas (< 100 clauses)**:
- Preprocessing time: a few milliseconds
- Reduction: 20-50%
- Net speedup: 2-5x

**Medium formulas (100-1,000 clauses)**:
- Preprocessing time: milliseconds to ~0.5s
- Reduction: 50-80%
- Net speedup: 10-100x

**Large formulas (> 1,000 clauses)**:
- Preprocessing time: seconds and up (~9s at 3,000 clauses); consider `subsumption=False, self_subsumption=False`
- Reduction: 70-95%
- Net speedup: 100-10,000x

### When to Use

**Always preprocess when:**
- ✅ Formula has > 50 clauses
- ✅ You suspect structure (not random)
- ✅ Solving time > 100ms

**Skip preprocessing when:**
- ❌ Very small formulas (< 20 clauses)
- ❌ Completely random 3-SAT near threshold
- ❌ Already preprocessed

---

## Examples

### Example 1: Independent Components

```python
from bsat import decompose_into_components, CNFExpression

# Two independent problems
cnf = CNFExpression.parse("(a | b) & (c | d)")
components = decompose_into_components(cnf)

print(f"Components: {len(components)}")
# Output: Components: 2

# Solve independently
for i, comp in enumerate(components):
    print(f"Component {i+1}: {comp}")
# Output:
#   Component 1: (a | b)
#   Component 2: (c | d)
```

### Example 2: Unit Propagation Chain

```python
from bsat import preprocess_cnf, CNFExpression

# Chain of implications
cnf = CNFExpression.parse("a & (~a | b) & (~b | c)")
result = preprocess_cnf(cnf)

print(f"Forced: {result.assignments}")
# Output: Forced: {'a': True, 'b': True, 'c': True}

print(f"Simplified: {result.simplified}")
# Output: Simplified: ⊤ (completely solved)
```

### Example 3: Pure Literals

```python
from bsat import preprocess_cnf, CNFExpression

# 'a' appears only positive
cnf = CNFExpression.parse("(a | b) & (a | c)")
result = preprocess_cnf(cnf)

print(f"Pure literal 'a' = {result.assignments['a']}")
# Output: Pure literal 'a' = True
```

### Example 4: Complete Workflow

```python
from bsat import CNFExpression, decompose_and_preprocess, solve_sat

# Complex formula
cnf = CNFExpression.parse(
    "a & (a | b) & (~a | c) & (d | e) & d"
)

# Decompose and preprocess
components, forced, stats = decompose_and_preprocess(cnf)

print(f"Original: {stats.original_clauses} clauses")
print(f"After: {stats.final_clauses} clauses")
print(f"Forced: {forced}")
print(f"Components: {len(components)}")

# Solve what remains
solution = forced.copy()
for comp in components:
    sol = solve_sat(comp)
    if sol is None:
        # One UNSAT component makes the whole formula UNSAT
        solution = None
        break
    solution.update(sol)

print(f"Solution: {solution}")  # None means UNSAT
```

---

## Best Practices

### 1. Always Decompose First

```python
# ✅ Good: Decompose then preprocess
components, forced, stats = decompose_and_preprocess(cnf)

# ❌ Less efficient: Preprocess entire formula
result = preprocess_cnf(cnf)
```

**Why**: Independent components are exponentially easier to solve separately.

### 2. Use All Techniques

```python
# ✅ Good: Use all preprocessing
result = preprocess_cnf(cnf)

# ❌ Missing opportunities
result = preprocess_cnf(cnf, subsumption=False)
```

**Why**: Techniques synergize - unit propagation creates pure literals, etc.

### 3. Check for Trivial Solutions

```python
result = preprocess_cnf(cnf)

if result.is_sat == True:
    print("Solved by preprocessing alone!")
    solution = result.assignments
elif result.is_sat == False:
    print("UNSAT detected early!")
else:
    # Need to solve
    solution = solve_sat(result.simplified)
```

**Why**: Preprocessing often solves problems completely.

### 4. Combine Forced Assignments

```python
result = preprocess_cnf(cnf)

if result.simplified.clauses:
    solution = solve_sat(result.simplified)
    if solution:
        # ✅ Merge with forced assignments
        solution.update(result.assignments)
else:
    # Already solved
    solution = result.assignments
```

**Why**: Preprocessing finds partial solutions.

**Note**: variables that drop out during simplification (e.g. `b` in `a & (a | b)`) are unconstrained and do not appear in `result.assignments` or in the solution of the simplified formula. The merged dictionary still satisfies the original formula (`cnf.evaluate` treats missing variables as False), but add them yourself if you need a total assignment.

### 5. Use Statistics

```python
result = preprocess_cnf(cnf)

print(result.stats)
# Shows:
#   - Variables reduced
#   - Clauses reduced
#   - Techniques applied
#   - Component count

# Make decisions based on reduction
if result.stats.final_clauses < 10:
    # Small enough for brute force
    solution = solve_sat(result.simplified)
else:
    # Use advanced solver
    solution = solve_cdcl(result.simplified)
```

---

## API Reference

### Functions

```python
def decompose_into_components(cnf: CNFExpression) -> List[CNFExpression]
    """Split into independent subproblems."""

def preprocess_cnf(cnf: CNFExpression, **options) -> PreprocessingResult
    """Apply all preprocessing techniques."""

def decompose_and_preprocess(cnf: CNFExpression, **options) -> Tuple[List, Dict, Stats]
    """Decompose then preprocess each component (recommended).
    Returns only the components that still need solving, the forced assignments, and combined stats."""
```

### Classes

```python
class SATPreprocessor:
    """Low-level preprocessing engine."""
    def __init__(cnf: CNFExpression)
    def preprocess(**options) -> PreprocessingResult

class PreprocessingResult:
    simplified: CNFExpression       # Simplified formula
    assignments: Dict[str, bool]    # Forced assignments
    stats: PreprocessingStats       # Statistics
    is_sat: Optional[bool]          # Trivial SAT/UNSAT?
    components: Optional[List]      # Reserved; currently always None (use decompose_into_components)

class PreprocessingStats:
    original_vars: int
    original_clauses: int
    final_vars: int
    final_clauses: int
    unit_propagations: int
    pure_literals: int
    subsumed_clauses: int
    self_subsumptions: int
    components: int
```

---

## Further Reading

### Papers

- **Eén & Biere (2005)**: "Effective Preprocessing in SAT Through Variable and Clause Elimination"
  - [PDF](https://www.cs.cmu.edu/~mheule/publications/EenBiere-SAT05.pdf)

- **Järvisalo et al. (2012)**: "Preprocessing in SAT Solving"
  - Handbook of Satisfiability, Chapter 6

### Related

- [Formula Simplification](simplification.md) - Further techniques in the separate `bsat.simplification` module: failed literal probing, blocked clause elimination, bounded variable elimination and vivification, with reconstruction of solutions for the original formula
- [DPLL Solver](dpll-solver.md) - Uses unit propagation internally
- [2SAT Solver](2sat-solver.md) - Polynomial via implication graph (related to components)
- [Benchmarking](benchmarking.md) - Measure preprocessing impact

---

## Summary

**Preprocessing transforms hard problems into easy ones:**

🔄 **Decompose** → Split independent subproblems
⚡ **Unit Propagation** → Find forced assignments
🎯 **Pure Literals** → Eliminate single-polarity variables
🗑️ **Subsumption** → Remove redundancy
📊 **Track Stats** → Understand reduction

**Best Practice**: Always `decompose_and_preprocess()` before solving!

**Typical Impact**: 50-90% reduction, 10-1000x speedup

Next: See [examples/example_preprocessing.py](../examples/example_preprocessing.py) for practical usage!
