"""
Topology Analyzer for SSTA-SAT

Analyzes the topological structure of the solution space by:
1. Building a solution graph based on Hamming distances
2. Detecting clusters/communities of similar solutions
3. Computing centrality metrics to identify important solutions
4. Finding bridge variables that connect different clusters
"""

from typing import Dict, List, Tuple, Set, Optional
from collections import defaultdict
import math


class SolutionGraph:
    """
    Graph representing the solution space topology.

    Nodes are solutions, edges connect nearby solutions (small Hamming distance).
    """

    def __init__(self, distance_threshold: int = 5):
        """
        Initialize solution graph.

        Args:
            distance_threshold: Maximum Hamming distance to create an edge
        """
        self.distance_threshold = distance_threshold
        self.nodes: List[int] = []  # Node IDs (solution indices)
        self.edges: Dict[int, List[Tuple[int, int]]] = defaultdict(list)  # node -> [(neighbor, weight)]
        self.solutions: List[Dict[str, bool]] = []

    def add_solution(self, solution_id: int, solution: Dict[str, bool]):
        """Add a solution as a node in the graph."""
        self.nodes.append(solution_id)
        self.solutions.append(solution)

    def add_edge(self, node1: int, node2: int, weight: int):
        """Add an edge between two solutions."""
        self.edges[node1].append((node2, weight))
        self.edges[node2].append((node1, weight))

    def get_neighbors(self, node: int) -> List[int]:
        """Get neighbors of a node."""
        return [neighbor for neighbor, _ in self.edges.get(node, [])]

    def get_degree(self, node: int) -> int:
        """Get degree (number of neighbors) of a node."""
        return len(self.edges.get(node, []))

    def is_connected(self, node1: int, node2: int) -> bool:
        """Check if two nodes are connected."""
        neighbors = self.get_neighbors(node1)
        return node2 in neighbors


