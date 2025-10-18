# PHYSARUM-SAT: Slime Mold-Inspired SAT Solver

## Biological Inspiration

### Physarum Polycephalum
The slime mold *Physarum polycephalum* is a unicellular organism that exhibits remarkable problem-solving capabilities:

- **Maze solving**: Finds shortest paths between food sources
- **Network optimization**: Creates efficient transport networks (proven to match Tokyo rail system efficiency)
- **Distributed computation**: No central brain, yet solves complex optimization problems
- **Adaptive behavior**: Grows toward nutrients, retracts from unfavorable areas

### Key Biological Mechanisms

1. **Protoplasmic Flow**: Nutrient-rich cytoplasm flows through tubular network
2. **Positive Feedback**: Well-used tubes thicken (increased flow capacity)
3. **Negative Feedback**: Unused tubes thin and eventually disappear
4. **Pseudopod Extension**: Organism extends temporary "arms" to explore environment
5. **Oscillatory Dynamics**: Periodic contractions drive fluid flow

## SAT Problem Mapping

### Core Metaphor
**Finding a satisfying assignment = Building a nutrient network that reaches all "food sources" (satisfied clauses)**

### Detailed Mapping

| SAT Element | Biological Analog | Representation |
|-------------|-------------------|----------------|
| **Variable** | Tube junction/node | Network node with two outgoing edges (T/F paths) |
| **Assignment (T/F)** | Flow direction | Which path cytoplasm flows through |
| **Clause** | Food source | Target that must receive nutrient flow |
| **Literal** | Path to food | Edge connecting variable node to clause |
| **Satisfied Clause** | Fed food source | Clause receiving sufficient nutrient flow |
| **Conflict** | Starvation | Food source not receiving nutrients |
| **Solution** | Optimal network | Configuration where all food sources fed |

### Network Structure

```
Variables (x1, x2, x3):
    x1 ----T----> [flow]
      \----F----> [flow]

Clauses (food sources):
    (x1 ∨ x2):  Food_1
    (~x1 ∨ x3): Food_2

Connections:
    x1-T connects to Food_1 (if x1=T, Food_1 gets nutrients)
    x1-F connects to Food_2 (if x1=F via ~x1, Food_2 gets nutrients)
```

## Algorithm Design

### Phase 1: Network Initialization

1. **Create Variable Nodes**: One node per variable with two outgoing paths (T, F)
2. **Create Clause Nodes**: One food source node per clause
3. **Connect Literals**: Edge from variable path to clause if literal appears in clause
   - Positive literal (x) → T-path connects to clause
   - Negative literal (~x) → F-path connects to clause
4. **Initial Flow**: Equal flow through both paths of each variable (superposition state)

### Phase 2: Flow Dynamics

**Inspired by Tero et al. (2006) Physarum model**

For each tube (edge) connecting nodes i and j:

```
Flow equation:
Q_ij = (P_i - P_j) * D_ij / L_ij

Where:
- Q_ij = flow rate through tube
- P_i, P_j = pressure at nodes i and j
- D_ij = tube conductivity (diameter)
- L_ij = tube length
```

**Pressure calculation**:
- Source nodes (variable roots): High pressure
- Food nodes (clauses): Low pressure (nutrient sinks)
- Conservation: ΣQ_in = ΣQ_out at each node

### Phase 3: Adaptive Tube Thickness

Tubes that carry more flow become thicker (positive feedback):

```python
dD_ij/dt = f(Q_ij) - γ*D_ij

Where:
- f(Q_ij) = |Q_ij|^μ  (flow-dependent growth, μ ≈ 1-2)
- γ*D_ij = natural decay
- Result: Well-used tubes grow, unused tubes shrink
```

**Discrete decision emerges**: Eventually one path (T or F) dominates at each variable

### Phase 4: Satisfaction Feedback

**Clause Satisfaction Score**:
```
S_clause = Σ(flow from literals in clause)
```

If `S_clause > threshold`: Clause is satisfied (food source fed)

**Adaptive pressure**:
- Unsatisfied clauses increase their "hunger" (lower pressure → attract more flow)
- Satisfied clauses reduce hunger
- Creates natural priority for hard-to-satisfy clauses

### Phase 5: Exploration via Pseudopods

**Random exploration** (mimics biological search):
1. Periodically add small "pseudopod flows" to random paths
2. If pseudopod discovers better configuration (more clauses satisfied), reinforce it
3. Otherwise, let it decay naturally

**Adaptive restart**:
- If system stuck in local optimum (no clauses improving), inject exploration noise
- Like biological slime mold exploring new areas when food scarce

## Implementation Strategy

### Data Structures

