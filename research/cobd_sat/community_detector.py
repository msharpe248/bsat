"""
Community Detection for SAT Formulas

This module detects community structure in the variable-clause bipartite graph
of a CNF formula using modularity optimization.

The algorithm builds a bipartite graph where:
- Variables and clauses are nodes
- Edges connect variables to the clauses they appear in

Communities are detected using a greedy modularity optimization algorithm
similar to the Louvain method.
"""

from typing import Dict, List, Set, Tuple, Optional
from collections import defaultdict
import random


class BipartiteGraph:
    """
    Bipartite graph representation of CNF formula.

    One partition contains variables, the other contains clauses.
    Edges exist only between variables and clauses.
    """

    def __init__(self):
        self.var_nodes: Set[str] = set()  # Variable nodes
        self.clause_nodes: Set[str] = set()  # Clause nodes (labeled as 'C0', 'C1', etc.)
        self.edges: Dict[str, Set[str]] = defaultdict(set)  # Adjacency list
        self.degrees: Dict[str, int] = defaultdict(int)
        self.total_edges = 0

    def add_variable(self, var: str):
        """Add a variable node."""
        self.var_nodes.add(var)

    def add_clause(self, clause_id: str):
        """Add a clause node."""
        self.clause_nodes.add(clause_id)

    def add_edge(self, var: str, clause_id: str):
        """Add an edge between variable and clause."""
        self.edges[var].add(clause_id)
        self.edges[clause_id].add(var)
        self.degrees[var] += 1
        self.degrees[clause_id] += 1
        self.total_edges += 1

    def neighbors(self, node: str) -> Set[str]:
        """Get neighbors of a node."""
        return self.edges.get(node, set())

    def get_all_nodes(self) -> Set[str]:
        """Get all nodes (variables + clauses)."""
        return self.var_nodes | self.clause_nodes


class VariableGraph:
    """
    Variable-variable graph (projection of bipartite graph).

    Two variables are connected if they appear in the same clause.
    Edge weight = number of clauses they share.
    """

    def __init__(self):
        self.nodes: Set[str] = set()
        self.edges: Dict[Tuple[str, str], int] = {}  # (var1, var2) -> weight
        self.neighbors: Dict[str, Set[str]] = defaultdict(set)
        self.degrees: Dict[str, int] = defaultdict(int)
        self.total_edges = 0

    def add_node(self, var: str):
        """Add a variable node."""
        self.nodes.add(var)

    def add_edge(self, var1: str, var2: str):
        """Add edge between two variables (increment weight if exists)."""
        if var1 == var2:
            return

        edge_key = tuple(sorted([var1, var2]))

        if edge_key in self.edges:
            self.edges[edge_key] += 1
        else:
            self.edges[edge_key] = 1
            self.neighbors[var1].add(var2)
            self.neighbors[var2].add(var1)
            self.total_edges += 1

        # Update degrees (weighted)
        self.degrees[var1] += 1
        self.degrees[var2] += 1


