"""
Solution Sampler for SSTA-SAT

Samples multiple solutions from the solution space using WalkSAT with different
random seeds. These samples are used to analyze the topology of the solution space.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional, Set
import random

from bsat.cnf import CNFExpression
from bsat.walksat import WalkSATSolver


class SolutionSampler:
    """
    Samples multiple solutions from a SAT instance using WalkSAT.

    Uses different random seeds to find diverse solutions, which are then
    used to analyze the topology of the solution space.
    """

    def __init__(self,
                 cnf: CNFExpression,
                 max_flips: int = 10000,
                 noise: float = 0.5):
        """
        Initialize solution sampler.

        Args:
            cnf: CNF formula to sample from
            max_flips: Maximum flips per WalkSAT attempt
            noise: WalkSAT noise parameter (0.0-1.0)
        """
        self.cnf = cnf
        self.max_flips = max_flips
        self.noise = noise
        self.solutions: List[Dict[str, bool]] = []

    def sample(self,
               num_samples: int = 100,
               timeout_per_sample: int = 5,
               min_unique_solutions: int = 10) -> List[Dict[str, bool]]:
        """
        Sample multiple solutions using WalkSAT with different seeds.

        Args:
            num_samples: Number of sampling attempts
            timeout_per_sample: Timeout in seconds per attempt (not yet implemented)
            min_unique_solutions: Stop if we have this many unique solutions

        Returns:
            List of solution dictionaries (may contain duplicates)
        """
        self.solutions = []
        seen_solutions: Set[frozenset] = set()

        for seed in range(num_samples):
            # Use different seed for each attempt
            random.seed(seed)

            # Create WalkSAT solver
            solver = WalkSATSolver(
                self.cnf,
                noise=self.noise,
                max_flips=self.max_flips
            )

            # Try to find a solution
            solution = solver.solve()

            if solution is not None:
                # Convert to frozenset for uniqueness check
                sol_set = frozenset(solution.items())

                if sol_set not in seen_solutions:
                    seen_solutions.add(sol_set)
                    self.solutions.append(solution)

                    # Stop if we have enough unique solutions
                    if len(self.solutions) >= min_unique_solutions:
                        break

        return self.solutions

    def get_solutions(self) -> List[Dict[str, bool]]:
        """Get the sampled solutions."""
        return self.solutions

    def get_num_solutions(self) -> int:
        """Get the number of solutions sampled."""
        return len(self.solutions)

    def compute_hamming_distance(self,
                                   sol1: Dict[str, bool],
                                   sol2: Dict[str, bool]) -> int:
        """
        Compute Hamming distance between two solutions.

        The Hamming distance is the number of variables that have different
        values in the two solutions.

        Args:
            sol1: First solution
            sol2: Second solution

        Returns:
            Hamming distance (0 to n)
        """
        distance = 0
        all_vars = set(sol1.keys()) | set(sol2.keys())

        for var in all_vars:
            val1 = sol1.get(var, None)
            val2 = sol2.get(var, None)

            if val1 != val2:
                distance += 1

        return distance

    def compute_all_hamming_distances(self) -> List[List[int]]:
        """
        Compute Hamming distances between all pairs of solutions.

        Returns:
            2D matrix where matrix[i][j] = hamming distance between solution i and j
        """
        n = len(self.solutions)
        distances = [[0] * n for _ in range(n)]

        for i in range(n):
            for j in range(i + 1, n):
                dist = self.compute_hamming_distance(
                    self.solutions[i],
                    self.solutions[j]
                )
                distances[i][j] = dist
                distances[j][i] = dist

        return distances

    def get_average_hamming_distance(self) -> float:
        """
        Compute average Hamming distance across all solution pairs.

        Returns:
            Average Hamming distance (0.0 to n)
        """
        if len(self.solutions) < 2:
            return 0.0

        total_distance = 0
        num_pairs = 0

        for i in range(len(self.solutions)):
            for j in range(i + 1, len(self.solutions)):
                total_distance += self.compute_hamming_distance(
                    self.solutions[i],
                    self.solutions[j]
                )
                num_pairs += 1

        return total_distance / num_pairs if num_pairs > 0 else 0.0

    def find_closest_solution(self,
                               reference: Dict[str, bool]) -> Optional[Dict[str, bool]]:
        """
        Find the sampled solution closest to a reference assignment.

        Args:
            reference: Reference solution/assignment

        Returns:
            Closest solution from the sample set, or None if no solutions
        """
        if not self.solutions:
            return None

        min_distance = float('inf')
        closest = None

        for sol in self.solutions:
            dist = self.compute_hamming_distance(reference, sol)
            if dist < min_distance:
                min_distance = dist
                closest = sol

        return closest

    def get_statistics(self) -> dict:
        """Get sampling statistics."""
        if not self.solutions:
            return {
                'num_solutions': 0,
                'avg_hamming_distance': 0.0,
                'num_variables': 0,
            }

        num_vars = len(self.solutions[0]) if self.solutions else 0

        return {
            'num_solutions': len(self.solutions),
            'avg_hamming_distance': self.get_average_hamming_distance(),
            'num_variables': num_vars,
            'diversity': self.get_average_hamming_distance() / num_vars if num_vars > 0 else 0.0,
        }

    def __repr__(self):
        stats = self.get_statistics()
        return (
            f"SolutionSampler("
            f"solutions={stats['num_solutions']}, "
            f"avg_dist={stats['avg_hamming_distance']:.1f})"
        )
