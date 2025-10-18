"""
CCG-SAT: Conflict Causality Graph SAT Solver

Uses causality analysis to guide restart decisions during CDCL search.
Tracks causal relationships between conflicts and learned clauses.

Partially novel: Related to CausalSAT (Yang 2023) but uses causality ONLINE
during solving (not post-hoc explanation).

Key innovation: Restart when root causes are old (stuck in bad search region).
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional

from bsat.cnf import CNFExpression, Clause
from bsat.cdcl import CDCLSolver, CDCLStats

from .causality_graph import CausalityGraph
from .root_cause_analyzer import RootCauseAnalyzer


class CCGStats(CDCLStats):
    """Extended statistics for CCG-SAT solver."""

    def __init__(self):
        super().__init__()
        self.causality_restarts = 0
        self.root_causes_detected = 0
        self.causality_nodes = 0

    def __str__(self):
        base_stats = super().__str__()
        ccg_stats = (
            f"  Causality restarts: {self.causality_restarts}\n"
            f"  Root causes detected: {self.root_causes_detected}\n"
            f"  Causality nodes: {self.causality_nodes}\n"
        )
        return base_stats.rstrip(')') + '\n' + ccg_stats + ')'


class CCGSATSolver(CDCLSolver):
    """
    CDCL solver with Conflict Causality Graph analysis.

    Tracks causal relationships between conflicts and uses root cause analysis
    to make intelligent restart decisions.

    Partially novel approach based on CausalSAT (Yang 2023).
    """

    def __init__(self,
                 cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000,
                 # CCG-specific parameters
                 use_causality: bool = True,
                 max_causality_nodes: int = 10000,
                 old_age_threshold: int = 5000):
        """
        Initialize CCG-SAT solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: VSIDS decay factor
            restart_base: Base interval for restarts
            learned_clause_limit: Max learned clauses
            use_causality: Enable causality analysis
            max_causality_nodes: Max nodes in causality graph (memory limit)
            old_age_threshold: Age threshold for "old" root causes
        """
        # Initialize base CDCL solver
        super().__init__(cnf, vsids_decay, restart_base, learned_clause_limit)

        # CCG parameters
        self.use_causality = use_causality
        self.old_age_threshold = old_age_threshold

        # Causality components
        if self.use_causality:
            self.causality_graph = CausalityGraph(max_nodes=max_causality_nodes)
            self.root_cause_analyzer = RootCauseAnalyzer(
                self.causality_graph,
                old_age_threshold=old_age_threshold
            )

        # Extended statistics
        self.stats = CCGStats()

    def _analyze_conflict(self, conflict_clause):
        """
        Override conflict analysis to track causality.

        Args:
            conflict_clause: The conflicting clause

        Returns:
            (learned_clause, backtrack_level)
        """
        # Call parent conflict analysis
        learned_clause, backtrack_level = super()._analyze_conflict(conflict_clause)

        # Track causality
        if self.use_causality:
            # Add conflict node to causality graph
            conflict_id = f"C{self.stats.conflicts}"

            # Find learned clauses that participated (simplified: all learned clauses)
            # In full implementation, would track which clauses were used in resolution
            participating = []  # Simplified for now

            self.causality_graph.add_conflict(conflict_id, participating)

            # Add learned clause node
            self.causality_graph.add_learned_clause(learned_clause, conflict_id)

            # Update statistics
            self.stats.causality_nodes = len(self.causality_graph.nodes)

        return learned_clause, backtrack_level

    def _should_restart(self) -> bool:
        """
        Override restart heuristic to use causality analysis.

        Returns:
            True if restart recommended
        """
        # Check causality-based restart
        if self.use_causality and self.stats.conflicts > 0:
            if self.stats.conflicts % 1000 == 0:  # Check periodically
                causality_restart = self.root_cause_analyzer.should_restart_based_on_causality(
                    self.stats.conflicts,
                    threshold=0.7
                )

                if causality_restart:
                    self.stats.causality_restarts += 1

                    # Update root cause statistics
                    root_causes = self.root_cause_analyzer.identify_root_causes(top_k=5)
                    if root_causes:
                        self.stats.root_causes_detected = len(root_causes)

                    return True

        # Fall back to standard restart heuristic
        return super()._should_restart()

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using causality-guided CDCL.

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
                return dict(self.assignment)

            # Make decision (phase: True by default)
            phase = True
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

                # Analyze conflict (with causality tracking)
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

                # Check for restart (causality-aware)
                if self._should_restart():
                    self._restart()
                    conflict = self._propagate()
                    if conflict is not None:
                        return None  # UNSAT
                    break

    def get_causality_statistics(self) -> dict:
        """Get detailed causality statistics."""
        stats = {
            'enabled': self.use_causality,
            'causality_restarts': self.stats.causality_restarts,
            'root_causes_detected': self.stats.root_causes_detected,
            'causality_nodes': self.stats.causality_nodes,
        }

        if self.use_causality:
            stats['graph'] = self.causality_graph.analyze_causality_chains()
            stats['root_cause_analysis'] = self.root_cause_analyzer.get_statistics()

        return stats


def solve_ccg_sat(cnf: CNFExpression,
                  use_causality: bool = True,
                  old_age_threshold: int = 5000,
                  max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using CCG-SAT (Conflict Causality Graph).

    Partially novel approach using causality analysis for restart decisions.

    Args:
        cnf: CNF formula to solve
        use_causality: Enable causality analysis
        old_age_threshold: Age threshold for old root causes
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Solution if SAT, None if UNSAT
    """
    solver = CCGSATSolver(
        cnf,
        use_causality=use_causality,
        old_age_threshold=old_age_threshold
    )
    return solver.solve(max_conflicts=max_conflicts)