class CommunityDetector:
    """
    Detect communities in the variable-clause bipartite graph.

    Uses a greedy modularity optimization algorithm to partition the graph
    into communities that maximize modularity Q.

    FIXED: Projects bipartite graph to variable-variable graph first,
    then applies standard modularity optimization (works correctly!).

    Modularity Q = 1/(2m) * Σ[A_ij - k_i*k_j/(2m)] * δ(c_i, c_j)

    where:
    - m = total number of edges
    - A_ij = 1 if edge exists between i and j
    - k_i = degree of node i
    - c_i = community of node i
    - δ(c_i, c_j) = 1 if c_i == c_j else 0
    """

    def __init__(self, cnf):
        """
        Initialize community detector with a CNF formula.

        Args:
            cnf: CNFExpression to analyze
        """
        self.cnf = cnf
        self.graph = BipartiteGraph()  # Keep for interface detection
        self.var_graph = VariableGraph()  # NEW: projected variable graph
        self.communities: Dict[str, int] = {}  # node -> community_id
        self.community_members: Dict[int, Set[str]] = defaultdict(set)  # community_id -> nodes
        self._build_graph()

    def _build_graph(self):
        """Build bipartite graph AND projected variable graph from CNF formula."""
        # Add all variables
        variables = sorted(self.cnf.get_variables())
        for var in variables:
            self.graph.add_variable(var)
            self.var_graph.add_node(var)

        # Add clauses and edges
        for idx, clause in enumerate(self.cnf.clauses):
            clause_id = f"C{idx}"
            self.graph.add_clause(clause_id)

            # Get variables in this clause
            clause_vars = [lit.variable for lit in clause.literals]

            # Connect clause to its variables (bipartite graph)
            for var in clause_vars:
                self.graph.add_edge(var, clause_id)

            # Connect variables to each other (projected graph)
            # Two variables are connected if they appear in the same clause
            for i in range(len(clause_vars)):
                for j in range(i + 1, len(clause_vars)):
                    self.var_graph.add_edge(clause_vars[i], clause_vars[j])

    def detect_communities(self, min_communities: int = 2, max_communities: int = 10) -> Dict[str, int]:
        """
        Detect communities using greedy modularity optimization on PROJECTED variable graph.

        FIXED: Works on variable-variable graph instead of bipartite graph.
        This ensures modularity calculation works correctly!

        Args:
            min_communities: Minimum number of communities to form
            max_communities: Maximum number of communities to form

        Returns:
            Dictionary mapping variable names to community IDs
        """
        # Initialize: each VARIABLE in its own community
        nodes = list(self.var_graph.nodes)
        self.communities = {node: i for i, node in enumerate(nodes)}
        self.community_members = {i: {node} for i, node in enumerate(nodes)}

        current_modularity = self._compute_modularity()
        improved = True
        iteration = 0
        max_iterations = 100

        while improved and iteration < max_iterations:
            improved = False
            iteration += 1

            # Try moving each node to neighboring communities
            random.shuffle(nodes)  # Random order for fairness

            for node in nodes:
                current_community = self.communities[node]
                best_community = current_community
                best_modularity = current_modularity

                # Find neighboring communities (from variable graph)
                neighbor_communities = set()
                for neighbor in self.var_graph.neighbors.get(node, set()):
                    neighbor_communities.add(self.communities[neighbor])

                # Try moving to each neighbor community
                for target_community in neighbor_communities:
                    if target_community == current_community:
                        continue

                    # Try the move
                    delta_Q = self._modularity_delta(node, current_community, target_community)
                    new_modularity = current_modularity + delta_Q

                    if new_modularity > best_modularity:
                        best_modularity = new_modularity
                        best_community = target_community

                # Make the best move if it improves modularity
                if best_community != current_community:
                    self._move_node(node, current_community, best_community)
                    current_modularity = best_modularity
                    improved = True

        # Compact community IDs (remove empty communities)
        self._compact_communities()

        # If too many communities, merge smallest ones
        while len(self.community_members) > max_communities:
            self._merge_smallest_communities()

        # If too few communities and possible to split, split largest
        while len(self.community_members) < min_communities:
            if not self._split_largest_community():
                break  # Can't split further

        return self.communities

    def _compute_modularity(self) -> float:
        """
        Compute modularity Q of current community assignment on VARIABLE graph.

        FIXED: Uses projected variable graph for correct modularity!

        Q = 1/(2m) * Σ[A_ij - k_i*k_j/(2m)] * δ(c_i, c_j)
        """
        if self.var_graph.total_edges == 0:
            return 0.0

        m = self.var_graph.total_edges
        Q = 0.0

        # Sum over all edges in variable graph
        for (node_i, node_j), weight in self.var_graph.edges.items():
            # Check if same community
            if self.communities[node_i] == self.communities[node_j]:
                k_i = self.var_graph.degrees[node_i]
                k_j = self.var_graph.degrees[node_j]
                # Weighted contribution
                Q += weight - (k_i * k_j) / (2.0 * m)

        return Q / (2.0 * m)

    def _modularity_delta(self, node: str, old_community: int, new_community: int) -> float:
        """
        Compute change in modularity if node moves from old to new community.

        FIXED: Uses variable graph for incremental calculation.

        This is an incremental calculation that's much faster than recomputing
        the entire modularity.
        """
        m = self.var_graph.total_edges
        if m == 0:
            return 0.0

        k_i = self.var_graph.degrees.get(node, 0)

        # Sum of degrees in old and new communities
        sum_old = sum(self.var_graph.degrees.get(n, 0) for n in self.community_members[old_community])
        sum_new = sum(self.var_graph.degrees.get(n, 0) for n in self.community_members[new_community])

        # Weighted edges from node to old community
        edges_to_old = 0
        for neighbor in self.var_graph.neighbors.get(node, set()):
            if self.communities[neighbor] == old_community and neighbor != node:
                edge_key = tuple(sorted([node, neighbor]))
                edges_to_old += self.var_graph.edges.get(edge_key, 0)

        # Weighted edges from node to new community
        edges_to_new = 0
        for neighbor in self.var_graph.neighbors.get(node, set()):
            if self.communities[neighbor] == new_community:
                edge_key = tuple(sorted([node, neighbor]))
                edges_to_new += self.var_graph.edges.get(edge_key, 0)

        # Modularity change (simplified formula with weights)
        delta_Q = (edges_to_new - edges_to_old) / m
        delta_Q += k_i * (sum_old - sum_new - k_i) / (2.0 * m * m)

        return delta_Q

    def _move_node(self, node: str, old_community: int, new_community: int):
        """Move node from old community to new community."""
        self.community_members[old_community].remove(node)
        self.community_members[new_community].add(node)
        self.communities[node] = new_community

    def _compact_communities(self):
        """Remove empty communities and renumber sequentially."""
        # Find non-empty communities
        non_empty = sorted([cid for cid, members in self.community_members.items() if members])

        # Create new mapping
        new_id_map = {old_id: new_id for new_id, old_id in enumerate(non_empty)}

        # Update communities
        self.communities = {node: new_id_map[old_id]
                           for node, old_id in self.communities.items()}

        # Update community members
        new_members = defaultdict(set)
        for old_id, members in self.community_members.items():
            if old_id in new_id_map:
                new_members[new_id_map[old_id]] = members
        self.community_members = new_members

    def _merge_smallest_communities(self):
        """Merge the two smallest communities."""
        if len(self.community_members) < 2:
            return

        # Find two smallest communities
        sizes = [(cid, len(members)) for cid, members in self.community_members.items()]
        sizes.sort(key=lambda x: x[1])

        comm1, comm2 = sizes[0][0], sizes[1][0]

        # Merge comm2 into comm1
        for node in self.community_members[comm2]:
            self.communities[node] = comm1
        self.community_members[comm1].update(self.community_members[comm2])
        del self.community_members[comm2]

    def _split_largest_community(self) -> bool:
        """
        Split the largest community into two.

        Returns:
            True if split was performed, False if community too small to split
        """
        if not self.community_members:
            return False

        # Find largest community
        largest_id = max(self.community_members.keys(),
                        key=lambda cid: len(self.community_members[cid]))
        members = list(self.community_members[largest_id])

        if len(members) < 2:
            return False  # Can't split further

        # Split in half
        mid = len(members) // 2
        new_id = max(self.community_members.keys()) + 1

        # Move second half to new community
        for node in members[mid:]:
            self.communities[node] = new_id

        self.community_members[new_id] = set(members[mid:])
        self.community_members[largest_id] = set(members[:mid])

        return True

    def get_variable_communities(self) -> Dict[str, int]:
        """
        Get community assignments for variables.

        Returns:
            Dictionary mapping variable names to community IDs
        """
        # Since we only work with variables now, return all communities
        return dict(self.communities)

    def get_clause_communities(self) -> Dict[str, int]:
        """
        Get community assignments for clauses based on their variables.

        A clause is assigned to the community that contains the majority of its variables.

        Returns:
            Dictionary mapping clause IDs to community IDs
        """
        clause_communities = {}

        for idx, clause in enumerate(self.cnf.clauses):
            clause_id = f"C{idx}"
            clause_vars = [lit.variable for lit in clause.literals]

            # Count which communities the clause's variables belong to
            community_counts = defaultdict(int)
            for var in clause_vars:
                if var in self.communities:
                    community_counts[self.communities[var]] += 1

            # Assign clause to community with most variables
            if community_counts:
                best_community = max(community_counts.items(), key=lambda x: x[1])[0]
                clause_communities[clause_id] = best_community

        return clause_communities

    def identify_interface_variables(self) -> Set[str]:
        """
        Identify interface variables that connect multiple communities.

        An interface variable is one that appears in clauses from multiple communities.

        Returns:
            Set of interface variable names
        """
        interface_vars = set()
        var_communities = self.get_variable_communities()
        clause_communities = self.get_clause_communities()

        for var in self.graph.var_nodes:
            var_community = var_communities[var]

            # Check clauses this variable appears in
            communities_seen = set()
            for neighbor in self.graph.neighbors(var):
                if neighbor in clause_communities:
                    communities_seen.add(clause_communities[neighbor])

            # If variable connects to clauses in multiple communities, it's an interface
            if len(communities_seen) > 1:
                interface_vars.add(var)

        return interface_vars

    def get_statistics(self) -> Dict:
        """
        Get statistics about the detected communities.

        Returns:
            Dictionary with community statistics
        """
        var_communities = self.get_variable_communities()
        clause_communities = self.get_clause_communities()
        interface_vars = self.identify_interface_variables()

        # Count variables per community
        vars_per_community = defaultdict(int)
        for var, comm_id in var_communities.items():
            vars_per_community[comm_id] += 1

        # Count clauses per community
        clauses_per_community = defaultdict(int)
        for clause, comm_id in clause_communities.items():
            clauses_per_community[comm_id] += 1

        modularity = self._compute_modularity()

        return {
            'num_communities': len(self.community_members),
            'modularity': modularity,
            'num_variables': len(self.graph.var_nodes),
            'num_clauses': len(self.graph.clause_nodes),
            'num_interface_variables': len(interface_vars),
            'interface_percentage': 100.0 * len(interface_vars) / len(self.graph.var_nodes) if self.graph.var_nodes else 0,
            'vars_per_community': dict(vars_per_community),
            'clauses_per_community': dict(clauses_per_community),
            'average_vars_per_community': sum(vars_per_community.values()) / len(vars_per_community) if vars_per_community else 0,
            'average_clauses_per_community': sum(clauses_per_community.values()) / len(clauses_per_community) if clauses_per_community else 0,
        }

    def visualize_graph_data(self) -> Dict:
        """
        Export graph data for visualization.

        Returns:
            Dictionary with nodes and edges for D3.js visualization
        """
        nodes = []

        # Add variable nodes
        var_communities = self.get_variable_communities()
        interface_vars = self.identify_interface_variables()
        for var in self.graph.var_nodes:
            nodes.append({
                'id': var,
                'type': 'variable',
                'community': var_communities[var],
                'is_interface': var in interface_vars,
                'degree': self.graph.degrees[var]
            })

        # Add clause nodes
        clause_communities = self.get_clause_communities()
        for clause_id in self.graph.clause_nodes:
            nodes.append({
                'id': clause_id,
                'type': 'clause',
                'community': clause_communities[clause_id],
                'is_interface': False,
                'degree': self.graph.degrees[clause_id]
            })

        # Add edges
        edges = []
        processed = set()
        for node_i in self.graph.get_all_nodes():
            for node_j in self.graph.neighbors(node_i):
                edge_key = tuple(sorted([node_i, node_j]))
                if edge_key not in processed:
                    edges.append({
                        'source': node_i,
                        'target': node_j
                    })
                    processed.add(edge_key)

        return {
            'nodes': nodes,
            'edges': edges,
            'statistics': self.get_statistics()
        }
