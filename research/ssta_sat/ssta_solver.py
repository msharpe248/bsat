"""
SSTA-SAT: Solution Space Topology Analysis SAT Solver

A SAT solver that analyzes the topological structure of the solution space
and uses this knowledge to guide CDCL search.

Key Innovation:
- Samples solutions to build a solution graph
- Analyzes topology (clusters, centrality, connectivity)
- Uses topological structure to guide variable selection
- Prefers variables that appear in central/well-connected solutions

This appears to be novel - theory on SAT solution space topology exists,
but no prior algorithms use it to guide CDCL search.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional, Set
from collections import defaultdict

from bsat.cnf import CNFExpression
from bsat.cdcl import CDCLSolver, CDCLStats

from .solution_sampler import SolutionSampler
from .topology_analyzer import TopologyAnalyzer


class SSTAStats(CDCLStats):
    """Extended statistics for SSTA-SAT solver."""

    def __init__(self):
        super().__init__()
        self.solutions_sampled = 0
        self.clusters_detected = 0
        self.topology_guided_decisions = 0
        self.central_solution_hits = 0

    def __str__(self):
        base_stats = super().__str__()
        ssta_stats = (
            f"  Solutions sampled: {self.solutions_sampled}\n"
            f"  Clusters detected: {self.clusters_detected}\n"
            f"  Topology-guided decisions: {self.topology_guided_decisions}\n"
            f"  Central solution hits: {self.central_solution_hits}\n"
        )
        return base_stats.rstrip(')') + '\n' + ssta_stats + ')'


class SSTASATSolver(CDCLSolver):
    """
    CDCL solver with Solution Space Topology Analysis.

    Samples solutions before systematic search, analyzes their topological
    structure, and uses topology to guide variable selection toward
    well-connected/central regions of the solution space.

    Example:
        >>> from bsat import CNFExpression
        >>> from research.ssta_sat import SSTASATSolver
        >>>
        >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
        >>> solver = SSTASATSolver(cnf, num_samples=50)
        >>> result = solver.solve()
    """

    def __init__(self,
                 cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000,
                 # SSTA-specific parameters
                 num_samples: int = 50,
                 distance_threshold: int = 5,
                 use_topology_guidance: bool = True,
                 min_unique_solutions: int = 10):
        """
        Initialize SSTA-SAT solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: VSIDS decay factor
            restart_base: Base interval for restarts
            learned_clause_limit: Max learned clauses
            num_samples: Number of solutions to sample
            distance_threshold: Hamming distance threshold for solution graph edges
            use_topology_guidance: Enable topology-guided variable selection
            min_unique_solutions: Stop sampling after this many unique solutions
        """
        # Initialize base CDCL solver
        super().__init__(cnf, vsids_decay, restart_base, learned_clause_limit)

        # SSTA parameters
        self.num_samples = num_samples
        self.distance_threshold = distance_threshold
        self.use_topology_guidance = use_topology_guidance
        self.min_unique_solutions = min_unique_solutions

        # Topology analysis components (initialized during solve)
        self.sampler: Optional[SolutionSampler] = None
        self.topology: Optional[TopologyAnalyzer] = None
        self.central_solutions: List[Dict[str, bool]] = []
        self.bridge_variables: Dict[str, int] = {}
        self.variable_preferences: Dict[str, float] = {}

        # Extended statistics
        self.stats = SSTAStats()

    def _sample_and_analyze_topology(self):
        """
        Sample solutions and analyze topology before systematic search.

        This is the preprocessing phase that builds our understanding of
        the solution space structure.
        """
        # Sample solutions using WalkSAT
        self.sampler = SolutionSampler(self.original_cnf)
        solutions = self.sampler.sample(
            num_samples=self.num_samples,
            min_unique_solutions=self.min_unique_solutions
        )

        self.stats.solutions_sampled = len(solutions)

        if len(solutions) < 2:
            # Not enough solutions for topology analysis
            return

        # Analyze topology
        self.topology = TopologyAnalyzer(
            solutions,
            distance_threshold=self.distance_threshold
        )

        # Detect clusters
        clusters = self.topology.detect_simple_clusters()
        self.stats.clusters_detected = len(clusters)

        # Find central solutions
        central_nodes = self.topology.find_central_solutions(top_k=min(5, len(solutions)))
        self.central_solutions = [solutions[node_id] for node_id, _ in central_nodes]

        # Find bridge variables
        if len(clusters) > 1:
            self.bridge_variables = self.topology.find_bridge_variables(clusters)

        # Compute variable preferences from topology
        self._compute_variable_preferences()

    def _compute_variable_preferences(self):
        """
        Compute variable preferences based on topology analysis.

        Variables that:
        1. Appear consistently in central solutions → high preference
        2. Are bridge variables between clusters → high preference
        3. Have consistent values across solutions → medium preference
        """
        self.variable_preferences = defaultdict(float)

        # Preference from central solutions
        if self.central_solutions:
            # Count how often each (var, value) pair appears in central solutions
            var_value_counts = defaultdict(int)
            for sol in self.central_solutions:
                for var, value in sol.items():
                    var_value_counts[(var, value)] += 1

            # Variables appearing frequently in central solutions get higher scores
            for (var, value), count in var_value_counts.items():
                consistency = count / len(self.central_solutions)
                self.variable_preferences[var] += consistency * 0.5  # Weight: 0.5

        # Preference from bridge variables
        if self.bridge_variables:
            max_bridge_score = max(self.bridge_variables.values()) if self.bridge_variables else 1
            for var, score in self.bridge_variables.items():
                normalized_score = score / max_bridge_score
                self.variable_preferences[var] += normalized_score * 0.3  # Weight: 0.3

    def _pick_branching_variable(self) -> Optional[str]:
        """
        Override variable selection to incorporate topology-guided preferences.

        Strategy:
        1. Get top candidates from VSIDS
        2. Boost scores based on topology preferences
        3. Choose variable with combined score
        """
        unassigned = [var for var in self.variables if var not in self.assignment]
        if not unassigned:
            return None

        if not self.use_topology_guidance or not self.variable_preferences:
            # Fall back to standard VSIDS
            return max(unassigned, key=lambda v: self.vsids_scores[v])

        # Compute combined scores: VSIDS + topology preference
        combined_scores = {}
        for var in unassigned:
            vsids_score = self.vsids_scores[var]
            topo_pref = self.variable_preferences.get(var, 0.0)

            # Combined score: weighted sum
            # VSIDS weight: 0.7, Topology weight: 0.3
            combined = 0.7 * vsids_score + 0.3 * topo_pref
            combined_scores[var] = combined

        chosen = max(unassigned, key=lambda v: combined_scores[v])

        # Track statistics
        if self.variable_preferences.get(chosen, 0.0) > 0:
            self.stats.topology_guided_decisions += 1

        return chosen

    def _pick_branching_phase(self, variable: str) -> bool:
        """
        Choose phase based on central solutions.

        If variable appears in central solutions, use the common value.
        Otherwise, default to True.
        """
        if not self.central_solutions:
            return True  # Default

        # Count values in central solutions
        true_count = sum(1 for sol in self.central_solutions
                        if sol.get(variable, None) is True)
        false_count = sum(1 for sol in self.central_solutions
                         if sol.get(variable, None) is False)

        if true_count > false_count:
            return True
        elif false_count > true_count:
            return False
        else:
            return True  # Default if tie

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using topology-guided CDCL.

        Args:
            max_conflicts: Maximum conflicts before giving up

        Returns:
            Solution if SAT, None if UNSAT
        """
        # Phase 1: Sample and analyze topology (only for SAT instances)
        if self.use_topology_guidance:
            self._sample_and_analyze_topology()

        # Phase 2: Standard CDCL with topology-guided decisions
        # Check for empty clause
        for clause in self.clauses:
            if len(clause.literals) == 0:
                return None

        # Initial unit propagation
        conflict = self._propagate()
        if conflict is not None:
            return None  # UNSAT at level 0

        while True:
            # Check conflict limit
            if self.stats.conflicts >= max_conflicts:
                return None  # Give up

            # Pick branching variable (topology-aware)
            var = self._pick_branching_variable()

            if var is None:
                # All variables assigned - SAT!
                return dict(self.assignment)

            # Pick phase (topology-aware)
            phase = self._pick_branching_phase(var)

            # Make decision
            self.decision_level += 1
            self.stats.max_decision_level = max(self.stats.max_decision_level, self.decision_level)
            self._assign(var, phase)

            # Propagate
            while True:
                conflict = self._propagate()

                if conflict is None:
                    break

                # Conflict!
                self.stats.conflicts += 1

                if self.decision_level == 0:
                    return None  # UNSAT

                # Analyze conflict and learn clause
                learned_clause, backtrack_level = self._analyze_conflict(conflict)

                if backtrack_level < 0:
                    return None  # UNSAT

                # Add learned clause
                self._add_learned_clause(learned_clause)

                # Backtrack
                self._unassign_to_level(backtrack_level)
                self.decision_level = backtrack_level
                self.stats.backjumps += 1

                # Decay VSIDS scores
                self._decay_vsids_scores()

                # Check for restart
                if self._should_restart():
                    self._restart()
                    conflict = self._propagate()
                    if conflict is not None:
                        return None  # UNSAT
                    break

    def get_topology_statistics(self) -> dict:
        """Get detailed topology statistics."""
        stats = {
            'sampling': {
                'num_samples_requested': self.num_samples,
                'num_solutions_found': self.stats.solutions_sampled,
                'min_unique_target': self.min_unique_solutions,
            },
            'topology': {},
            'guidance': {
                'topology_guided_decisions': self.stats.topology_guided_decisions,
                'central_solution_hits': self.stats.central_solution_hits,
            },
        }

        if self.topology:
            stats['topology'] = self.topology.get_statistics()

        if self.sampler:
            stats['sampling'].update(self.sampler.get_statistics())

        return stats


def solve_ssta_sat(cnf: CNFExpression,
                   num_samples: int = 50,
                   distance_threshold: int = 5,
                   max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using SSTA-SAT (Solution Space Topology Analysis).

    Args:
        cnf: CNF formula to solve
        num_samples: Number of solutions to sample for topology
        distance_threshold: Hamming distance threshold for solution graph
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Solution if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z)")
        >>> result = solve_ssta_sat(cnf, num_samples=30)
    """
    solver = SSTASATSolver(
        cnf,
        num_samples=num_samples,
        distance_threshold=distance_threshold
    )
    return solver.solve(max_conflicts=max_conflicts)
