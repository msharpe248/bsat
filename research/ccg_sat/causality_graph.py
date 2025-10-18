"""
Conflict Causality Graph for CCG-SAT

Tracks causal relationships between conflicts and learned clauses during CDCL search.
Builds a directed graph where edges represent "clause was used to derive conflict".

Novel contribution: Uses causality analysis ONLINE during solving (not post-hoc).
Related work: CausalSAT (Yang 2023) - but for post-hoc explanation, not online guidance.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Set, Tuple, Optional
from collections import defaultdict
from bsat.cnf import Clause


class CausalityNode:
    """Node in the causality graph (conflict or learned clause)."""

    def __init__(self, node_id: str, node_type: str, timestamp: int):
        """
        Initialize causality node.

        Args:
            node_id: Unique node identifier
            node_type: 'conflict' or 'learned'
            timestamp: When this node was created (conflict count)
        """
        self.node_id = node_id
        self.node_type = node_type
        self.timestamp = timestamp
        self.in_edges: Set[str] = set()  # Nodes that caused this
        self.out_edges: Set[str] = set()  # Nodes this caused

    def __repr__(self):
        return f"{self.node_type}({self.node_id}, in={len(self.in_edges)}, out={len(self.out_edges)})"


class CausalityGraph:
    """
    Directed graph tracking causal relationships in CDCL search.

    Nodes: Conflicts and learned clauses
    Edges: "Clause L was used to derive conflict C"

    This enables root cause analysis and intelligent restart decisions.
    """

    def __init__(self, max_nodes: int = 10000):
        """
        Initialize causality graph.

        Args:
            max_nodes: Maximum nodes to track (memory limit)
        """
        self.max_nodes = max_nodes
        self.nodes: Dict[str, CausalityNode] = {}
        self.clause_to_node: Dict[Clause, str] = {}  # Map clause objects to node IDs

        # Counters
        self.conflict_count = 0
        self.learned_count = 0

    def add_conflict(self,
                    conflict_id: str,
                    participating_clauses: List[Clause]) -> str:
        """
        Add a conflict node to the graph.

        Args:
            conflict_id: Unique identifier for this conflict
            participating_clauses: Clauses that participated in conflict analysis

        Returns:
            The conflict node ID
        """
        # Create conflict node
        conflict_node = CausalityNode(
            conflict_id,
            'conflict',
            self.conflict_count
        )

        # Add edges from participating learned clauses to this conflict
        for clause in participating_clauses:
            if clause in self.clause_to_node:
                clause_node_id = self.clause_to_node[clause]
                if clause_node_id in self.nodes:
                    # Edge: clause → conflict
                    conflict_node.in_edges.add(clause_node_id)
                    self.nodes[clause_node_id].out_edges.add(conflict_id)

        # Add to graph (if under limit)
        if len(self.nodes) < self.max_nodes:
            self.nodes[conflict_id] = conflict_node

        self.conflict_count += 1
        return conflict_id

    def add_learned_clause(self,
                          learned_clause: Clause,
                          source_conflict_id: str) -> str:
        """
        Add a learned clause node to the graph.

        Args:
            learned_clause: The learned clause
            source_conflict_id: ID of conflict that generated this clause

        Returns:
            The learned clause node ID
        """
        clause_id = f"L{self.learned_count}"

        # Create learned clause node
        clause_node = CausalityNode(
            clause_id,
            'learned',
            self.conflict_count
        )

        # Edge: conflict → learned clause
        if source_conflict_id in self.nodes:
            clause_node.in_edges.add(source_conflict_id)
            self.nodes[source_conflict_id].out_edges.add(clause_id)

        # Add to graph (if under limit)
        if len(self.nodes) < self.max_nodes:
            self.nodes[clause_id] = clause_node
            self.clause_to_node[learned_clause] = clause_id

        self.learned_count += 1
        return clause_id

    def compute_out_degree(self) -> Dict[str, int]:
        """
        Compute out-degree for all nodes.

        High out-degree = root cause (caused many downstream conflicts).

        Returns:
            Map from node_id to out-degree
        """
        return {node_id: len(node.out_edges)
                for node_id, node in self.nodes.items()}

    def find_root_causes(self, top_k: int = 10) -> List[Tuple[str, int]]:
        """
        Find nodes with highest out-degree (root causes).

        Args:
            top_k: Number of top root causes to return

        Returns:
            List of (node_id, out_degree) tuples, sorted by degree
        """
        out_degrees = self.compute_out_degree()
        sorted_nodes = sorted(out_degrees.items(),
                             key=lambda x: x[1],
                             reverse=True)
        return sorted_nodes[:top_k]

    def get_node_age(self, node_id: str, current_conflict: int) -> int:
        """
        Get age of a node (conflicts since it was created).

        Args:
            node_id: Node to check
            current_conflict: Current conflict count

        Returns:
            Age in conflicts
        """
        if node_id not in self.nodes:
            return 0
        return current_conflict - self.nodes[node_id].timestamp

    def analyze_causality_chains(self) -> dict:
        """
        Analyze causality chains in the graph.

        Returns:
            Statistics about causal structure
        """
        if not self.nodes:
            return {
                'total_nodes': 0,
                'conflict_nodes': 0,
                'learned_nodes': 0,
                'total_edges': 0,
                'avg_out_degree': 0.0,
                'max_out_degree': 0,
            }

        conflict_nodes = sum(1 for n in self.nodes.values() if n.node_type == 'conflict')
        learned_nodes = sum(1 for n in self.nodes.values() if n.node_type == 'learned')
        total_edges = sum(len(n.out_edges) for n in self.nodes.values())

        out_degrees = [len(n.out_edges) for n in self.nodes.values()]
        avg_out = sum(out_degrees) / len(out_degrees) if out_degrees else 0.0
        max_out = max(out_degrees) if out_degrees else 0

        return {
            'total_nodes': len(self.nodes),
            'conflict_nodes': conflict_nodes,
            'learned_nodes': learned_nodes,
            'total_edges': total_edges,
            'avg_out_degree': avg_out,
            'max_out_degree': max_out,
        }

    def __repr__(self):
        stats = self.analyze_causality_chains()
        return (
            f"CausalityGraph("
            f"nodes={stats['total_nodes']}, "
            f"edges={stats['total_edges']}, "
            f"avg_out={stats['avg_out_degree']:.1f})"
        )