class TopologyAnalyzer:
    """
    Analyzes the topology of the solution space.

    Computes various metrics like clustering, centrality, and community structure
    to understand how solutions are distributed and connected.
    """

    def __init__(self, solutions: List[Dict[str, bool]], distance_threshold: int = 5):
        """
        Initialize topology analyzer.

        Args:
            solutions: List of solution dictionaries
            distance_threshold: Hamming distance threshold for edges
        """
        self.solutions = solutions
        self.distance_threshold = distance_threshold
        self.graph = SolutionGraph(distance_threshold)
        self._build_graph()

    def _build_graph(self):
        """Build solution graph from solutions."""
        # Add all solutions as nodes
        for i, sol in enumerate(self.solutions):
            self.graph.add_solution(i, sol)

        # Add edges between nearby solutions
        for i in range(len(self.solutions)):
            for j in range(i + 1, len(self.solutions)):
                dist = self._hamming_distance(self.solutions[i], self.solutions[j])

                if dist <= self.distance_threshold:
                    self.graph.add_edge(i, j, dist)

    def _hamming_distance(self, sol1: Dict[str, bool], sol2: Dict[str, bool]) -> int:
        """Compute Hamming distance between two solutions."""
        distance = 0
        all_vars = set(sol1.keys()) | set(sol2.keys())

        for var in all_vars:
            if sol1.get(var) != sol2.get(var):
                distance += 1

        return distance

    def compute_clustering_coefficient(self, node: int) -> float:
        """
        Compute local clustering coefficient for a node.

        Measures how densely connected a node's neighbors are.

        Args:
            node: Node ID

        Returns:
            Clustering coefficient (0.0 to 1.0)
        """
        neighbors = self.graph.get_neighbors(node)
        k = len(neighbors)

        if k < 2:
            return 0.0

        # Count edges between neighbors
        edges_between_neighbors = 0
        for i in range(len(neighbors)):
            for j in range(i + 1, len(neighbors)):
                if self.graph.is_connected(neighbors[i], neighbors[j]):
                    edges_between_neighbors += 1

        # Clustering coefficient = (actual edges) / (possible edges)
        possible_edges = k * (k - 1) / 2
        return edges_between_neighbors / possible_edges if possible_edges > 0 else 0.0

    def compute_average_clustering(self) -> float:
        """Compute average clustering coefficient across all nodes."""
        if not self.graph.nodes:
            return 0.0

        total = sum(self.compute_clustering_coefficient(node) for node in self.graph.nodes)
        return total / len(self.graph.nodes)

    def compute_betweenness_centrality(self) -> Dict[int, float]:
        """
        Compute betweenness centrality for all nodes.

        Measures how often a node appears on shortest paths between other nodes.
        High betweenness = bridge between communities.

        Returns:
            Dictionary mapping node ID to centrality score
        """
        centrality = {node: 0.0 for node in self.graph.nodes}

        # For each pair of nodes, find shortest paths
        for source in self.graph.nodes:
            # BFS to find shortest paths from source
            shortest_paths = self._bfs_shortest_paths(source)

            for target in self.graph.nodes:
                if source >= target:  # Avoid double counting
                    continue

                paths = shortest_paths.get(target, [])
                if len(paths) == 0:
                    continue

                # Each node on a shortest path gets credit
                for path in paths:
                    for node in path[1:-1]:  # Exclude source and target
                        centrality[node] += 1.0 / len(paths)

        # Normalize by number of pairs
        n = len(self.graph.nodes)
        if n > 2:
            norm_factor = 2.0 / ((n - 1) * (n - 2))
            centrality = {node: score * norm_factor for node, score in centrality.items()}

        return centrality

    def _bfs_shortest_paths(self, source: int) -> Dict[int, List[List[int]]]:
        """
        Find all shortest paths from source using BFS.

        Returns:
            Dictionary mapping target node to list of shortest paths
        """
        from collections import deque

        # BFS data structures
        queue = deque([(source, [source])])
        visited_at_distance = {source: 0}
        paths = defaultdict(list)

        while queue:
            current, path = queue.popleft()
            current_dist = len(path) - 1

            for neighbor in self.graph.get_neighbors(current):
                new_dist = current_dist + 1

                # First time visiting this node at this distance
                if neighbor not in visited_at_distance:
                    visited_at_distance[neighbor] = new_dist
                    new_path = path + [neighbor]
                    paths[neighbor].append(new_path)
                    queue.append((neighbor, new_path))

                # Found another path of same length
                elif visited_at_distance[neighbor] == new_dist:
                    new_path = path + [neighbor]
                    paths[neighbor].append(new_path)

        return paths

    def detect_simple_clusters(self, min_cluster_size: int = 2) -> List[Set[int]]:
        """
        Detect clusters using connected components.

        Simple clustering: nodes in the same connected component are in the same cluster.

        Args:
            min_cluster_size: Minimum nodes per cluster

        Returns:
            List of clusters (sets of node IDs)
        """
        visited = set()
        clusters = []

        def dfs(node, cluster):
            """Depth-first search to find connected component."""
            if node in visited:
                return
            visited.add(node)
            cluster.add(node)

            for neighbor in self.graph.get_neighbors(node):
                dfs(neighbor, cluster)

        # Find all connected components
        for node in self.graph.nodes:
            if node not in visited:
                cluster = set()
                dfs(node, cluster)
                if len(cluster) >= min_cluster_size:
                    clusters.append(cluster)

        return clusters

    def find_central_solutions(self, top_k: int = 5) -> List[Tuple[int, float]]:
        """
        Find the most central solutions using betweenness centrality.

        Args:
            top_k: Number of central solutions to return

        Returns:
            List of (solution_id, centrality_score) tuples, sorted by centrality
        """
        centrality = self.compute_betweenness_centrality()
        sorted_nodes = sorted(centrality.items(), key=lambda x: -x[1])
        return sorted_nodes[:top_k]

    def find_bridge_variables(self, clusters: List[Set[int]]) -> Dict[str, int]:
        """
        Find variables that differ between clusters (bridge variables).

        Bridge variables are those that have different typical values in different clusters.

        Args:
            clusters: List of clusters (sets of solution IDs)

        Returns:
            Dictionary mapping variable name to "bridgeness" score
        """
        if len(clusters) < 2:
            return {}

        bridge_scores = defaultdict(int)

        # For each variable, check if it differs across clusters
        if not self.solutions:
            return {}

        all_vars = set(self.solutions[0].keys())

        for var in all_vars:
            # Compute majority value in each cluster
            cluster_values = []
            for cluster in clusters:
                values = [self.solutions[node_id].get(var, None) for node_id in cluster]
                true_count = sum(1 for v in values if v is True)
                false_count = sum(1 for v in values if v is False)
                majority = True if true_count > false_count else False
                cluster_values.append(majority)

            # Count how many clusters disagree
            disagreements = sum(
                1 for i in range(len(cluster_values))
                for j in range(i + 1, len(cluster_values))
                if cluster_values[i] != cluster_values[j]
            )

            bridge_scores[var] = disagreements

        return dict(bridge_scores)

    def get_statistics(self) -> dict:
        """Get topology statistics."""
        clusters = self.detect_simple_clusters()

        return {
            'num_solutions': len(self.solutions),
            'num_edges': sum(len(neighbors) for neighbors in self.graph.edges.values()) // 2,
            'avg_clustering': self.compute_average_clustering(),
            'num_clusters': len(clusters),
            'avg_cluster_size': sum(len(c) for c in clusters) / len(clusters) if clusters else 0,
            'largest_cluster_size': max((len(c) for c in clusters), default=0),
        }

    def __repr__(self):
        stats = self.get_statistics()
        return (
            f"TopologyAnalyzer("
            f"solutions={stats['num_solutions']}, "
            f"clusters={stats['num_clusters']}, "
            f"clustering={stats['avg_clustering']:.2f})"
        )
