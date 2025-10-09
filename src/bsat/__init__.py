"""Boolean Satisfiability (SAT) package for CNF expressions."""

from .cnf import Literal, Clause, CNFExpression
from .twosatsolver import TwoSATSolver, solve_2sat, is_2sat_satisfiable, is_2sat
from .dpll import DPLLSolver, solve_sat
from .hornsat import HornSATSolver, solve_horn_sat, is_horn_formula
from .xorsat import XORSATSolver, solve_xorsat, get_xorsat_stats
from .walksat import WalkSATSolver, solve_walksat, get_walksat_stats
from .cdcl import CDCLSolver, solve_cdcl, get_cdcl_stats
from .schoening import SchoeningSolver, solve_schoening, get_schoening_stats
from .reductions import (
    reduce_to_3sat,
    extract_original_solution,
    solve_with_reduction,
    is_3sat,
    get_max_clause_size,
    ReductionStats
)
from .dimacs import (
    parse_dimacs,
    to_dimacs,
    read_dimacs_file,
    write_dimacs_file,
    parse_dimacs_solution,
    solution_to_dimacs,
    DIMACSParseError
)

__all__ = [
    'Literal',
    'Clause',
    'CNFExpression',
    'TwoSATSolver',
    'solve_2sat',
    'is_2sat_satisfiable',
    'is_2sat',
    'DPLLSolver',
    'solve_sat',
    'HornSATSolver',
    'solve_horn_sat',
    'is_horn_formula',
    'XORSATSolver',
    'solve_xorsat',
    'get_xorsat_stats',
    'WalkSATSolver',
    'solve_walksat',
    'get_walksat_stats',
    'CDCLSolver',
    'solve_cdcl',
    'get_cdcl_stats',
    'SchoeningSolver',
    'solve_schoening',
    'get_schoening_stats',
    'reduce_to_3sat',
    'extract_original_solution',
    'solve_with_reduction',
    'is_3sat',
    'get_max_clause_size',
    'ReductionStats',
    'parse_dimacs',
    'to_dimacs',
    'read_dimacs_file',
    'write_dimacs_file',
    'parse_dimacs_solution',
    'solution_to_dimacs',
    'DIMACSParseError'
]
__version__ = '0.1.0'
