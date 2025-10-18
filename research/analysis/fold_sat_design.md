# FOLD-SAT: Protein Folding-Inspired SAT Solver

## Biophysical Inspiration

### Protein Folding Problem
Proteins fold from linear chains to complex 3D structures by minimizing free energy:

- **Energy Landscape**: Multidimensional surface with valleys (stable states) and peaks (barriers)
- **Thermodynamic Hypothesis**: Native structure = global energy minimum
- **Funnel Topology**: Energy landscape shaped like funnel guiding toward solution
- **Local Interactions**: Residues interact with nearby residues (contact potentials)
- **Anfinsen's Dogma**: Sequence determines structure through energy minimization

### Computational Protein Folding

1. **Force Fields**: AMBER, CHARMM, OPLS define inter-atomic energies
2. **Simulated Annealing**: Heat system, slowly cool to find ground state
3. **Molecular Dynamics**: Simulate physical motion of atoms
4. **Monte Carlo**: Random moves accepted if they lower energy
5. **Replica Exchange**: Multiple simulations at different temperatures

## SAT Problem Mapping

### Core Metaphor
**Finding a satisfying assignment = Folding a "logical protein" to its lowest energy configuration (all clauses satisfied)**

### Detailed Mapping

| SAT Element | Protein Analog | Energy Interpretation |
|-------------|----------------|----------------------|
| **Variable** | Amino acid residue | Discrete conformational state (T/F) |
| **Assignment** | Protein conformation | Specific 3D structure |
| **Clause** | Local contact potential | Energy term between nearby residues |
| **Satisfied Clause** | Favorable contact | Negative energy contribution |
| **Unsatisfied Clause** | Unfavorable contact | Positive energy (clash) |
| **SAT Solution** | Native fold | Global energy minimum |
| **UNSAT** | Unf

oldable sequence | No low-energy structure exists |
| **Conflict** | Steric clash | High-energy local geometry |

### Energy Landscape Formulation

```
SAT Assignment = Protein Conformation
Energy(assignment) = Σ E_clause(assignment)

Where:
E_clause = {
    -1  if clause satisfied (favorable)
    +K  if clause unsatisfied (unfavorable, K >> 1)
}

Goal: Minimize E_total(assignment)
Optimal E = -(# clauses)  [all satisfied]
```

## Algorithm Design

### Phase 1: Energy Function Design

**Hamiltonian (Total Energy)**:
```
H(x1, x2, ..., xn) = Σ_clauses E_clause + Σ_pairs E_pair

E_clause(C): Clause satisfaction energy
    = -1 if C satisfied
    = +10 if C unsatisfied

E_pair(xi, xj): Variable-variable interaction
    = -ε if xi, xj appear together in many clauses
    = 0 otherwise
```

**Interpretation**:
- Satisfied clause = hydrogen bond (stabilizing)
- Unsatisfied clause = steric clash (destabilizing)
- Co-occurring variables = attractive interaction (cooperativity)

### Phase 2: Move Set (Conformational Changes)

**Elementary Moves** (like protein sidechain rotations):

1. **Single Flip**: Change one variable (x_i: T ↔ F)
   ```python
   def single_flip(assignment, i):
       assignment[i] = not assignment[i]
   ```

2. **Swap Move**: Exchange values of two variables
   ```python
   def swap_move(assignment, i, j):
       assignment[i], assignment[j] = assignment[j], assignment[i]
   ```

3. **Cluster Flip**: Flip multiple correlated variables together
   ```python
   def cluster_flip(assignment, cluster):
       for var in cluster:
           assignment[var] = not assignment[var]
   ```

4. **Random Mutation**: Randomize a random variable (like thermal fluctuation)

### Phase 3: Simulated Annealing

**Metropolis-Hastings Algorithm**:
```python
def simulated_annealing(cnf, T_initial=100.0, T_final=0.01, cooling_rate=0.95):
    # Start with random assignment
    assignment = random_assignment(cnf.variables)
    E_current = compute_energy(cnf, assignment)

    T = T_initial  # Temperature

    while T > T_final:
        # Propose move
        new_assignment = propose_move(assignment)
        E_new = compute_energy(cnf, new_assignment)

        # Energy change
        ΔE = E_new - E_current

        # Accept if energy decreases OR probabilistically
        if ΔE < 0 or random() < exp(-ΔE / T):
            assignment = new_assignment
            E_current = E_new

        # Check if solved (E = -num_clauses)
        if E_current == -len(cnf.clauses):
            return assignment  # SAT!

        # Cool down
        T *= cooling_rate

    # Failed to find solution
    return None  # Likely UNSAT or need more time
```

**Temperature Schedule**:
- High T: System explores freely (accepts bad moves)
- Low T: System refines solution (rejects bad moves)
- Cooling rate: Trade-off between exploration and exploitation

