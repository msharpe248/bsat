# FOLD-SAT: Protein Folding-Inspired SAT Solver

## Overview

FOLD-SAT solves Boolean satisfiability problems using **energy minimization** inspired by protein folding. Instead of traditional search algorithms, it treats SAT as a thermodynamic problem where:

- **Variables** are amino acid residues with two conformations (T/F)
- **Assignments** are protein conformations
- **Clauses** are local contact energies
- **Satisfied clauses** contribute favorable (negative) energy
- **Unsatisfied clauses** contribute unfavorable (positive) energy
- **SAT solution** emerges as global energy minimum (ground state)

**Novel Contribution**: First rigorous application of protein folding energy minimization and simulated annealing to SAT solving!

## Biophysical Foundation

### Protein Folding Problem

Proteins are linear chains of amino acids that fold into complex 3D structures:

**Anfinsen's Thermodynamic Hypothesis** (Nobel Prize, 1972):
> "The native structure of a protein is the one that minimizes its free energy."

Key principles:
- **Energy Landscape**: Multidimensional surface with valleys (stable states) and peaks (barriers)
- **Funnel Topology**: Energy gradually decreases toward native structure
- **Local Interactions**: Residues form contacts (hydrogen bonds, hydrophobic interactions)
- **Global Minimum**: Native fold has lowest energy

### Computational Protein Folding

Methods used to simulate protein folding:

1. **Force Fields** (AMBER, CHARMM, OPLS):
   - Define inter-atomic energy functions
   - Sum of terms: bonds, angles, torsions, van der Waals, electrostatics

2. **Simulated Annealing**:
   - Heat system to high temperature (random exploration)
   - Slowly cool to low temperature (refinement)
   - Converges to low-energy states

3. **Monte Carlo Simulation**:
   - Propose random conformational changes
   - Accept based on Metropolis criterion
   - Samples Boltzmann distribution

4. **Molecular Dynamics**:
   - Simulate Newtonian motion of atoms
   - Integrate equations of motion
   - Explore energy landscape physically

5. **Replica Exchange** (Parallel Tempering):
   - Run multiple simulations at different temperatures
   - Periodically swap configurations
   - Overcome energy barriers

## SAT as Protein Folding

### Energy Mapping

| SAT Element | Protein Analog | Energy Contribution |
|-------------|----------------|-------------------|
| Variable | Amino acid residue | Two conformations (T/F) |
| Assignment | Protein conformation | Complete 3D structure |
| Clause | Contact energy | Interaction between residues |
| Satisfied clause | Favorable contact | -1 kcal/mol (stabilizing) |
| Unsatisfied clause | Unfavorable clash | +10 kcal/mol (destabilizing) |
| Unit clause | Critical contact | +100 kcal/mol penalty |
| SAT solution | Native fold | Global energy minimum |
| UNSAT | Unfoldable sequence | No low-energy state |

### Energy Function (Hamiltonian)

```
H(x₁, x₂, ..., xₙ) = Σ E_clause + Σ E_pair

E_clause(C) = {
    -1   if C satisfied (favorable contact)
    +10  if C unsatisfied (steric clash)
    +100 if C is unit clause unsatisfied
}

E_pair(xᵢ, xⱼ) = {
    -ε  if xᵢ, xⱼ co-occur and aligned
    +ε  if xᵢ, xⱼ co-occur and misaligned
}

Goal: Minimize H(x₁, ..., xₙ)
Optimal: H = -|clauses| (all satisfied)
```

### Why This Mapping Works

1. **Optimization Problem**: Both SAT and folding seek optimal configurations
2. **Local Interactions**: Clauses are like contacts (affect nearby variables)
3. **Energy Minimization**: Nature solves hard optimization via thermodynamics
4. **Robust Algorithm**: Simulated annealing has theoretical convergence guarantees
5. **Proven Success**: Folding algorithms work for proteins, might work for SAT!

## Algorithm

### Simulated Annealing (Standard Mode)

```
1. Initialize:
   - Create random assignment (unfolded state)
   - Set temperature T = T_initial (high, ~100K)
   - Compute initial energy E

2. Annealing Loop (repeat N iterations):

   a. Propose Move:
      - Choose move type (flip, cluster, swap, etc.)
      - Generate new assignment

   b. Compute Energy:
      - E_new = compute_energy(new_assignment)
      - ΔE = E_new - E_current

   c. Metropolis Acceptance:
      If ΔE < 0:
         Accept move (always)
      Else:
         Accept with probability P = exp(-ΔE / T)

   d. Update State:
      If accepted:
         assignment = new_assignment
         E_current = E_new

   e. Check Ground State:
      If E_current == -|clauses|:
         Return assignment (SAT!)

   f. Cool Down:
      T = T * cooling_rate (e.g., 0.9995)

3. Result:
   If ground state reached: SAT
   Else: Likely UNSAT or need more time
```

### Parallel Tempering (Advanced Mode)

