"""
MAB-SAT: Multi-Armed Bandit SAT Solver

Educational reimplementation of MapleSAT's LRB and Kissat's MAB heuristics.
Uses UCB1 for variable selection to balance exploration and exploitation.

Based on:
- MapleSAT's Learning Rate Branching (LRB)
- Kissat's MAB-based heuristic selection
- Classic UCB1 algorithm from reinforcement learning

This is an EDUCATIONAL REIMPLEMENTATION - NOT a novel contribution.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional

from bsat.cnf import CNFExpression
from bsat.cdcl import CDCLSolver, CDCLStats

from .bandit_tracker import BanditTracker
from .reward_functions import RewardComputer, HybridReward


class MABStats(CDCLStats):
    """Extended statistics for MAB-SAT solver."""

    def __init__(self):
        super().__init__()
        self.ucb1_decisions = 0
        self.exploration_decisions = 0
        self.exploitation_decisions = 0
        self.avg_reward = 0.0

    def __str__(self):
        base_stats = super().__str__()
        mab_stats = (
            f"  UCB1 decisions: {self.ucb1_decisions}\n"
            f"  Exploration: {self.exploration_decisions}\n"
            f"  Exploitation: {self.exploitation_decisions}\n"
            f"  Average reward: {self.avg_reward:.2f}\n"
        )
        return base_stats.rstrip(')') + '\n' + mab_stats + ')'


class MABSATSolver(CDCLSolver):
    """
    CDCL solver with Multi-Armed Bandit variable selection.

    Uses UCB1 to balance exploration (trying new variables) and exploitation
    (choosing variables that historically led to good outcomes).

    Educational implementation of MapleSAT and Kissat approaches.
    """

    def __init__(self,
                 cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000,
                 # MAB-specific parameters
                 use_mab: bool = True,
                 exploration_constant: float = 1.4):
        """
        Initialize MAB-SAT solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: VSIDS decay factor
            restart_base: Base interval for restarts
            learned_clause_limit: Max learned clauses
            use_mab: Enable MAB-based variable selection
            exploration_constant: UCB1 exploration parameter (sqrt(2) ≈ 1.4)
        """
        # Initialize base CDCL solver
        super().__init__(cnf, vsids_decay, restart_base, learned_clause_limit)

        # MAB parameters
        self.use_mab = use_mab
        self.exploration_constant = exploration_constant

        # MAB components
        if self.use_mab:
            self.bandit = BanditTracker(self.variables, exploration_constant)
            self.reward_computer = RewardComputer(HybridReward())

            # Track current decision for reward computation
            self.current_decision_var: Optional[str] = None
            self.propagations_before_decision = 0

        # Extended statistics
        self.stats = MABStats()

    def _pick_branching_variable(self) -> Optional[str]:
        """
        Override variable selection to use UCB1.

        Returns:
            Selected variable or None if all assigned
        """
        unassigned = [var for var in self.variables if var not in self.assignment]
        if not unassigned:
            return None

        if not self.use_mab:
            # Fall back to VSIDS
            return max(unassigned, key=lambda v: self.vsids_scores[v])

        # Use UCB1 for selection
        selected = self.bandit.select_variable(unassigned)
        self.stats.ucb1_decisions += 1

        # Track exploration vs. exploitation
        var_stats = self.bandit.get_variable_stats(selected)
        if var_stats and var_stats.times_selected == 0:
            self.stats.exploration_decisions += 1
        else:
            self.stats.exploitation_decisions += 1

        # Remember this decision for reward computation
        self.current_decision_var = selected
        self.propagations_before_decision = len(self.trail)

        return selected

    def _compute_and_record_reward(self,
                                  conflict: bool = False,
                                  backtrack_distance: int = 0,
                                  found_solution: bool = False):
        """
        Compute reward for last decision and update bandit.

        Args:
            conflict: Whether decision led to conflict
            backtrack_distance: How many levels we backtracked
            found_solution: Whether solution was found
        """
        if not self.use_mab or not self.current_decision_var:
            return

        # Count propagations that happened after decision
        propagations_after = len(self.trail) - self.propagations_before_decision

        # Compute reward
        reward = self.reward_computer.compute_reward(
            propagations=propagations_after,
            conflict=conflict,
            backtrack_distance=backtrack_distance,
            found_solution=found_solution
        )

        # Update bandit statistics
        self.bandit.record_reward(self.current_decision_var, reward)

        # Update solver stats
        reward_stats = self.reward_computer.get_statistics()
        self.stats.avg_reward = reward_stats['avg_reward']

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using MAB-guided CDCL.

        Args:
            max_conflicts: Maximum conflicts before giving up

        Returns:
            Solution if SAT, None if UNSAT
        """
        # Check for empty clause
        for clause in self.clauses:
            if len(clause.literals) == 0:
                return None

        # Initial unit propagation
        conflict = self._propagate()
        if conflict is not None:
            return None  # UNSAT at level 0

        while True:
            # Check conflict limit
            if self.stats.conflicts >= max_conflicts:
                return None  # Give up

            # Pick branching variable (UCB1)
            var = self._pick_branching_variable()

            if var is None:
                # All variables assigned - SAT!
                solution = dict(self.assignment)

                # Record solution reward
                if self.use_mab:
                    self._compute_and_record_reward(found_solution=True)

                return solution

            # Make decision (phase: True by default)
            phase = True
            self.decision_level += 1
            self.stats.max_decision_level = max(self.stats.max_decision_level, self.decision_level)
            self._assign(var, phase)

            # Propagate
            while True:
                conflict = self._propagate()

                if conflict is None:
                    # No conflict - decision was good
                    if self.use_mab:
                        self._compute_and_record_reward(conflict=False)
                    break

                # Conflict!
                self.stats.conflicts += 1

                if self.decision_level == 0:
                    return None  # UNSAT

                # Analyze conflict and learn clause
                learned_clause, backtrack_level = self._analyze_conflict(conflict)

                if backtrack_level < 0:
                    return None  # UNSAT

                # Compute backtrack distance for reward
                backtrack_distance = self.decision_level - backtrack_level

                # Record conflict reward
                if self.use_mab:
                    self._compute_and_record_reward(
                        conflict=True,
                        backtrack_distance=backtrack_distance
                    )

                # Add learned clause
                self._add_learned_clause(learned_clause)

                # Backtrack
                self._unassign_to_level(backtrack_level)
                self.decision_level = backtrack_level
                self.stats.backjumps += 1

                # Decay VSIDS scores
                self._decay_vsids_scores()

                # Check for restart
                if self._should_restart():
                    self._restart()
                    conflict = self._propagate()
                    if conflict is not None:
                        return None  # UNSAT
                    break

    def get_mab_statistics(self) -> dict:
        """Get detailed MAB statistics."""
        stats = {
            'enabled': self.use_mab,
            'exploration_constant': self.exploration_constant,
        }

        if self.use_mab:
            stats['bandit'] = self.bandit.get_summary_statistics()
            stats['rewards'] = self.reward_computer.get_statistics()

        return stats


def solve_mab_sat(cnf: CNFExpression,
                  use_mab: bool = True,
                  exploration_constant: float = 1.4,
                  max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using MAB-SAT (Multi-Armed Bandit).

    Educational implementation of MapleSAT/Kissat approach.

    Args:
        cnf: CNF formula to solve
        use_mab: Enable MAB-based variable selection
        exploration_constant: UCB1 exploration parameter
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Solution if SAT, None if UNSAT
    """
    solver = MABSATSolver(
        cnf,
        use_mab=use_mab,
        exploration_constant=exploration_constant
    )
    return solver.solve(max_conflicts=max_conflicts)
