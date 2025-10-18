"""
Reward Functions for MAB-SAT

Computes rewards based on the outcome of variable selection decisions.
Different reward functions capture different aspects of search quality.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional


class RewardFunction:
    """Base class for reward functions."""

    def compute_reward(self,
                      propagations: int = 0,
                      conflict: bool = False,
                      backtrack_distance: int = 0,
                      found_solution: bool = False) -> float:
        """
        Compute reward for a variable selection.

        Args:
            propagations: Number of unit propagations after decision
            conflict: Whether decision led to immediate conflict
            backtrack_distance: How many levels we backtracked (if conflict)
            found_solution: Whether decision led to finding a solution

        Returns:
            Reward value (higher = better outcome)
        """
        raise NotImplementedError


class PropagationReward(RewardFunction):
    """
    Reward based on propagations.

    More propagations = good (reduces search space).
    Conflicts = bad.
    """

    def __init__(self, prop_weight: float = 0.5, conflict_penalty: float = 5.0):
        """
        Initialize propagation-based reward.

        Args:
            prop_weight: Reward per propagation
            conflict_penalty: Penalty for immediate conflict
        """
        self.prop_weight = prop_weight
        self.conflict_penalty = conflict_penalty

    def compute_reward(self,
                      propagations: int = 0,
                      conflict: bool = False,
                      backtrack_distance: int = 0,
                      found_solution: bool = False) -> float:
        """Compute reward based on propagations."""
        if found_solution:
            return 100.0  # Huge reward for finding solution

        if conflict:
            # Penalize conflicts
            backtrack_penalty = backtrack_distance * 0.5
            return -(self.conflict_penalty + backtrack_penalty)

        # Reward propagations
        return propagations * self.prop_weight


class ProgressReward(RewardFunction):
    """
    Reward based on overall search progress.

    Rewards:
    - Propagations (reduces search space)
    - Finding solution (highest reward)

    Penalties:
    - Conflicts
    - Deep backtracks (worse than shallow)
    """

    def __init__(self):
        """Initialize progress-based reward."""
        self.prop_weight = 0.3
        self.conflict_base_penalty = 3.0
        self.backtrack_penalty = 1.0
        self.solution_reward = 100.0

    def compute_reward(self,
                      propagations: int = 0,
                      conflict: bool = False,
                      backtrack_distance: int = 0,
                      found_solution: bool = False) -> float:
        """Compute reward based on progress."""
        if found_solution:
            return self.solution_reward

        if conflict:
            # Base conflict penalty + backtrack distance penalty
            penalty = self.conflict_base_penalty + (backtrack_distance * self.backtrack_penalty)
            return -penalty

        # Reward propagations (search space reduction)
        return propagations * self.prop_weight


class HybridReward(RewardFunction):
    """
    Hybrid reward combining multiple signals.

    Considers:
    - Immediate outcome (propagations vs. conflict)
    - Search depth (deep backtracks are worse)
    - Solution finding (ultimate success)
    """

    def __init__(self,
                 prop_weight: float = 0.4,
                 conflict_penalty: float = 4.0,
                 backtrack_penalty: float = 0.8,
                 solution_bonus: float = 100.0):
        """
        Initialize hybrid reward.

        Args:
            prop_weight: Weight per propagation
            conflict_penalty: Base penalty for conflict
            backtrack_penalty: Penalty per backtrack level
            solution_bonus: Bonus for finding solution
        """
        self.prop_weight = prop_weight
        self.conflict_penalty = conflict_penalty
        self.backtrack_penalty = backtrack_penalty
        self.solution_bonus = solution_bonus

    def compute_reward(self,
                      propagations: int = 0,
                      conflict: bool = False,
                      backtrack_distance: int = 0,
                      found_solution: bool = False) -> float:
        """Compute hybrid reward."""
        # Solution found - highest reward
        if found_solution:
            return self.solution_bonus

        # Conflict - penalty based on severity
        if conflict:
            base_penalty = self.conflict_penalty
            backtrack_cost = backtrack_distance * self.backtrack_penalty
            return -(base_penalty + backtrack_cost)

        # Normal decision - reward propagations
        return propagations * self.prop_weight


class RewardComputer:
    """
    Computes rewards for MAB-SAT using configurable reward function.

    Tracks decision outcomes and computes appropriate rewards.
    """

    def __init__(self, reward_function: RewardFunction = None):
        """
        Initialize reward computer.

        Args:
            reward_function: Reward function to use (default: HybridReward)
        """
        self.reward_function = reward_function if reward_function else HybridReward()

        # Statistics
        self.total_rewards_computed = 0
        self.positive_rewards = 0
        self.negative_rewards = 0
        self.total_reward = 0.0

    def compute_reward(self,
                      propagations: int = 0,
                      conflict: bool = False,
                      backtrack_distance: int = 0,
                      found_solution: bool = False) -> float:
        """
        Compute reward for a decision outcome.

        Args:
            propagations: Number of propagations after decision
            conflict: Whether decision led to conflict
            backtrack_distance: Backtrack distance (if conflict)
            found_solution: Whether solution was found

        Returns:
            Reward value
        """
        reward = self.reward_function.compute_reward(
            propagations=propagations,
            conflict=conflict,
            backtrack_distance=backtrack_distance,
            found_solution=found_solution
        )

        # Track statistics
        self.total_rewards_computed += 1
        self.total_reward += reward
        if reward > 0:
            self.positive_rewards += 1
        else:
            self.negative_rewards += 1

        return reward

    def get_statistics(self) -> dict:
        """Get reward computation statistics."""
        avg_reward = (self.total_reward / self.total_rewards_computed
                     if self.total_rewards_computed > 0 else 0.0)

        return {
            'total_computed': self.total_rewards_computed,
            'positive_rewards': self.positive_rewards,
            'negative_rewards': self.negative_rewards,
            'total_reward': self.total_reward,
            'avg_reward': avg_reward,
        }

    def __repr__(self):
        stats = self.get_statistics()
        return (
            f"RewardComputer("
            f"computed={stats['total_computed']}, "
            f"avg_reward={stats['avg_reward']:.2f})"
        )
