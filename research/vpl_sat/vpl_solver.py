"""
VPL-SAT: Variable Phase Learning SAT Solver

A SAT solver that learns from conflict history to make intelligent phase
selection decisions. Tracks which phases (True/False) lead to conflicts vs.
successes and dynamically adjusts phase preferences.

Key Innovation:
- Dynamic per-variable phase learning from conflict/success ratios
- Adapts phase preferences based on recent search history
- Multiple selection strategies (conflict rate, adaptive, hybrid)

This appears partially novel - phase saving exists (Pipatsrisawat & Darwiche 2007),
but dynamic learning from conflict history per-variable is not well-covered.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional
from collections import defaultdict

from bsat.cnf import CNFExpression, Literal
from bsat.cdcl import CDCLSolver, CDCLStats

from .phase_tracker import PhaseTracker
from .phase_selector import PhaseSelector, AdaptiveStrategy, ConflictRateStrategy, HybridStrategy


class VPLStats(CDCLStats):
    """Extended statistics for VPL-SAT solver."""

    def __init__(self):
        super().__init__()
        self.learned_phase_decisions = 0
        self.saved_phase_decisions = 0
        self.phase_flips = 0
        self.phase_learning_hits = 0  # Times learned preference avoided conflict

    def __str__(self):
        base_stats = super().__str__()
        vpl_stats = (
            f"  Learned phase decisions: {self.learned_phase_decisions}\n"
            f"  Saved phase decisions: {self.saved_phase_decisions}\n"
            f"  Phase flips: {self.phase_flips}\n"
            f"  Phase learning hits: {self.phase_learning_hits}\n"
        )
        return base_stats.rstrip(')') + '\n' + vpl_stats + ')'


class VPLSATSolver(CDCLSolver):
    """
    CDCL solver with Variable Phase Learning.

    Learns from conflict history to make intelligent phase selection decisions,
    preferring phases that have historically led to fewer conflicts.

    Example:
        >>> from bsat import CNFExpression
        >>> from research.vpl_sat import VPLSATSolver
        >>>
        >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
        >>> solver = VPLSATSolver(cnf, use_phase_learning=True)
        >>> result = solver.solve()
    """

    def __init__(self,
                 cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000,
                 # VPL-specific parameters
                 use_phase_learning: bool = True,
                 window_size: int = 100,
                 threshold: float = 0.15,
                 strategy: str = 'adaptive'):
        """
        Initialize VPL-SAT solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: VSIDS decay factor
            restart_base: Base interval for restarts
            learned_clause_limit: Max learned clauses
            use_phase_learning: Enable phase learning
            window_size: Recent history window size
            threshold: Conflict rate difference threshold
            strategy: Phase selection strategy ('adaptive', 'conflict_rate', 'hybrid')
        """
        # Initialize base CDCL solver
        super().__init__(cnf, vsids_decay, restart_base, learned_clause_limit)

        # VPL parameters
        self.use_phase_learning = use_phase_learning
        self.window_size = window_size
        self.threshold = threshold
        self.strategy_name = strategy

        # Phase learning components
        if self.use_phase_learning:
            self.phase_tracker = PhaseTracker(self.variables, window_size)

            # Select strategy
            if strategy == 'adaptive':
                strategy_obj = AdaptiveStrategy(threshold)
            elif strategy == 'conflict_rate':
                strategy_obj = ConflictRateStrategy(threshold)
            elif strategy == 'hybrid':
                strategy_obj = HybridStrategy(threshold)
            else:
                raise ValueError(f"Unknown strategy: {strategy}")

            self.phase_selector = PhaseSelector(self.phase_tracker, strategy_obj)
        else:
            self.phase_tracker = None
            self.phase_selector = None

        # VSIDS phase saving (standard)
        self.saved_phases: Dict[str, bool] = {}

        # Extended statistics
        self.stats = VPLStats()

    def _assign(self, variable: str, value: bool, antecedent=None):
        """
        Override assignment to track phase decisions.

        Args:
            variable: Variable to assign
            value: Value to assign
            antecedent: Optional antecedent clause
        """
        # Call parent assignment
        super()._assign(variable, value, antecedent)

        # Save phase for VSIDS phase saving
        self.saved_phases[variable] = value

        # Track decision in phase tracker
        if self.use_phase_learning and self.phase_tracker:
            self.phase_tracker.record_decision(variable, value, self.decision_level)

    def _analyze_conflict(self, conflict_clause):
        """
        Override conflict analysis to track phase conflicts.

        Args:
            conflict_clause: The conflicting clause

        Returns:
            (learned_clause, backtrack_level)
        """
        # Call parent conflict analysis
        learned_clause, backtrack_level = super()._analyze_conflict(conflict_clause)

        # Track conflict in phase tracker
        if self.use_phase_learning and self.phase_tracker:
            # Extract (variable, phase) pairs from conflict
            conflict_vars = []
            for lit in conflict_clause.literals:
                var = lit.variable
                phase = not lit.negated  # True if positive literal was conflicting
                conflict_vars.append((var, phase))

            self.phase_tracker.record_conflict(conflict_vars, self.decision_level)

        return learned_clause, backtrack_level

    def _pick_branching_phase(self, variable: str) -> bool:
        """
        Override phase selection to use learned preferences.

        Args:
            variable: Variable to assign

        Returns:
            Chosen phase (True or False)
        """
        if not self.use_phase_learning or not self.phase_selector:
            # Fall back to VSIDS phase saving
            return self.saved_phases.get(variable, True)

        # Get saved phase
        saved_phase = self.saved_phases.get(variable, None)

        # Use learned phase selector
        chosen_phase = self.phase_selector.select_phase(variable, saved_phase)

        # Track statistics
        preferred = self.phase_tracker.get_preferred_phase(variable, self.threshold)
        if preferred is not None and chosen_phase == preferred:
            self.stats.learned_phase_decisions += 1
        elif saved_phase is not None and chosen_phase == saved_phase:
            self.stats.saved_phase_decisions += 1

        if saved_phase is not None and chosen_phase != saved_phase:
            self.stats.phase_flips += 1

        return chosen_phase

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using phase learning CDCL.

        Args:
            max_conflicts: Maximum conflicts before giving up

        Returns:
            Solution if SAT, None if UNSAT
        """
        # Check for empty clause
        for clause in self.clauses:
            if len(clause.literals) == 0:
                return None

        # Initial unit propagation
        conflict = self._propagate()
        if conflict is not None:
            return None  # UNSAT at level 0

        while True:
            # Check conflict limit
            if self.stats.conflicts >= max_conflicts:
                return None  # Give up

            # Pick branching variable (VSIDS)
            var = self._pick_branching_variable()

            if var is None:
                # All variables assigned - SAT!
                solution = dict(self.assignment)

                # Record success for assigned variables
                if self.use_phase_learning and self.phase_tracker:
                    for v, phase in solution.items():
                        self.phase_tracker.record_success(v, phase, self.decision_level)

                return solution

            # Pick phase (learned or saved)
            phase = self._pick_branching_phase(var)

            # Make decision
            self.decision_level += 1
            self.stats.max_decision_level = max(self.stats.max_decision_level, self.decision_level)
            self._assign(var, phase)

            # Propagate
            while True:
                conflict = self._propagate()

                if conflict is None:
                    break

                # Conflict!
                self.stats.conflicts += 1

                if self.decision_level == 0:
                    return None  # UNSAT

                # Analyze conflict and learn clause
                learned_clause, backtrack_level = self._analyze_conflict(conflict)

                if backtrack_level < 0:
                    return None  # UNSAT

                # Add learned clause
                self._add_learned_clause(learned_clause)

                # Backtrack
                self._unassign_to_level(backtrack_level)
                self.decision_level = backtrack_level
                self.stats.backjumps += 1

                # Decay VSIDS scores
                self._decay_vsids_scores()

                # Check for restart
                if self._should_restart():
                    self._restart()
                    conflict = self._propagate()
                    if conflict is not None:
                        return None  # UNSAT
                    break

    def get_phase_statistics(self) -> dict:
        """Get detailed phase learning statistics."""
        stats = {
            'enabled': self.use_phase_learning,
            'strategy': self.strategy_name,
            'window_size': self.window_size,
            'threshold': self.threshold,
        }

        if self.phase_tracker:
            stats['tracker'] = self.phase_tracker.get_summary_statistics()

        if self.phase_selector:
            stats['selector'] = self.phase_selector.get_statistics()

        return stats


def solve_vpl_sat(cnf: CNFExpression,
                  use_phase_learning: bool = True,
                  strategy: str = 'adaptive',
                  max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using VPL-SAT (Variable Phase Learning).

    Args:
        cnf: CNF formula to solve
        use_phase_learning: Enable phase learning
        strategy: Phase selection strategy ('adaptive', 'conflict_rate', 'hybrid')
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Solution if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z)")
        >>> result = solve_vpl_sat(cnf, strategy='adaptive')
    """
    solver = VPLSATSolver(
        cnf,
        use_phase_learning=use_phase_learning,
        strategy=strategy
    )
    return solver.solve(max_conflicts=max_conflicts)
