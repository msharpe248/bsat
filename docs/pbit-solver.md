# P-Bit Solver

## Overview

The p-bit solver finds satisfying assignments by simulating a network of
**probabilistic bits** — the building blocks of probabilistic ("p-") computers.
It performs Gibbs sampling of a Boltzmann distribution whose lowest-energy
states are exactly the satisfying assignments, and anneals the temperature so
the sampler settles into a ground state.

**Important**: Like WalkSAT and Schöning's algorithm, this is an **incomplete**
solver. It may fail to find a solution even when one exists, and a `None`
result is *not* a proof of unsatisfiability. Use `solve_cdcl()` or
`solve_sat()` when UNSAT must be proven.

```python
from bsat import CNFExpression, solve_pbit

cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c)")
solution = solve_pbit(cnf, seed=42)
```

## What Are p-Bits?

A **p-bit** (probabilistic bit) sits between a classical bit and a qubit: it
fluctuates randomly between its two states, but the *probability* of each
state is controlled by an analog input `I`:

```
mᵢ = sgn( rand(−1, 1) + tanh(Iᵢ) )        (device equation)

P(mᵢ = +1) = σ(2·Iᵢ)                       (equivalent form)
```

where `σ(x) = 1/(1+e^(−x))` is the sigmoid. With `I = 0` the p-bit is a fair
coin; large positive `I` pins it to 1, large negative `I` pins it to 0.

p-bits were introduced by Camsari, Faria, Sutton, and Datta (Purdue, 2017) as
the building block of *probabilistic computers*. In hardware they are built
from **stochastic magnetic tunnel junctions** (sMTJs) — nanomagnets engineered
to be thermally unstable so they flip billions of times per second, with the
flip probability steered by a spin-polarized current. A p-computer with a few
thousand such devices performs massively parallel Monte Carlo sampling in
physics rather than in software (Aadit et al., 2022).

## From p-Bits to Boltzmann Sampling

Connect p-bits into a network with symmetric couplings `J` and biases `h`,
and give each p-bit the input

```
Iᵢ = β · ( Σⱼ Jᵢⱼ mⱼ + hᵢ )
```

Updating the p-bits one at a time then performs **Gibbs sampling** of the
Boltzmann distribution over the Ising energy:

```
E(m) = − Σᵢⱼ Jᵢⱼ mᵢ mⱼ − Σᵢ hᵢ mᵢ

P(m) ∝ exp(−β · E(m))
```

`β` is the *inverse temperature*: small β (hot) makes every state nearly
equally likely; large β (cold) concentrates all probability on the minimum-
energy states. Updating one p-bit given the others is exactly the **heat-bath
rule**:

```
P(flip) = σ(−β · ΔE)
```

where `ΔE` is the energy change the flip would cause. This is the update the
solver implements.

## SAT as Energy Minimization

To solve SAT we need an energy function whose ground states are the
satisfying assignments. There are two standard mappings.

### Mapping (a): clause-count energy — *implemented here*

Let each variable be one p-bit and define

```
E(assignment) = number of unsatisfied clauses
```

Then `E = 0` ⟺ the assignment satisfies the formula. The p-bit input for
variable `v` is computed from the energy change of flipping it:

```
ΔE(v) = breaks(v) − makes(v)
Iᵥ = −β·ΔE/2,   so   P(flip v) = σ(−β·ΔE)
```

where `breaks(v)` counts satisfied clauses that flipping `v` would falsify
and `makes(v)` counts unsatisfied clauses it would satisfy. This mapping is
**exact for any clause length** (any k-SAT) and needs no auxiliary variables,
which is why the solver uses it.

### Mapping (b): Ising/QUBO embedding — *used on real hardware*

Physical Ising machines can only realize pairwise couplings `Jᵢⱼ mᵢ mⱼ`, but a
3-literal clause is a *three*-body constraint. Hardware implementations
therefore add **one auxiliary spin per clause** and build a quadratic (QUBO)
energy whose minimum enforces the clause. For example, the clause
`(x ∨ y ∨ z)` can be encoded with an auxiliary spin `a` via a penalty like

