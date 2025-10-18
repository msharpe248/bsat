# HAS-SAT: Hierarchical Abstraction SAT Solver

**EDUCATIONAL: Abstraction-refinement for SAT solving**

Solves SAT using hierarchical abstraction with variable clustering and refinement loops.

## Citation and Attribution

### 📚 **EDUCATIONAL** - Abstraction-Refinement Concept

This implementation demonstrates abstraction-refinement applied to SAT solving.

**Related Concepts**:
- **Hierarchical Planning**: Solve high-level problem first, refine with details
- **Model Checking with Abstraction**: CEGAR (Counter-Example Guided Abstraction Refinement)
- **Hierarchical SAT**: Various approaches in SAT literature (not novel)

**This Implementation**:
- Educational demonstration of abstraction-refinement
- Variable clustering based on co-occurrence
- Multi-level solving: abstract → concrete
- Clean code showing hierarchical search concept

**NOT a Novel Contribution**: Abstraction-refinement is well-established in planning and verification. This is a teaching implementation.

---

## Overview

SAT solving typically treats all variables equally. **HAS-SAT** introduces hierarchy:

1. **Build abstraction**: Cluster related variables into "super-variables"
2. **Solve abstract**: Solve high-level problem with clusters
3. **Refine**: If SAT, refine to concrete level
4. **Verify**: Check if concrete solution satisfies original formula

### Key Insight

> Structured problems often have **variable locality** (variables appear together in clauses). Abstraction can reduce search space by treating clusters as single entities.

**Example**: Variable Clustering
```
Original variables: {a, b, c, d, e, f}

Clauses:
  (a | b)    ← a, b co-occur
  (~a | b)   ← a, b co-occur
  (c | d)    ← c, d co-occur
  (~c | d)   ← c, d co-occur
  (e | f)    ← e, f co-occur

Clustering based on co-occurrence:
  Cluster C0: {a, b}  ← appear together often
  Cluster C1: {c, d}  ← appear together often
  Cluster C2: {e, f}  ← appear together often

Abstract formula (cluster level):
  C0 & C0 & C1 & C1 & C2
  → Simplified to: C0 & C1 & C2

Solve abstract: C0=T, C1=T, C2=T
Refine to concrete: a=T, b=T, c=T, d=T, e=T, f=T
Verify: Check if satisfies original
```

---

## Algorithm

### Abstraction Building

```python
def build_abstraction(cnf):
    """
    Build variable clusters based on co-occurrence in clauses.

    1. Compute co-occurrence graph: how often pairs of variables appear together
    2. Cluster variables greedily: start with seed, add most co-occurring neighbors
    3. Create abstraction levels: high-level (clusters) → low-level (individual vars)
    """
    # Co-occurrence graph
    cooccurrence = {}
    for clause in cnf.clauses:
        for var1, var2 in pairs(clause.variables):
            cooccurrence[(var1, var2)] += 1

    # Greedy clustering
    clusters = []
    unclustered = set(cnf.variables)

    while unclustered:
        # Start new cluster
        cluster = {unclustered.pop()}

        # Grow cluster
        while len(cluster) < target_size and unclustered:
            # Find most co-occurring variable
            best_var = max(unclustered, key=lambda v: sum(
                cooccurrence[(v, c)] for c in cluster
            ))
            cluster.add(best_var)
            unclustered.remove(best_var)

        clusters.append(cluster)

    return clusters
```

### Abstraction of Clauses

```python
def abstract_clause(clause, clusters):
    """
    Map clause to abstract level using clusters.

    Example:
      Original: (a | b | ~c)
      Clusters: C0={a,b}, C1={c,d}
      Abstract: (C0 | ~C1)  ← both a,b map to C0, c maps to C1
    """
    abstract_literals = set()

    for literal in clause.literals:
        # Find cluster containing this variable
        cluster = find_cluster(literal.variable, clusters)

        # Create abstract literal with cluster variable
        abstract_lit = Literal(cluster.name, literal.polarity)
        abstract_literals.add(abstract_lit)

    # Remove duplicates
    return Clause(abstract_literals)
```

### Refinement Loop

```python
def solve_with_refinement(cnf, hierarchy):
    """
    Solve using abstraction-refinement.

    1. Solve most abstract level
    2. If UNSAT → original is UNSAT (over-approximation)
    3. If SAT → refine to next level
    4. Repeat until concrete solution found
    """
    for level in hierarchy.levels:
        # Get abstract CNF at this level
        abstract_cnf = hierarchy.abstract(cnf, level)

        # Solve abstract problem
        result = solve(abstract_cnf)

        if result is None:
            return None  # UNSAT at abstract level → UNSAT

        if is_concrete_level(level):
            # Verify solution satisfies original
            if verify(cnf, result):
                return result
            else:
                # Abstraction lost information - fall back to concrete
                return solve(cnf)

    # Solved all levels - return concrete solution
    return solve(cnf)
```

---

## Usage

### Basic Usage

```python
from bsat import CNFExpression
from research.has_sat import HASSATSolver

# Parse formula
cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

# Create solver with abstraction
solver = HASSATSolver(
    cnf,
    use_abstraction=True,
    num_levels=2  # 2 abstraction levels
)

# Solve
result = solver.solve()

if result:
    print(f"SAT: {result}")

    # Get abstraction statistics
    stats = solver.get_abstraction_statistics()
    print(f"Levels solved: {stats['levels_solved']}")
    print(f"Refinements: {stats['refinements']}")
```

### Configuration

```python
solver = HASSATSolver(
    cnf,
    use_abstraction=True,        # Enable abstraction
    num_levels=2,                 # Number of abstraction levels
    max_conflicts_per_level=10000 # Max conflicts per level
)
```

### Analyzing Abstraction Hierarchy

