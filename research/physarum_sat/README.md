# PHYSARUM-SAT: Slime Mold-Inspired SAT Solver

## Overview

PHYSARUM-SAT solves Boolean satisfiability problems using **biological network flow dynamics** inspired by the slime mold *Physarum polycephalum*. Instead of traditional search algorithms, it treats SAT as a nutrient transport problem where:

- **Variables** are network junctions with True/False paths
- **Clauses** are food sources (nutrient sinks)
- **Flow** represents solution exploration
- **Tube reinforcement** learns optimal paths
- **Satisfying assignment** emerges from dominant flow patterns

**Novel Contribution**: First application of Physarum network optimization to SAT solving!

## Biological Foundation

### Physarum polycephalum (Slime Mold)

The slime mold is a remarkable unicellular organism that:
- Forms tube networks to transport nutrients
- **Reinforces** well-used paths (tubes thicken with flow)
- **Prunes** unused paths (tubes decay without flow)
- Solves optimization problems (shortest path, TSP)
- Exhibits **distributed intelligence** without a brain!

Famous experiment: Physarum recreated Tokyo rail network optimally!

### Tero et al. (2006) Model

Our solver is based on the Physarum network optimization model:

**Flow equation** (Poiseuille's law):
```
Q = (P_source - P_target) * D / L
```
- Q = flow rate
- P = hydraulic pressure
- D = tube diameter (conductivity)
- L = tube length

**Adaptation equation** (reinforcement learning):
```
dD/dt = |Q|^μ - γ*D
```
- μ = growth exponent (high flow → rapid thickening)
- γ = decay rate (unused tubes shrink)
- Result: Well-used paths dominate!

## SAT as Network Flow

### Network Topology

| SAT Element | Network Analog |
|-------------|----------------|
| Variable x | Junction node with T/F outflow paths |
| Clause C | Food source (nutrient sink) |
| Literal x | Path from variable to clause |
| Literal ~x | Path from variable to clause |
| Assignment | Dominant flow pattern |
| Satisfaction | Clause receives sufficient flow |

### Network Structure

```
Source (high pressure)
    ↓
Variable x (junction)
    ├─→ x_T (True path) ──→ Clauses containing x
    └─→ x_F (False path) ──→ Clauses containing ~x
                ↓
         Food sources (low pressure)
```

### How It Solves SAT

1. **Initial state**: All tubes have equal diameter
2. **Pressure gradient**: Variables=high, clauses=low (creates pull)
3. **Flow dynamics**: Flow from variables to hungry clauses
4. **Reinforcement**: Paths delivering flow thicken
5. **Competition**: T and F paths compete for dominance
6. **Convergence**: One path per variable becomes dominant
7. **Solution**: Dominant T-path → True, dominant F-path → False

## Algorithm

```
1. Build Network:
   For each variable x:
     - Create junction node
     - Create T-path node and F-path node
     - Connect junction → paths

   For each clause C:
     - Create food source node (hungry!)
     - For each literal in C:
         Connect appropriate path → clause

2. Initialize Flow:
   - Set variable pressures = 1.0 (sources)
   - Set clause pressures = 0.0 (sinks)
   - Set all tube diameters = 1.0

3. Flow Iteration (repeat max_iterations):

   a. Update Pressures:
      - Variables: P = 1.0 (source)
      - Clauses: P = 0.0 if hungry, 0.5 if fed
      - Paths: P = weighted average of neighbors

   b. Update Flows:
      For each tube:
        Q = ΔP * D / L
      For each clause:
        received_flow = Σ(inflow)

   c. Adapt Tube Diameters (Physarum learning):
      For each tube:
        growth = |Q|^μ
        decay = γ * D
        D += dt * (growth - decay)
        D = clamp(D, 0.01, 10.0)

   d. Check Satisfaction:
      If all clauses.received_flow >= threshold:
        Extract assignment from dominant flows
        If verified: return SAT

   e. Exploration (every 100 iterations):
      Randomly boost weak tubes (pseudopod extension)

4. Fallback:
   If no convergence: use CDCL solver
```

## Implementation

### Core Classes

**NetworkNode**:
```python
class NetworkNode:
    - node_id: Unique identifier
    - node_type: 'variable', 'path', 'clause'
    - pressure: Hydraulic pressure
    - inflow_edges: Incoming tubes
    - outflow_edges: Outgoing tubes
```

**NetworkEdge** (Tube):
```python
class NetworkEdge:
    - diameter: Tube conductivity (adapts!)
    - length: Fixed length
    - flow_rate: Current flow
    - compute_flow(): Q = ΔP * D / L
```

**SlimeMoldNetwork**:
```python
class SlimeMoldNetwork:
    - _build_network(): Construct from CNF
    - get_variable_nodes(): All junctions
    - get_clause_nodes(): All food sources
    - get_path_edges(var): T/F edges for variable
```

**PHYSARUMSATSolver**:
```python
class PHYSARUMSATSolver:
    - solve(): Main flow dynamics loop
    - _update_pressures(): Solve pressure field
    - _update_flows(): Compute flows
    - _update_tube_diameters(): Reinforce paths
    - _extract_assignment(): Dominant flow → assignment
    - _inject_exploration_noise(): Pseudopod extension
```

## Usage

### Basic Usage

```python
from bsat import CNFExpression
from physarum_sat import PHYSARUMSATSolver

# Create CNF formula
cnf = CNFExpression.parse("(a | b) & (~a | c)")

# Solve with slime mold network
solver = PHYSARUMSATSolver(cnf, max_iterations=1000)
result = solver.solve()

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### With Statistics

```python
solver = PHYSARUMSATSolver(cnf, max_iterations=2000)
result = solver.solve()

# Get network statistics
stats = solver.get_network_statistics()
print(f"Nodes: {stats['nodes']}")
print(f"Edges: {stats['edges']}")
print(f"Flow iterations: {stats['flow_iterations']}")
print(f"Satisfied clauses: {stats['satisfied_clauses']}")
```

### Advanced Usage

```python
# Custom dynamics parameters
solver = PHYSARUMSATSolver(
    cnf,
    max_iterations=5000,  # More iterations for hard problems
    mu=1.5,               # Flow growth exponent (reinforcement strength)
    gamma=0.5,            # Tube decay rate
    dt=0.01               # Time step size
)

# Analyze flow patterns
for var in cnf.get_variables():
    true_edge, false_edge = solver.network.get_path_edges(var)
    print(f"{var}: T-flow={true_edge.flow_rate:.3f}, "
          f"F-flow={false_edge.flow_rate:.3f}")
```

## Examples

See `example.py` for comprehensive examples:

```bash
python3 example.py
```

Examples include:
- Simple SAT formula
- Unit clauses (forced flow)
- UNSAT formula (no flow equilibrium)
- Larger problem showing network dynamics
- Comparison with CDCL solver
- Flow visualization

## Advantages

1. **Biological Inspiration**: Proven optimization strategy from nature
2. **Distributed**: No central control, emergent behavior
3. **Adaptive**: Learns optimal paths through reinforcement
4. **Robust**: Exploration prevents local optima
5. **Interpretable**: Flow patterns reveal solution structure
6. **Natural Parallelism**: Flows propagate simultaneously

## Limitations

1. **Convergence Time**: May require many iterations
2. **Parameters**: Performance sensitive to μ, γ, dt
3. **UNSAT Detection**: Hard to distinguish from slow convergence
4. **Computational Overhead**: Flow simulation adds cost
5. **No Guarantees**: May not converge (uses CDCL fallback)

## Educational Value

PHYSARUM-SAT demonstrates:
- **Bio-inspired Computation**: Learning from nature
- **Network Flow**: Hydraulic analogies and dynamics
- **Reinforcement Learning**: Path adaptation through feedback
- **Emergent Behavior**: Solution arises from local interactions
- **Distributed Systems**: No centralized decision-making

## Biological References

### Physarum Research
- **Tero et al. (2006)**: "A mathematical model for adaptive transport network in path finding by true slime mold"
- **Nakagaki et al. (2000)**: "Intelligence: Maze-solving by an amoeboid organism"
- **Tero et al. (2010)**: "Rules for biologically inspired adaptive network design" (Tokyo rail network)

### Network Optimization
- **Bonifaci et al. (2012)**: "Physarum can compute shortest paths"
- **Adamatzky (2010)**: "Physarum Machines: Computers from Slime Mould"

### Flow Dynamics
- **Poiseuille (1840)**: Law of fluid flow through tubes
- **Murray (1926)**: Murray's law for biological networks

## Future Extensions

1. **Multi-Phase Flow**: Different "nutrients" for different clause types
2. **Osmotic Pressure**: Clause urgency affects pressure gradient
3. **Tube Branching**: Allow network topology to evolve
4. **Chemotaxis**: Gradient-based exploration toward unsatisfied clauses
5. **Parallel Simulations**: Run multiple networks, share best flows

## Comparison to Other Approaches

| Aspect | PHYSARUM-SAT | CDCL | Local Search |
|--------|-------------|------|--------------|
| Paradigm | Network flow | Search tree | Variable flipping |
| Decisions | Flow dominance | Variable assignment | Random flip |
| Learning | Tube reinforcement | Clause learning | None |
| Exploration | Pseudopods | Restarts | Random walk |
| Guarantee | No (uses fallback) | Yes (complete) | No (incomplete) |
| Inspiration | Slime mold | Logic | Gradient descent |

## Performance Notes

- **Best for**: Problems with clear flow paths
- **Struggles with**: Highly conflicting constraints
- **Interesting behavior**: Tube diameters reveal variable importance
- **Typical iterations**: 100-1000 for simple, 1000-10000 for complex

## Why It Might Work

Nature has optimized network flow over millions of years:
- **Blood vessels** adapt to usage (Wolff's law)
- **Neural pathways** strengthen with use (Hebbian learning)
- **River networks** minimize energy dissipation
- **Leaf veins** optimize nutrient distribution

SAT is fundamentally about finding consistent paths through constraint space. Flow-based reinforcement might discover these paths naturally!

---

**Educational/Experimental**: This is a research implementation demonstrating bio-inspired approaches to SAT. Not intended for production use—use state-of-the-art solvers like CryptoMiniSat or Glucose for real applications.