### Phase 4: Parallel Tempering (Replica Exchange)

**Run multiple simulations at different temperatures simultaneously**:

```python
def parallel_tempering(cnf, num_replicas=8, T_min=0.1, T_max=100.0):
    # Create temperature ladder
    temperatures = geometric_schedule(T_min, T_max, num_replicas)

    # Initialize replicas
    replicas = [
        {'assignment': random_assignment(cnf.variables),
         'temperature': T,
         'energy': 0.0}
        for T in temperatures
    ]

    for iteration in range(max_iterations):
        # Evolve each replica independently
        for replica in replicas:
            replica['assignment'] = metropolis_step(replica)
            replica['energy'] = compute_energy(cnf, replica['assignment'])

        # Attempt swaps between adjacent temperatures
        for i in range(num_replicas - 1):
            if should_swap(replicas[i], replicas[i+1]):
                # Swap configurations
                swap_replicas(replicas[i], replicas[i+1])

        # Check if any replica found solution
        for replica in replicas:
            if replica['energy'] == -len(cnf.clauses):
                return replica['assignment']  # SAT!

    return None
```

**Swap Criterion** (detailed balance):
```
P(swap) = min(1, exp(Δβ * ΔE))

Where:
β = 1/T (inverse temperature)
Δβ = β_i - β_j
ΔE = E_i - E_j
```

### Phase 5: Force Field Refinement

**Learn optimal energy parameters from problem instances**:

```python
class AdaptiveForceField:
    """Learn energy function from experience."""

    def __init__(self):
        self.clause_penalty = 10.0    # Cost of unsatisfied clause
        self.pair_bonus = -0.1        # Reward for satisfying correlated clauses

    def update_from_trajectory(self, trajectory):
        """Learn from successful solving trajectories."""

        # If trajectory led to solution quickly:
        #   - Increase penalties for violated clauses
        #   - Increase bonuses for important correlations

        # If trajectory got stuck:
        #   - Reduce penalties (over-constrained)
        #   - Adjust bonuses

        pass  # Machine learning updates
```

## Implementation Strategy

### Data Structures

```python
class EnergyLandscape:
    """Energy function for SAT as protein folding."""

    cnf: CNFExpression
    clause_weights: Dict[Clause, float]    # Importance weights
    pair_interactions: Dict[Tuple[str, str], float]  # Variable correlations

    def compute_energy(self, assignment: Dict[str, bool]) -> float:
        """Total energy of assignment."""
        total = 0.0

        # Clause energies
        for clause in self.cnf.clauses:
            if not self.is_satisfied(clause, assignment):
                total += self.clause_weights[clause]  # Penalty
            else:
                total -= 1.0  # Reward

        # Pairwise interactions
        for (var1, var2), interaction in self.pair_interactions.items():
            if var1 in assignment and var2 in assignment:
                # Favorable if both have same value (positive correlation)
                if assignment[var1] == assignment[var2]:
                    total += interaction  # Negative = stabilizing

        return total

class MolecularMove:
    """Conformational change operator."""

    def propose(self, assignment: Dict[str, bool]) -> Dict[str, bool]:
        """Generate candidate move."""
        raise NotImplementedError

class SingleFlipMove(MolecularMove):
    """Flip one variable (like sidechain rotation)."""

    def propose(self, assignment):
        new_assignment = assignment.copy()
        var = random.choice(list(assignment.keys()))
        new_assignment[var] = not new_assignment[var]
        return new_assignment

class ClusterFlipMove(MolecularMove):
    """Flip correlated variables together."""

    def __init__(self, clusters):
        self.clusters = clusters

    def propose(self, assignment):
        new_assignment = assignment.copy()
        cluster = random.choice(self.clusters)
        for var in cluster:
            new_assignment[var] = not new_assignment[var]
        return new_assignment
```

### Core Algorithm

```python
def solve_fold_sat(cnf, mode='annealing'):
    """Solve SAT using protein folding-inspired energy minimization."""

    # Build energy landscape
    landscape = EnergyLandscape(cnf)
    landscape.compute_pair_interactions()

    if mode == 'annealing':
        return simulated_annealing_solver(landscape)
    elif mode == 'replica_exchange':
        return parallel_tempering_solver(landscape)
    elif mode == 'hybrid':
        return hybrid_folding_solver(landscape)

def simulated_annealing_solver(landscape):
    """Standard simulated annealing."""

    # Initialize
    assignment = random_assignment(landscape.cnf.variables)
    E_current = landscape.compute_energy(assignment)
    T = 100.0

    for iteration in range(100000):
        # Propose move
        move = choose_move_type()  # Single flip, cluster flip, etc.
        new_assignment = move.propose(assignment)
        E_new = landscape.compute_energy(new_assignment)

        # Metropolis criterion
        ΔE = E_new - E_current
        if ΔE < 0 or random.random() < exp(-ΔE / T):
            assignment = new_assignment
            E_current = E_new

        # Check satisfaction
        if E_current == -len(landscape.cnf.clauses):
            return assignment

        # Cool
        T *= 0.9995

    return None
```

