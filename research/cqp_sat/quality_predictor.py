"""
Clause Quality Prediction for CQP-SAT

Predicts clause quality based on extracted features, primarily LBD.
Implements the Glucose approach (Audemard & Simon 2009) for clause database management.

Key Insight: Low LBD clauses are "glue" clauses - they're highly useful
for propagation and should be kept.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import List, Optional
from .clause_features import ClauseFeatures, ClauseFeatureExtractor


class QualityPredictor:
    """
    Predicts clause quality for deletion decisions.

    Based on Glucose (Audemard & Simon 2009):
    - Low LBD → High quality (keep)
    - High LBD → Low quality (delete)
    - Stable LBD → High quality (keep)
    """

    def __init__(self,
                 lbd_threshold: int = 30,
                 activity_weight: float = 0.1,
                 age_penalty: float = 0.01):
        """
        Initialize quality predictor.

        Args:
            lbd_threshold: LBD above this is considered low quality
            activity_weight: Weight for activity in quality score
            age_penalty: Penalty per age unit (older = lower quality)
        """
        self.lbd_threshold = lbd_threshold
        self.activity_weight = activity_weight
        self.age_penalty = age_penalty

        # Glucose thresholds
        self.glue_threshold = 2  # LBD ≤ 2 = "glue clause" (always keep)

    def predict_quality(self, features: ClauseFeatures) -> float:
        """
        Predict clause quality score.

        Higher score = better quality (keep clause).
        Lower score = worse quality (delete clause).

        Args:
            features: Clause features

        Returns:
            Quality score (0.0 to 1.0)
        """
        # Base score from LBD (most important)
        # Low LBD = high quality
        if features.lbd is None:
            lbd_score = 0.5
        elif features.lbd <= self.glue_threshold:
            # Glue clause - maximum quality
            lbd_score = 1.0
        else:
            # Normalize LBD: lower is better
            lbd_score = max(0.0, 1.0 - (features.lbd / self.lbd_threshold))

        # Activity contribution (clauses with active variables are useful)
        activity_score = min(1.0, features.activity * self.activity_weight)

        # Age penalty (older clauses less useful unless proven otherwise)
        age_factor = max(0.5, 1.0 - (features.age * self.age_penalty))

        # Usage bonus (clauses that are actually used are valuable)
        usage_rate = (features.propagation_count + features.conflict_count) / max(features.age, 1)
        usage_bonus = min(0.2, usage_rate * 0.2)

        # Combined quality score
        quality = (lbd_score * 0.7 +  # LBD dominates (70%)
                  activity_score * 0.2 +  # Activity helps (20%)
                  usage_bonus) * age_factor  # Age penalty applied

        return max(0.0, min(1.0, quality))

    def should_keep(self, features: ClauseFeatures, threshold: float = 0.5) -> bool:
        """
        Decide if a clause should be kept.

        Args:
            features: Clause features
            threshold: Quality threshold for keeping

        Returns:
            True if clause should be kept
        """
        # Always keep glue clauses (LBD ≤ 2)
        if features.lbd is not None and features.lbd <= self.glue_threshold:
            return True

        # Check quality score
        quality = self.predict_quality(features)
        return quality >= threshold

    def should_protect(self, features: ClauseFeatures) -> bool:
        """
        Decide if a clause should be protected from deletion.

        Protected clauses are never deleted (highest quality).

        Args:
            features: Clause features

        Returns:
            True if clause should be protected
        """
        # Protect glue clauses
        if features.lbd is not None and features.lbd <= self.glue_threshold:
            return True

        # Protect highly used clauses
        if features.age > 10:
            usage_rate = (features.propagation_count + features.conflict_count) / features.age
            if usage_rate > 0.5:  # Used in >50% of conflicts
                return True

        return False

    def rank_clauses(self, clause_features: List[ClauseFeatures]) -> List[ClauseFeatures]:
        """
        Rank clauses by quality (best first).

        Args:
            clause_features: List of clause features

        Returns:
            Sorted list (best quality first)
        """
        return sorted(clause_features,
                     key=lambda f: self.predict_quality(f),
                     reverse=True)

    def select_for_deletion(self,
                           clause_features: List[ClauseFeatures],
                           target_count: int) -> List[ClauseFeatures]:
        """
        Select clauses for deletion to reduce database size.

        Implements Glucose's deletion strategy:
        1. Never delete glue clauses (LBD ≤ 2)
        2. Delete lowest quality clauses first
        3. Keep target_count clauses

        Args:
            clause_features: List of all learned clause features
            target_count: Target number of clauses to keep

        Returns:
            List of clause features to delete
        """
        # Separate protected clauses
        protected = [f for f in clause_features if self.should_protect(f)]
        deletable = [f for f in clause_features if not self.should_protect(f)]

        # If protected clauses already exceed target, delete nothing
        if len(protected) >= target_count:
            return []

        # Rank deletable clauses by quality
        ranked_deletable = self.rank_clauses(deletable)

        # Keep best quality clauses up to target
        remaining_slots = target_count - len(protected)
        to_keep = ranked_deletable[:remaining_slots]
        to_delete = ranked_deletable[remaining_slots:]

        return to_delete

    def get_statistics(self, clause_features: List[ClauseFeatures]) -> dict:
        """Get statistics about clause quality distribution."""
        if not clause_features:
            return {
                'total_clauses': 0,
                'glue_clauses': 0,
                'high_quality': 0,
                'medium_quality': 0,
                'low_quality': 0,
                'avg_lbd': 0.0,
                'avg_quality': 0.0,
            }

        glue_count = sum(1 for f in clause_features
                        if f.lbd is not None and f.lbd <= self.glue_threshold)

        qualities = [self.predict_quality(f) for f in clause_features]
        high_quality = sum(1 for q in qualities if q >= 0.7)
        medium_quality = sum(1 for q in qualities if 0.3 <= q < 0.7)
        low_quality = sum(1 for q in qualities if q < 0.3)

        lbds = [f.lbd for f in clause_features if f.lbd is not None]
        avg_lbd = sum(lbds) / len(lbds) if lbds else 0.0
        avg_quality = sum(qualities) / len(qualities) if qualities else 0.0

        return {
            'total_clauses': len(clause_features),
            'glue_clauses': glue_count,
            'high_quality': high_quality,
            'medium_quality': medium_quality,
            'low_quality': low_quality,
            'avg_lbd': avg_lbd,
            'avg_quality': avg_quality,
        }

    def __repr__(self):
        return (
            f"QualityPredictor("
            f"lbd_threshold={self.lbd_threshold}, "
            f"glue_threshold={self.glue_threshold})"
        )