```
E_clause = (1−x)(1−y) · a-terms …  →  quadratic in {x, y, z, a}
```

concretely, one standard OR-gadget penalty is

```
E_OR(x, y, a) = x·y − 2a·(x + y) + 3a      (a = x ∨ y at minimum, E = 0)
```

chained pairwise to cover longer clauses. A 3-clause formula over variables
`x, y, z` thus becomes a sparse Ising network of 3 variable spins plus 3
auxiliary spins, and the whole network is annealed in hardware. This is the
construction used by sparse Ising machines (Aadit et al., 2022).

The embedding multiplies the spin count and turns the exact clause count into
a penalty-weighted approximation of it, so in software — where arbitrary-order
energy differences cost nothing — mapping (a) is strictly better. Mapping (b)
matters when the sampler is a physical device limited to pairwise couplings.

## The Algorithm

```
solve(sweeps, restarts, beta_min, beta_max, schedule):
    if formula contains an empty clause: return None
    if formula has no variables: return {}

    for each restart:
        assignment ← uniform random bits
        energy ← number of unsatisfied clauses
        for t in 0 .. sweeps−1:
            β ← schedule(t)                      # anneal β_min → β_max
            for each variable v in random order: # one "sweep"
                ΔE ← breaks(v) − makes(v)        # from occurrence lists
                with probability σ(−β·ΔE):
                    flip v, update energy incrementally
                    if energy == 0: return assignment
    return None
```

### Annealing schedules

Each restart sweeps β from `beta_min` (exploration) to `beta_max`
(exploitation):

- **geometric** (default): `β(t) = β_min · (β_max/β_min)^(t/(T−1))` — spends
  more sweeps at low β, which usually anneals better
- **linear**: `β(t) = β_min + (β_max−β_min) · t/(T−1)`

Setting `beta_min == beta_max` gives constant-temperature Gibbs sampling.

## Key Components

### Occurrence lists and incremental ΔE

The solver precompiles, for every variable, the list of clauses it appears in
(and with which sign), plus a per-clause count of currently-true literals.
Computing `ΔE` and applying a flip then costs `O(deg(v))` — proportional to
how many clauses mention `v` — instead of rescanning the whole formula. One
full sweep costs `O(total literal occurrences)`.

### Heat-bath vs Metropolis

Two classic Monte Carlo acceptance rules exist:

- **Heat-bath (Gibbs)**: `P(flip) = σ(−β·ΔE)` — used here, because it *is*
  the p-bit device equation
- **Metropolis**: `P(flip) = min(1, e^(−β·ΔE))` — used by FOLD-SAT in the
  research suite

Both sample the same Boltzmann distribution; heat-bath even accepts some
zero- and positive-ΔE moves at finite β, which is what lets it escape local
minima while hot.

## Usage

### Basic usage

```python
from bsat import CNFExpression, solve_pbit

cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
solution = solve_pbit(cnf, seed=42)
if solution:
    print(f"SAT: {solution}")
else:
    print("No solution found (but may exist)")
```

### With parameters

```python
solution = solve_pbit(
    cnf,
    sweeps=2000,        # sweeps per restart (1 sweep = update every p-bit once)
    restarts=20,        # random restarts
    beta_min=0.1,       # starting inverse temperature (hot)
    beta_max=4.0,       # ending inverse temperature (cold)
    schedule='geometric',
    seed=42,
)
```

### Using the solver class

```python
from bsat import PBitSolver

solver = PBitSolver(cnf, seed=42)
solution = solver.solve(sweeps=1000, restarts=10)
print(solver.get_stats())
```

### Getting statistics

```python
from bsat import get_pbit_stats

solution, stats = get_pbit_stats(cnf, seed=42)
print(f"Sweeps: {stats.sweeps}, flips: {stats.flips}")
print(f"Acceptance rate: {stats.flips / stats.updates:.2f}")
print(f"Best energy: {stats.best_energy}")   # 0 means solved
print(f"Energy per sweep: {stats.energy_history}")
```

## Choosing Parameters

