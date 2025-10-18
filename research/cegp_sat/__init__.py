"""
CEGP-SAT: Clause Evolution with Genetic Programming

Educational/experimental approach using genetic operators to evolve
learned clauses for better propagation effectiveness.

Exports:
    - GeneticOperators: Crossover, mutation, selection operators
    - FitnessEvaluator, ClauseFitness: Fitness evaluation for clauses
    - CEGPSATSolver, CEGPStats: Main solver with clause evolution
    - solve_cegp_sat: Convenience function for solving
"""

from .genetic_operators import GeneticOperators
from .fitness_evaluator import FitnessEvaluator, ClauseFitness
from .cegp_solver import CEGPSATSolver, CEGPStats, solve_cegp_sat

__all__ = [
    'GeneticOperators',
    'FitnessEvaluator',
    'ClauseFitness',
    'CEGPSATSolver',
    'CEGPStats',
    'solve_cegp_sat',
]
