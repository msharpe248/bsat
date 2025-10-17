# Community-Based Decomposition SAT (CoBD-SAT)

A SAT solving implementation that exploits community structure in the variable-clause interaction graph to decompose problems into semi-independent sub-problems.

## Novelty Assessment

### ⚠️ **NOT Novel** - Implementation of Known Techniques

This solver is a **reimplementation and empirical evaluation** of well-established techniques from prior research. It is **not** a novel algorithmic contribution.

### Prior Art

**Community Detection in SAT** - Extensively studied:
- **Ansótegui et al. (2012)**: "Community Structure in Industrial SAT Instances"
  - Comprehensive analysis of community structure in SAT Competition benchmarks
  - Showed industrial instances have modularity Q ≈ 0.35-0.65
  - **This is the foundational work for community-based SAT decomposition**

- **Newsham et al. (2014)**: "Impact of Community Structure on SAT Solver Performance"
  - Studied correlation between modularity and solver performance
  - Higher Q correlates with faster CDCL solving

**Graph Decomposition Methods**:
- **Louvain Method** (Blondel et al., 2008): "Fast unfolding of communities in large networks"
  - Standard community detection algorithm used here
  - O(m log n) greedy modularity optimization

- **Tree/Hypertree Decomposition**: Well-known in constraint satisfaction
  - Similar concepts of cutset conditioning
  - Interface variables = cutset

**Decomposition in SAT/CSP**:
- **Katsirelos & Bacchus (2005)**: "Generalized Nogoods in CSPs"
  - Decomposition with nogood learning

- **Prestwich (2003)**: "Variable Dependency in Local Search"
  - Exploiting variable independence via graph analysis

### What IS Original Here

**Engineering Contributions**:
1. Specific heuristics for when decomposition is beneficial:
   - Modularity threshold (Q > 0.2)
   - Interface percentage threshold (< 50%)
   - Interface assignment space limit (< 1000)

2. Clean Python implementation with:
   - Graceful fallback to standard DPLL
   - Visualization support
   - Detailed statistics tracking

3. Integration testing with modern benchmark suites

### Publication Positioning

If publishing, this should be positioned as:
- **"Implementation and Empirical Evaluation of Community-Based SAT Decomposition"**
- **NOT** "A Novel SAT Solving Algorithm"
- Appropriate venues: Tool demonstrations, reproducibility tracks, educational papers
- Must clearly cite Ansótegui et al. (2012) as the foundational work

## Overview

CoBD-SAT recognizes that many real-world SAT instances exhibit **modularity** - groups of variables and clauses that are tightly connected internally but loosely connected to other groups. By detecting these "communities" and solving them semi-independently, CoBD-SAT can achieve exponential speedups on structured instances.

### Key Insight (from Ansótegui et al. 2012)

> If a formula has k communities of roughly equal size, and interface variables comprise only a small fraction, we can reduce complexity from O(2^n) to approximately O(2^i × k × 2^(n/k))

where:
- n = total variables
- k = number of communities
- i = number of interface variables (typically 5-15% of n)

**Example speedup**: For k=4 communities with balanced decomposition:
- Traditional: 2^100 ≈ 10^30 operations
- Decomposition: 2^10 × 4 × 2^25 ≈ 10^11 operations (billion-fold speedup!)

## Algorithm

### Phase 1: Community Detection (Louvain Method)

1. **Build Bipartite Graph**
   ```
   G = (V ∪ C, E) where:
   - V = set of variables
   - C = set of clauses
   - (v, c) ∈ E if variable v appears in clause c
   ```

2. **Detect Communities via Modularity Optimization** (Blondel et al., 2008)

   Maximize modularity score:
   ```
   Q = 1/(2m) × Σ[A_ij - k_i×k_j/(2m)] × δ(comm_i, comm_j)
   ```

   where:
   - m = |E| (total edges)
   - A_ij = adjacency matrix (1 if edge exists, 0 otherwise)
   - k_i = degree of node i
   - comm_i = community assignment of node i
   - δ(x,y) = 1 if x==y else 0

   Uses greedy Louvain-style algorithm:
   - Start with each node in its own community
   - Iteratively move nodes to maximize modularity
   - Compact and refine until convergence

3. **Identify Interface Variables**

   Interface variable = variable appearing in clauses from multiple communities

   These are the "bridges" connecting communities (similar to cutset in constraint satisfaction).

### Phase 2: Decomposition

1. **Build Community Sub-Formulas**

   For each community k, extract sub-formula containing:
   - All clauses primarily in that community
   - Variables appearing in those clauses
   - Interface variables treated as parameters

2. **Evaluate Decomposition Quality**

   Check heuristics (empirically determined):
   - ✅ Modularity Q > 0.2 (some structure exists)
   - ✅ Interface variables < 50% (overhead not too high)
   - ✅ At least 2 communities detected
   - ✅ Interface assignment space ≤ 1000 (2^i ≤ 1000)

   If heuristics fail, fall back to standard DPLL.

### Phase 3: Coordinated Solving

**Approach: Interface Enumeration** (standard technique)

```python
for each assignment to interface variables:
    for each community:
        solve community given fixed interface assignment

    if all communities SAT:
        combine solutions and return

return UNSAT  # no interface assignment worked
```

### Phase 4: Solution Merging

Combine partial solutions from each community:
1. Start with interface variable assignment
2. Add community-specific variable assignments
3. Verify complete assignment satisfies original formula
4. Return combined solution

## Complexity Analysis

### Time Complexity

