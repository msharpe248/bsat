"""
Lookahead-Enhanced CDCL (LA-CDCL) Solver

A SAT solver that enhances CDCL with lookahead to make better variable selection decisions.
Before branching, performs shallow lookahead on top candidates to predict which leads to fewer conflicts.
"""

from .la_cdcl_solver import LACDCLSolver, solve_la_cdcl

__all__ = ['LACDCLSolver', 'solve_la_cdcl']
