"""
FOLD-SAT: Protein Folding-Inspired SAT Solver

Solves SAT using energy minimization and simulated annealing.
Educational implementation demonstrating biophysical computation.
"""

from .fold_solver import FOLDSATSolver, solve_fold_sat, FoldStats
from .energy_landscape import EnergyLandscape
from .molecular_moves import (
    MolecularMove,
    SingleFlipMove,
    SwapMove,
    ClusterFlipMove,
    BiasedFlipMove,
    RandomMutation,
    MoveSelector
)
from .annealing_schedule import (
    AnnealingSchedule,
    ParallelTempering,
    Replica
)

__all__ = [
    'FOLDSATSolver',
    'solve_fold_sat',
    'FoldStats',
    'EnergyLandscape',
    'MolecularMove',
    'SingleFlipMove',
    'SwapMove',
    'ClusterFlipMove',
    'BiasedFlipMove',
    'RandomMutation',
    'MoveSelector',
    'AnnealingSchedule',
    'ParallelTempering',
    'Replica',
]
