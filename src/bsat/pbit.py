"""
P-Bit (Probabilistic Bit) SAT Solver

A SAT solver modeled on probabilistic-bit hardware dynamics. A p-bit is a
bit that fluctuates between 0 and 1 with a probability controlled by its
input: P(mᵢ=1) = σ(2·Iᵢ), equivalently mᵢ = sgn(rand(−1,1) + tanh(Iᵢ)).
Networks of p-bits with symmetric couplings perform Gibbs sampling of the
Boltzmann distribution P(m) ∝ exp(−β·E(m)) over an Ising-style energy E.

To solve SAT, we take E(assignment) = number of unsatisfied clauses, so the
ground states (E = 0) are exactly the satisfying assignments. Each variable
is one p-bit; its input is computed from the energy change ΔE of flipping
it, giving the heat-bath update P(flip) = σ(−β·ΔE). Annealing the inverse
temperature β from low (random exploration) to high (greedy descent) turns
the sampler into simulated annealing.

Key Characteristics:
- Incomplete: may fail to find a solution even if one exists (returns None)
- Handles arbitrary clause lengths (any k-SAT) with no auxiliary variables
- Incremental ΔE via occurrence lists: O(deg(v)) per p-bit update
- Hardware realizations use stochastic magnetic tunnel junctions (sMTJs)

References:
- Camsari, Faria, Sutton, Datta. "Stochastic p-bits for invertible logic".
  Physical Review X 7, 031014 (2017).
- Aadit et al. "Massively parallel probabilistic computing with sparse
  Ising machines". Nature Electronics 5, 460-468 (2022).
- Kirkpatrick, Gelatt, Vecchi. "Optimization by simulated annealing".
  Science 220, 671-680 (1983).
"""

from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
import math
import random
from .cnf import CNFExpression


@dataclass
class PBitStats:
    """Statistics from a p-bit solver run."""
    restarts: int = 0           # Number of restarts performed
    sweeps: int = 0             # Total sweeps across all restarts
    updates: int = 0            # Total p-bit updates evaluated
    flips: int = 0              # Updates that changed a bit's state
    best_energy: int = -1       # Lowest #unsatisfied clauses seen (0 = solved)
    final_energy: int = -1      # Energy at termination
    energy_history: List[int] = None  # Energy after each sweep (last restart only)

    def __post_init__(self):
        if self.energy_history is None:
            self.energy_history = []

    def __str__(self):
        acceptance = self.flips / self.updates if self.updates else 0.0
        return (
            f"PBitStats(\n"
            f"  Restarts: {self.restarts}\n"
            f"  Sweeps: {self.sweeps}\n"
            f"  Updates: {self.updates}\n"
            f"  Flips: {self.flips}\n"
            f"  Acceptance rate: {acceptance:.3f}\n"
            f"  Best energy: {self.best_energy}\n"
            f"  Final energy: {self.final_energy}\n"
            f")"
        )


