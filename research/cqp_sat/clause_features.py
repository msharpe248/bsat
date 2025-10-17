"""
Clause Feature Extraction for CQP-SAT

Extracts features from learned clauses to predict their quality/usefulness.
Based on the foundational work by Audemard & Simon (2009) introducing LBD.

Key Feature: Literal Block Distance (LBD) - the "glue" of a clause.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Set, Optional
from bsat.cnf import Clause, Literal


class ClauseFeatures:
    """
    Features extracted from a learned clause for quality prediction.

    Based on Glucose solver (Audemard & Simon 2009).
    """

    def __init__(self, clause: Clause):
        """
        Initialize clause features.

        Args:
            clause: The learned clause
        """
        self.clause = clause

        # Basic features
        self.size = len(clause.literals)  # Clause length

        # Quality features (computed later)
        self.lbd: Optional[int] = None  # Literal Block Distance
        self.activity: float = 0.0  # Clause activity score
        self.decision_level: int = 0  # Level where learned

        # Usage statistics (tracked over time)
        self.age: int = 0  # Conflicts since learned
        self.propagation_count: int = 0  # Times used for propagation
        self.conflict_count: int = 0  # Times in conflict analysis
        self.used_recently: bool = True  # Used in recent conflicts

        # LBD tracking (for stability)
        self.lbd_history: List[int] = []

    def __repr__(self):
        return (
            f"ClauseFeatures("
            f"size={self.size}, "
            f"lbd={self.lbd}, "
            f"activity={self.activity:.2f}, "
            f"age={self.age})"
        )


class ClauseFeatureExtractor:
    """
    Extracts quality features from clauses.

    Main feature: LBD (Literal Block Distance) - counts decision levels
    represented in the clause. Low LBD = high quality (more "glue").
    """

    def __init__(self):
        """Initialize feature extractor."""
        pass

    def compute_lbd(self,
                   clause: Clause,
                   decision_levels: Dict[str, int]) -> int:
        """
        Compute Literal Block Distance (LBD) for a clause.

        LBD is the number of distinct decision levels in the clause.
        Introduced by Audemard & Simon (2009) in Glucose.

        A low LBD means the clause has "glue" - it connects literals
        from few decision levels, making it likely useful for future
        propagation.

        Args:
            clause: The clause to analyze
            decision_levels: Map from variable to decision level

        Returns:
            LBD score (lower is better quality)
        """
        levels: Set[int] = set()

        for lit in clause.literals:
            var = lit.name
            if var in decision_levels:
                level = decision_levels[var]
                levels.add(level)

        # LBD is the count of distinct levels
        lbd = len(levels)

        return lbd if lbd > 0 else 1

    def compute_activity(self,
                        clause: Clause,
                        variable_activities: Dict[str, float]) -> float:
        """
        Compute clause activity based on variable activities.

        Clauses with high-activity variables are more likely to be useful.

        Args:
            clause: The clause
            variable_activities: Map from variable to activity score

        Returns:
            Clause activity (sum of variable activities)
        """
        total_activity = 0.0

        for lit in clause.literals:
            var = lit.name
            total_activity += variable_activities.get(var, 0.0)

        return total_activity

    def extract_features(self,
                        clause: Clause,
                        decision_levels: Dict[str, int],
                        variable_activities: Dict[str, float],
                        learned_at_level: int) -> ClauseFeatures:
        """
        Extract all features for a clause.

        Args:
            clause: The clause
            decision_levels: Variable to decision level map
            variable_activities: Variable to activity map
            learned_at_level: Decision level where clause was learned

        Returns:
            ClauseFeatures object
        """
        features = ClauseFeatures(clause)

        # Compute LBD (most important feature)
        features.lbd = self.compute_lbd(clause, decision_levels)

        # Compute activity
        features.activity = self.compute_activity(clause, variable_activities)

        # Record learning context
        features.decision_level = learned_at_level

        # Initialize LBD history
        features.lbd_history.append(features.lbd)

        return features

    def update_lbd(self,
                  features: ClauseFeatures,
                  decision_levels: Dict[str, int]):
        """
        Update LBD for a clause (recompute based on current assignment).

        Glucose tracks LBD stability - clauses with stable LBD are high quality.

        Args:
            features: Clause features to update
            decision_levels: Current variable to decision level map
        """
        new_lbd = self.compute_lbd(features.clause, decision_levels)

        # Update LBD
        old_lbd = features.lbd
        features.lbd = new_lbd

        # Track history (for computing variance/stability)
        features.lbd_history.append(new_lbd)

        # Keep history bounded
        if len(features.lbd_history) > 10:
            features.lbd_history.pop(0)

    def compute_lbd_stability(self, features: ClauseFeatures) -> float:
        """
        Compute LBD stability (low variance = stable = high quality).

        Args:
            features: Clause features

        Returns:
            LBD variance (lower is more stable)
        """
        if len(features.lbd_history) < 2:
            return 0.0

        mean_lbd = sum(features.lbd_history) / len(features.lbd_history)
        variance = sum((lbd - mean_lbd) ** 2 for lbd in features.lbd_history)
        variance /= len(features.lbd_history)

        return variance

    def get_feature_summary(self, features: ClauseFeatures) -> dict:
        """Get summary of all features for analysis."""
        return {
            'size': features.size,
            'lbd': features.lbd,
            'activity': features.activity,
            'decision_level': features.decision_level,
            'age': features.age,
            'propagation_count': features.propagation_count,
            'conflict_count': features.conflict_count,
            'lbd_stability': self.compute_lbd_stability(features),
            'usage_rate': (features.propagation_count + features.conflict_count) / max(features.age, 1),
        }
