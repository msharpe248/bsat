"""
CQP-SAT: Clause Quality Prediction SAT Solver

Educational reimplementation of Glucose's clause quality prediction approach.
Uses Literal Block Distance (LBD) to predict clause usefulness and manage
learned clause database efficiently.

Based on foundational work:
- Audemard & Simon (2009): "Predicting Learnt Clauses Quality in Modern SAT Solvers"
- Glucose SAT solver

This is an EDUCATIONAL REIMPLEMENTATION demonstrating how Glucose works.
NOT a novel contribution - clearly cites prior art.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional
from collections import defaultdict

from bsat.cnf import CNFExpression, Clause
from bsat.cdcl import CDCLSolver, CDCLStats

from .clause_features import ClauseFeatureExtractor, ClauseFeatures
from .quality_predictor import QualityPredictor


class CQPStats(CDCLStats):
    """Extended statistics for CQP-SAT solver."""

    def __init__(self):
        super().__init__()
        self.clauses_learned_total = 0
        self.clauses_deleted = 0
        self.glue_clauses = 0  # LBD ≤ 2
        self.database_reductions = 0
        self.avg_lbd = 0.0

    def __str__(self):
        base_stats = super().__str__()
        cqp_stats = (
            f"  Learned clauses (total): {self.clauses_learned_total}\n"
            f"  Clauses deleted: {self.clauses_deleted}\n"
            f"  Glue clauses (LBD≤2): {self.glue_clauses}\n"
            f"  Database reductions: {self.database_reductions}\n"
            f"  Average LBD: {self.avg_lbd:.2f}\n"
        )
        return base_stats.rstrip(')') + '\n' + cqp_stats + ')'


class CQPSATSolver(CDCLSolver):
    """
    CDCL solver with Clause Quality Prediction.

    Implements Glucose's approach to learned clause management:
    - Extract features (LBD, activity, usage) from learned clauses
    - Predict clause quality based on features
    - Keep high-quality clauses (glue clauses with LBD ≤ 2)
    - Aggressively delete low-quality clauses

    Educational reimplementation of Audemard & Simon (2009).

    Example:
        >>> from bsat import CNFExpression
        >>> from research.cqp_sat import CQPSATSolver
        >>>
        >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
        >>> solver = CQPSATSolver(cnf, use_quality_prediction=True)
        >>> result = solver.solve()
    """

    def __init__(self,
                 cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000,
                 # CQP-specific parameters
                 use_quality_prediction: bool = True,
                 lbd_threshold: int = 30,
                 glue_threshold: int = 2,
                 reduction_interval: int = 2000):
        """
        Initialize CQP-SAT solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: VSIDS decay factor
            restart_base: Base interval for restarts
            learned_clause_limit: Max learned clauses before reduction
            use_quality_prediction: Enable quality-based clause management
            lbd_threshold: LBD above this is low quality
            glue_threshold: LBD at or below this is "glue" (always keep)
            reduction_interval: Conflicts between database reductions
        """
        # Initialize base CDCL solver
        super().__init__(cnf, vsids_decay, restart_base, learned_clause_limit)

        # CQP parameters
        self.use_quality_prediction = use_quality_prediction
        self.lbd_threshold = lbd_threshold
        self.glue_threshold = glue_threshold
        self.reduction_interval = reduction_interval

        # Quality prediction components
        if self.use_quality_prediction:
            self.feature_extractor = ClauseFeatureExtractor()
            self.quality_predictor = QualityPredictor(
                lbd_threshold=lbd_threshold,
                activity_weight=0.1,
                age_penalty=0.01
            )
            self.quality_predictor.glue_threshold = glue_threshold

            # Track features for learned clauses
            self.clause_features: Dict[Clause, ClauseFeatures] = {}

            # Decision level tracking (for LBD computation)
            self.variable_decision_levels: Dict[str, int] = {}

        # Extended statistics
        self.stats = CQPStats()
        self.conflicts_since_reduction = 0

    def _assign(self, variable: str, value: bool, antecedent=None):
        """Override to track decision levels for LBD computation."""
        # Call parent assignment
        super()._assign(variable, value, antecedent)

        # Track decision level for this variable (needed for LBD)
        if self.use_quality_prediction:
            self.variable_decision_levels[variable] = self.decision_level

    def _add_learned_clause(self, clause: Clause):
        """
        Override to extract features and predict quality when learning clause.

        Args:
            clause: The learned clause
        """
        # Call parent to add clause
        super()._add_learned_clause(clause)

        self.stats.clauses_learned_total += 1

        # Extract features and predict quality
        if self.use_quality_prediction:
            features = self.feature_extractor.extract_features(
                clause,
                self.variable_decision_levels,
                self.vsids_scores,
                self.decision_level
            )

            # Store features
            self.clause_features[clause] = features

            # Track glue clauses
            if features.lbd is not None and features.lbd <= self.glue_threshold:
                self.stats.glue_clauses += 1

            # Update average LBD
            if features.lbd is not None:
                n = self.stats.clauses_learned_total
                self.stats.avg_lbd = ((n - 1) * self.stats.avg_lbd + features.lbd) / n

    def _should_reduce_database(self) -> bool:
        """
        Check if we should reduce the learned clause database.

        Glucose reduces periodically based on conflict count.

        Returns:
            True if reduction should happen
        """
        if not self.use_quality_prediction:
            # Use parent's logic (simple limit)
            learned_count = len(self.clauses) - self.num_original_clauses
            return learned_count > self.learned_clause_limit

        # Reduce based on interval
        return self.conflicts_since_reduction >= self.reduction_interval

    def _reduce_database(self):
        """
        Reduce learned clause database using quality prediction.

        Implements Glucose strategy:
        1. Never delete glue clauses (LBD ≤ 2)
        2. Rank clauses by quality
        3. Delete lowest quality clauses
        4. Keep database size manageable
        """
        if not self.use_quality_prediction:
            # Fall back to parent's simple reduction
            super()._reduce_database()
            return

        # Get learned clauses (clauses after original ones)
        learned_clauses = self.clauses[self.num_original_clauses:]

        if not learned_clauses:
            return

        # Update ages
        for clause in learned_clauses:
            if clause in self.clause_features:
                self.clause_features[clause].age += 1

        # Collect features for all learned clauses
        features_list = []
        for clause in learned_clauses:
            if clause in self.clause_features:
                features_list.append(self.clause_features[clause])

        # Select clauses for deletion
        target_count = int(len(learned_clauses) * 0.5)  # Keep 50%
        to_delete = self.quality_predictor.select_for_deletion(
            features_list,
            target_count
        )

        # Delete low-quality clauses
        deleted_count = 0
        for features in to_delete:
            clause = features.clause
            if clause in self.clauses:
                self.clauses.remove(clause)
                del self.clause_features[clause]
                deleted_count += 1

        self.stats.clauses_deleted += deleted_count
        self.stats.database_reductions += 1
        self.conflicts_since_reduction = 0

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using quality-aware CDCL.

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

            # Make decision (phase: True by default, CQP doesn't focus on phase selection)
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
                self.conflicts_since_reduction += 1

                if self.decision_level == 0:
                    return None  # UNSAT

                # Analyze conflict and learn clause
                learned_clause, backtrack_level = self._analyze_conflict(conflict)

                if backtrack_level < 0:
                    return None  # UNSAT

                # Add learned clause (with quality tracking)
                self._add_learned_clause(learned_clause)

                # Check if database reduction needed
                if self._should_reduce_database():
                    self._reduce_database()

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

    def get_quality_statistics(self) -> dict:
        """Get detailed clause quality statistics."""
        stats = {
            'enabled': self.use_quality_prediction,
            'lbd_threshold': self.lbd_threshold,
            'glue_threshold': self.glue_threshold,
        }

        if self.use_quality_prediction and self.clause_features:
            features_list = list(self.clause_features.values())
            quality_stats = self.quality_predictor.get_statistics(features_list)
            stats.update(quality_stats)

        return stats


def solve_cqp_sat(cnf: CNFExpression,
                  use_quality_prediction: bool = True,
                  glue_threshold: int = 2,
                  max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using CQP-SAT (Clause Quality Prediction).

    Educational reimplementation of Glucose's approach.

    Args:
        cnf: CNF formula to solve
        use_quality_prediction: Enable quality-based clause management
        glue_threshold: LBD threshold for glue clauses (default: 2)
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Solution if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z)")
        >>> result = solve_cqp_sat(cnf)
    """
    solver = CQPSATSolver(
        cnf,
        use_quality_prediction=use_quality_prediction,
        glue_threshold=glue_threshold
    )
    return solver.solve(max_conflicts=max_conflicts)
