"""
TPM-SAT: Temporal Pattern Mining SAT Solver

A CDCL SAT solver enhanced with temporal pattern mining to avoid repeating
decision sequences that historically lead to conflicts.

Key Innovation:
- Mines patterns (sequences of decisions) that frequently lead to conflicts
- Uses pattern matching to guide variable selection and phase choice
- Learns from past mistakes in a temporal/sequential manner

This appears to be novel - no prior work found connecting temporal pattern
mining to SAT variable selection.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional, Tuple
from collections import deque

from bsat.cnf import CNFExpression, Clause
from bsat.cdcl import CDCLSolver, CDCLStats

from .pattern_miner import TemporalPatternMiner
from .pattern_matcher import PatternMatcher


class TPMStats(CDCLStats):
    """Extended statistics for TPM-SAT solver."""

    def __init__(self):
        super().__init__()
        self.patterns_learned = 0
        self.bad_patterns_avoided = 0
        self.phase_flips_from_patterns = 0
        self.pattern_guided_decisions = 0

    def __str__(self):
        base_stats = super().__str__()
        tpm_stats = (
            f"  Patterns learned: {self.patterns_learned}\n"
            f"  Bad patterns avoided: {self.bad_patterns_avoided}\n"
            f"  Phase flips from patterns: {self.phase_flips_from_patterns}\n"
            f"  Pattern-guided decisions: {self.pattern_guided_decisions}\n"
        )
        return base_stats.rstrip(')') + '\n' + tpm_stats + ')'


class TPMSATSolver(CDCLSolver):
    """
    CDCL solver with Temporal Pattern Mining.

    Extends standard CDCL by:
    1. Mining temporal patterns (decision sequences) that lead to conflicts
    2. Avoiding variable/phase choices that would repeat bad patterns
    3. Learning from sequential mistake patterns, not just individual conflicts

    Example:
        >>> from bsat import CNFExpression
        >>> from research.tpm_sat import TPMSATSolver
        >>>
        >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c) & (c | d)")
        >>> solver = TPMSATSolver(cnf, window_size=5)
        >>> result = solver.solve()
        >>> if result:
        ...     print(f"SAT: {result}")
        ...     print(f"Stats: {solver.get_stats()}")
    """

    def __init__(self,
                 cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000,
                 # TPM-specific parameters
                 window_size: int = 5,
                 conflict_threshold: float = 0.8,
                 max_patterns: int = 10000,
                 use_pattern_guidance: bool = True):
        """
        Initialize TPM-SAT solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: VSIDS decay factor
            restart_base: Base interval for restarts
            learned_clause_limit: Max learned clauses
            window_size: Length of decision sequences to mine (2-10 typical)
            conflict_threshold: Conflict rate to consider pattern "bad" (0.7-0.9 typical)
            max_patterns: Maximum patterns to store in database
            use_pattern_guidance: Enable pattern-guided decisions
        """
        # Initialize base CDCL solver
        super().__init__(cnf, vsids_decay, restart_base, learned_clause_limit)

        # Pattern mining components
        self.pattern_miner = TemporalPatternMiner(
            window_size=window_size,
            max_patterns=max_patterns,
            min_occurrences=2
        )

        self.pattern_matcher = PatternMatcher(
            miner=self.pattern_miner,
            conflict_threshold=conflict_threshold,
            min_pattern_occurrences=3
        )

        self.use_pattern_guidance = use_pattern_guidance

        # Track decision-level decisions (not propagations) for pattern mining
        # This mirrors self.trail but only contains branching decisions
        self.decision_only_trail: deque = deque(maxlen=window_size)

        # Extended statistics
        self.stats = TPMStats()

    def _assign(self, variable: str, value: bool, antecedent: Optional[Clause] = None):
        """
        Override _assign to track decisions for pattern mining.

        Only branching decisions (not propagations) are added to decision trail.
        """
        # Call parent assign
        super()._assign(variable, value, antecedent)

        # If this is a decision (not a propagation), record it for pattern mining
        if antecedent is None:  # Decision, not propagation
            self.decision_only_trail.append((variable, value))
            self.pattern_miner.record_decision(variable, value)

    def _unassign_to_level(self, level: int):
        """
        Override backtracking to maintain decision trail.

        We need to remove decisions from the decision_only_trail when backtracking.
        """
        # Count how many decisions to remove
        decisions_to_remove = 0
        for assignment in reversed(self.trail):
            if assignment.decision_level > level:
                if assignment.antecedent is None:  # It's a decision
                    decisions_to_remove += 1
            else:
                break

        # Remove decisions from decision trail
        for _ in range(decisions_to_remove):
            if self.decision_only_trail:
                self.decision_only_trail.pop()

        # Call parent backtrack
        super()._unassign_to_level(level)

    def _analyze_conflict(self, conflict_clause: Clause) -> Tuple[Clause, int]:
        """
        Override conflict analysis to record patterns that led to conflicts.
        """
        # Record conflict pattern
        self.pattern_miner.record_conflict(conflict_depth=self.decision_level)
        self.stats.patterns_learned = len(self.pattern_miner.patterns)

        # Call parent conflict analysis
        return super()._analyze_conflict(conflict_clause)

    def _pick_branching_variable(self) -> Optional[str]:
        """
        Override variable selection to incorporate pattern-aware guidance.

        Strategy:
        1. Get top candidates from VSIDS
        2. Use pattern matcher to avoid variables/phases that complete bad patterns
        3. Prefer variables that won't trigger known bad patterns
        """
        unassigned = [var for var in self.variables if var not in self.assignment]
        if not unassigned:
            return None

        if not self.use_pattern_guidance or len(self.decision_only_trail) == 0:
            # Fall back to standard VSIDS
            return max(unassigned, key=lambda v: self.vsids_scores[v])

        # Get top-k candidates from VSIDS
        k = min(10, len(unassigned))
        top_candidates = sorted(unassigned, key=lambda v: self.vsids_scores[v], reverse=True)[:k]

        # Check which candidates would avoid bad patterns
        safe_candidates = []
        for var in top_candidates:
            # Check both phases
            true_is_bad, true_stats = self.pattern_matcher.would_complete_bad_pattern(
                self.decision_only_trail, var, True
            )
            false_is_bad, false_stats = self.pattern_matcher.would_complete_bad_pattern(
                self.decision_only_trail, var, False
            )

            # If at least one phase is safe, this is a good candidate
            if not true_is_bad or not false_is_bad:
                safe_candidates.append(var)

        if safe_candidates:
            # Choose highest VSIDS among safe candidates
            chosen = max(safe_candidates, key=lambda v: self.vsids_scores[v])
            self.stats.pattern_guided_decisions += 1
            if chosen != top_candidates[0]:
                self.stats.bad_patterns_avoided += 1
            return chosen
        else:
            # All candidates complete bad patterns - choose best VSIDS
            # (the bad patterns might be worth it for a high-activity variable)
            return top_candidates[0]

    def _pick_branching_phase(self, variable: str) -> bool:
        """
        Choose phase (True/False) for a variable, avoiding bad patterns.

        Args:
            variable: Variable to assign

        Returns:
            Phase (True/False) to try
        """
        if not self.use_pattern_guidance or len(self.decision_only_trail) == 0:
            # Fall back to default: try True first
            # (Could implement phase saving here)
            return True

        # Use pattern matcher to get safe phase
        default_phase = True  # Default to True
        safe_phase, reason = self.pattern_matcher.get_safe_phase(
            self.decision_only_trail, variable, default_phase
        )

        # Track statistics
        if reason != "default":
            self.stats.phase_flips_from_patterns += 1

        return safe_phase

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve the SAT formula using TPM-enhanced CDCL.

        Args:
            max_conflicts: Maximum number of conflicts before giving up

        Returns:
            Dictionary mapping variables to values if SAT, None if UNSAT
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

            # Pick branching variable (pattern-aware)
            var = self._pick_branching_variable()

            if var is None:
                # All variables assigned - SAT!
                return dict(self.assignment)

            # Pick phase (pattern-aware)
            phase = self._pick_branching_phase(var)

            # Make decision
            self.decision_level += 1
            self.stats.max_decision_level = max(self.stats.max_decision_level, self.decision_level)
            self._assign(var, phase)

            # Propagate
            while True:
                conflict = self._propagate()

                if conflict is None:
                    # No conflict - continue
                    break

                # Conflict!
                self.stats.conflicts += 1

                if self.decision_level == 0:
                    # Conflict at level 0 - UNSAT
                    return None

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

    def get_pattern_statistics(self) -> dict:
        """Get detailed pattern mining statistics."""
        return {
            'miner_stats': self.pattern_miner.get_statistics(),
            'matcher_stats': self.pattern_matcher.get_statistics(),
            'top_bad_patterns': [
                {
                    'pattern': pattern,
                    'conflict_rate': stats.conflict_rate,
                    'occurrences': stats.times_seen,
                }
                for pattern, stats in self.pattern_miner.get_top_bad_patterns(5)
            ],
        }


def solve_tpm_sat(cnf: CNFExpression,
                  window_size: int = 5,
                  conflict_threshold: float = 0.8,
                  max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve a CNF formula using TPM-SAT (Temporal Pattern Mining SAT).

    Args:
        cnf: CNF formula to solve
        window_size: Length of decision sequences to mine
        conflict_threshold: Conflict rate to consider pattern bad
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Dictionary mapping variables to values if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z) & (~y | ~z)")
        >>> result = solve_tpm_sat(cnf, window_size=5)
        >>> if result:
        ...     print(f"SAT: {result}")
    """
    solver = TPMSATSolver(
        cnf,
        window_size=window_size,
        conflict_threshold=conflict_threshold
    )
    return solver.solve(max_conflicts=max_conflicts)
