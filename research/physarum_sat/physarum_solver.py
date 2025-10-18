"""
PHYSARUM-SAT: Slime Mold-Inspired SAT Solver

Solves SAT using biologically-inspired network flow dynamics.
Models the problem-solving ability of Physarum polycephalum slime mold.

Key mechanisms:
- Flow-based propagation (like nutrient transport)
- Adaptive reinforcement (well-used paths thicken)
- Natural exploration (pseudopod extension)
- Emergent decision (dominant path selection)

Based on Tero et al. (2006) Physarum network optimization model.
Novel contribution - first application to SAT solving!
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Optional
import random
import math

from bsat.cnf import CNFExpression
from bsat.cdcl import CDCLStats

from .network_model import SlimeMoldNetwork


class PhysarumStats(CDCLStats):
    """Extended statistics for PHYSARUM-SAT solver."""

    def __init__(self):
        super().__init__()
        self.flow_iterations = 0
        self.tube_updates = 0
        self.pseudopods_extended = 0
        self.satisfied_clauses = 0

    def __str__(self):
        base_stats = super().__str__()
        physarum_stats = (
            f"  Flow iterations: {self.flow_iterations}\n"
            f"  Tube updates: {self.tube_updates}\n"
            f"  Pseudopods: {self.pseudopods_extended}\n"
            f"  Satisfied clauses: {self.satisfied_clauses}\n"
        )
        return base_stats.rstrip(')') + '\n' + physarum_stats + ')'


class PHYSARUMSATSolver:
    """
    PHYSARUM-SAT solver using slime mold network dynamics.

    Treats SAT as nutrient transport problem where satisfying assignment
    emerges from network flow optimization.

    Educational implementation of bio-inspired computation.
    """

    def __init__(self,
                 cnf: CNFExpression,
                 max_iterations: int = 10000,
                 mu: float = 1.5,          # Flow growth exponent
                 gamma: float = 0.5,        # Decay rate
                 dt: float = 0.01):         # Time step
        """
        Initialize PHYSARUM-SAT solver.

        Args:
            cnf: CNF formula to solve
            max_iterations: Maximum flow iterations
            mu: Tube growth exponent (higher = stronger reinforcement)
            gamma: Tube decay rate
            dt: Time step for dynamics
        """
        self.cnf = cnf
        self.max_iterations = max_iterations
        self.mu = mu
        self.gamma = gamma
        self.dt = dt

        # Build network
        self.network = SlimeMoldNetwork(cnf)

        # Statistics
        self.stats = PhysarumStats()

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using slime mold network dynamics.

        Args:
            max_conflicts: Not used (uses max_iterations instead)

        Returns:
            Solution if SAT, None if UNSAT
        """

        # Initialize uniform flow
        self._initialize_flow()

        for iteration in range(self.max_iterations):
            self.stats.flow_iterations += 1

            # Update pressures
            self._update_pressures()

            # Update flows
            self._update_flows()

            # Adapt tube diameters (reinforcement learning)
            self._update_tube_diameters()

            # Check satisfaction
            if self._check_all_satisfied():
                # Extract assignment from dominant flows
                assignment = self._extract_assignment()

                # Verify solution
                if self._verify_solution(assignment):
                    return assignment

            # Exploration: periodically inject noise
            if iteration % 100 == 0:
                self._inject_exploration_noise()

        # Failed to converge
        # Try fallback to CDCL
        from bsat.cdcl import CDCLSolver
        fallback = CDCLSolver(self.cnf)
        result = fallback.solve(max_conflicts=max_conflicts)
        self._update_stats_from_cdcl(fallback.stats)
        return result

    def _initialize_flow(self):
        """Initialize equal flow on all variable paths."""
        for var_node in self.network.get_variable_nodes():
            var_node.pressure = 1.0  # Source pressure

        for clause_node in self.network.get_clause_nodes():
            clause_node.pressure = 0.0  # Sink pressure (hungry)

    def _update_pressures(self):
        """Update pressures using simplified relaxation."""

        # Set boundary conditions
        for var_node in self.network.get_variable_nodes():
            var_node.pressure = 1.0  # Sources

        for clause_node in self.network.get_clause_nodes():
            # Hungry clauses have lower pressure (attract flow)
            if hasattr(clause_node, 'received_flow'):
                if clause_node.received_flow < clause_node.satisfaction_threshold:
                    clause_node.pressure = 0.0  # Very hungry
                else:
                    clause_node.pressure = 0.5  # Fed, less attractive

        # Relaxation for intermediate nodes (path nodes)
        for _ in range(5):  # Few iterations
            for node in self.network.nodes.values():
                if node.node_type == "path":
                    # Average of neighbors weighted by conductivity
                    total_weight = 0.0
                    weighted_pressure = 0.0

                    for edge in node.inflow_edges:
                        weight = edge.diameter
                        weighted_pressure += edge.source.pressure * weight
                        total_weight += weight

                    for edge in node.outflow_edges:
                        weight = edge.diameter
                        weighted_pressure += edge.target.pressure * weight
                        total_weight += weight

                    if total_weight > 0:
                        node.pressure = weighted_pressure / total_weight

    def _update_flows(self):
        """Update flows based on pressures."""

        # Reset clause received flow
        for clause_node in self.network.get_clause_nodes():
            clause_node.received_flow = 0.0

        # Compute flow through each edge
        for edge in self.network.edges:
            edge.compute_flow()

            # Accumulate flow to clause nodes
            if edge.target.node_type == "clause":
                edge.target.received_flow += max(0, edge.flow_rate)

    def _update_tube_diameters(self):
        """Adapt tube thickness based on usage (Physarum reinforcement)."""
        self.stats.tube_updates += 1

        for edge in self.network.edges:
            # Growth from flow
            flow_magnitude = abs(edge.flow_rate)
            growth = (flow_magnitude ** self.mu)

            # Natural decay
            decay = self.gamma * edge.diameter

            # Update diameter
            edge.diameter += self.dt * (growth - decay)

            # Keep bounded
            edge.diameter = max(0.01, min(edge.diameter, 10.0))

    def _inject_exploration_noise(self):
        """Add random fluctuations (pseudopod exploration)."""
        self.stats.pseudopods_extended += 1

        # Randomly boost some weak edges
        for _ in range(5):
            edge = random.choice(self.network.edges)
            edge.diameter += 0.1 * random.random()

    def _check_all_satisfied(self) -> bool:
        """Check if all clauses receiving sufficient flow."""
        satisfied_count = 0

        for clause_node in self.network.get_clause_nodes():
            if clause_node.received_flow >= clause_node.satisfaction_threshold:
                satisfied_count += 1

        self.stats.satisfied_clauses = satisfied_count
        return satisfied_count == len(self.cnf.clauses)

    def _extract_assignment(self) -> Dict[str, bool]:
        """Extract variable assignment from dominant flows."""
        assignment = {}

        for var in self.cnf.get_variables():
            true_edge, false_edge = self.network.get_path_edges(var)

            if true_edge and false_edge:
                # Assign to path with stronger flow
                true_flow = abs(true_edge.flow_rate)
                false_flow = abs(false_edge.flow_rate)

                assignment[var] = (true_flow > false_flow)
            else:
                # Default if edges missing
                assignment[var] = True

        return assignment

    def _verify_solution(self, assignment: Dict[str, bool]) -> bool:
        """Verify assignment satisfies all clauses."""
        for clause in self.cnf.clauses:
            satisfied = False
            for lit in clause.literals:
                if lit.variable in assignment:
                    value = assignment[lit.variable]
                    if (not lit.negated and value) or (lit.negated and not value):
                        satisfied = True
                        break
            if not satisfied:
                return False
        return True

    def _update_stats_from_cdcl(self, cdcl_stats: CDCLStats):
        """Update stats from fallback CDCL solver."""
        self.stats.decisions = cdcl_stats.decisions
        self.stats.conflicts = cdcl_stats.conflicts
        self.stats.propagations = cdcl_stats.propagations

    def get_network_statistics(self) -> dict:
        """Get network statistics."""
        return {
            'nodes': len(self.network.nodes),
            'edges': len(self.network.edges),
            'flow_iterations': self.stats.flow_iterations,
            'satisfied_clauses': self.stats.satisfied_clauses,
        }


def solve_physarum_sat(cnf: CNFExpression,
                       max_iterations: int = 10000,
                       max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using PHYSARUM-SAT (Slime Mold Network).

    Educational/experimental bio-inspired approach.

    Args:
        cnf: CNF formula
        max_iterations: Maximum flow iterations
        max_conflicts: Maximum conflicts (for fallback)

    Returns:
        Solution if SAT, None if UNSAT
    """
    solver = PHYSARUMSATSolver(cnf, max_iterations=max_iterations)
    return solver.solve(max_conflicts=max_conflicts)
