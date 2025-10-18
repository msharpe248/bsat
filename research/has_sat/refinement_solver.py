"""
Refinement Solver for HAS-SAT

Implements abstraction-refinement loop:
1. Solve abstract problem at high level
2. If SAT, refine assignment to next level
3. If UNSAT at abstract level, original is UNSAT
4. Continue until concrete solution found
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional
from bsat.cnf import CNFExpression
from bsat.cdcl import CDCLSolver
from .abstraction_builder import AbstractionHierarchy


class RefinementResult:
    """Result of refinement step."""

    def __init__(self,
                 level: int,
                 status: str,  # 'SAT', 'UNSAT', 'REFINE'
                 assignment: Optional[Dict[str, bool]] = None):
        """
        Initialize refinement result.

        Args:
            level: Abstraction level
            status: Status ('SAT' = concrete solution, 'UNSAT' = proven UNSAT, 'REFINE' = need refinement)
            assignment: Variable assignment (if SAT or REFINE)
        """
        self.level = level
        self.status = status
        self.assignment = assignment if assignment else {}

    def __repr__(self):
        return f"RefinementResult(level={self.level}, status={self.status}, vars={len(self.assignment)})"


class RefinementSolver:
    """
    Implements abstraction-refinement for hierarchical SAT solving.

    Algorithm:
    1. Solve most abstract problem
    2. If UNSAT → original is UNSAT
    3. If SAT → refine to next level with assignment as constraint
    4. Repeat until concrete solution found
    """

    def __init__(self,
                 hierarchy: AbstractionHierarchy,
                 max_conflicts_per_level: int = 100000):
        """
        Initialize refinement solver.

        Args:
            hierarchy: Abstraction hierarchy
            max_conflicts_per_level: Max conflicts per abstraction level
        """
        self.hierarchy = hierarchy
        self.max_conflicts_per_level = max_conflicts_per_level

        # Statistics
        self.levels_solved = 0
        self.total_conflicts = 0
        self.refinements = 0

    def solve_with_refinement(self) -> Optional[Dict[str, bool]]:
        """
        Solve using abstraction-refinement.

        Returns:
            Solution if SAT, None if UNSAT
        """
        # Start at most abstract level
        for level_idx, level in enumerate(self.hierarchy.levels):
            result = self._solve_level(level_idx)

            self.levels_solved += 1

            if result.status == 'UNSAT':
                # UNSAT at abstract level is INCONCLUSIVE
                # The abstraction might be too restrictive (over-approximation)
                # We must fall back to solving the concrete problem
                # Continue to next level or solve concrete
                continue

            elif result.status == 'SAT':
                # Found concrete solution
                return result.assignment

            elif result.status == 'REFINE':
                # Need to refine to next level
                self.refinements += 1
                continue

        # Reached concrete level - solve original formula
        return self._solve_concrete()

    def _solve_level(self, level: int) -> RefinementResult:
        """
        Solve at specified abstraction level.

        Args:
            level: Abstraction level

        Returns:
            Refinement result
        """
        # Get abstract CNF at this level
        abstract_cnf = self.hierarchy.get_abstract_cnf(level)

        # Solve abstract problem
        solver = CDCLSolver(abstract_cnf)
        solution = solver.solve(max_conflicts=self.max_conflicts_per_level)

        # Update statistics
        self.total_conflicts += solver.stats.conflicts

        if solution is None:
            # UNSAT at abstract level
            return RefinementResult(level, 'UNSAT')

        # SAT at abstract level
        if level == len(self.hierarchy.levels) - 1:
            # This is most concrete level - we have solution
            concrete_assignment = self.hierarchy.refine_assignment(level, solution)
            return RefinementResult(level, 'SAT', concrete_assignment)
        else:
            # Need to refine to next level
            return RefinementResult(level, 'REFINE', solution)

    def _solve_concrete(self) -> Optional[Dict[str, bool]]:
        """
        Solve concrete (original) formula.

        Returns:
            Solution if SAT, None if UNSAT
        """
        # Solve original formula
        solver = CDCLSolver(self.hierarchy.cnf)
        solution = solver.solve(max_conflicts=self.max_conflicts_per_level)

        # Update statistics
        self.total_conflicts += solver.stats.conflicts

        return solution

    def get_statistics(self) -> dict:
        """Get refinement statistics."""
        return {
            'levels_solved': self.levels_solved,
            'total_conflicts': self.total_conflicts,
            'refinements': self.refinements,
            'avg_conflicts_per_level': (
                self.total_conflicts / self.levels_solved
                if self.levels_solved > 0 else 0
            ),
        }

    def __repr__(self):
        return (
            f"RefinementSolver("
            f"levels={len(self.hierarchy.levels)}, "
            f"solved={self.levels_solved}, "
            f"refinements={self.refinements})"
        )
