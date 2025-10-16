"""
Community-Based Decomposition SAT (CoBD-SAT) Solver

A novel SAT solver that exploits community structure in the variable-clause
interaction graph to decompose problems into semi-independent sub-problems.
"""

from .cobd_solver import CoBDSATSolver, solve_cobd_sat
from .community_detector import CommunityDetector

__all__ = ['CoBDSATSolver', 'solve_cobd_sat', 'CommunityDetector']
