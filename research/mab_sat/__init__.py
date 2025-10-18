"""
MAB-SAT: Multi-Armed Bandit SAT Solver

Educational reimplementation of MapleSAT's LRB and Kissat's MAB heuristics.
Uses UCB1 for variable selection to balance exploration and exploitation.

Modules:
- bandit_tracker: UCB1 statistics and variable selection
- reward_functions: Reward computation based on decision outcomes
- mab_solver: Main MAB-SAT solver integrating UCB1 with CDCL

Example usage:
    >>> from bsat import CNFExpression
    >>> from research.mab_sat import MABSATSolver, solve_mab_sat
    >>>
    >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
    >>> result = solve_mab_sat(cnf, use_mab=True)
    >>> if result:
    ...     print(f"SAT: {result}")
"""

from .bandit_tracker import BanditTracker, VariableStats
from .reward_functions import (
    RewardFunction,
    PropagationReward,
    ProgressReward,
    HybridReward,
    RewardComputer
)
from .mab_solver import MABSATSolver, MABStats, solve_mab_sat

__all__ = [
    'BanditTracker',
    'VariableStats',
    'RewardFunction',
    'PropagationReward',
    'ProgressReward',
    'HybridReward',
    'RewardComputer',
    'MABSATSolver',
    'MABStats',
    'solve_mab_sat',
]

__version__ = '0.1.0'