- **`beta_min` (≈ 0.1–0.5)**: how hot the sampler starts. Too high and the
  run behaves greedily from the start and can get stuck early.
- **`beta_max` (≈ 3–10)**: how cold it finishes. Since `ΔE` is an integer,
  `β·ΔE ≈ 10` already makes uphill moves essentially impossible — values far
  above 10 change nothing.
- **`sweeps` vs `restarts`**: more sweeps = slower, better annealing; more
  restarts = more independent tries. For hard instances prefer raising
  `sweeps` first (annealing quality matters more than retry count).

## When to Use the P-Bit Solver

### ✅ Good For

- Learning how Boltzmann sampling, Gibbs updates, and simulated annealing
  solve combinatorial problems
- Understanding probabilistic-bit hardware (Ising machines, p-computers)
- Formulas with arbitrary clause lengths (no 3-SAT reduction needed)
- Random k-SAT instances

### ❌ Not Good For

- Proving UNSAT (incomplete — use `solve_cdcl()` / `solve_sat()`)
- Structured industrial instances (CDCL exploits structure far better)
- Formulas where a specialized polynomial solver applies
  (2SAT, Horn-SAT, XOR-SAT)

## Relation to Other Algorithms

| Algorithm | Move selection | Escape mechanism |
|-----------|----------------|------------------|
| **P-bit** | Every variable, each sweep | Thermal noise (annealed β) |
| **WalkSAT** | Variable from a random unsatisfied clause | Noise parameter p |
| **Schöning** | Random variable from a random unsatisfied clause | Full restarts every 3n flips |
| **FOLD-SAT** (research/) | Metropolis annealing + parallel tempering | Temperature ladder |

WalkSAT and Schöning are *focused* random walks — they only ever touch
variables in unsatisfied clauses. The p-bit solver instead sweeps **all**
variables every pass, exactly as a hardware p-bit array would, and relies on
the temperature schedule rather than focusing for its search efficiency.

## Performance Characteristics

- One sweep costs O(L) where L = total number of literal occurrences.
- There is **no polynomial runtime guarantee**: Gibbs sampling can mix
  exponentially slowly on hard energy landscapes. In practice annealed
  restarts solve random 3SAT instances near the phase transition
  (m/n ≈ 4.26) at educational sizes comfortably.
- Contrast with Schöning's O(1.334ⁿ) *provable* expected time for 3SAT — the
  p-bit solver trades that guarantee for hardware realism and arbitrary-k
  generality.

## Variants and Extensions

- **Graph-colored parallel updates**: p-bits that share no clause can update
  simultaneously; coloring the variable-interaction graph yields parallel
  sweeps — this is how hardware achieves massive parallelism.
- **Asynchronous "hardware mode"**: real sMTJ arrays update continuously and
  asynchronously with no global clock; the sequential random-order sweep used
  here is its faithful serializable approximation.
- **Parallel tempering**: run several replicas at different temperatures and
  swap them (see FOLD-SAT in `research/`).
- **QUBO embedding**: mapping (b) above, for running on actual Ising-machine
  or annealer hardware.

## References

- Camsari, K. Y., Faria, R., Sutton, B. M., Datta, S. (2017). "Stochastic
  p-bits for invertible logic". *Physical Review X* 7, 031014.
- Aadit, N. A., et al. (2022). "Massively parallel probabilistic computing
  with sparse Ising machines". *Nature Electronics* 5, 460–468.
- Kirkpatrick, S., Gelatt, C. D., Vecchi, M. P. (1983). "Optimization by
  simulated annealing". *Science* 220, 671–680.
- Schöning, U. (1999). "A probabilistic algorithm for k-SAT and constraint
  satisfaction problems". FOCS 1999.
- Selman, B., Kautz, H., Cohen, B. (1994). "Noise strategies for improving
  local search". AAAI 1994. (WalkSAT)

## See Also

- [WalkSAT Solver](walksat-solver.md) — focused random-walk local search
- [Schöning's Algorithm](schoening-solver.md) — provably fast randomized 3SAT
- [CDCL Solver](cdcl-solver.md) — complete solver for when UNSAT matters
