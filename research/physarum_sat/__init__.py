"""
PHYSARUM-SAT: Slime Mold-Inspired SAT Solver

Solves SAT using biologically-inspired network flow dynamics.
Educational implementation demonstrating bio-inspired computation.
"""

from .physarum_solver import PHYSARUMSATSolver, solve_physarum_sat, PhysarumStats
from .network_model import NetworkNode, NetworkEdge, SlimeMoldNetwork

__all__ = [
    'PHYSARUMSATSolver',
    'solve_physarum_sat',
    'PhysarumStats',
    'NetworkNode',
    'NetworkEdge',
    'SlimeMoldNetwork',
]