class PBitSolver:
    """
    P-bit (probabilistic bit) SAT solver using annealed Gibbs sampling.

    Each variable is a p-bit. One sweep updates every p-bit once in random
    order: the bit is flipped with probability σ(−β·ΔE), where ΔE is the
    change in the number of unsatisfied clauses caused by the flip. This is
    the heat-bath rule — exactly the p-bit device equation
    mᵥ = sgn(rand(−1,1) + tanh(Iᵥ)) with input Iᵥ = −β·ΔE/2.

    Annealing β from beta_min to beta_max over the sweeps of each restart
    performs simulated annealing on the clause-count energy landscape; the
    solver stops as soon as the energy reaches 0 (all clauses satisfied).

    Example:
        >>> from bsat import CNFExpression, PBitSolver
        >>> cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
        >>> solver = PBitSolver(cnf, seed=42)
        >>> solution = solver.solve()
    """

    def __init__(self, cnf: CNFExpression, seed: Optional[int] = None):
        """
        Initialize the p-bit solver.

        Args:
            cnf: CNF formula to solve
            seed: Random seed for reproducibility

        Note:
            Uses a private random.Random instance, so it never disturbs the
            global random module state. Variables are processed in sorted
            order so runs with the same seed are reproducible across
            processes.
        """
        self.cnf = cnf
        self.variables = sorted(cnf.get_variables())
        self.n = len(self.variables)
        self.stats = PBitStats()
        self._rng = random.Random(seed)

        var_index = {var: i for i, var in enumerate(self.variables)}
        # Each clause compiled to (variable index, is_positive) pairs
        self.clause_lits: List[List[Tuple[int, bool]]] = [
            [(var_index[lit.variable], not lit.negated) for lit in clause.literals]
            for clause in cnf.clauses
        ]
        # Occurrence lists: occ[v] = [(clause index, is_positive), ...]
        self.occ: List[List[Tuple[int, bool]]] = [[] for _ in range(self.n)]
        for c, lits in enumerate(self.clause_lits):
            for v, positive in lits:
                self.occ[v].append((c, positive))

    def solve(self, sweeps: int = 1000, restarts: int = 10,
              beta_min: float = 0.1, beta_max: float = 4.0,
              schedule: str = 'geometric') -> Optional[Dict[str, bool]]:
        """
        Run annealed p-bit Gibbs sampling to find a satisfying assignment.

        Args:
            sweeps: Sweeps per restart (one sweep updates every p-bit once)
            restarts: Number of restarts from fresh random assignments
            beta_min: Inverse temperature at the start of each restart
            beta_max: Inverse temperature at the end of each restart
            schedule: Annealing schedule, 'geometric' or 'linear'.
                Setting beta_min == beta_max gives constant-temperature
                Gibbs sampling.

        Returns:
            Satisfying assignment if found, None otherwise

        Raises:
            ValueError: If schedule is not 'geometric' or 'linear'

        Note:
            This is an INCOMPLETE algorithm — None does not prove UNSAT.
            There is no polynomial runtime guarantee; mixing can be
            exponential in the worst case.
        """
        if schedule not in ('geometric', 'linear'):
            raise ValueError(f"Unknown schedule '{schedule}' (use 'geometric' or 'linear')")

        self.stats = PBitStats()

        # An empty clause can never be satisfied: energy can never reach 0
        if any(not lits for lits in self.clause_lits):
            return None
        if self.n == 0:
            self.stats.best_energy = 0
            self.stats.final_energy = 0
            return {}

        m = len(self.clause_lits)
        for restart in range(1, restarts + 1):
            self.stats.restarts = restart
            assignment = [self._rng.random() < 0.5 for _ in range(self.n)]
            sat_count = [0] * m
            for c, lits in enumerate(self.clause_lits):
                sat_count[c] = sum(1 for v, positive in lits if assignment[v] == positive)
            energy = sum(1 for count in sat_count if count == 0)
            self.stats.energy_history = []
            self._note_energy(energy)
            if energy == 0:
                return self._to_dict(assignment)

            order = list(range(self.n))
            for t in range(sweeps):
                beta = self._beta_at(t, sweeps, beta_min, beta_max, schedule)
                self._rng.shuffle(order)
                for v in order:
                    # ΔE for flipping v, from its occurrence list only
                    breaks = 0
                    makes = 0
                    for c, positive in self.occ[v]:
                        if assignment[v] == positive:  # literal currently true
                            if sat_count[c] == 1:
                                breaks += 1
                        elif sat_count[c] == 0:
                            makes += 1
                    delta_e = breaks - makes

                    # Heat-bath p-bit update: P(flip) = σ(−β·ΔE)
                    x = min(60.0, max(-60.0, beta * delta_e))
                    p_flip = 1.0 / (1.0 + math.exp(x))
                    self.stats.updates += 1
                    if self._rng.random() < p_flip:
                        self.stats.flips += 1
                        was_true = assignment[v]
                        assignment[v] = not was_true
                        for c, positive in self.occ[v]:
                            if was_true == positive:  # literal became false
                                sat_count[c] -= 1
                                if sat_count[c] == 0:
                                    energy += 1
                            else:  # literal became true
                                if sat_count[c] == 0:
                                    energy -= 1
                                sat_count[c] += 1
                        if energy == 0:
                            self.stats.sweeps += 1
                            self.stats.energy_history.append(energy)
                            self._note_energy(energy)
                            return self._to_dict(assignment)
                self.stats.sweeps += 1
                self.stats.energy_history.append(energy)
                self._note_energy(energy)

        return None

    def get_stats(self) -> PBitStats:
        """Get statistics from last solve() call."""
        return self.stats

    def _beta_at(self, t: int, sweeps: int, beta_min: float, beta_max: float,
                 schedule: str) -> float:
        """Inverse temperature for sweep t of the annealing schedule."""
        if sweeps <= 1 or beta_min == beta_max:
            return beta_max
        frac = t / (sweeps - 1)
        if schedule == 'geometric':
            return beta_min * (beta_max / beta_min) ** frac
        return beta_min + (beta_max - beta_min) * frac

    def _note_energy(self, energy: int) -> None:
        """Record current energy into best/final stats fields."""
        if self.stats.best_energy < 0 or energy < self.stats.best_energy:
            self.stats.best_energy = energy
        self.stats.final_energy = energy

    def _to_dict(self, assignment: List[bool]) -> Dict[str, bool]:
        """Convert internal assignment list to a variable dictionary."""
        return {var: assignment[i] for i, var in enumerate(self.variables)}


