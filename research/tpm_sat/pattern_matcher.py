"""
Pattern Matcher for Temporal Pattern Mining SAT

This module provides pattern matching functionality to check if a candidate
decision would complete a known bad pattern (one that frequently leads to conflicts).
"""

from typing import Tuple, List, Optional
from collections import deque
from .pattern_miner import TemporalPatternMiner, PatternStats


class PatternMatcher:
    """
    Checks if candidate decisions would complete known bad patterns.

    Works in conjunction with TemporalPatternMiner to avoid repeating
    decision sequences that have historically led to conflicts.
    """

    def __init__(self,
                 miner: TemporalPatternMiner,
                 conflict_threshold: float = 0.8,
                 min_pattern_occurrences: int = 3):
        """
        Initialize pattern matcher.

        Args:
            miner: TemporalPatternMiner containing learned patterns
            conflict_threshold: Minimum conflict rate to consider pattern "bad" (0.0-1.0)
            min_pattern_occurrences: Minimum times pattern must be seen to be considered
        """
        self.miner = miner
        self.conflict_threshold = conflict_threshold
        self.min_pattern_occurrences = min_pattern_occurrences

    def would_complete_bad_pattern(self,
                                     current_trail: deque,
                                     candidate_var: str,
                                     candidate_value: bool) -> Tuple[bool, Optional[PatternStats]]:
        """
        Check if making a decision would complete a known bad pattern.

        Args:
            current_trail: Current decision trail (recent decisions)
            candidate_var: Variable being considered for decision
            candidate_value: Value being considered (True/False)

        Returns:
            Tuple of (is_bad, pattern_stats)
            - is_bad: True if this would complete a bad pattern
            - pattern_stats: Statistics for the matching pattern (if any)
        """
        if len(current_trail) == 0:
            return False, None

        # Try patterns of different lengths
        # Check if adding candidate decision would complete any bad patterns
        for pattern_len in range(2, min(len(current_trail) + 2, self.miner.window_size + 1)):
            # Build candidate pattern
            trail_segment = list(current_trail)[-(pattern_len - 1):]
            candidate_pattern = tuple(trail_segment + [(candidate_var, candidate_value)])

            # Check if this pattern exists and is bad
            stats = self.miner.get_pattern_stats(candidate_pattern)

            if stats is not None:
                # Pattern exists - check if it's bad
                if (stats.times_seen >= self.min_pattern_occurrences and
                    stats.conflict_rate >= self.conflict_threshold):
                    return True, stats

        return False, None

    def find_bad_patterns_for_variable(self,
                                        current_trail: deque,
                                        variable: str) -> List[Tuple[bool, PatternStats]]:
        """
        Find all bad patterns that would be completed by assigning this variable.

        Args:
            current_trail: Current decision trail
            variable: Variable to check

        Returns:
            List of (value, stats) tuples for bad patterns
            - value: The assignment value (True/False) that completes bad pattern
            - stats: Statistics for that pattern
        """
        bad_patterns = []

        for value in [True, False]:
            is_bad, stats = self.would_complete_bad_pattern(current_trail, variable, value)
            if is_bad:
                bad_patterns.append((value, stats))

        return bad_patterns

    def get_safe_phase(self,
                        current_trail: deque,
                        variable: str,
                        default_phase: bool) -> Tuple[bool, str]:
        """
        Get a safe phase (True/False) for a variable that avoids bad patterns.

        Args:
            current_trail: Current decision trail
            variable: Variable to assign
            default_phase: Default phase if both are equally safe

        Returns:
            Tuple of (phase, reason)
            - phase: Recommended value (True/False)
            - reason: Explanation ("default", "avoid_bad_pattern_true", "avoid_bad_pattern_false", "both_bad")
        """
        bad_patterns = self.find_bad_patterns_for_variable(current_trail, variable)

        if len(bad_patterns) == 0:
            # Neither phase completes a bad pattern
            return default_phase, "default"

        elif len(bad_patterns) == 1:
            # One phase is bad, choose the other
            bad_value, stats = bad_patterns[0]
            safe_value = not bad_value
            reason = f"avoid_bad_pattern_{str(bad_value).lower()}"
            return safe_value, reason

        else:
            # Both phases complete bad patterns
            # Choose the one with lower conflict rate
            true_pattern = next((stats for val, stats in bad_patterns if val is True), None)
            false_pattern = next((stats for val, stats in bad_patterns if val is False), None)

            if true_pattern and false_pattern:
                if true_pattern.conflict_rate <= false_pattern.conflict_rate:
                    return True, "both_bad_choose_lesser_evil"
                else:
                    return False, "both_bad_choose_lesser_evil"
            else:
                # Shouldn't happen, but fallback to default
                return default_phase, "both_bad_fallback"

    def get_pattern_aware_recommendations(self,
                                           current_trail: deque,
                                           candidate_variables: List[str]) -> List[Tuple[str, bool, str, float]]:
        """
        Get pattern-aware recommendations for variable selection.

        Analyzes all candidate variables and their potential phases to identify
        which choices would avoid bad patterns.

        Args:
            current_trail: Current decision trail
            candidate_variables: List of variables to consider

        Returns:
            List of (variable, phase, reason, badness_score) tuples sorted by safety
            - badness_score: 0.0 = safe, 1.0 = very bad
        """
        recommendations = []

        for var in candidate_variables:
            # Check both phases
            for phase in [True, False]:
                is_bad, stats = self.would_complete_bad_pattern(current_trail, var, phase)

                if is_bad and stats:
                    badness_score = stats.conflict_rate
                    reason = f"completes_bad_pattern_rate={stats.conflict_rate:.2f}"
                else:
                    badness_score = 0.0
                    reason = "safe"

                recommendations.append((var, phase, reason, badness_score))

        # Sort by badness (safest first)
        recommendations.sort(key=lambda x: x[3])

        return recommendations

    def get_statistics(self) -> dict:
        """Get pattern matcher statistics."""
        all_patterns = self.miner.get_all_patterns()

        bad_pattern_count = sum(
            1 for stats in all_patterns.values()
            if stats.times_seen >= self.min_pattern_occurrences
            and stats.conflict_rate >= self.conflict_threshold
        )

        return {
            'total_patterns': len(all_patterns),
            'bad_patterns': bad_pattern_count,
            'conflict_threshold': self.conflict_threshold,
            'min_occurrences': self.min_pattern_occurrences,
        }

    def __repr__(self):
        stats = self.get_statistics()
        return (
            f"PatternMatcher("
            f"bad_patterns={stats['bad_patterns']}/{stats['total_patterns']}, "
            f"threshold={self.conflict_threshold})"
        )
