"""
HAS-SAT: Hierarchical Abstraction SAT Solver

Solves SAT using abstraction-refinement:
1. Build abstraction hierarchy (variable clusters)
2. Solve abstract problem at high level
3. Refine to concrete level if SAT

Related to hierarchical approaches in planning and model checking.
Educational implementation demonstrating abstraction-refinement concept.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Optional
from bsat.cnf import CNFExpression
from bsat.cdcl import CDCLSolver, CDCLStats

from .abstraction_builder import AbstractionHierarchy
from .refinement_solver import RefinementSolver


class HASStats(CDCLStats):
    """Extended statistics for HAS-SAT solver."""

    def __init__(self):
        super().__init__()
        self.abstraction_levels = 0
        self.levels_solved = 0
        self.refinements = 0
        self.abstract_conflicts = 0

    def __str__(self):
        base_stats = super().__str__()
        has_stats = (
            f"  Abstraction levels: {self.abstraction_levels}\n"
            f"  Levels solved: {self.levels_solved}\n"
            f"  Refinements: {self.refinements}\n"
            f"  Abstract conflicts: {self.abstract_conflicts}\n"
        )
        return base_stats.rstrip(')') + '\n' + has_stats + ')'


class HASSATSolver:
    """
    Hierarchical Abstraction SAT Solver.

    Uses abstraction-refinement to solve SAT:
    - High level: Solve with variable clusters (abstract)
    - Low level: Solve with individual variables (concrete)

    Educational implementation of hierarchical SAT solving.
    """

    def __init__(self,
                 cnf: CNFExpression,
                 num_levels: int = 2,
                 use_abstraction: bool = True,
                 max_conflicts_per_level: int = 100000):
        """
        Initialize HAS-SAT solver.

        Args:
            cnf: CNF formula to solve
            num_levels: Number of abstraction levels
            use_abstraction: Enable abstraction (if False, use standard CDCL)
            max_conflicts_per_level: Max conflicts per abstraction level
        """
        self.cnf = cnf
        self.num_levels = num_levels
        self.use_abstraction = use_abstraction
        self.max_conflicts_per_level = max_conflicts_per_level

        # Statistics
        self.stats = HASStats()

        # Abstraction components
        if self.use_abstraction:
            self.hierarchy = AbstractionHierarchy(cnf)
            self.hierarchy.build_hierarchy(num_levels)
            self.refinement_solver = RefinementSolver(
                self.hierarchy,
                max_conflicts_per_level
            )
            self.stats.abstraction_levels = len(self.hierarchy.levels)

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using hierarchical abstraction.

        Args:
            max_conflicts: Maximum total conflicts across all levels

        Returns:
            Solution if SAT, None if UNSAT
        """
        if not self.use_abstraction:
            # Fall back to standard CDCL
            solver = CDCLSolver(self.cnf)
            result = solver.solve(max_conflicts=max_conflicts)
            self._update_stats_from_cdcl(solver.stats)
            return result

        # Solve with abstraction-refinement
        result = self.refinement_solver.solve_with_refinement()

        # Update statistics
        ref_stats = self.refinement_solver.get_statistics()
        self.stats.levels_solved = ref_stats['levels_solved']
        self.stats.refinements = ref_stats['refinements']
        self.stats.abstract_conflicts = ref_stats['total_conflicts']
        self.stats.conflicts = ref_stats['total_conflicts']

        # Verify solution if found
        if result is not None:
            if not self._verify_solution(result):
                # Solution doesn't satisfy original formula
                # This can happen if abstraction lost information
                # Fall back to concrete solving
                return self._solve_concrete_fallback(max_conflicts)

        return result

    def _verify_solution(self, assignment: Dict[str, bool]) -> bool:
        """
        Verify that assignment satisfies original formula.

        Args:
            assignment: Variable assignment

        Returns:
            True if valid solution
        """
        for clause in self.cnf.clauses:
            satisfied = False
            for lit in clause.literals:
                if lit.variable in assignment:
                    value = assignment[lit.variable]
                    if (not lit.negated and value) or (lit.negated and not value):
                        satisfied = True
                        break

            if not satisfied:
                return False

        return True

    def _solve_concrete_fallback(self, max_conflicts: int) -> Optional[Dict[str, bool]]:
        """
        Fall back to concrete solving if abstraction fails.

        Args:
            max_conflicts: Max conflicts

        Returns:
            Solution if SAT, None if UNSAT
        """
        solver = CDCLSolver(self.cnf)
        result = solver.solve(max_conflicts=max_conflicts)
        self._update_stats_from_cdcl(solver.stats)
        return result

    def _update_stats_from_cdcl(self, cdcl_stats: CDCLStats):
        """Update stats from CDCL solver."""
        self.stats.decisions = cdcl_stats.decisions
        self.stats.conflicts = cdcl_stats.conflicts
        self.stats.propagations = cdcl_stats.propagations
        self.stats.restarts = cdcl_stats.restarts
        self.stats.backjumps = cdcl_stats.backjumps
        self.stats.learned_clauses = cdcl_stats.learned_clauses
        self.stats.max_decision_level = cdcl_stats.max_decision_level

    def get_abstraction_statistics(self) -> dict:
        """Get detailed abstraction statistics."""
        stats = {
            'enabled': self.use_abstraction,
            'abstraction_levels': self.stats.abstraction_levels,
            'levels_solved': self.stats.levels_solved,
            'refinements': self.stats.refinements,
            'abstract_conflicts': self.stats.abstract_conflicts,
        }

        if self.use_abstraction:
            # Add hierarchy details
            stats['hierarchy'] = {
                'total_levels': len(self.hierarchy.levels),
                'levels': [
                    {
                        'level': level.level,
                        'clusters': len(level.clusters),
                        'abstract_vars': len(level.clusters),
                    }
                    for level in self.hierarchy.levels
                ],
            }

            # Add refinement solver stats
            if hasattr(self, 'refinement_solver'):
                stats['refinement'] = self.refinement_solver.get_statistics()

        return stats

    def get_stats(self) -> HASStats:
        """Get solver statistics."""
        return self.stats


def solve_has_sat(cnf: CNFExpression,
                  use_abstraction: bool = True,
                  num_levels: int = 2,
                  max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using HAS-SAT (Hierarchical Abstraction).

    Educational implementation demonstrating abstraction-refinement.

    Args:
        cnf: CNF formula to solve
        use_abstraction: Enable abstraction
        num_levels: Number of abstraction levels
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Solution if SAT, None if UNSAT
    """
    solver = HASSATSolver(
        cnf,
        num_levels=num_levels,
        use_abstraction=use_abstraction
    )
    return solver.solve(max_conflicts=max_conflicts)
