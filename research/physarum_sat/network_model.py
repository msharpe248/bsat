"""
Network Model for PHYSARUM-SAT

Represents the slime mold transport network:
- Nodes (variable junctions, clause food sources)
- Edges (tubes carrying nutrient flow)
- Network topology connecting variables to clauses

Based on Tero et al. (2006) Physarum model.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import List, Dict, Optional
from bsat.cnf import CNFExpression, Clause, Literal


class NetworkNode:
    """Node in slime mold network."""

    def __init__(self, node_id: str, node_type: str):
        """
        Initialize network node.

        Args:
            node_id: Unique identifier
            node_type: 'source', 'variable', 'clause', or 'sink'
        """
        self.node_id = node_id
        self.node_type = node_type
        self.pressure = 0.0  # Hydraulic pressure
        self.inflow_edges: List['NetworkEdge'] = []
        self.outflow_edges: List['NetworkEdge'] = []

    def add_inflow(self, edge: 'NetworkEdge'):
        """Add incoming edge."""
        self.inflow_edges.append(edge)

    def add_outflow(self, edge: 'NetworkEdge'):
        """Add outgoing edge."""
        self.outflow_edges.append(edge)

    def __repr__(self):
        return f"Node({self.node_id}, {self.node_type}, P={self.pressure:.2f})"


class NetworkEdge:
    """Tube connecting nodes (edge in slime mold network)."""

    def __init__(self, source: NetworkNode, target: NetworkNode, edge_id: str):
        """
        Initialize network edge.

        Args:
            source: Source node
            target: Target node
            edge_id: Unique identifier
        """
        self.source = source
        self.target = target
        self.edge_id = edge_id

        # Physical properties
        self.diameter = 1.0  # Tube conductivity
        self.length = 1.0    # Fixed length
        self.flow_rate = 0.0  # Current flow

    def compute_flow(self):
        """Compute flow based on pressure difference and tube properties."""
        pressure_diff = self.source.pressure - self.target.pressure
        self.flow_rate = pressure_diff * self.diameter / self.length
        return self.flow_rate

    def __repr__(self):
        return f"Edge({self.source.node_id}→{self.target.node_id}, Q={self.flow_rate:.3f})"


class SlimeMoldNetwork:
    """
    Complete slime mold network for SAT.

    Network structure:
    - Source node (high pressure)
    - Variable nodes (T/F path junctions)
    - Clause nodes (food sources, low pressure if hungry)
    - Edges connecting variables to clauses via literals
    """

    def __init__(self, cnf: CNFExpression):
        """
        Build network from CNF formula.

        Args:
            cnf: CNF formula
        """
        self.cnf = cnf
        self.nodes: Dict[str, NetworkNode] = {}
        self.edges: List[NetworkEdge] = []

        # Build network
        self._build_network()

    def _build_network(self):
        """Construct network topology from CNF."""

        # Create variable nodes (each has T and F paths)
        for var in self.cnf.get_variables():
            # Variable node (junction)
            var_node = NetworkNode(f"var_{var}", "variable")
            self.nodes[var_node.node_id] = var_node

            # True path node
            true_node = NetworkNode(f"{var}_T", "path")
            self.nodes[true_node.node_id] = true_node

            # False path node
            false_node = NetworkNode(f"{var}_F", "path")
            self.nodes[false_node.node_id] = false_node

            # Connect variable to paths
            self._add_edge(var_node, true_node, f"edge_{var}_to_T")
            self._add_edge(var_node, false_node, f"edge_{var}_to_F")

        # Create clause nodes (food sources)
        for i, clause in enumerate(self.cnf.clauses):
            clause_node = NetworkNode(f"clause_{i}", "clause")
            clause_node.clause = clause  # Store clause reference
            clause_node.satisfaction_threshold = 0.1  # Min flow to be satisfied
            clause_node.received_flow = 0.0
            self.nodes[clause_node.node_id] = clause_node

            # Connect literals to clause
            for literal in clause.literals:
                var = literal.variable
                value = not literal.negated  # True if positive literal

                # Source path node
                path_suffix = "T" if value else "F"
                path_node_id = f"{var}_{path_suffix}"

                if path_node_id in self.nodes:
                    path_node = self.nodes[path_node_id]
                    edge_id = f"edge_{var}_{path_suffix}_to_clause{i}"
                    self._add_edge(path_node, clause_node, edge_id)

    def _add_edge(self, source: NetworkNode, target: NetworkNode, edge_id: str):
        """Add edge to network."""
        edge = NetworkEdge(source, target, edge_id)
        self.edges.append(edge)
        source.add_outflow(edge)
        target.add_inflow(edge)

    def get_variable_nodes(self) -> List[NetworkNode]:
        """Get all variable nodes."""
        return [n for n in self.nodes.values() if n.node_type == "variable"]

    def get_clause_nodes(self) -> List[NetworkNode]:
        """Get all clause nodes."""
        return [n for n in self.nodes.values() if n.node_type == "clause"]

    def get_path_edges(self, var: str) -> tuple:
        """Get T and F path edges for a variable."""
        true_edge = None
        false_edge = None

        for edge in self.edges:
            if edge.source.node_id == f"var_{var}":
                if edge.target.node_id == f"{var}_T":
                    true_edge = edge
                elif edge.target.node_id == f"{var}_F":
                    false_edge = edge

        return true_edge, false_edge

    def __repr__(self):
        return f"Network({len(self.nodes)} nodes, {len(self.edges)} edges)"
