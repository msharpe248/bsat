# WalkSAT Solver

A randomized local search algorithm for SAT that's incomplete but often very fast for satisfiable instances.

## Overview

**WalkSAT** is a randomized local search algorithm that tries to find satisfying assignments by:
1. Starting with a random assignment
2. Repeatedly flipping variables to fix unsatisfied clauses
3. Using a "noise" parameter to escape local minima

**Key Characteristics**:
- **Incomplete**: May not find a solution even if one exists
- **Probabilistic**: Different runs may give different results
- **Fast**: Often finds solutions much quicker than complete solvers
- **Cannot prove UNSAT**: Only useful for finding solutions, not proving none exist

**Time Complexity**: Varies (problem-dependent)
- Best case: O(n) if lucky
- Average case: Problem-dependent
- Worst case: May never terminate (use max_flips limit)

## What is WalkSAT?

WalkSAT is part of a family of **stochastic local search** algorithms for SAT. Unlike systematic solvers (DPLL, CDCL) that exhaustively explore the search space, WalkSAT uses randomized moves to navigate toward solutions.

### Local Search Intuition

Think of SAT solving as trying to get all lights turned on in a building:
- **Systematic search (DPLL)**: Try every possible switch combination methodically
- **Local search (WalkSAT)**: Start with random switches, then flip switches to turn on more lights

WalkSAT can be much faster, but might miss solutions or get stuck.

## The Algorithm

### Basic Algorithm

```
WalkSAT(formula, max_flips, max_tries):
  for try in 1 to max_tries:
    assignment ← random assignment

    for flip in 1 to max_flips:
      if assignment satisfies formula:
        return assignment

      clause ← random unsatisfied clause

      with probability p (noise):
        variable ← random variable from clause
      with probability 1-p:
        variable ← variable that minimizes "break count"

      flip variable in assignment

  return None  // No solution found
```

### Key Components

#### 1. Random Initial Assignment

Start with all variables assigned randomly to True or False:
```python
assignment = {x: random_choice([True, False]) for x in variables}
```

This provides diversity across multiple runs.

#### 2. Pick Unsatisfied Clause

From all clauses that are currently false, pick one at random:
```
unsatisfied = [clause for clause in formula if not clause.satisfied(assignment)]
target_clause = random.choice(unsatisfied)
```

#### 3. Choose Variable to Flip

This is where the **noise parameter** p comes in:

**With probability p (random walk)**:
- Pick a random variable from the unsatisfied clause
- Encourages exploration, helps escape local minima

**With probability 1-p (greedy)**:
- Pick the variable that minimizes "break count"
- Break count = # of currently satisfied clauses that become unsatisfied
- Encourages exploitation of current progress

#### 4. Flip the Variable

```python
assignment[variable] = not assignment[variable]
```

### The Noise Parameter

The noise parameter **p** controls the exploration/exploitation tradeoff:

| Noise | Behavior | Performance |
|-------|----------|-------------|
| p = 0.0 | Pure greedy | Can get stuck in local minima |
| p = 0.3-0.5 | Balanced | Good for most problems ✓ |
| p = 1.0 | Pure random walk | Very inefficient |

**Typical values**: 0.4-0.5 work well for random 3-SAT.

## Usage

### Basic Usage

```python
from bsat import CNFExpression, solve_walksat

# Parse formula
formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
cnf = CNFExpression.parse(formula)

# Solve with WalkSAT
result = solve_walksat(cnf)

if result:
    print(f"SAT: {result}")
else:
    print("No solution found (but may still be SAT!)")
```

### With Parameters

```python
result = solve_walksat(
    cnf,
    noise=0.5,        # Noise parameter (0.0-1.0)
    max_flips=10000,  # Max flips per try
    max_tries=10,     # Max random restarts
    seed=42           # For reproducibility
)
```

### Using the Solver Class

```python
from bsat import WalkSATSolver

solver = WalkSATSolver(
    cnf,
    noise=0.5,
    max_flips=10000,
    max_tries=10,
    seed=42
)

result = solver.solve()
print(f"Statistics: {solver.stats}")
```

### Getting Statistics

```python
from bsat import get_walksat_stats

stats = get_walksat_stats(cnf, noise=0.5, seed=42)

print(f"Found: {stats['found']}")
print(f"Solution: {stats['solution']}")
print(f"Total flips: {stats['stats']['total_flips']}")
print(f"Total tries: {stats['stats']['total_tries']}")
```

## Examples

### Example 1: Simple 3-SAT

