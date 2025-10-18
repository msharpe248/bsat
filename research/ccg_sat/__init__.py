"""
CCG-SAT: Conflict Causality Graph SAT Solver

Partially novel approach using causality analysis to guide restart decisions.
Related to CausalSAT (Yang 2023) but uses causality ONLINE during solving.

Exports:
    - CausalityGraph, CausalityNode: Causality graph data structures
    - RootCauseAnalyzer: Root cause analysis for restart decisions
    - CCGSATSolver, CCGStats: Main solver with causality tracking
    - solve_ccg_sat: Convenience function for solving
"""

from .causality_graph import CausalityGraph, CausalityNode
from .root_cause_analyzer import RootCauseAnalyzer
from .ccg_solver import CCGSATSolver, CCGStats, solve_ccg_sat

__all__ = [
    'CausalityGraph',
    'CausalityNode',
    'RootCauseAnalyzer',
    'CCGSATSolver',
    'CCGStats',
    'solve_ccg_sat',
]