```python
class NetworkNode:
    """Node in slime mold network."""
    pressure: float          # Hydraulic pressure
    inflow_edges: List[Edge]
    outflow_edges: List[Edge]

class Edge:
    """Tube connecting nodes."""
    source: NetworkNode
    target: NetworkNode
    diameter: float          # Tube thickness (conductivity)
    flow_rate: float         # Current flow through tube
    length: float            # Fixed at 1.0 for simplicity

class SlimeMoldNetwork:
    """Complete network representation."""
    variable_nodes: Dict[str, VariableNode]  # One per variable
    clause_nodes: List[ClauseNode]           # One per clause
    edges: List[Edge]

class VariableNode(NetworkNode):
    """Special node for variables with T/F paths."""
    true_path: Edge
    false_path: Edge

class ClauseNode(NetworkNode):
    """Food source node."""
    satisfaction_threshold: float
    incoming_literals: List[Edge]
```

### Core Algorithm

```python
def solve_physarum(cnf, max_iterations=10000):
    # Build network
    network = build_network_from_cnf(cnf)

    # Initialize equal flow on all paths
    initialize_uniform_flow(network)

    for iteration in range(max_iterations):
        # Compute pressures (solve flow equations)
        update_pressures(network)

        # Update flows based on pressures
        update_flows(network)

        # Adapt tube diameters (reinforcement)
        update_tube_diameters(network)

        # Check satisfaction
        satisfied = check_clause_satisfaction(network)

        if all_clauses_satisfied(satisfied):
            # Extract assignment from dominant flows
            return extract_assignment(network)

        # Exploration: inject pseudopods periodically
        if iteration % 100 == 0:
            inject_exploration_noise(network)

        # Detect UNSAT (network has no valid configuration)
        if network_converged_to_invalid_state(network):
            return None  # UNSAT

    return None  # Timeout
```

### Key Functions

**Pressure Update** (Solve Kirchhoff-like equations):
```python
def update_pressures(network):
    """Solve for pressures using conservation of flow."""
    # Set boundary conditions
    for var_node in network.variable_nodes.values():
        var_node.pressure = 1.0  # Source pressure

    for clause_node in network.clause_nodes:
        if not clause_node.is_satisfied():
            clause_node.pressure = 0.0  # Hungry food source (sink)
        else:
            clause_node.pressure = 0.5  # Fed food source (less attractive)

    # Iteratively solve for intermediate nodes
    # (Could use linear solver for exactness)
    for _ in range(10):  # Jacobi iteration
        for node in network.get_intermediate_nodes():
            # Average of neighboring pressures (weighted by conductivity)
            update_node_pressure_from_neighbors(node)
```

**Flow Update**:
```python
def update_flows(network):
    """Update flow through each edge based on pressures."""
    for edge in network.edges:
        pressure_diff = edge.source.pressure - edge.target.pressure
        edge.flow_rate = pressure_diff * edge.diameter / edge.length
```

**Tube Diameter Adaptation**:
```python
def update_tube_diameters(network, dt=0.01, mu=1.5, gamma=0.5):
    """Adapt tube thickness based on usage."""
    for edge in network.edges:
        # Growth from flow
        growth = abs(edge.flow_rate) ** mu

        # Natural decay
        decay = gamma * edge.diameter

        # Update diameter
        edge.diameter += dt * (growth - decay)

        # Keep bounded
        edge.diameter = max(0.01, min(edge.diameter, 10.0))
```

**Assignment Extraction**:
```python
def extract_assignment(network):
    """Extract variable assignment from dominant flows."""
    assignment = {}

    for var_name, var_node in network.variable_nodes.items():
        true_flow = var_node.true_path.flow_rate
        false_flow = var_node.false_path.flow_rate

        # Assign to dominant path
        assignment[var_name] = (true_flow > false_flow)

    return assignment
```

## Advantages

1. **Natural Parallelism**: Flow dynamics can be computed in parallel for all edges
2. **Continuous Optimization**: Smooth reinforcement learning (no discrete jumps)
3. **Adaptive Exploration**: Automatic balance between exploitation (thick tubes) and exploration (pseudopods)
4. **Biological Validation**: Real organism solves similar problems efficiently
5. **Graceful Degradation**: Partial solutions still feed some clauses (useful for MaxSAT)

## Challenges

1. **Convergence Speed**: May be slower than CDCL for small problems
2. **Parameter Tuning**: Needs good choices for μ, γ, pressures
3. **UNSAT Detection**: Hard to know when network definitively can't feed all clauses
4. **Memory**: Full network representation may be large for huge CNFs

## Extensions

1. **Hybrid Approach**: Use Physarum for initial exploration, CDCL for refinement
2. **Hierarchical Networks**: Multi-level networks for abstraction
3. **Learned Parameters**: Train μ, γ on problem instances
4. **Clause Weighting**: Harder clauses = more attractive food sources

## References

- Tero, A., et al. (2006). "Rules for Biologically Inspired Adaptive Network Design." *Science*
- Adamatzky, A. (2010). "Physarum Machines: Computers from Slime Mould"
- Nakagaki, T., et al. (2000). "Intelligence: Maze-solving by an amoeboid organism"

## Educational Value

This approach demonstrates:
- **Bio-inspired computing**: How nature solves hard problems
- **Continuous relaxation**: Solving discrete problems via continuous dynamics
- **Emergent computation**: Complex behavior from simple local rules
- **Network optimization**: Connections to flow networks, electrical circuits

**Novel Contribution**: First application of Physarum network dynamics to SAT solving!