```python
from bsat import CNFExpression, solve_walksat

# (x ∨ y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ z)
cnf = CNFExpression([
    Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
    Clause([Literal('x', True), Literal('y', False), Literal('z', True)]),
    Clause([Literal('x', False), Literal('y', True), Literal('z', False)])
])

result = solve_walksat(cnf, seed=42)
# Result: {'x': True, 'y': True, 'z': False}
```

### Example 2: Tuning Noise Parameter

```python
# Try different noise values
for noise in [0.0, 0.3, 0.5, 0.7, 1.0]:
    result = solve_walksat(cnf, noise=noise, seed=42)
    print(f"Noise {noise}: {result}")
```

### Example 3: Multiple Restarts

```python
# Use multiple random restarts to increase success probability
result = solve_walksat(
    cnf,
    max_flips=1000,   # Fewer flips per try
    max_tries=20,     # But more tries
    seed=42
)
```

### Example 4: Tracking Progress

```python
stats = get_walksat_stats(cnf)

# See how unsatisfied clauses decreased over time
history = stats['stats']['unsatisfied_clauses_history']
print(f"Unsatisfied over time: {history[:10]}")  # First 10 flips
```

## When to Use WalkSAT

### ✅ Good For

1. **Finding solutions quickly**
   - Don't care which solution, just need one
   - Speed is more important than completeness

2. **Large satisfiable instances**
   - Thousands or millions of variables
   - Known to be SAT or likely SAT

3. **Real-time applications**
   - Time limits require fast answers
   - Approximate solutions acceptable

4. **Optimization problems**
   - Can be adapted for MaxSAT (maximize satisfied clauses)
   - Good starting point for other algorithms

5. **Multiple solution generation**
   - Run multiple times with different seeds
   - Get diverse solutions

### ❌ Not Good For

1. **Proving unsatisfiability**
   - WalkSAT cannot prove UNSAT
   - Use DPLL or CDCL instead

2. **Small instances**
   - DPLL is fast enough and complete
   - No advantage to incompleteness

3. **Critical correctness**
   - When you absolutely need a solution
   - Cannot tolerate "no solution found"

4. **Hard UNSAT-like instances**
   - Will waste time searching
   - Better to use complete solver

## Incomplete vs Complete Solvers

### WalkSAT (Incomplete)

**Advantages**:
- Often much faster on SAT instances
- Scales to very large problems
- Simple to implement
- Can find solutions in seconds that take DPLL hours

**Disadvantages**:
- May fail to find solutions that exist
- Cannot prove UNSAT
- Non-deterministic (different results each run)
- Success depends on parameter tuning

### DPLL/CDCL (Complete)

**Advantages**:
- Always finds solution if one exists
- Can prove UNSAT
- Deterministic
- No parameter tuning needed

**Disadvantages**:
- Can be much slower on large SAT instances
- Memory intensive for some problems
- Exponential worst-case time

### Decision Guide

```
Start with DPLL if:
- Problem is small (< 100 variables)
- Need to prove UNSAT
- Need guaranteed solution
- Don't know if SAT or UNSAT

Try WalkSAT if:
- Problem is large (> 1000 variables)
- Likely to be SAT
- Speed is critical
- Can tolerate missing some solutions
- Want multiple different solutions
```

## Performance Characteristics

### Typical Performance

| Problem Size | WalkSAT Time | DPLL Time | Winner |
|--------------|--------------|-----------|--------|
| 50 vars, SAT | < 1 ms | < 10 ms | Similar |
| 500 vars, SAT | < 100 ms | 1-10 s | WalkSAT |
| 5000 vars, SAT | < 1 s | Minutes-hours | WalkSAT |
| 50 vars, UNSAT | Never proves it | < 10 ms | DPLL |

### Phase Transitions

For random 3-SAT, the hardest instances occur at the **phase transition** around clause/variable ratio ≈ 4.26:

- **Ratio < 4.26**: Easy (underconstrained, many solutions)
- **Ratio ≈ 4.26**: Hardest (on the edge of SAT/UNSAT)
- **Ratio > 4.26**: Easy to prove UNSAT

WalkSAT excels on underconstrained instances with many solutions.

## Variants and Extensions

### SKC (Original WalkSAT)

The original Selman-Kautz-Cohen version:
- Fixed noise parameter
- Our implementation follows this

### Novelty

Avoid flipping recently flipped variables:
- Track flip history
- Penalize variables flipped recently
- Reduces cycling

### Novelty+

Adaptive noise:
- Adjust p based on progress
- Increase p when stuck
- Decrease p when making progress

### Adaptive WalkSAT

- Automatically tune noise parameter
- Adjust based on problem characteristics
- Better general-purpose performance

