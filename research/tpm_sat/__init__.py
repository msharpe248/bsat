"""
TPM-SAT: Temporal Pattern Mining SAT Solver

A CDCL SAT solver enhanced with temporal pattern mining to learn from
and avoid repeating decision sequences that lead to conflicts.

Modules:
- pattern_miner: Mines temporal patterns from decision sequences
- pattern_matcher: Matches candidate decisions against known bad patterns
- tpm_solver: Main TPM-SAT solver integrating pattern mining with CDCL

Example usage:
    >>> from bsat import CNFExpression
    >>> from research.tpm_sat import TPMSATSolver, solve_tpm_sat
    >>>
    >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
    >>> result = solve_tpm_sat(cnf, window_size=5)
    >>> if result:
    ...     print(f"SAT: {result}")
"""

from .pattern_miner import TemporalPatternMiner, PatternStats
from .pattern_matcher import PatternMatcher
from .tpm_solver import TPMSATSolver, TPMStats, solve_tpm_sat

__all__ = [
    'TemporalPatternMiner',
    'PatternStats',
    'PatternMatcher',
    'TPMSATSolver',
    'TPMStats',
    'solve_tpm_sat',
]

__version__ = '0.1.0'
