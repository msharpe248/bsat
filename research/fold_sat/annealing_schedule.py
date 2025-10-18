"""
Annealing Schedule for FOLD-SAT

Manages temperature control for simulated annealing.
Includes standard cooling schedules and replica exchange (parallel tempering).
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import List, Dict, Optional
import math
import random


class AnnealingSchedule:
    """
    Temperature schedule for simulated annealing.

    Controls exploration vs exploitation trade-off.
    """

    def __init__(self,
                 T_initial: float = 100.0,
                 T_final: float = 0.01,
                 cooling_rate: float = 0.95,
                 schedule_type: str = 'geometric'):
        """
        Initialize annealing schedule.

        Args:
            T_initial: Starting temperature (high = more exploration)
            T_final: Final temperature (low = more exploitation)
            cooling_rate: Rate of temperature decrease
            schedule_type: 'geometric', 'linear', 'logarithmic', 'adaptive'
        """
        self.T_initial = T_initial
        self.T_final = T_final
        self.cooling_rate = cooling_rate
        self.schedule_type = schedule_type

        self.current_temperature = T_initial
        self.iteration = 0

    def get_temperature(self) -> float:
        """Get current temperature."""
        return self.current_temperature

    def step(self) -> float:
        """
        Advance schedule by one step.

        Returns:
            New temperature
        """
        self.iteration += 1

        if self.schedule_type == 'geometric':
            # T(k) = T_0 * α^k
            self.current_temperature = max(
                self.T_final,
                self.T_initial * (self.cooling_rate ** self.iteration)
            )

        elif self.schedule_type == 'linear':
            # T(k) = T_0 - k * (T_0 - T_f) / max_iter
            rate = (self.T_initial - self.T_final) / 10000
            self.current_temperature = max(
                self.T_final,
                self.T_initial - self.iteration * rate
            )

        elif self.schedule_type == 'logarithmic':
            # T(k) = T_0 / log(1 + k)
            self.current_temperature = max(
                self.T_final,
                self.T_initial / math.log(2 + self.iteration)
            )

        elif self.schedule_type == 'adaptive':
            # Adaptive cooling (slower when improving)
            # Implemented by caller based on performance
            pass

        return self.current_temperature

    def reset(self):
        """Reset schedule to initial temperature."""
        self.current_temperature = self.T_initial
        self.iteration = 0

    def is_finished(self) -> bool:
        """Check if reached final temperature."""
        return self.current_temperature <= self.T_final


class Replica:
    """
    Single replica in parallel tempering.

    Each replica runs at different temperature.
    """

    def __init__(self, temperature: float, assignment: Dict[str, bool]):
        """
        Initialize replica.

        Args:
            temperature: Replica temperature
            assignment: Initial assignment
        """
        self.temperature = temperature
        self.assignment = assignment
        self.energy = 0.0
        self.accepted_moves = 0
        self.total_moves = 0

    def acceptance_rate(self) -> float:
        """Get fraction of accepted moves."""
        if self.total_moves == 0:
            return 0.0
        return self.accepted_moves / self.total_moves

    def __repr__(self):
        return (f"Replica(T={self.temperature:.2f}, E={self.energy:.2f}, "
                f"acc_rate={self.acceptance_rate():.2%})")


class ParallelTempering:
    """
    Parallel tempering (replica exchange) algorithm.

    Runs multiple simulations at different temperatures.
    Periodically swaps configurations to escape local minima.
    """

    def __init__(self,
                 num_replicas: int = 8,
                 T_min: float = 0.1,
                 T_max: float = 100.0,
                 swap_interval: int = 100):
        """
        Initialize parallel tempering.

        Args:
            num_replicas: Number of replicas (parallel chains)
            T_min: Minimum temperature (coldest replica)
            T_max: Maximum temperature (hottest replica)
            swap_interval: Steps between swap attempts
        """
        self.num_replicas = num_replicas
        self.T_min = T_min
        self.T_max = T_max
        self.swap_interval = swap_interval

        # Generate temperature ladder
        self.temperatures = self._generate_temperature_ladder()

        # Replicas (initialized later with assignments)
        self.replicas: List[Replica] = []

        # Statistics
        self.swap_attempts = 0
        self.swap_successes = 0

    def _generate_temperature_ladder(self) -> List[float]:
        """
        Generate temperature ladder for replicas.

        Uses geometric spacing for better overlap.
        """
        if self.num_replicas == 1:
            return [self.T_min]

        # Geometric spacing: T_i = T_min * (T_max/T_min)^(i/(n-1))
        ratio = (self.T_max / self.T_min) ** (1.0 / (self.num_replicas - 1))
        temperatures = [self.T_min * (ratio ** i) for i in range(self.num_replicas)]

        return temperatures

    def initialize_replicas(self, initial_assignments: List[Dict[str, bool]]):
        """
        Initialize replicas with assignments.

        Args:
            initial_assignments: List of initial assignments (one per replica)
        """
        self.replicas = []

        for i, temperature in enumerate(self.temperatures):
            assignment = initial_assignments[i]
            replica = Replica(temperature, assignment)
            self.replicas.append(replica)

    def attempt_swap(self, replica_i: Replica, replica_j: Replica) -> bool:
        """
        Attempt to swap configurations between two replicas.

        Uses Metropolis criterion for detailed balance.

        Args:
            replica_i: First replica
            replica_j: Second replica

        Returns:
            True if swap accepted
        """
        self.swap_attempts += 1

        # Compute swap probability (detailed balance)
        # P(swap) = min(1, exp(Δβ * ΔE))
        # where Δβ = (1/T_i - 1/T_j), ΔE = (E_i - E_j)

        beta_i = 1.0 / replica_i.temperature
        beta_j = 1.0 / replica_j.temperature

        delta_beta = beta_i - beta_j
        delta_energy = replica_i.energy - replica_j.energy

        # Swap probability
        log_prob = delta_beta * delta_energy

        if log_prob >= 0 or random.random() < math.exp(log_prob):
            # Accept swap - exchange configurations
            replica_i.assignment, replica_j.assignment = \
                replica_j.assignment, replica_i.assignment

            replica_i.energy, replica_j.energy = \
                replica_j.energy, replica_i.energy

            self.swap_successes += 1
            return True

        return False

    def attempt_all_swaps(self):
        """Attempt swaps between adjacent temperature replicas."""
        # Swap adjacent pairs
        # Use odd-even scheme to allow parallel swaps

        # Even pairs: (0,1), (2,3), (4,5), ...
        for i in range(0, self.num_replicas - 1, 2):
            self.attempt_swap(self.replicas[i], self.replicas[i + 1])

        # Odd pairs: (1,2), (3,4), (5,6), ...
        for i in range(1, self.num_replicas - 1, 2):
            self.attempt_swap(self.replicas[i], self.replicas[i + 1])

    def get_coldest_replica(self) -> Replica:
        """Get replica with lowest temperature (most refined)."""
        return self.replicas[0]  # First is coldest

    def get_best_replica(self) -> Replica:
        """Get replica with lowest energy (best solution)."""
        return min(self.replicas, key=lambda r: r.energy)

    def swap_acceptance_rate(self) -> float:
        """Get fraction of successful swaps."""
        if self.swap_attempts == 0:
            return 0.0
        return self.swap_successes / self.swap_attempts

    def get_statistics(self) -> Dict:
        """Get parallel tempering statistics."""
        return {
            'num_replicas': self.num_replicas,
            'temperatures': self.temperatures,
            'swap_attempts': self.swap_attempts,
            'swap_successes': self.swap_successes,
            'swap_acceptance_rate': self.swap_acceptance_rate(),
            'best_energy': self.get_best_replica().energy if self.replicas else None,
        }

    def __repr__(self):
        return (f"ParallelTempering(replicas={self.num_replicas}, "
                f"T_range=[{self.T_min:.2f}, {self.T_max:.2f}], "
                f"swap_rate={self.swap_acceptance_rate():.2%})")


def geometric_temperature_ladder(T_min: float, T_max: float, num_temps: int) -> List[float]:
    """
    Generate geometric temperature ladder.

    Args:
        T_min: Minimum temperature
        T_max: Maximum temperature
        num_temps: Number of temperatures

    Returns:
        List of temperatures
    """
    if num_temps == 1:
        return [T_min]

    ratio = (T_max / T_min) ** (1.0 / (num_temps - 1))
    return [T_min * (ratio ** i) for i in range(num_temps)]


def adaptive_cooling(current_temp: float,
                     recent_improvements: int,
                     total_steps: int) -> float:
    """
    Adaptive cooling rate based on performance.

    Slows cooling when making progress, speeds up when stuck.

    Args:
        current_temp: Current temperature
        recent_improvements: Number of improvements in recent steps
        total_steps: Total steps in recent window

    Returns:
        New temperature
    """
    improvement_rate = recent_improvements / max(1, total_steps)

    if improvement_rate > 0.1:
        # Making good progress - slow cooling
        cooling_factor = 0.995
    elif improvement_rate > 0.01:
        # Some progress - normal cooling
        cooling_factor = 0.98
    else:
        # Stuck - fast cooling
        cooling_factor = 0.95

    return current_temp * cooling_factor