### G²WSAT

- Use both greedy and second-best moves
- More sophisticated variable selection
- Better for hard instances

## Implementation Details

### Break Count Calculation

The "break count" for a variable is how many currently satisfied clauses would become unsatisfied if we flip it:

```python
def count_breaks(variable, assignment):
    breaks = 0
    for clause in formula:
        # Count clauses that are satisfied now but wouldn't be after flip
        if clause.is_satisfied(assignment):
            temp = assignment.copy()
            temp[variable] = not temp[variable]
            if not clause.is_satisfied(temp):
                breaks += 1
    return breaks
```

This is computed for each variable in the unsatisfied clause, and we pick the one with minimum breaks.

### Random Restarts

Multiple restarts with different initial assignments increase the probability of finding a solution:

```python
for try_num in range(max_tries):
    assignment = random_assignment()

    for flip_num in range(max_flips):
        # Local search...

    # If we get here, this try failed
    # Continue to next restart
```

## Applications

### 1. Hardware Testing

Generate test vectors quickly:
```python
# Find any input that triggers a specific behavior
test_vector = solve_walksat(hardware_constraints)
```

### 2. Configuration Problems

Find valid system configurations:
```python
# Server configuration with many constraints
config = solve_walksat(config_constraints, max_tries=50)
```

### 3. Planning

Generate action plans:
```python
# Find sequence of actions to reach goal
plan = solve_walksat(planning_formula)
```

### 4. Puzzle Solving

Sudoku, N-Queens, etc.:
```python
# Encode puzzle as SAT, find solution
solution = solve_walksat(sudoku_cnf)
```

### 5. Cryptanalysis

Finding keys/plaintexts:
```python
# Try to break cipher by finding key
key = solve_walksat(cipher_constraints)
```

## Comparison with Other Solvers

| Solver | Type | Speed | Completeness | Best For |
|--------|------|-------|--------------|----------|
| **WalkSAT** | Local Search | Fast ⚡ | Incomplete | Large SAT instances |
| **2SAT** | Graph | Very Fast ⚡⚡ | Complete | 2-SAT only |
| **Horn-SAT** | Unit Prop | Very Fast ⚡⚡ | Complete | Horn formulas |
| **XOR-SAT** | Gaussian Elim | Fast ⚡ | Complete | XOR constraints |
| **DPLL** | Systematic | Medium | Complete | General SAT |
| **CDCL** | Systematic | Medium-Fast | Complete | Large structured SAT |

## Tuning Tips

### Finding Good Noise

Try a range of values for your problem:
```python
for noise in [0.3, 0.4, 0.5, 0.6, 0.7]:
    result = solve_walksat(cnf, noise=noise)
    if result:
        print(f"Success with noise={noise}")
```

### Balancing Flips and Tries

Two strategies:
1. **Few tries, many flips**: Explore deeply from each start
2. **Many tries, few flips**: Sample many starting points

For hard problems, strategy 2 often works better.

### Using Seeds for Debugging

During development, use fixed seeds:
```python
result = solve_walksat(cnf, seed=42)  # Reproducible
```

In production, use random seeds:
```python
result = solve_walksat(cnf)  # Different every time
```

## Further Reading

### Classic Papers

1. **Selman, Kautz, Cohen (1994)**: "Noise Strategies for Improving Local Search"
   - [AAAI-94](https://www.cs.cornell.edu/selman/papers/pdf/94.aaai.walksat.pdf)
   - Original WalkSAT paper

2. **Hoos (2002)**: "An Adaptive Noise Mechanism for WalkSAT"
   - [AAAI-02](https://www.cs.ubc.ca/~hoos/Publ/HoosAAAI02.pdf)
   - Adaptive noise tuning

3. **McAllester, Selman, Kautz (1997)**: "Evidence for Invariants in Local Search"
   - Why WalkSAT works well on SAT instances

### Books

- **"Handbook of Satisfiability"** (2021): Chapter 5 - Local Search
- **"Heuristic Search"** by Pearl (1984): General local search theory

### Online Resources

- [WalkSAT Homepage](https://www.cs.rochester.edu/u/kautz/walksat/)
- [SAT Competition](http://www.satcompetition.org/) - Compare solvers

## Next Steps

- Try the [examples](../examples/example_walksat.py) to see WalkSAT in action
- Compare with [DPLL](dpll-solver.md) for completeness
- Read about [SAT theory](theory.md)
- Explore [other solvers](solvers.md)

---

**Remember**: WalkSAT is incomplete. A result of `None` doesn't mean UNSAT, just that no solution was found in the given time!
