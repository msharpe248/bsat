"""
HAS-SAT: Hierarchical Abstraction SAT Solver

Educational implementation of abstraction-refinement for SAT solving.
Builds variable clusters and solves at multiple abstraction levels.

Exports:
    - AbstractionHierarchy, AbstractionLevel, VariableCluster: Abstraction structures
    - RefinementSolver, RefinementResult: Refinement loop
    - HASSATSolver, HASStats: Main solver with abstraction
    - solve_has_sat: Convenience function for solving
"""

from .abstraction_builder import (
    AbstractionHierarchy,
    AbstractionLevel,
    VariableCluster
)
from .refinement_solver import RefinementSolver, RefinementResult
from .has_solver import HASSATSolver, HASStats, solve_has_sat

__all__ = [
    'AbstractionHierarchy',
    'AbstractionLevel',
    'VariableCluster',
    'RefinementSolver',
    'RefinementResult',
    'HASSATSolver',
    'HASStats',
    'solve_has_sat',
]
