"""
VPL-SAT: Variable Phase Learning SAT Solver

A CDCL SAT solver that learns from conflict history to make intelligent
phase selection decisions.

Modules:
- phase_tracker: Tracks phase performance statistics (conflicts/successes per phase)
- phase_selector: Learned phase selection strategies
- vpl_solver: Main VPL-SAT solver integrating phase learning with CDCL

Example usage:
    >>> from bsat import CNFExpression
    >>> from research.vpl_sat import VPLSATSolver, solve_vpl_sat
    >>>
    >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
    >>> result = solve_vpl_sat(cnf, strategy='adaptive')
    >>> if result:
    ...     print(f"SAT: {result}")
"""

from .phase_tracker import PhaseTracker, PhaseStats
from .phase_selector import (
    PhaseSelector,
    PhaseSelectionStrategy,
    ConflictRateStrategy,
    AdaptiveStrategy,
    HybridStrategy
)
from .vpl_solver import VPLSATSolver, VPLStats, solve_vpl_sat

__all__ = [
    'PhaseTracker',
    'PhaseStats',
    'PhaseSelector',
    'PhaseSelectionStrategy',
    'ConflictRateStrategy',
    'AdaptiveStrategy',
    'HybridStrategy',
    'VPLSATSolver',
    'VPLStats',
    'solve_vpl_sat',
]

__version__ = '0.1.0'
