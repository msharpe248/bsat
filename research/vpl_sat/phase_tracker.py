"""
Phase Performance Tracker for VPL-SAT

Tracks the performance of variable phase assignments (True/False) throughout
the search process. Records which phases lead to conflicts vs. successful
assignments, enabling learned phase preferences.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional, Tuple
from collections import deque, defaultdict


class PhaseStats:
    """
    Statistics for a single variable's phase performance.

    Tracks conflicts and successes for both True and False phases,
    maintaining a recent history window for adaptive learning.
    """

    def __init__(self, window_size: int = 100):
        """
        Initialize phase statistics.

        Args:
            window_size: Size of recent history window
        """
        # Conflict counters
        self.true_conflicts = 0
        self.false_conflicts = 0

        # Success counters (decisions that didn't immediately conflict)
        self.true_success = 0
        self.false_success = 0

        # Recent history window: (event_type, phase, decision_level)
        self.recent_window: deque = deque(maxlen=window_size)

        # Last used phase (for comparison with learned preference)
        self.last_phase: Optional[bool] = None

        # Timestamp tracking
        self.last_conflict_phase: Optional[bool] = None
        self.conflict_streak = 0  # Consecutive conflicts with same phase

    def record_decision(self, phase: bool, decision_level: int):
        """
        Record a decision made with this phase.

        Args:
            phase: True or False
            decision_level: Decision level where this was assigned
        """
        self.recent_window.append(('decision', phase, decision_level))
        self.last_phase = phase

    def record_conflict(self, phase: bool, decision_level: int):
        """
        Record that a phase led to a conflict.

        Args:
            phase: The phase that was involved in the conflict
            decision_level: Decision level where conflict occurred
        """
        if phase:
            self.true_conflicts += 1
        else:
            self.false_conflicts += 1

        self.recent_window.append(('conflict', phase, decision_level))

        # Track conflict streaks
        if self.last_conflict_phase == phase:
            self.conflict_streak += 1
        else:
            self.conflict_streak = 1
            self.last_conflict_phase = phase

    def record_success(self, phase: bool, decision_level: int):
        """
        Record that a phase was part of a successful branch.

        This is called when we find a satisfying assignment or make
        significant progress without conflicts.

        Args:
            phase: The phase that was successful
            decision_level: Decision level where this occurred
        """
        if phase:
            self.true_success += 1
        else:
            self.false_success += 1

        self.recent_window.append(('success', phase, decision_level))

    def get_conflict_rate(self, phase: bool) -> float:
        """
        Calculate conflict rate for a given phase.

        Args:
            phase: True or False

        Returns:
            Conflict rate (0.0 to 1.0)
        """
        if phase:
            total = self.true_conflicts + self.true_success
            return self.true_conflicts / max(total, 1)
        else:
            total = self.false_conflicts + self.false_success
            return self.false_conflicts / max(total, 1)

    def get_recent_conflict_rate(self, phase: bool, window: int = 50) -> float:
        """
        Calculate conflict rate from recent history only.

        Args:
            phase: True or False
            window: Number of recent events to consider

        Returns:
            Recent conflict rate (0.0 to 1.0)
        """
        recent = list(self.recent_window)[-window:]

        conflicts = sum(1 for event, p, _ in recent
                       if event == 'conflict' and p == phase)
        total = sum(1 for event, p, _ in recent if p == phase)

        return conflicts / max(total, 1)

    def get_preferred_phase(self, threshold: float = 0.15) -> Optional[bool]:
        """
        Determine if there's a strong preference for one phase.

        Args:
            threshold: Minimum difference in conflict rates to prefer a phase

        Returns:
            Preferred phase (True/False) or None if no clear preference
        """
        true_rate = self.get_conflict_rate(True)
        false_rate = self.get_conflict_rate(False)

        diff = abs(true_rate - false_rate)

        if diff >= threshold:
            # Prefer the phase with LOWER conflict rate
            return true_rate < false_rate
        else:
            return None

    def should_flip_phase(self) -> bool:
        """
        Check if recent pattern suggests we should flip to opposite phase.

        Returns:
            True if we should flip from last_phase
        """
        if self.last_phase is None:
            return False

        # If we've had a streak of conflicts with same phase, flip
        if self.conflict_streak >= 3:
            return True

        # Check recent window for strong signal
        recent_rate = self.get_recent_conflict_rate(self.last_phase, window=20)
        if recent_rate > 0.7:  # 70% of recent decisions conflicted
            return True

        return False

    def get_statistics(self) -> dict:
        """Get summary statistics."""
        return {
            'true_conflicts': self.true_conflicts,
            'false_conflicts': self.false_conflicts,
            'true_success': self.true_success,
            'false_success': self.false_success,
            'true_conflict_rate': self.get_conflict_rate(True),
            'false_conflict_rate': self.get_conflict_rate(False),
            'conflict_streak': self.conflict_streak,
            'last_phase': self.last_phase,
            'window_size': len(self.recent_window),
        }

    def __repr__(self):
        true_rate = self.get_conflict_rate(True)
        false_rate = self.get_conflict_rate(False)
        return (
            f"PhaseStats(T: {self.true_conflicts}c/{self.true_success}s={true_rate:.2f}, "
            f"F: {self.false_conflicts}c/{self.false_success}s={false_rate:.2f})"
        )


class PhaseTracker:
    """
    Tracks phase performance for all variables.

    Maintains statistics about which phases (True/False) lead to conflicts
    vs. successes for each variable, enabling learned phase preferences.
    """

    def __init__(self, variables: List[str], window_size: int = 100):
        """
        Initialize phase tracker.

        Args:
            variables: List of variable names
            window_size: Size of recent history window per variable
        """
        self.variables = variables
        self.window_size = window_size

        # Per-variable phase statistics
        self.phase_stats: Dict[str, PhaseStats] = {
            var: PhaseStats(window_size) for var in variables
        }

        # Global statistics
        self.total_conflicts = 0
        self.total_decisions = 0
        self.phase_flips_recommended = 0

    def record_decision(self, variable: str, phase: bool, decision_level: int):
        """
        Record a decision made during search.

        Args:
            variable: Variable name
            phase: Value assigned (True/False)
            decision_level: Decision level
        """
        if variable in self.phase_stats:
            self.phase_stats[variable].record_decision(phase, decision_level)
            self.total_decisions += 1

    def record_conflict(self, conflict_clause: List[Tuple[str, bool]],
                       decision_level: int):
        """
        Record that a conflict occurred, updating stats for involved variables.

        Args:
            conflict_clause: List of (variable, phase) tuples in conflict
            decision_level: Decision level where conflict occurred
        """
        self.total_conflicts += 1

        for var, phase in conflict_clause:
            if var in self.phase_stats:
                self.phase_stats[var].record_conflict(phase, decision_level)

    def record_success(self, variable: str, phase: bool, decision_level: int):
        """
        Record that a variable assignment was part of successful search.

        Args:
            variable: Variable name
            phase: Phase that succeeded
            decision_level: Decision level
        """
        if variable in self.phase_stats:
            self.phase_stats[variable].record_success(phase, decision_level)

    def get_preferred_phase(self, variable: str,
                          threshold: float = 0.15) -> Optional[bool]:
        """
        Get learned phase preference for a variable.

        Args:
            variable: Variable name
            threshold: Minimum conflict rate difference

        Returns:
            Preferred phase or None if no clear preference
        """
        if variable not in self.phase_stats:
            return None

        return self.phase_stats[variable].get_preferred_phase(threshold)

    def should_flip_phase(self, variable: str) -> bool:
        """
        Check if we should flip to opposite phase based on recent pattern.

        Args:
            variable: Variable name

        Returns:
            True if flip recommended
        """
        if variable not in self.phase_stats:
            return False

        should_flip = self.phase_stats[variable].should_flip_phase()
        if should_flip:
            self.phase_flips_recommended += 1

        return should_flip

    def get_variable_stats(self, variable: str) -> Optional[dict]:
        """Get statistics for a specific variable."""
        if variable not in self.phase_stats:
            return None
        return self.phase_stats[variable].get_statistics()

    def get_summary_statistics(self) -> dict:
        """Get overall statistics across all variables."""
        total_true_conflicts = sum(s.true_conflicts for s in self.phase_stats.values())
        total_false_conflicts = sum(s.false_conflicts for s in self.phase_stats.values())
        total_true_success = sum(s.true_success for s in self.phase_stats.values())
        total_false_success = sum(s.false_success for s in self.phase_stats.values())

        # Count variables with strong preferences
        strong_preferences = sum(
            1 for s in self.phase_stats.values()
            if s.get_preferred_phase() is not None
        )

        return {
            'total_conflicts': self.total_conflicts,
            'total_decisions': self.total_decisions,
            'true_conflicts': total_true_conflicts,
            'false_conflicts': total_false_conflicts,
            'true_success': total_true_success,
            'false_success': total_false_success,
            'variables_with_preference': strong_preferences,
            'total_variables': len(self.variables),
            'phase_flips_recommended': self.phase_flips_recommended,
        }

    def __repr__(self):
        stats = self.get_summary_statistics()
        return (
            f"PhaseTracker("
            f"vars={len(self.variables)}, "
            f"conflicts={stats['total_conflicts']}, "
            f"preferences={stats['variables_with_preference']})"
        )