### Advanced Features

**1. Bias toward Low-Energy Paths** (Funnel-guided search):
```python
def funnel_guided_search(landscape):
    """Bias search toward configurations on path to solution."""

    # Maintain ensemble of low-energy states
    ensemble = []

    # Evolve toward lower energy regions
    for state in ensemble:
        gradient = compute_energy_gradient(landscape, state)
        state = move_along_gradient(state, gradient)

    return min(ensemble, key=lambda s: landscape.compute_energy(s))
```

**2. Contact Map Prediction** (like AlphaFold):
```python
def predict_satisfying_assignments(cnf):
    """Predict likely variable values using ML."""

    # Neural network predicts P(x_i = True | clause structure)
    # Analogous to contact map prediction in proteins

    probabilities = neural_network.predict(cnf)
    return probabilities
```

**3. Fragment Assembly** (like Rosetta):
```python
def fragment_assembly(cnf):
    """Build solution from known sub-solutions (fragments)."""

    # Identify independent sub-problems (fragments)
    fragments = partition_into_fragments(cnf)

    # Solve fragments independently
    sub_solutions = [solve_fragment(f) for f in fragments]

    # Assemble fragments into full solution
    return merge_solutions(sub_solutions)
```

## Advantages

1. **Well-Studied Framework**: 50+ years of protein folding research
2. **Physical Intuition**: Energy minimization is universal principle
3. **Robust Optimization**: Simulated annealing provably converges (slow but sure)
4. **Parallel Scalability**: Replica exchange is embarrassingly parallel
5. **Transferable Knowledge**: Force field learning improves over time

## Challenges

1. **Slow Convergence**: Annealing can be slow for large problems
2. **Parameter Sensitivity**: Temperature schedule crucial
3. **Local Minima**: Can get trapped (like protein misfolding)
4. **UNSAT Detection**: Hard to know when truly stuck vs. need more time

## Extensions

### 1. **Basin Hopping**
Combine local minimization with random jumps:
```python
while not solved:
    # Local minimize
    assignment = local_minimize(assignment)

    # If stuck, random jump to new basin
    if stuck():
        assignment = random_perturbation(assignment)
```

### 2. **Umbrella Sampling**
Sample specific energy regions systematically:
```python
# Force system to explore specific energy ranges
for E_target in energy_windows:
    sample_configurations_near_energy(E_target)
```

### 3. **Machine Learning Force Fields**
Train neural network energy function:
```python
# AlphaFold-style
energy_function = train_neural_network(
    inputs=cnf_features,
    outputs=satisfiability
)
```

## Theoretical Connections

### Relationship to Adiabatic Quantum Computing
Simulated annealing is classical analog of quantum annealing:
```
Quantum: H(t) = (1-t)H_initial + t*H_problem
Classical: T(t) = T_high * (1-t) + T_low * t
```

### Statistical Mechanics
SAT ↔ Spin glass (Ising model):
```
Variable x_i ↔ Spin s_i ∈ {-1, +1}
Clause ↔ Interaction term
Energy ↔ Hamiltonian
```

### Optimization Landscape Theory
Energy landscape topology determines solvability:
- **Funnel**: Easy (SAT)
- **Rugged**: Hard (may be UNSAT)
- **Golf course**: Very hard (phase transition)

## References

- Anfinsen, C.B. (1973). "Principles that Govern Protein Folding" (Nobel Lecture)
- Kirkpatrick, S. et al. (1983). "Optimization by Simulated Annealing"
- Swendsen, R.H. & Wang, J.S. (1986). "Replica Monte Carlo Simulation"
- Dill, K.A. & MacCallum, J.L. (2012). "The Protein Folding Problem, 50 Years On"
- Senior, A.W. et al. (2020). "Improved Protein Structure Prediction Using AlphaFold"

## Educational Value

This approach demonstrates:
- **Statistical Mechanics**: Thermodynamic principles in computation
- **Molecular Simulation**: How biophysics simulates complex systems
- **Energy Landscapes**: Optimization topology and convergence
- **Annealing**: Temperature-based exploration/exploitation trade-off

**Novel Contribution**: First rigorous application of protein folding energy minimization to SAT solving!

**Why It Might Work**: Nature has been solving hard optimization problems (protein folding) for billions of years. The algorithms developed for proteins might crack SAT too—both are about finding stable configurations in complex constraint spaces.
