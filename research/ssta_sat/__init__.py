"""
SSTA-SAT: Solution Space Topology Analysis SAT Solver

A CDCL SAT solver that analyzes the topological structure of the solution
space and uses this knowledge to guide search.

Modules:
- solution_sampler: Samples multiple solutions using WalkSAT
- topology_analyzer: Builds solution graph and computes topology metrics
- ssta_solver: Main SSTA-SAT solver integrating topology analysis with CDCL

Example usage:
    >>> from bsat import CNFExpression
    >>> from research.ssta_sat import SSTASATSolver, solve_ssta_sat
    >>>
    >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
    >>> result = solve_ssta_sat(cnf, num_samples=30)
    >>> if result:
    ...     print(f"SAT: {result}")
"""

from .solution_sampler import SolutionSampler
from .topology_analyzer import TopologyAnalyzer, SolutionGraph
from .ssta_solver import SSTASATSolver, SSTAStats, solve_ssta_sat

__all__ = [
    'SolutionSampler',
    'TopologyAnalyzer',
    'SolutionGraph',
    'SSTASATSolver',
    'SSTAStats',
    'solve_ssta_sat',
]

__version__ = '0.1.0'