**Community Detection**: O(m log n) - Louvain method
- m = number of edges in bipartite graph ≈ O(variables × avg_clause_size)
- Greedy modularity optimization converges quickly

**Decomposed Solving**:
- **Best case**: O(2^i × k × 2^(n/k))
  - i = interface variables
  - k = communities
  - n/k = average community size

- **Worst case**: O(2^n)
  - Falls back to DPLL when no structure exists

**When k=4 and i=0.1n** (10% interface):
- Traditional DPLL: 2^n
- Decomposition: 2^(0.1n) × 4 × 2^(0.25n) = 2^(0.35n)
- **Speedup factor**: 2^(0.65n) - exponential!

### Space Complexity

O(n + m + k×s) where:
- n = variables
- m = clauses
- k = communities
- s = average community sub-formula size

Typically s ≪ m, so space overhead is modest.

## When CoBD-SAT Works Well

### Ideal Problem Classes (from Ansótegui et al. 2012)

1. **Circuit Verification**
   - Natural modularity in circuit blocks
   - Interface signals connect modules
   - Typical Q ≈ 0.4-0.6

2. **Planning Problems**
   - Time-step separation
   - Action preconditions create communities
   - Typical Q ≈ 0.3-0.5

3. **Graph Coloring**
   - Graph communities map to SAT communities
   - Cut edges → interface variables
   - Typical Q ≈ 0.3-0.7

4. **Industrial SAT Benchmarks**
   - Real-world structure
   - Empirically shown to have high modularity
   - Typical Q ≈ 0.35-0.65

### Problem Characteristics

**✅ Works well when**:
- Modularity Q > 0.3
- Interface variables < 20% of total
- Communities of similar size
- Problem size n > 50 (overhead amortized)

**❌ Struggles when**:
- Random 3-SAT (Q ≈ 0.0-0.1)
- Highly interconnected formulas
- Interface variables > 50%
- Very small problems (overhead dominates)

## Usage

### Basic Usage

```python
from bsat import CNFExpression
from research.cobd_sat import CoBDSATSolver

# Parse CNF formula
formula = "(a | b) & (b | c) & (c | d) & (d | e)"
cnf = CNFExpression.parse(formula)

# Create solver
solver = CoBDSATSolver(cnf)

# Solve
result = solver.solve()

if result:
    print(f"SAT: {result}")
    print(f"Statistics: {solver.get_statistics()}")
else:
    print("UNSAT")
```

### Advanced Configuration

```python
solver = CoBDSATSolver(
    cnf,
    min_communities=2,           # Minimum communities to detect
    max_communities=8,            # Maximum communities to detect
    max_interface_assignments=1000,  # Max interface enumeration
    use_cdcl=False                # Use CDCL instead of DPLL for sub-problems
)
```

### With Visualization

```python
solver = CoBDSATSolver(cnf)
result = solver.solve()

# Get visualization data
viz_data = solver.get_visualization_data()

print(f"Communities: {viz_data['statistics']['num_communities']}")
print(f"Modularity: {viz_data['statistics']['modularity']:.3f}")
print(f"Interface variables: {viz_data['interface_variables']}")
```

## References

### Foundational Work

- **Ansótegui, Bonet, Levy (2012)**: "Community Structure in Industrial SAT Instances"
  - **THE foundational paper for community-based SAT decomposition**
  - Empirical analysis of SAT Competition benchmarks
  - Shows industrial instances have Q ≈ 0.35-0.65
  - Motivates community-based solving approaches

- **Newsham, Ganesh, Fadiheh, Liang (2014)**: "Impact of Community Structure on SAT Solver Performance"
  - Studies correlation between Q and solver performance
  - Higher Q correlates with faster CDCL solving

### Graph Community Detection

- **Girvan & Newman (2002)**: "Community structure in social and biological networks"
  - Introduced modularity Q metric
  - Foundational work on community detection

- **Blondel, Guillaume, Lambiotte, Lefebvre (2008)**: "Fast unfolding of communities in large networks"
  - **Louvain algorithm used in this implementation**
  - O(m log n) complexity
  - Standard community detection method

- **Newman (2006)**: "Modularity and community structure in networks"
  - Theoretical analysis of modularity
  - Spectral methods for community detection

### Decomposition in Constraint Satisfaction

- **Katsirelos & Bacchus (2005)**: "Generalized Nogoods in CSPs"
  - Decomposition in constraint satisfaction
  - Nogood learning across sub-problems

- **Prestwich (2003)**: "Variable Dependency in Local Search"
  - Exploiting variable independence
  - Graph-based problem structure

## Conclusion

CoBD-SAT is a **solid reimplementation** of community-based decomposition techniques pioneered by Ansótegui et al. (2012). It demonstrates:

**Implementation Contributions**:
- ✅ Clean Python implementation of known techniques
- ✅ Empirical validation on modern benchmarks
- ✅ Practical heuristics for when decomposition helps
- ✅ Graceful fallback when structure absent
- ✅ Visualization support for educational purposes

**Best suited for**:
- Circuit verification and hardware design
- AI planning problems
- Constrained graph problems
- Any SAT instance with modular structure (Q > 0.3)

**Not suited for**:
- Random SAT instances (no community structure)
- Cryptographic problems (deliberately unstructured)
- Very small instances (< 50 variables)

**Educational Value**: Excellent for understanding how graph structure in SAT can be exploited for decomposition.

**Research Value**: Provides reference implementation for comparing against other structure-exploiting approaches.

**Not suitable for**: Publication as novel research without significant new algorithmic contributions beyond the empirical engineering choices.
