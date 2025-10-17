"""
Learned Phase Selection for VPL-SAT

Uses phase performance statistics to make intelligent phase selection decisions,
preferring phases that have historically led to fewer conflicts.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Optional, Tuple
from .phase_tracker import PhaseTracker


class PhaseSelectionStrategy:
    """
    Base class for phase selection strategies.

    Different strategies can be implemented by subclassing this.
    """

    def select_phase(self, variable: str, tracker: PhaseTracker,
                    saved_phase: Optional[bool] = None) -> bool:
        """
        Select phase for a variable.

        Args:
            variable: Variable to assign
            tracker: Phase performance tracker
            saved_phase: Previously saved phase (VSIDS phase saving)

        Returns:
            Chosen phase (True or False)
        """
        raise NotImplementedError


class ConflictRateStrategy(PhaseSelectionStrategy):
    """
    Selects phase based on historical conflict rates.

    Prefers the phase with lower conflict rate, falling back to
    VSIDS phase saving if no clear preference.
    """

    def __init__(self, threshold: float = 0.15, use_recent: bool = True):
        """
        Initialize conflict rate strategy.

        Args:
            threshold: Minimum conflict rate difference to override saved phase
            use_recent: Weight recent history more heavily
        """
        self.threshold = threshold
        self.use_recent = use_recent

    def select_phase(self, variable: str, tracker: PhaseTracker,
                    saved_phase: Optional[bool] = None) -> bool:
        """Select phase with lowest conflict rate."""
        # Get learned preference
        preferred = tracker.get_preferred_phase(variable, self.threshold)

        if preferred is not None:
            # Strong learned preference
            return preferred
        elif saved_phase is not None:
            # Fall back to VSIDS phase saving
            return saved_phase
        else:
            # Default: True
            return True


class AdaptiveStrategy(PhaseSelectionStrategy):
    """
    Adaptive phase selection combining multiple signals.

    Considers:
    1. Recent conflict patterns (should_flip_phase)
    2. Overall conflict rates
    3. Conflict streaks
    4. VSIDS phase saving
    """

    def __init__(self, threshold: float = 0.15, flip_weight: float = 0.7):
        """
        Initialize adaptive strategy.

        Args:
            threshold: Conflict rate difference threshold
            flip_weight: Weight for flip recommendation vs. learned preference
        """
        self.threshold = threshold
        self.flip_weight = flip_weight

    def select_phase(self, variable: str, tracker: PhaseTracker,
                    saved_phase: Optional[bool] = None) -> bool:
        """Adaptively select phase based on multiple signals."""
        stats = tracker.get_variable_stats(variable)

        if stats is None:
            return saved_phase if saved_phase is not None else True

        # Check if we should flip from recent pattern
        should_flip = tracker.should_flip_phase(variable)

        if should_flip and saved_phase is not None:
            # Flip from saved phase
            return not saved_phase

        # Otherwise use learned preference
        preferred = tracker.get_preferred_phase(variable, self.threshold)

        if preferred is not None:
            return preferred
        elif saved_phase is not None:
            return saved_phase
        else:
            return True


class HybridStrategy(PhaseSelectionStrategy):
    """
    Hybrid strategy combining learned preferences with VSIDS phase saving.

    Weights learned preferences and saved phases based on confidence.
    """

    def __init__(self,
                 threshold: float = 0.15,
                 confidence_weight: float = 0.6,
                 min_samples: int = 10):
        """
        Initialize hybrid strategy.

        Args:
            threshold: Conflict rate difference threshold
            confidence_weight: Weight for learned preference (0.0-1.0)
            min_samples: Minimum samples before trusting learned preference
        """
        self.threshold = threshold
        self.confidence_weight = confidence_weight
        self.min_samples = min_samples

    def select_phase(self, variable: str, tracker: PhaseTracker,
                    saved_phase: Optional[bool] = None) -> bool:
        """Select phase using hybrid approach."""
        stats = tracker.get_variable_stats(variable)

        if stats is None:
            return saved_phase if saved_phase is not None else True

        # Calculate confidence in learned preference
        total_samples = (stats['true_conflicts'] + stats['true_success'] +
                        stats['false_conflicts'] + stats['false_success'])

        if total_samples < self.min_samples:
            # Not enough data, use saved phase
            return saved_phase if saved_phase is not None else True

        # Get learned preference
        preferred = tracker.get_preferred_phase(variable, self.threshold)

        if preferred is None:
            # No clear preference, use saved phase
            return saved_phase if saved_phase is not None else True

        # Strong learned preference with enough samples
        if saved_phase is None or saved_phase == preferred:
            # Learned and saved agree, or no saved phase
            return preferred
        else:
            # Conflict between learned and saved
            # Use confidence to decide
            confidence = min(total_samples / 50.0, 1.0)  # Max confidence at 50 samples

            if confidence >= self.confidence_weight:
                # Trust learned preference
                return preferred
            else:
                # Trust saved phase
                return saved_phase


class PhaseSelector:
    """
    Main phase selector integrating tracker and strategy.

    Provides simple interface for phase selection during CDCL search.
    """

    def __init__(self,
                 tracker: PhaseTracker,
                 strategy: PhaseSelectionStrategy = None):
        """
        Initialize phase selector.

        Args:
            tracker: Phase performance tracker
            strategy: Phase selection strategy (default: AdaptiveStrategy)
        """
        self.tracker = tracker
        self.strategy = strategy if strategy else AdaptiveStrategy()

        # Statistics
        self.learned_decisions = 0
        self.saved_decisions = 0
        self.default_decisions = 0

    def select_phase(self, variable: str,
                    saved_phase: Optional[bool] = None) -> bool:
        """
        Select phase for a variable.

        Args:
            variable: Variable to assign
            saved_phase: VSIDS saved phase

        Returns:
            Selected phase (True or False)
        """
        phase = self.strategy.select_phase(variable, self.tracker, saved_phase)

        # Track statistics
        preferred = self.tracker.get_preferred_phase(variable)
        if preferred is not None and phase == preferred:
            self.learned_decisions += 1
        elif saved_phase is not None and phase == saved_phase:
            self.saved_decisions += 1
        else:
            self.default_decisions += 1

        return phase

    def get_statistics(self) -> dict:
        """Get phase selection statistics."""
        total = self.learned_decisions + self.saved_decisions + self.default_decisions
        return {
            'learned_decisions': self.learned_decisions,
            'saved_decisions': self.saved_decisions,
            'default_decisions': self.default_decisions,
            'total_decisions': total,
            'learned_percentage': (self.learned_decisions / total * 100) if total > 0 else 0,
        }

    def __repr__(self):
        stats = self.get_statistics()
        return (
            f"PhaseSelector("
            f"learned={self.learned_decisions}, "
            f"saved={self.saved_decisions}, "
            f"default={self.default_decisions}, "
            f"learned%={stats['learned_percentage']:.1f}%)"
        )
