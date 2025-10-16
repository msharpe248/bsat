"""
Backbone-Based CDCL (BB-CDCL) Solver

A novel SAT solver that combines statistical sampling with systematic search.
Uses WalkSAT to identify backbone variables (variables that must have the same
value in all solutions), then runs CDCL on the reduced problem.
"""

from .bb_cdcl_solver import BBCDCLSolver, solve_bb_cdcl
from .backbone_detector import BackboneDetector

__all__ = ['BBCDCLSolver', 'solve_bb_cdcl', 'BackboneDetector']