```
1. Initialize Replicas:
   - Create N replicas at different temperatures
   - T_ladder = [T_min, ..., T_max] (geometric spacing)
   - Each replica: random initial assignment

2. Evolution:
   For each iteration:
      a. Evolve Each Replica:
         For each replica r:
            - Propose move
            - Accept via Metropolis at T_r

      b. Attempt Replica Swaps (every M steps):
         For adjacent replicas (i, i+1):
            - Compute swap probability
            - P(swap) = min(1, exp(Δβ * ΔE))
            - If accepted: exchange configurations

      c. Check All Replicas:
         If any replica at ground state: SAT!

3. Result:
   Return best replica or UNSAT
```

### Move Operators (Conformational Changes)

**1. Single Flip** (most common):
```python
# Flip one variable (like sidechain rotation)
new_assignment[var] = not old_assignment[var]
```

**2. Cluster Flip**:
```python
# Flip correlated variables together (like domain movement)
for var in cluster:
    new_assignment[var] = not old_assignment[var]
```

**3. Swap Move**:
```python
# Exchange values of two variables
new_assignment[i], new_assignment[j] = old_assignment[j], old_assignment[i]
```

**4. Biased Flip**:
```python
# Flip variable from unsatisfied clause (guided search)
unsat_clauses = find_unsatisfied(assignment)
var = random_variable_from(random.choice(unsat_clauses))
new_assignment[var] = not old_assignment[var]
```

**5. Random Mutation**:
```python
# Randomize multiple variables (thermal noise)
for _ in range(num_mutations):
    var = random.choice(variables)
    new_assignment[var] = random.choice([True, False])
```

## Implementation

### Core Classes

**EnergyLandscape**:
```python
class EnergyLandscape:
    - compute_energy(assignment): Total energy
    - compute_delta_energy(var, new_value): Efficient incremental update
    - is_ground_state(assignment): Check if all satisfied
    - get_num_unsatisfied(assignment): Count violations
```

**MolecularMove**:
```python
class MolecularMove:
    - propose(assignment): Generate new assignment

class MoveSelector:
    - select_move(temperature): Choose move type
    - Temperature-adaptive: exploration (high T) vs refinement (low T)
```

**AnnealingSchedule**:
```python
class AnnealingSchedule:
    - get_temperature(): Current temperature
    - step(): Advance schedule (cool down)
    - Types: geometric, linear, logarithmic, adaptive
```

**ParallelTempering**:
```python
class ParallelTempering:
    - replicas: List of simulations at different T
    - attempt_swap(i, j): Exchange configurations
    - get_best_replica(): Lowest energy
```

**FOLDSATSolver**:
```python
class FOLDSATSolver:
    - solve(): Main algorithm
    - _solve_simulated_annealing(): Standard mode
    - _solve_parallel_tempering(): Advanced mode
    - get_energy_statistics(): Performance metrics
```

## Usage

### Basic Usage

```python
from bsat import CNFExpression
from fold_sat import FOLDSATSolver

# Create CNF formula
cnf = CNFExpression.parse("(a | b) & (~a | c)")

# Solve with simulated annealing
solver = FOLDSATSolver(cnf, max_iterations=10000)
result = solver.solve()

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

### With Statistics

```python
solver = FOLDSATSolver(cnf, max_iterations=20000)
result = solver.solve()

# Get energy and annealing statistics
stats = solver.get_energy_statistics()
print(f"Iterations: {stats['annealing_iterations']}")
print(f"Acceptance rate: {stats['acceptance_rate']:.2%}")
print(f"Final energy: {stats['final_energy']:.2f}")
print(f"Ground state: {stats['ground_state_energy']:.2f}")
print(f"Reached ground state: {stats['reached_ground_state']}")
```

### Advanced Usage

```python
# Custom annealing parameters
solver = FOLDSATSolver(
    cnf,
    max_iterations=100000,  # More iterations for hard problems
    T_initial=100.0,        # High initial temperature
    T_final=0.01,           # Low final temperature
    cooling_rate=0.9995,    # Slow cooling
    mode='annealing'        # or 'parallel_tempering'
)

# Parallel tempering mode
solver = FOLDSATSolver(
    cnf,
    max_iterations=50000,
    mode='parallel_tempering'  # 8 replicas at different temperatures
)
result = solver.solve()
```

### Energy Landscape Analysis

```python
from fold_sat import EnergyLandscape

landscape = EnergyLandscape(cnf)

# Analyze energy
assignment = {'a': True, 'b': False, 'c': True}
energy = landscape.compute_energy(assignment)
num_unsat = landscape.get_num_unsatisfied(assignment)

print(f"Energy: {energy:.2f}")
print(f"Unsatisfied: {num_unsat}")
print(f"Ground state: {landscape.get_ground_state_energy():.2f}")

# Sample landscape
distribution = landscape.get_energy_distribution(num_samples=1000)
for energy_level, count in distribution.items():
    print(f"E={energy_level:+4.0f}: {'█' * (count // 10)}")
