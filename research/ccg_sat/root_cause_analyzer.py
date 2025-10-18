"""
Root Cause Analysis for CCG-SAT

Analyzes causality graph to identify root cause conflicts and guide restart decisions.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import List, Tuple, Optional
from .causality_graph import CausalityGraph


class RootCauseAnalyzer:
    """
    Analyzes causality graph to find root causes and guide restarts.

    Root causes: Conflicts/clauses with high out-degree (caused many downstream conflicts)
    Symptoms: Conflicts caused by root causes (should restart to avoid)
    """

    def __init__(self,
                 causality_graph: CausalityGraph,
                 root_cause_threshold: float = 0.8,
                 old_age_threshold: int = 5000):
        """
        Initialize root cause analyzer.

        Args:
            causality_graph: The causality graph to analyze
            root_cause_threshold: Percentile for root cause classification
            old_age_threshold: Age threshold for "old" root causes
        """
        self.graph = causality_graph
        self.root_cause_threshold = root_cause_threshold
        self.old_age_threshold = old_age_threshold

    def identify_root_causes(self, top_k: int = 10) -> List[Tuple[str, int]]:
        """
        Identify root cause nodes (high out-degree).

        Args:
            top_k: Number of root causes to return

        Returns:
            List of (node_id, out_degree) tuples
        """
        return self.graph.find_root_causes(top_k)

    def are_root_causes_old(self, current_conflict: int) -> bool:
        """
        Check if root causes are old (happened long ago).

        Old root causes suggest we're stuck in a bad search region → restart.

        Args:
            current_conflict: Current conflict count

        Returns:
            True if root causes are old
        """
        root_causes = self.identify_root_causes(top_k=5)

        if not root_causes:
            return False

        # Check if all top root causes are old
        ages = [self.graph.get_node_age(node_id, current_conflict)
                for node_id, _ in root_causes]

        if not ages:
            return False

        # All root causes old → restart recommended
        return all(age > self.old_age_threshold for age in ages)

    def compute_restart_score(self, current_conflict: int) -> float:
        """
        Compute score indicating whether restart is recommended.

        Higher score → restart more recommended.

        Args:
            current_conflict: Current conflict count

        Returns:
            Restart score (0.0 to 1.0)
        """
        # Check if root causes are old
        if self.are_root_causes_old(current_conflict):
            return 1.0  # Strong restart recommendation

        # Check graph structure
        stats = self.graph.analyze_causality_chains()

        if stats['total_nodes'] == 0:
            return 0.0

        # High average out-degree suggests cascading conflicts
        avg_out = stats['avg_out_degree']
        if avg_out > 3.0:  # Many causal chains
            return min(1.0, avg_out / 5.0)

        return 0.0

    def should_restart_based_on_causality(self,
                                         current_conflict: int,
                                         threshold: float = 0.7) -> bool:
        """
        Decide whether to restart based on causality analysis.

        Args:
            current_conflict: Current conflict count
            threshold: Score threshold for restart

        Returns:
            True if restart recommended
        """
        score = self.compute_restart_score(current_conflict)
        return score >= threshold

    def get_statistics(self) -> dict:
        """Get root cause analysis statistics."""
        root_causes = self.identify_root_causes(top_k=10)

        return {
            'root_causes_found': len(root_causes),
            'top_root_cause_degree': root_causes[0][1] if root_causes else 0,
            'graph_stats': self.graph.analyze_causality_chains(),
        }

    def __repr__(self):
        stats = self.get_statistics()
        return (
            f"RootCauseAnalyzer("
            f"root_causes={stats['root_causes_found']}, "
            f"top_degree={stats['top_root_cause_degree']})"
        )
