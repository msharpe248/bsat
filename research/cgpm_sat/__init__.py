"""
Conflict Graph Pattern Mining SAT (CGPM-SAT) Solver

A SAT solver that builds a meta-level conflict graph to guide variable selection.
Analyzes patterns in conflicts to identify problematic variable interactions and
makes smarter branching decisions.
"""

from .cgpm_solver import CGPMSolver, solve_cgpm

__all__ = ['CGPMSolver', 'solve_cgpm']
