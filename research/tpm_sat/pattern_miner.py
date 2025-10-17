"""
Temporal Pattern Mining for SAT Solving

This module implements pattern mining from decision sequences that lead to conflicts.
Mines temporal patterns (sequences of decisions) and tracks their conflict rates.

The key idea: If a particular sequence of decisions (pattern) repeatedly leads to
conflicts, we can learn to avoid repeating that pattern in the future.
"""

from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, field
from collections import defaultdict, deque


@dataclass
class PatternStats:
    """Statistics for a decision pattern."""
    conflict_count: int = 0
    times_seen: int = 0
    avg_conflict_depth: float = 0.0
    last_seen_conflict: int = 0  # Conflict number when last seen

    @property
    def conflict_rate(self) -> float:
        """Fraction of times this pattern led to conflicts."""
        if self.times_seen == 0:
            return 0.0
        return self.conflict_count / self.times_seen

    def __repr__(self):
        return (
            f"PatternStats(conflicts={self.conflict_count}/{self.times_seen}, "
            f"rate={self.conflict_rate:.2f}, depth={self.avg_conflict_depth:.1f})"
        )


class TemporalPatternMiner:
    """
    Mines temporal patterns from decision sequences leading to conflicts.

    Maintains a database of patterns (sequences of variable assignments) and
    their conflict rates to help avoid repeating mistakes.
    """

    def __init__(self,
                 window_size: int = 5,
                 max_patterns: int = 10000,
                 min_occurrences: int = 2):
        """
        Initialize pattern miner.

        Args:
            window_size: Number of decisions to include in each pattern
            max_patterns: Maximum number of patterns to store
            min_occurrences: Minimum times pattern must be seen to be useful
        """
        self.window_size = window_size
        self.max_patterns = max_patterns
        self.min_occurrences = min_occurrences

        # Pattern database: pattern tuple -> statistics
        # Pattern is tuple of (variable, value) pairs representing decision sequence
        self.patterns: Dict[Tuple[Tuple[str, bool], ...], PatternStats] = {}

        # Running conflict counter
        self.total_conflicts = 0

        # Recent decision trail for pattern extraction
        self.decision_trail: deque = deque(maxlen=window_size)

    def record_decision(self, variable: str, value: bool):
        """
        Record a decision (branching choice).

        Args:
            variable: Variable being decided
            value: Value assigned (True/False)
        """
        self.decision_trail.append((variable, value))

    def record_conflict(self, conflict_depth: int):
        """
        Record that current decision trail led to a conflict.

        Extracts patterns of various lengths from the current trail and
        updates their conflict statistics.

        Args:
            conflict_depth: Decision level at which conflict occurred
        """
        self.total_conflicts += 1

        if len(self.decision_trail) == 0:
            return

        # Extract patterns of different lengths from trail
        # For window_size=5, extract patterns of length 2, 3, 4, 5
        for pattern_len in range(2, min(len(self.decision_trail) + 1, self.window_size + 1)):
            # Get last pattern_len decisions
            pattern_list = list(self.decision_trail)[-pattern_len:]
            pattern = tuple(pattern_list)

            # Update or create pattern statistics
            if pattern not in self.patterns:
                if len(self.patterns) >= self.max_patterns:
                    # Database full - remove least useful pattern
                    self._evict_pattern()
                self.patterns[pattern] = PatternStats()

            stats = self.patterns[pattern]
            stats.conflict_count += 1
            stats.times_seen += 1
            stats.last_seen_conflict = self.total_conflicts

            # Update average conflict depth (exponential moving average)
            alpha = 0.3  # Smoothing factor
            if stats.avg_conflict_depth == 0:
                stats.avg_conflict_depth = conflict_depth
            else:
                stats.avg_conflict_depth = (
                    alpha * conflict_depth + (1 - alpha) * stats.avg_conflict_depth
                )

    def record_pattern_seen(self, pattern: Tuple[Tuple[str, bool], ...]):
        """
        Record that a pattern was seen (but didn't lead to conflict).

        This helps us track patterns that sometimes work and sometimes don't.

        Args:
            pattern: Tuple of (variable, value) decisions
        """
        if pattern in self.patterns:
            self.patterns[pattern].times_seen += 1
            self.patterns[pattern].last_seen_conflict = self.total_conflicts

    def _evict_pattern(self):
        """
        Remove the least useful pattern from the database.

        Eviction strategy:
        1. Prefer patterns seen few times (low confidence)
        2. Among those, prefer patterns seen long ago (stale)
        """
        if not self.patterns:
            return

        # Find pattern with lowest usefulness score
        def usefulness_score(item):
            pattern, stats = item
            # Patterns with low occurrence count are less useful
            occurrence_weight = min(stats.times_seen / 10.0, 1.0)
            # Patterns not seen recently are less useful
            recency_weight = 1.0 / (1.0 + self.total_conflicts - stats.last_seen_conflict)
            # Patterns with high conflict rate are more useful
            conflict_weight = stats.conflict_rate

            return occurrence_weight * recency_weight * (1 + conflict_weight)

        least_useful = min(self.patterns.items(), key=usefulness_score)
        del self.patterns[least_useful[0]]

    def get_pattern_stats(self, pattern: Tuple[Tuple[str, bool], ...]) -> Optional[PatternStats]:
        """
        Get statistics for a specific pattern.

        Args:
            pattern: Tuple of (variable, value) decisions

        Returns:
            PatternStats if pattern is known, None otherwise
        """
        return self.patterns.get(pattern)

    def get_all_patterns(self) -> Dict[Tuple[Tuple[str, bool], ...], PatternStats]:
        """Get all known patterns and their statistics."""
        return dict(self.patterns)

    def get_top_bad_patterns(self, n: int = 10) -> List[Tuple[Tuple[Tuple[str, bool], ...], PatternStats]]:
        """
        Get the N worst patterns (highest conflict rates with sufficient occurrences).

        Args:
            n: Number of patterns to return

        Returns:
            List of (pattern, stats) tuples sorted by badness
        """
        # Filter for patterns with sufficient occurrences
        qualified_patterns = [
            (pattern, stats) for pattern, stats in self.patterns.items()
            if stats.times_seen >= self.min_occurrences
        ]

        # Sort by conflict rate (descending)
        qualified_patterns.sort(key=lambda x: x[1].conflict_rate, reverse=True)

        return qualified_patterns[:n]

    def clear_stale_patterns(self, staleness_threshold: int = 1000):
        """
        Remove patterns that haven't been seen recently.

        Args:
            staleness_threshold: Remove patterns not seen in last N conflicts
        """
        stale_patterns = [
            pattern for pattern, stats in self.patterns.items()
            if self.total_conflicts - stats.last_seen_conflict > staleness_threshold
        ]

        for pattern in stale_patterns:
            del self.patterns[pattern]

    def reset(self):
        """Reset the pattern miner (clear all patterns and statistics)."""
        self.patterns.clear()
        self.decision_trail.clear()
        self.total_conflicts = 0

    def get_statistics(self) -> Dict:
        """Get summary statistics about pattern mining."""
        if not self.patterns:
            return {
                'num_patterns': 0,
                'total_conflicts': self.total_conflicts,
                'avg_conflict_rate': 0.0,
                'max_conflict_rate': 0.0,
            }

        conflict_rates = [stats.conflict_rate for stats in self.patterns.values()]

        return {
            'num_patterns': len(self.patterns),
            'total_conflicts': self.total_conflicts,
            'avg_conflict_rate': sum(conflict_rates) / len(conflict_rates),
            'max_conflict_rate': max(conflict_rates),
            'patterns_with_high_conflict_rate': sum(
                1 for rate in conflict_rates if rate > 0.7
            ),
        }

    def __repr__(self):
        stats = self.get_statistics()
        return (
            f"TemporalPatternMiner("
            f"patterns={stats['num_patterns']}, "
            f"conflicts={stats['total_conflicts']}, "
            f"avg_rate={stats['avg_conflict_rate']:.2f})"
        )