```

## Examples

See `example.py` for comprehensive examples:

```bash
python3 example.py
```

Examples include:
- Simple SAT formula
- Unit clauses (high energy penalty)
- UNSAT formula (no ground state)
- Larger problem showing annealing dynamics
- Comparison with CDCL solver
- Parallel tempering mode
- Energy landscape analysis

## Advantages

1. **Theoretical Foundation**: Grounded in statistical mechanics and thermodynamics
2. **Provable Convergence**: Simulated annealing converges to global minimum (given infinite time)
3. **Robust Exploration**: Temperature controls exploration/exploitation trade-off
4. **Natural Priorities**: Unit clauses automatically get high energy penalties
5. **Interpretable**: Energy reveals problem structure
6. **Parallel Scalability**: Replica exchange is embarrassingly parallel
7. **Physically Motivated**: Energy minimization is universal optimization principle

## Limitations

1. **Slow Convergence**: Annealing requires many iterations
2. **Parameter Sensitivity**: Performance depends on temperature schedule
3. **No Completeness Guarantee**: May not find solution in finite time
4. **UNSAT Detection**: Hard to distinguish no-solution from need-more-time
5. **Local Minima**: Can get trapped (like protein misfolding)

## Educational Value

FOLD-SAT demonstrates:
- **Statistical Mechanics**: Thermodynamic principles in computation
- **Simulated Annealing**: Classic optimization meta-heuristic
- **Energy Landscapes**: How topology affects solvability
- **Biophysical Simulation**: How proteins fold via energy minimization
- **Metropolis Algorithm**: Foundation of Monte Carlo methods

## Theoretical Connections

### Statistical Mechanics

SAT is equivalent to **spin glass** problem:

```
Variable xᵢ ↔ Spin sᵢ ∈ {↑, ↓}
Clause ↔ Interaction term Jᵢⱼ
Energy ↔ Ising Hamiltonian H = -Σ Jᵢⱼ sᵢ sⱼ
```

### Adiabatic Quantum Computing

Simulated annealing is classical analog of quantum annealing:

```
Quantum: H(t) = (1-t)H₀ + t·H_problem
Classical: T(t) = T_high·(1-t) + T_low·t
```

### Metropolis-Hastings Algorithm

Core of Bayesian inference and MCMC sampling:

```
P(accept) = min(1, exp(-ΔE / T))

Samples Boltzmann distribution: P(state) ∝ exp(-E/T)
```

### Landscape Topology

Energy landscape shape determines difficulty:

- **Funnel**: Smooth path to solution (Easy SAT)
- **Rugged**: Many local minima (Hard SAT)
- **Golf Course**: Flat with few deep holes (UNSAT)

## Performance Notes

- **Best for**: Problems with smooth energy landscapes
- **Struggles with**: Highly conflicting constraints, phase transition region
- **Typical iterations**: 10,000-100,000 (depends on problem size and difficulty)
- **Temperature tuning**: Critical for performance
  - Too fast cooling: Trapped in local minima
  - Too slow cooling: Wastes time exploring
  - Rule of thumb: T_initial × 0.9995^iterations ≈ T_final

## Comparison to Other Approaches

| Aspect | FOLD-SAT | CDCL | Local Search | PHYSARUM | MARKET |
|--------|---------|------|--------------|----------|---------|
| Paradigm | Energy minimization | Tree search | Hill climbing | Network flow | Economic equilibrium |
| Decisions | Metropolis | VSIDS | Greedy | Flow dominance | Bidding |
| Learning | Temperature schedule | Clause learning | None | Tube reinforcement | Price adjustment |
| Guarantee | No (incomplete) | Yes (complete) | No | No | No |
| Parallelism | High (replica exchange) | Low | Medium | High | High |
| Inspiration | Protein folding | Logic | Heuristics | Slime mold | Markets |

## Why It Might Work

Nature has used energy minimization to solve hard problems for billions of years:

- **Proteins fold** from 10³⁰⁰ possible conformations to unique native structure
- **Crystals form** by minimizing lattice energy
- **Neural networks learn** by minimizing loss functions
- **Ecosystems stabilize** at energy/entropy equilibria

SAT is fundamentally about finding stable configurations subject to constraints. Energy-based methods might discover these configurations naturally!

## References

### Protein Folding
- **Anfinsen (1973)**: "Principles that Govern Protein Folding" - Nobel Lecture
- **Dill & MacCallum (2012)**: "The Protein Folding Problem, 50 Years On"
- **Levinthal (1969)**: "How to Fold Graciously" - Levinthal's paradox

### Simulated Annealing
- **Kirkpatrick et al. (1983)**: "Optimization by Simulated Annealing"
- **Metropolis et al. (1953)**: "Equation of State Calculations by Fast Computing Machines"

### Replica Exchange
- **Swendsen & Wang (1986)**: "Replica Monte Carlo Simulation"
- **Sugita & Okamoto (1999)**: "Replica-Exchange Molecular Dynamics Method"

### Statistical Mechanics
- **Ising (1925)**: "Beitrag zur Theorie des Ferromagnetismus"
- **Boltzmann (1877)**: Statistical interpretation of entropy

---

**Educational/Experimental**: This is a research implementation demonstrating bio-inspired approaches to SAT. Not intended for production use—use state-of-the-art solvers like CryptoMiniSat or Glucose for real applications.