def solve_pbit(cnf: CNFExpression, sweeps: int = 1000, restarts: int = 10,
               beta_min: float = 0.1, beta_max: float = 4.0,
               schedule: str = 'geometric',
               seed: Optional[int] = None) -> Optional[Dict[str, bool]]:
    """
    Solve SAT using the p-bit (probabilistic bit) solver.

    This is a simple functional interface to PBitSolver.

    Args:
        cnf: CNF formula to solve
        sweeps: Sweeps per restart (default: 1000)
        restarts: Number of random restarts (default: 10)
        beta_min: Starting inverse temperature (default: 0.1)
        beta_max: Ending inverse temperature (default: 4.0)
        schedule: Annealing schedule, 'geometric' or 'linear'
        seed: Random seed for reproducibility

    Returns:
        Satisfying assignment if found, None otherwise

    Example:
        >>> from bsat import CNFExpression, solve_pbit
        >>> cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c)")
        >>> solution = solve_pbit(cnf, seed=42)
        >>> if solution:
        ...     print(f"SAT: {solution}")
        ... else:
        ...     print("No solution found (but may exist)")

    Note:
        This is an INCOMPLETE algorithm — it may not find a solution even
        if one exists, and None does not prove UNSAT. Use solve_cdcl() or
        solve_sat() when an UNSAT answer must be trusted.
    """
    solver = PBitSolver(cnf, seed=seed)
    return solver.solve(sweeps=sweeps, restarts=restarts,
                        beta_min=beta_min, beta_max=beta_max, schedule=schedule)


def get_pbit_stats(cnf: CNFExpression, sweeps: int = 1000, restarts: int = 10,
                   beta_min: float = 0.1, beta_max: float = 4.0,
                   schedule: str = 'geometric',
                   seed: Optional[int] = None) -> Tuple[Optional[Dict[str, bool]], PBitStats]:
    """
    Solve SAT with the p-bit solver and return detailed statistics.

    Args:
        cnf: CNF formula to solve
        sweeps: Sweeps per restart
        restarts: Number of random restarts
        beta_min: Starting inverse temperature
        beta_max: Ending inverse temperature
        schedule: Annealing schedule, 'geometric' or 'linear'
        seed: Random seed for reproducibility

    Returns:
        Tuple of (solution, statistics)

    Example:
        >>> solution, stats = get_pbit_stats(cnf, seed=42)
        >>> print(f"Sweeps: {stats.sweeps}")
        >>> print(f"Best energy: {stats.best_energy}")
    """
    solver = PBitSolver(cnf, seed=seed)
    solution = solver.solve(sweeps=sweeps, restarts=restarts,
                            beta_min=beta_min, beta_max=beta_max, schedule=schedule)
    return solution, solver.get_stats()
