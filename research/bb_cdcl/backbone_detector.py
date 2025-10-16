"""
Backbone Detection via Statistical Sampling

This module uses randomized sampling (WalkSAT) to identify backbone variables -
variables that must have the same value in ALL satisfying assignments.

Key Concept:
If we sample many solutions and variable x is always True (or always False),
it's likely part of the backbone. We use confidence thresholds to decide when
to fix variables.
"""

import sys
import os
import math
from typing import Dict, List, Optional, Set, Tuple
from collections import defaultdict

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression
from bsat.walksat import WalkSATSolver


class BackboneDetector:
    """
    Detect backbone variables via statistical sampling.

    Backbone variable: A variable that has the same value in ALL solutions.

    Algorithm:
    1. Run WalkSAT with different random seeds to collect sample solutions
    2. Compute per-variable statistics (fraction of times True/False)
    3. Identify high-confidence backbone variables (>threshold consistency)
    4. Return backbone assignments with confidence scores

    Complexity:
    - Sampling: O(num_samples × WalkSAT_time)
    - Statistics: O(num_samples × num_variables)
    - Total: Typically very fast (samples take milliseconds each)
    """

    def __init__(self, cnf: CNFExpression,
                 num_samples: int = 100,
                 confidence_threshold: float = 0.95,
                 walksat_max_flips: int = 10000,
                 walksat_noise: float = 0.5):
        """
        Initialize backbone detector.

        Args:
            cnf: CNF formula to analyze
            num_samples: Number of solutions to sample
            confidence_threshold: Minimum confidence to consider backbone (0.95 = 95%)
            walksat_max_flips: Max flips per WalkSAT try
            walksat_noise: Noise parameter for WalkSAT
        """
        self.cnf = cnf
        self.num_samples = num_samples
        self.confidence_threshold = confidence_threshold
        self.walksat_max_flips = walksat_max_flips
        self.walksat_noise = walksat_noise

        # Sample solutions
        self.samples: List[Dict[str, bool]] = []

        # Statistics
        self.variable_stats: Dict[str, Dict[str, int]] = {}  # var -> {'true': count, 'false': count}
        self.backbone: Dict[str, bool] = {}  # var -> forced_value
        self.confidence_scores: Dict[str, Tuple[float, float]] = {}  # var -> (conf_true, conf_false)

    def detect_backbone(self) -> Dict[str, bool]:
        """
        Detect backbone variables via sampling.

        Returns:
            Dictionary mapping backbone variables to their forced values
        """
        # Collect samples
        self._collect_samples()

        if not self.samples:
            # No samples collected - formula might be UNSAT or very hard
            return {}

        # Compute statistics
        self._compute_statistics()

        # Identify backbone
        self._identify_backbone()

        return self.backbone

    def _collect_samples(self):
        """Collect sample solutions using WalkSAT with different seeds."""
        self.samples = []

        for seed in range(self.num_samples):
            solver = WalkSATSolver(
                self.cnf,
                noise=self.walksat_noise,
                max_flips=self.walksat_max_flips,
                max_tries=1,  # Single try per seed
                seed=seed
            )

            solution = solver.solve()

            if solution is not None:
                self.samples.append(solution)

            # Early stopping if we have enough diverse samples
            if len(self.samples) >= self.num_samples:
                break

    def _compute_statistics(self):
        """Compute per-variable statistics from samples."""
        if not self.samples:
            return

        variables = self.samples[0].keys()

        # Initialize counters
        for var in variables:
            self.variable_stats[var] = {'true': 0, 'false': 0}

        # Count occurrences
        for sample in self.samples:
            for var, value in sample.items():
                if value:
                    self.variable_stats[var]['true'] += 1
                else:
                    self.variable_stats[var]['false'] += 1

        # Compute confidence scores
        num_samples = len(self.samples)
        for var, counts in self.variable_stats.items():
            conf_true = counts['true'] / num_samples
            conf_false = counts['false'] / num_samples
            self.confidence_scores[var] = (conf_true, conf_false)

    def _identify_backbone(self):
        """Identify backbone variables based on confidence threshold."""
        self.backbone = {}

        for var, (conf_true, conf_false) in self.confidence_scores.items():
            if conf_true >= self.confidence_threshold:
                # Variable is almost always True -> backbone True
                self.backbone[var] = True
            elif conf_false >= self.confidence_threshold:
                # Variable is almost always False -> backbone False
                self.backbone[var] = False
            # else: not confident enough, not backbone

    def get_statistics(self) -> Dict:
        """
        Get detailed statistics about backbone detection.

        Returns:
            Dictionary with backbone statistics
        """
        total_vars = len(self.variable_stats)
        backbone_vars = len(self.backbone)

        if total_vars == 0:
            backbone_percentage = 0.0
        else:
            backbone_percentage = 100.0 * backbone_vars / total_vars

        # Count by confidence level
        high_conf = sum(1 for var, (ct, cf) in self.confidence_scores.items()
                       if max(ct, cf) >= 0.95)
        medium_conf = sum(1 for var, (ct, cf) in self.confidence_scores.items()
                         if 0.80 <= max(ct, cf) < 0.95)
        low_conf = sum(1 for var, (ct, cf) in self.confidence_scores.items()
                      if max(ct, cf) < 0.80)

        # Estimate search space reduction
        if total_vars > 0:
            search_space_reduction = 2 ** backbone_vars
        else:
            search_space_reduction = 1

        return {
            'num_samples': len(self.samples),
            'num_variables': total_vars,
            'num_backbone': backbone_vars,
            'backbone_percentage': backbone_percentage,
            'confidence_threshold': self.confidence_threshold,
            'high_confidence_vars': high_conf,  # >= 0.95
            'medium_confidence_vars': medium_conf,  # 0.80-0.95
            'low_confidence_vars': low_conf,  # < 0.80
            'estimated_search_reduction': search_space_reduction,
            'reduction_factor': f"2^{total_vars} → 2^{total_vars - backbone_vars}"
        }

    def get_confidence_for_variable(self, variable: str) -> Tuple[float, float]:
        """
        Get confidence scores for a specific variable.

        Args:
            variable: Variable name

        Returns:
            Tuple of (confidence_true, confidence_false)
        """
        return self.confidence_scores.get(variable, (0.0, 0.0))

    def get_all_confidence_scores(self) -> Dict[str, Tuple[float, float]]:
        """
        Get confidence scores for all variables.

        Returns:
            Dictionary mapping variables to (conf_true, conf_false)
        """
        return dict(self.confidence_scores)

    def get_backbone_strength(self) -> float:
        """
        Compute overall backbone strength.

        Returns a score between 0 and 1 indicating how strong the backbone is:
        - 1.0 = all variables are backbone (highly constrained)
        - 0.0 = no backbone variables (many solutions)

        Returns:
            Backbone strength score
        """
        total_vars = len(self.variable_stats)
        if total_vars == 0:
            return 0.0

        return len(self.backbone) / total_vars

    def visualize_confidence_data(self) -> Dict:
        """
        Export confidence data for visualization.

        Returns:
            Dictionary with visualization-ready confidence data
        """
        # Sort variables by confidence (highest first)
        sorted_vars = sorted(
            self.confidence_scores.items(),
            key=lambda x: max(x[1]),
            reverse=True
        )

        confidence_data = []
        for var, (conf_true, conf_false) in sorted_vars:
            is_backbone = var in self.backbone
            backbone_value = self.backbone.get(var, None)

            # Calculate Shannon entropy: H = -p*log2(p) - q*log2(q)
            # where p = conf_true, q = conf_false
            if conf_true > 0 and conf_false > 0:
                entropy = -(conf_true * math.log2(conf_true) + conf_false * math.log2(conf_false))
            else:
                entropy = 0.0  # No uncertainty if one probability is 0

            confidence_data.append({
                'variable': var,
                'confidence_true': conf_true,
                'confidence_false': conf_false,
                'max_confidence': max(conf_true, conf_false),
                'is_backbone': is_backbone,
                'backbone_value': backbone_value,
                'entropy': entropy
            })

        return {
            'confidence_data': confidence_data,
            'statistics': self.get_statistics(),
            'backbone_variables': self.backbone
        }


def detect_backbone(cnf: CNFExpression,
                   num_samples: int = 100,
                   confidence_threshold: float = 0.95) -> Dict[str, bool]:
    """
    Convenience function to detect backbone variables.

    Args:
        cnf: CNF formula to analyze
        num_samples: Number of solutions to sample
        confidence_threshold: Confidence threshold for backbone

    Returns:
        Dictionary mapping backbone variables to their forced values

    Example:
        >>> from bsat import CNFExpression
        >>> from research.bb_cdcl import detect_backbone
        >>> cnf = CNFExpression.parse("(a | b) & (a) & (~b | c)")
        >>> backbone = detect_backbone(cnf)
        >>> print(backbone)  # {'a': True} - 'a' is forced True
    """
    detector = BackboneDetector(cnf, num_samples=num_samples, confidence_threshold=confidence_threshold)
    return detector.detect_backbone()
