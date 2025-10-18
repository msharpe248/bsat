"""
FOLD-SAT: Protein Folding-Inspired SAT Solver

Solves SAT using energy minimization and simulated annealing.
Models the problem-solving as protein folding to lowest energy state.

Based on:
- Anfinsen's thermodynamic hypothesis
- Simulated annealing (Metropolis-Hastings)
- Parallel tempering (replica exchange)
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Optional
import random
import math

from bsat.cnf import CNFExpression
from bsat.cdcl import CDCLStats

from .energy_landscape import EnergyLandscape
from .molecular_moves import MoveSelector
from .annealing_schedule import AnnealingSchedule, ParallelTempering


class FoldStats(CDCLStats):
    """Extended statistics for FOLD-SAT solver."""

    def __init__(self):
        super().__init__()
        self.annealing_iterations = 0
        self.accepted_moves = 0
        self.rejected_moves = 0
        self.temperature_final = 0.0
        self.energy_final = 0.0
        self.ground_state_energy = 0.0

    def acceptance_rate(self) -> float:
        """Get fraction of accepted moves."""
        total = self.accepted_moves + self.rejected_moves
        if total == 0:
            return 0.0
        return self.accepted_moves / total

    def __str__(self):
        base_stats = super().__str__()
        fold_stats = (
            f"  Annealing iterations: {self.annealing_iterations}\n"
            f"  Accepted moves: {self.accepted_moves}\n"
            f"  Acceptance rate: {self.acceptance_rate():.2%}\n"
            f"  Final temperature: {self.temperature_final:.4f}\n"
            f"  Final energy: {self.energy_final:.2f}\n"
            f"  Ground state: {self.ground_state_energy:.2f}\n"
        )
        return base_stats.rstrip(')') + '\n' + fold_stats + ')'


class FOLDSATSolver:
    """
    FOLD-SAT solver using protein folding energy minimization.

    Treats SAT as energy minimization where:
    - Satisfied clauses contribute negative energy
    - Unsatisfied clauses contribute positive energy
    - Goal: Find global energy minimum (all clauses satisfied)

    Uses simulated annealing (Metropolis-Hastings) to explore landscape.
    """

    def __init__(self,
                 cnf: CNFExpression,
                 max_iterations: int = 100000,
                 T_initial: float = 100.0,
                 T_final: float = 0.01,
                 cooling_rate: float = 0.9995,
                 mode: str = 'annealing'):
        """
        Initialize FOLD-SAT solver.

        Args:
            cnf: CNF formula to solve
            max_iterations: Maximum annealing steps
            T_initial: Initial temperature (high = exploration)
            T_final: Final temperature (low = exploitation)
            cooling_rate: Temperature decay rate
            mode: 'annealing' or 'parallel_tempering'
        """
        self.cnf = cnf
        self.max_iterations = max_iterations
        self.mode = mode

        # Build energy landscape
        self.landscape = EnergyLandscape(
            cnf,
            clause_penalty=10.0,
            clause_reward=-1.0,
            pair_strength=-0.1
        )

        # Move selector
        self.move_selector = MoveSelector(cnf)

        # Annealing schedule
        self.schedule = AnnealingSchedule(
            T_initial=T_initial,
            T_final=T_final,
            cooling_rate=cooling_rate,
            schedule_type='geometric'
        )

        # Statistics
        self.stats = FoldStats()
        self.stats.ground_state_energy = self.landscape.get_ground_state_energy()

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using simulated annealing or parallel tempering.

        Args:
            max_conflicts: Not used (uses max_iterations instead)

        Returns:
            Solution if SAT, None if UNSAT
        """
        if self.mode == 'parallel_tempering':
            return self._solve_parallel_tempering(max_conflicts)
        else:
            return self._solve_simulated_annealing(max_conflicts)

    def _solve_simulated_annealing(self, max_conflicts: int) -> Optional[Dict[str, bool]]:
        """Standard simulated annealing."""

        # Initialize random assignment
        variables = list(self.cnf.get_variables())
        assignment = {var: random.choice([True, False]) for var in variables}

        # Compute initial energy
        current_energy = self.landscape.compute_energy(assignment)
        best_energy = current_energy
        best_assignment = assignment.copy()

        # Annealing loop
        for iteration in range(self.max_iterations):
            self.stats.annealing_iterations += 1

            # Get current temperature
            temperature = self.schedule.get_temperature()

            # Propose move
            new_assignment = self.move_selector.propose(assignment, temperature)

            # Compute new energy
            new_energy = self.landscape.compute_energy(new_assignment)

            # Energy change
            delta_energy = new_energy - current_energy

            # Metropolis acceptance criterion
            if delta_energy < 0:
                # Energy decreased - always accept
                accept = True
            else:
                # Energy increased - accept probabilistically
                # P(accept) = exp(-ΔE / T)
                acceptance_prob = math.exp(-delta_energy / temperature)
                accept = random.random() < acceptance_prob

            # Update assignment
            if accept:
                assignment = new_assignment
                current_energy = new_energy
                self.stats.accepted_moves += 1

                # Track best
                if current_energy < best_energy:
                    best_energy = current_energy
                    best_assignment = assignment.copy()
            else:
                self.stats.rejected_moves += 1

            # Check if found ground state (all satisfied)
            if self.landscape.is_ground_state(assignment):
                self.stats.temperature_final = temperature
                self.stats.energy_final = current_energy
                return assignment

            # Cool down
            self.schedule.step()

            # Early termination if converged
            if temperature < self.schedule.T_final:
                break

        # Failed to reach ground state
        # Return best found or fallback to CDCL
        self.stats.temperature_final = self.schedule.get_temperature()
        self.stats.energy_final = best_energy

        if self.landscape.is_ground_state(best_assignment):
            return best_assignment

        # Fallback to CDCL
        from bsat.cdcl import CDCLSolver
        fallback = CDCLSolver(self.cnf)
        result = fallback.solve(max_conflicts=max_conflicts)
        self._update_stats_from_cdcl(fallback.stats)
        return result

    def _solve_parallel_tempering(self, max_conflicts: int) -> Optional[Dict[str, bool]]:
        """Parallel tempering (replica exchange)."""

        # Initialize parallel tempering
        pt = ParallelTempering(
            num_replicas=8,
            T_min=0.1,
            T_max=100.0,
            swap_interval=100
        )

        # Create initial assignments for each replica
        variables = list(self.cnf.get_variables())
        initial_assignments = [
            {var: random.choice([True, False]) for var in variables}
            for _ in range(pt.num_replicas)
        ]

        pt.initialize_replicas(initial_assignments)

        # Initialize energies
        for replica in pt.replicas:
            replica.energy = self.landscape.compute_energy(replica.assignment)

        # Evolution loop
        for iteration in range(self.max_iterations):
            self.stats.annealing_iterations += 1

            # Evolve each replica independently
            for replica in pt.replicas:
                # Propose move
                new_assignment = self.move_selector.propose(
                    replica.assignment,
                    replica.temperature
                )

                # Compute new energy
                new_energy = self.landscape.compute_energy(new_assignment)
                delta_energy = new_energy - replica.energy

                # Metropolis acceptance
                if delta_energy < 0 or \
                   random.random() < math.exp(-delta_energy / replica.temperature):
                    replica.assignment = new_assignment
                    replica.energy = new_energy
                    replica.accepted_moves += 1
                    self.stats.accepted_moves += 1
                else:
                    self.stats.rejected_moves += 1

                replica.total_moves += 1

            # Check if any replica found solution
            for replica in pt.replicas:
                if self.landscape.is_ground_state(replica.assignment):
                    self.stats.temperature_final = replica.temperature
                    self.stats.energy_final = replica.energy
                    return replica.assignment

            # Attempt replica swaps
            if iteration % pt.swap_interval == 0:
                pt.attempt_all_swaps()

        # Failed to find solution
        # Return best replica or fallback
        best_replica = pt.get_best_replica()
        self.stats.temperature_final = best_replica.temperature
        self.stats.energy_final = best_replica.energy

        if self.landscape.is_ground_state(best_replica.assignment):
            return best_replica.assignment

        # Fallback to CDCL
        from bsat.cdcl import CDCLSolver
        fallback = CDCLSolver(self.cnf)
        result = fallback.solve(max_conflicts=max_conflicts)
        self._update_stats_from_cdcl(fallback.stats)
        return result

    def _update_stats_from_cdcl(self, cdcl_stats: CDCLStats):
        """Update stats from fallback CDCL solver."""
        self.stats.decisions = cdcl_stats.decisions
        self.stats.conflicts = cdcl_stats.conflicts
        self.stats.propagations = cdcl_stats.propagations

    def get_energy_statistics(self) -> dict:
        """Get energy and annealing statistics."""
        return {
            'annealing_iterations': self.stats.annealing_iterations,
            'accepted_moves': self.stats.accepted_moves,
            'rejected_moves': self.stats.rejected_moves,
            'acceptance_rate': self.stats.acceptance_rate(),
            'final_temperature': self.stats.temperature_final,
            'final_energy': self.stats.energy_final,
            'ground_state_energy': self.stats.ground_state_energy,
            'reached_ground_state': abs(self.stats.energy_final -
                                       self.stats.ground_state_energy) < 1e-6,
        }


def solve_fold_sat(cnf: CNFExpression,
                   max_iterations: int = 100000,
                   mode: str = 'annealing',
                   max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using FOLD-SAT (Protein Folding Energy Minimization).

    Educational/experimental bio-inspired approach.

    Args:
        cnf: CNF formula
        max_iterations: Maximum annealing iterations
        mode: 'annealing' or 'parallel_tempering'
        max_conflicts: Maximum conflicts (for fallback)

    Returns:
        Solution if SAT, None if UNSAT
    """
    solver = FOLDSATSolver(cnf, max_iterations=max_iterations, mode=mode)
    return solver.solve(max_conflicts=max_conflicts)
