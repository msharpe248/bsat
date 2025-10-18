"""
Multi-Armed Bandit Tracker for MAB-SAT

Tracks per-variable statistics and computes UCB1 scores for exploration/exploitation.
Educational implementation of MapleSAT's LRB approach and Kissat's MAB heuristics.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional
import math


class VariableStats:
    """Statistics for a single variable (one arm in the bandit)."""

    def __init__(self):
        """Initialize variable statistics."""
        self.times_selected = 0
        self.total_reward = 0.0
        self.avg_reward = 0.0
        self.last_reward = 0.0

    def update(self, reward: float):
        """
        Update statistics after selecting this variable.

        Args:
            reward: Reward received from this selection
        """
        self.times_selected += 1
        self.total_reward += reward
        self.avg_reward = self.total_reward / self.times_selected
        self.last_reward = reward

    def __repr__(self):
        return (
            f"VariableStats("
            f"selected={self.times_selected}, "
            f"avg_reward={self.avg_reward:.2f})"
        )


class BanditTracker:
    """
    Tracks variable selection statistics using Multi-Armed Bandit framework.

    Uses UCB1 (Upper Confidence Bound) algorithm to balance:
    - Exploitation: Choose variables with high average reward
    - Exploration: Try variables we haven't selected much

    Based on MapleSAT's LRB and Kissat's MAB heuristics.
    """

    def __init__(self, variables: List[str], exploration_constant: float = 1.4):
        """
        Initialize bandit tracker.

        Args:
            variables: List of variable names
            exploration_constant: UCB1 exploration parameter (default: sqrt(2) ≈ 1.4)
        """
        self.variables = variables
        self.exploration_constant = exploration_constant

        # Per-variable statistics
        self.stats: Dict[str, VariableStats] = {
            var: VariableStats() for var in variables
        }

        # Global selection counter
        self.total_selections = 0

        # Statistics
        self.exploration_decisions = 0
        self.exploitation_decisions = 0

    def compute_ucb1(self, variable: str) -> float:
        """
        Compute UCB1 score for a variable.

        UCB1 formula: exploitation + exploration
        - Exploitation term: avg_reward (how good this variable has been)
        - Exploration term: c * sqrt(ln(total) / times_selected)
          (higher for less-explored variables)

        Args:
            variable: Variable name

        Returns:
            UCB1 score (higher = should select this variable)
        """
        stats = self.stats[variable]

        # Never-selected variables get infinite UCB (explore first)
        if stats.times_selected == 0:
            return float('inf')

        # Exploitation: average reward
        exploitation = stats.avg_reward

        # Exploration: bonus for less-tried variables
        if self.total_selections > 0:
            exploration = self.exploration_constant * math.sqrt(
                math.log(self.total_selections) / stats.times_selected
            )
        else:
            exploration = 0.0

        return exploitation + exploration

    def select_variable(self, candidates: List[str]) -> str:
        """
        Select variable using UCB1.

        Args:
            candidates: List of unassigned variables to choose from

        Returns:
            Selected variable name
        """
        if not candidates:
            raise ValueError("No candidates to select from")

        # Compute UCB1 for all candidates
        ucb_scores = {var: self.compute_ucb1(var) for var in candidates}

        # Select variable with highest UCB1 score
        selected = max(candidates, key=lambda v: ucb_scores[v])

        # Track exploration vs. exploitation
        stats = self.stats[selected]
        if stats.times_selected == 0:
            self.exploration_decisions += 1
        else:
            self.exploitation_decisions += 1

        return selected

    def record_reward(self, variable: str, reward: float):
        """
        Record reward for a variable selection.

        Args:
            variable: Variable that was selected
            reward: Reward received from this selection
        """
        self.stats[variable].update(reward)
        self.total_selections += 1

    def get_variable_stats(self, variable: str) -> Optional[VariableStats]:
        """Get statistics for a specific variable."""
        return self.stats.get(variable, None)

    def get_summary_statistics(self) -> dict:
        """Get overall bandit statistics."""
        total_vars = len(self.variables)
        selected_vars = sum(1 for s in self.stats.values() if s.times_selected > 0)
        avg_reward = sum(s.avg_reward for s in self.stats.values()) / total_vars

        return {
            'total_selections': self.total_selections,
            'total_variables': total_vars,
            'variables_selected': selected_vars,
            'avg_reward': avg_reward,
            'exploration_decisions': self.exploration_decisions,
            'exploitation_decisions': self.exploitation_decisions,
            'exploration_rate': (self.exploration_decisions / self.total_selections * 100)
                              if self.total_selections > 0 else 0.0,
        }

    def __repr__(self):
        stats = self.get_summary_statistics()
        return (
            f"BanditTracker("
            f"selections={stats['total_selections']}, "
            f"exploration_rate={stats['exploration_rate']:.1f}%)"
        )