```python
# Get detailed abstraction statistics
abs_stats = solver.get_abstraction_statistics()

if abs_stats['enabled']:
    hierarchy = abs_stats['hierarchy']
    print(f"Total levels: {hierarchy['total_levels']}")

    for level_info in hierarchy['levels']:
        print(f"Level {level_info['level']}:")
        print(f"  Clusters: {level_info['clusters']}")
        print(f"  Abstract variables: {level_info['abstract_vars']}")

    # Refinement statistics
    if 'refinement' in abs_stats:
        ref = abs_stats['refinement']
        print(f"Refinements: {ref['refinements']}")
        print(f"Avg conflicts/level: {ref['avg_conflicts_per_level']:.1f}")
```

---

## When This Approach Works Well

**✅ Best suited for**:
- **Structured problems** with variable locality
- **Modular formulas** (variables grouped by functionality)
- **Large instances** where abstraction reduces search space
- Problems with **hierarchical structure** (e.g., circuits, planning)

**❌ Less effective when**:
- Variables are uniformly connected (no clusters)
- Very small instances (abstraction overhead not worth it)
- Random formulas (no structure to exploit)
- Abstraction loses critical information

---

## Complexity

**Time**:
- Abstraction building: O(V² + C) where V = variables, C = clauses
- Clustering: O(V² log V) greedy clustering
- Solving per level: O(CDCL complexity) on smaller abstract formula
- Overall: Can be faster if abstraction reduces search space significantly

**Space**:
- Abstraction hierarchy: O(V + C)
- Clusters: O(V)
- Typical: Minimal overhead beyond standard CDCL

---

## Comparison with Baseline

| Metric | Standard CDCL | HAS-SAT |
|--------|--------------|---------|
| **Search Space** | Full concrete space | Abstract first, refine |
| **Variable Selection** | VSIDS on all variables | Clustered at high level |
| **Structure Exploitation** | None | Uses co-occurrence patterns |
| **Overhead** | Minimal | Abstraction building |
| **Best For** | General SAT | Structured instances |

---

## Implementation Details

### Components

1. **AbstractionBuilder** (`abstraction_builder.py`):
   - `VariableCluster`: Group of related variables
   - `AbstractionLevel`: Single level in hierarchy
   - `AbstractionHierarchy`: Multi-level abstraction structure
   - Co-occurrence graph construction
   - Greedy variable clustering
   - Clause abstraction (map to cluster variables)

2. **RefinementSolver** (`refinement_solver.py`):
   - `RefinementResult`: Result of solving at one level
   - `RefinementSolver`: Abstraction-refinement loop
   - Level-by-level solving (abstract → concrete)
   - Solution verification
   - Refinement statistics

3. **HASSATSolver** (`has_solver.py`):
   - Main solver integrating abstraction and refinement
   - Extended statistics (levels, refinements)
   - Fallback to concrete solving if abstraction fails
   - Solution verification against original formula

### Metrics Tracked

- `abstraction_levels`: Number of levels in hierarchy
- `levels_solved`: How many levels were solved
- `refinements`: Number of refinement steps
- `abstract_conflicts`: Total conflicts across all abstract levels
- `clusters_per_level`: Number of clusters at each level

---

## References

### Related Work

- **Hierarchical Planning**: HTN (Hierarchical Task Network) planning uses abstraction-refinement
- **CEGAR (Model Checking)**: Counter-Example Guided Abstraction Refinement
- **Hierarchical SAT**: Various approaches in SAT literature (not a single standard approach)

### Why This Implementation Exists

**Educational Purpose**:
- Demonstrate abstraction-refinement concept
- Show how clustering can reduce search space
- Illustrate hierarchical problem solving

**NOT a Novel Contribution**:
- Abstraction-refinement is well-established
- This is a teaching tool
- For production SAT solving, use standard modern solvers

---

## Limitations

1. **Abstraction Overhead**: Building hierarchy takes time
2. **Information Loss**: Clustering can lose dependencies between variables
3. **Structure Dependence**: Only helps if problem has variable locality
4. **Verification Cost**: Must verify abstract solutions against original

**Mitigation**: HAS-SAT includes fallback to concrete solving if abstraction fails to find valid solution.

---

## Conclusion

HAS-SAT is an **educational implementation** demonstrating hierarchical abstraction-refinement for SAT solving. The implementation:

**📚 Educational**: Teaching tool for abstraction concepts
**✅ Functional**: Correctly implements clustering and refinement
**✅ Well-Documented**: Explains abstraction-refinement clearly
**❌ NOT Novel**: Established concept from planning/verification

### Key Takeaway

Abstraction-refinement solves problems at multiple levels:
1. **Abstract**: Solve high-level problem with clusters (smaller search space)
2. **Refine**: Map abstract solution to concrete level
3. **Verify**: Check if solution satisfies original formula

Works best for **structured problems** where variables cluster naturally (e.g., modular circuits, planning domains).

### When to Use

- **Research/Education**: Understanding hierarchical SAT solving
- **Structured Instances**: Problems with clear variable grouping
- **Experimentation**: Exploring abstraction strategies

### When NOT to Use

- **Production SAT Solving**: Use modern solvers (Glucose, CryptoMiniSat, Kissat)
- **Random/Unstructured**: Abstraction won't help
- **Small Instances**: Overhead not worth it

**Bottom Line**: This is a **teaching implementation** of abstraction-refinement concepts applied to SAT. For production use, stick with established modern SAT solvers.

---

## Examples

See `example.py` for working demonstrations:
- Simple SAT with abstraction
- Comparing with/without abstraction
- Multi-level abstraction hierarchies
- Detailed clustering and refinement analysis

Run examples:
```bash
python3 research/has_sat/example.py
```
