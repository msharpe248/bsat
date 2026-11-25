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
| Unsatisfied clause (3+ literals) | Unfavorable clash | +10 kcal/mol (destabilizing) |
| Unsatisfied binary clause | Stronger clash | +20 kcal/mol penalty |
| Unsatisfied unit clause | Critical contact | +100 kcal/mol penalty |
| SAT solution | Native fold | Global energy minimum |
| UNSAT | Unfoldable sequence | No low-energy state |

### Energy Function (Hamiltonian)

```
H(x₁, x₂, ..., xₙ) = Σ E_clause + Σ E_pair

E_clause(C) = {
    -1   if C satisfied (favorable contact)
    +10  if C unsatisfied (steric clash)
    +20  if C is binary clause unsatisfied
    +100 if C is unit clause unsatisfied
}

E_pair(xᵢ, xⱼ) = {      (ε = 0.1 per clause the pair shares)
    -ε  if xᵢ, xⱼ co-occur and aligned (same truth value)
    +ε  if xᵢ, xⱼ co-occur and misaligned
}

Goal: Minimize H(x₁, ..., xₙ)
Target: H = -|clauses| (all satisfied)
```

> **Implementation caveat**: `EnergyLandscape.is_ground_state()` tests `H == -|clauses|`
> on the *total* energy, including the pair terms. An assignment that satisfies every clause is
> therefore only recognised as the ground state when its pair terms happen to cancel exactly
> (as many aligned as misaligned co-occurring pairs). Otherwise annealing keeps running and the
> solver ends up in the CDCL fallback described below; e.g. `(a | b)` can never be recognised by
> annealing (energy is -1.1 or -0.9, never -1.0) and is always answered by the fallback.

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
   Else (iterations exhausted or T dropped below T_final):
      fall back to bsat's CDCLSolver, which returns SAT/UNSAT
```

Note: the loop also stops as soon as T < `T_final`. With the defaults (100 → 0.01 at 0.9995)
that is after about 18,400 iterations, regardless of a larger `max_iterations`.

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
   Return a ground-state replica, otherwise fall back to CDCLSolver
```

The solver uses 8 replicas, T from 0.1 to 100 (geometric ladder), swap attempts every 100
iterations; these are fixed in `fold_solver.py` (`T_initial`/`T_final`/`cooling_rate` are
ignored in this mode).

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
# Randomize multiple variables (thermal noise); num_mutations = max(1, 10% of variables)
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
    - compute_delta_energy(assignment, var, new_value): Incremental update (available, but the solver currently recomputes the full energy each step)
    - is_ground_state(assignment): Check if all satisfied
    - get_num_unsatisfied(assignment): Count violations
```

**MolecularMove**:
```python
class MolecularMove:
    - propose(assignment): Generate new assignment

class MoveSelector:
    - select_move(temperature): Choose move type
    - propose(assignment, temperature): Select a move and apply it
    - Temperature-adaptive: exploration (high T) vs refinement (low T)
```

**AnnealingSchedule**:
```python
class AnnealingSchedule:
    - get_temperature(): Current temperature
    - step(): Advance schedule (cool down)
    - Types: geometric (the only one the solver uses), linear, logarithmic, adaptive (placeholder: step() leaves T unchanged)
```

**ParallelTempering**:
```python
class ParallelTempering:
    - replicas: List of simulations at different T
    - attempt_swap(replica_i, replica_j): Exchange configurations
    - attempt_all_swaps(): Try swaps between adjacent replicas
    - get_best_replica(): Lowest energy
```

**FOLDSATSolver**:
```python
class FOLDSATSolver:
    - solve(max_conflicts=1000000): Main algorithm (max_conflicts only limits the CDCL fallback)
    - _solve_simulated_annealing(): Standard mode
    - _solve_parallel_tempering(): Advanced mode
    - get_energy_statistics(): Performance metrics
```

## Usage

### Basic Usage

Run from the repository root with `PYTHONPATH=src:.` (or add `research/` to `sys.path` and import `fold_sat` directly, as `example.py` does).

```python
from bsat import CNFExpression
from research.fold_sat import FOLDSATSolver

# Create CNF formula
cnf = CNFExpression.parse("(a | b) & (~a | c)")

# Solve with simulated annealing
solver = FOLDSATSolver(cnf, max_iterations=10000)
result = solver.solve()

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")  # answered by the CDCL fallback (or its conflict limit was hit)
```

A convenience function `solve_fold_sat(cnf, max_iterations=100000, mode='annealing', max_conflicts=1000000)` is also exported.

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

`get_energy_statistics()` returns the keys `annealing_iterations`, `accepted_moves`, `rejected_moves`, `acceptance_rate`, `final_temperature`, `final_energy`, `ground_state_energy`, `reached_ground_state`. `reached_ground_state` is `False` whenever the answer came from the CDCL fallback (then `solver.stats.decisions/conflicts/propagations` hold the fallback's counters).

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
from research.fold_sat import EnergyLandscape

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
python3 research/fold_sat/example.py   # from the repository root (also works from this directory)
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
3. **No Completeness Guarantee for the annealing itself**: May not find solution in finite time; the solver is only complete because it falls back to `CDCLSolver` when annealing fails
4. **UNSAT Detection**: Annealing cannot prove UNSAT; every UNSAT answer (after the full annealing budget is spent) comes from the CDCL fallback
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
| Guarantee | Annealing: no; complete only via CDCL fallback | Yes (complete) | No | No (CDCL fallback) | No (CDCL fallback) |
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
