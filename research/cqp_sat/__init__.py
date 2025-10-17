"""
CQP-SAT: Clause Quality Prediction SAT Solver

Educational reimplementation of Glucose's clause quality prediction approach.
Uses Literal Block Distance (LBD) to manage learned clause database efficiently.

Based on: Audemard & Simon (2009) - "Predicting Learnt Clauses Quality in Modern SAT Solvers"

Modules:
- clause_features: Extract features from learned clauses (LBD, activity, usage)
- quality_predictor: Predict clause quality and make deletion decisions
- cqp_solver: Main CQP-SAT solver with quality-aware clause management

Example usage:
    >>> from bsat import CNFExpression
    >>> from research.cqp_sat import CQPSATSolver, solve_cqp_sat
    >>>
    >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
    >>> result = solve_cqp_sat(cnf, glue_threshold=2)
    >>> if result:
    ...     print(f"SAT: {result}")
"""

from .clause_features import ClauseFeatures, ClauseFeatureExtractor
from .quality_predictor import QualityPredictor
from .cqp_solver import CQPSATSolver, CQPStats, solve_cqp_sat

__all__ = [
    'ClauseFeatures',
    'ClauseFeatureExtractor',
    'QualityPredictor',
    'CQPSATSolver',
    'CQPStats',
    'solve_cqp_sat',
]

__version__ = '0.1.0'
