"""
Conflict Graph Builder and Analyzer

Builds a meta-level graph of variable conflicts from CDCL's learned clauses.
Nodes = variables, edges = conflicts between variables.

Graph Metrics:
- PageRank: Importance of variable in conflict structure
- Clustering: How tightly connected conflict groups are
- Centrality: Which variables are central to conflicts

Key Insight:
Variables that appear together in many conflict clauses are likely
to be interdependent. Branching on central variables first can resolve
conflicts more efficiently.
"""

import sys
import os
from typing import Dict, List, Optional, Set, Tuple
from collections import defaultdict
from dataclasses import dataclass

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression, Clause


@dataclass
class GraphMetrics:
    """Graph metrics for a variable."""
    variable: str
    pagerank: float
    degree: int
    clustering_coefficient: float
    betweenness_centrality: float


class ConflictGraph:
    """
    Meta-level conflict graph for SAT solving.

    Nodes: Variables
    Edges: (v1, v2) if v1 and v2 appear in a conflict clause together
    Edge weight: Number of conflict clauses containing both

    Usage:
    1. Build initial graph from formula
    2. Update incrementally as CDCL learns clauses
    3. Query metrics to guide variable selection
    """

    def __init__(self):
        """Initialize empty conflict graph."""
        # Adjacency list: var -> {neighbor: weight}
        self.adjacency: Dict[str, Dict[str, int]] = defaultdict(lambda: defaultdict(int))

        # Set of all variables
        self.variables: Set[str] = set()

        # Cached metrics (invalidated on update)
        self._pagerank_cache: Optional[Dict[str, float]] = None
        self._clustering_cache: Optional[Dict[str, float]] = None
        self._centrality_cache: Optional[Dict[str, float]] = None

    def add_conflict_clause(self, clause: Clause):
        """
        Add a conflict clause to the graph.

        Creates edges between all pairs of variables in the clause.

        Args:
            clause: Conflict clause (learned or from formula)
        """
        variables_in_clause = list(set(lit.variable for lit in clause.literals))

        # Add all variables
        for var in variables_in_clause:
            self.variables.add(var)

        # Add edges between all pairs (conflict co-occurrence)
        for i, var1 in enumerate(variables_in_clause):
            for var2 in variables_in_clause[i+1:]:
                # Undirected edge (both directions)
                self.adjacency[var1][var2] += 1
                self.adjacency[var2][var1] += 1

        # Invalidate caches
        self._invalidate_caches()

    def add_formula(self, cnf: CNFExpression):
        """
        Build initial graph from CNF formula.

        Args:
            cnf: CNF formula
        """
        for clause in cnf.clauses:
            self.add_conflict_clause(clause)

    def get_degree(self, variable: str) -> int:
        """
        Get degree (number of neighbors) for a variable.

        Args:
            variable: Variable name

        Returns:
            Degree of variable
        """
        return len(self.adjacency.get(variable, {}))

    def get_neighbors(self, variable: str) -> List[str]:
        """
        Get neighbors of a variable.

        Args:
            variable: Variable name

        Returns:
            List of neighbor variables
        """
        return list(self.adjacency.get(variable, {}).keys())

    def get_edge_weight(self, var1: str, var2: str) -> int:
        """
        Get weight of edge between two variables.

        Args:
            var1: First variable
            var2: Second variable

        Returns:
            Edge weight (number of conflict clauses containing both)
        """
        return self.adjacency.get(var1, {}).get(var2, 0)

    def compute_pagerank(self, iterations: int = 20, damping: float = 0.85) -> Dict[str, float]:
        """
        Compute PageRank for all variables.

        PageRank identifies "important" variables in conflict structure.
        High PageRank = central to many conflicts.

        Args:
            iterations: Number of PageRank iterations
            damping: Damping factor (0.85 typical)

        Returns:
            Dictionary mapping variables to PageRank scores
        """
        if self._pagerank_cache is not None:
            return self._pagerank_cache

        if not self.variables:
            return {}

        # Initialize PageRank
        n = len(self.variables)
        pagerank = {var: 1.0 / n for var in self.variables}

        # Power iteration
        for _ in range(iterations):
            new_pagerank = {}

            for var in self.variables:
                # Incoming PageRank from neighbors
                incoming = 0.0
                for neighbor in self.adjacency:
                    if var in self.adjacency[neighbor]:
                        neighbor_degree = self.get_degree(neighbor)
                        if neighbor_degree > 0:
                            incoming += pagerank[neighbor] / neighbor_degree

                # PageRank formula
                new_pagerank[var] = (1 - damping) / n + damping * incoming

            pagerank = new_pagerank

        self._pagerank_cache = pagerank
        return pagerank

    def compute_clustering_coefficient(self, variable: str) -> float:
        """
        Compute clustering coefficient for a variable.

        Measures how connected a variable's neighbors are to each other.
        High clustering = variable is part of tight conflict group.

        Args:
            variable: Variable name

        Returns:
            Clustering coefficient (0-1)
        """
        neighbors = self.get_neighbors(variable)
        k = len(neighbors)

        if k < 2:
            return 0.0

        # Count edges between neighbors
        edges_between_neighbors = 0
        for i, n1 in enumerate(neighbors):
            for n2 in neighbors[i+1:]:
                if self.get_edge_weight(n1, n2) > 0:
                    edges_between_neighbors += 1

        # Clustering coefficient
        max_edges = k * (k - 1) / 2
        return edges_between_neighbors / max_edges if max_edges > 0 else 0.0

    def compute_all_clustering_coefficients(self) -> Dict[str, float]:
        """
        Compute clustering coefficients for all variables.

        Returns:
            Dictionary mapping variables to clustering coefficients
        """
        if self._clustering_cache is not None:
            return self._clustering_cache

        clustering = {}
        for var in self.variables:
            clustering[var] = self.compute_clustering_coefficient(var)

        self._clustering_cache = clustering
        return clustering

    def compute_betweenness_centrality(self) -> Dict[str, float]:
        """
        Compute betweenness centrality for all variables (simplified).

        Betweenness measures how often a variable lies on shortest paths.
        High betweenness = variable is a "bridge" in conflict structure.

        Note: Full betweenness is O(n^3). This is a simplified O(n^2) approximation.

        Returns:
            Dictionary mapping variables to betweenness scores
        """
        if self._centrality_cache is not None:
            return self._centrality_cache

        betweenness = {var: 0.0 for var in self.variables}

        # For each variable as a source
        for source in self.variables:
            # BFS to find distances
            distances = {source: 0}
            queue = [source]
            visited = {source}

            while queue:
                current = queue.pop(0)
                current_dist = distances[current]

                for neighbor in self.get_neighbors(current):
                    if neighbor not in visited:
                        visited.add(neighbor)
                        distances[neighbor] = current_dist + 1
                        queue.append(neighbor)

                        # Increment betweenness for intermediate nodes
                        # (simplified: just count paths)
                        if current != source:
                            betweenness[current] += 1.0

        # Normalize
        n = len(self.variables)
        if n > 2:
            normalization = (n - 1) * (n - 2)
            betweenness = {var: score / normalization for var, score in betweenness.items()}

        self._centrality_cache = betweenness
        return betweenness

    def get_metrics(self, variable: str) -> GraphMetrics:
        """
        Get all metrics for a variable.

        Args:
            variable: Variable name

        Returns:
            GraphMetrics object with all metrics
        """
        pagerank = self.compute_pagerank()
        clustering = self.compute_all_clustering_coefficients()
        centrality = self.compute_betweenness_centrality()

        return GraphMetrics(
            variable=variable,
            pagerank=pagerank.get(variable, 0.0),
            degree=self.get_degree(variable),
            clustering_coefficient=clustering.get(variable, 0.0),
            betweenness_centrality=centrality.get(variable, 0.0)
        )

    def get_all_metrics(self) -> List[GraphMetrics]:
        """
        Get metrics for all variables.

        Returns:
            List of GraphMetrics objects
        """
        pagerank = self.compute_pagerank()
        clustering = self.compute_all_clustering_coefficients()
        centrality = self.compute_betweenness_centrality()

        metrics = []
        for var in self.variables:
            metrics.append(GraphMetrics(
                variable=var,
                pagerank=pagerank.get(var, 0.0),
                degree=self.get_degree(var),
                clustering_coefficient=clustering.get(var, 0.0),
                betweenness_centrality=centrality.get(var, 0.0)
            ))

        return metrics

    def get_top_k_by_pagerank(self, k: int) -> List[str]:
        """
        Get top-k variables by PageRank.

        Args:
            k: Number of variables to return

        Returns:
            List of variable names (highest PageRank first)
        """
        pagerank = self.compute_pagerank()
        sorted_vars = sorted(pagerank.items(), key=lambda x: x[1], reverse=True)
        return [var for var, _ in sorted_vars[:k]]

    def get_top_k_by_degree(self, k: int) -> List[str]:
        """
        Get top-k variables by degree.

        Args:
            k: Number of variables to return

        Returns:
            List of variable names (highest degree first)
        """
        degrees = {var: self.get_degree(var) for var in self.variables}
        sorted_vars = sorted(degrees.items(), key=lambda x: x[1], reverse=True)
        return [var for var, _ in sorted_vars[:k]]

    def get_top_k_by_centrality(self, k: int) -> List[str]:
        """
        Get top-k variables by betweenness centrality.

        Args:
            k: Number of variables to return

        Returns:
            List of variable names (highest centrality first)
        """
        centrality = self.compute_betweenness_centrality()
        sorted_vars = sorted(centrality.items(), key=lambda x: x[1], reverse=True)
        return [var for var, _ in sorted_vars[:k]]

    def _invalidate_caches(self):
        """Invalidate cached metrics."""
        self._pagerank_cache = None
        self._clustering_cache = None
        self._centrality_cache = None

    def get_statistics(self) -> Dict:
        """
        Get graph statistics.

        Returns:
            Dictionary with graph statistics
        """
        if not self.variables:
            return {
                'num_variables': 0,
                'num_edges': 0,
                'avg_degree': 0.0,
                'max_degree': 0,
                'graph_density': 0.0
            }

        # Count edges
        num_edges = sum(len(neighbors) for neighbors in self.adjacency.values()) // 2

        # Compute statistics
        degrees = [self.get_degree(var) for var in self.variables]
        avg_degree = sum(degrees) / len(degrees) if degrees else 0.0
        max_degree = max(degrees) if degrees else 0

        # Graph density
        n = len(self.variables)
        max_edges = n * (n - 1) / 2
        density = num_edges / max_edges if max_edges > 0 else 0.0

        return {
            'num_variables': len(self.variables),
            'num_edges': num_edges,
            'avg_degree': avg_degree,
            'max_degree': max_degree,
            'graph_density': density
        }

    def export_visualization_data(self) -> Dict:
        """
        Export graph data for visualization.

        Returns:
            Dictionary with nodes, edges, and metrics
        """
        pagerank = self.compute_pagerank()
        clustering = self.compute_all_clustering_coefficients()

        nodes = []
        for var in self.variables:
            nodes.append({
                'id': var,
                'pagerank': pagerank.get(var, 0.0),
                'degree': self.get_degree(var),
                'clustering': clustering.get(var, 0.0)
            })

        edges = []
        seen = set()
        for var1 in self.adjacency:
            for var2, weight in self.adjacency[var1].items():
                edge_key = tuple(sorted([var1, var2]))
                if edge_key not in seen:
                    seen.add(edge_key)
                    edges.append({
                        'source': var1,
                        'target': var2,
                        'weight': weight
                    })

        return {
            'nodes': nodes,
            'edges': edges,
            'statistics': self.get_statistics()
        }
