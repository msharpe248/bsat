"""
Abstraction Builder for HAS-SAT

Builds hierarchical abstraction from variable clusters.
Creates multiple levels: high-level (clusters) → low-level (individual variables).
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Set, Tuple, Optional
from collections import defaultdict
from bsat.cnf import CNFExpression, Clause, Literal


class VariableCluster:
    """Cluster of related variables forming an abstract variable."""

    def __init__(self, cluster_id: int, variables: Set[str]):
        """
        Initialize variable cluster.

        Args:
            cluster_id: Unique cluster identifier
            variables: Set of variables in this cluster
        """
        self.cluster_id = cluster_id
        self.variables = variables
        self.abstract_var = f"C{cluster_id}"  # Abstract variable name

    def contains(self, variable: str) -> bool:
        """Check if variable is in this cluster."""
        return variable in self.variables

    def __repr__(self):
        vars_str = ", ".join(sorted(self.variables))
        return f"Cluster({self.abstract_var}: {{{vars_str}}})"


class AbstractionLevel:
    """Single level in abstraction hierarchy."""

    def __init__(self, level: int, clusters: List[VariableCluster]):
        """
        Initialize abstraction level.

        Args:
            level: Level number (0 = most abstract, higher = more concrete)
            clusters: Variable clusters at this level
        """
        self.level = level
        self.clusters = clusters

        # Map variable → cluster
        self.var_to_cluster: Dict[str, VariableCluster] = {}
        for cluster in clusters:
            for var in cluster.variables:
                self.var_to_cluster[var] = cluster

    def get_cluster(self, variable: str) -> Optional[VariableCluster]:
        """Get cluster containing variable."""
        return self.var_to_cluster.get(variable)

    def abstract_clause(self, clause: Clause) -> Clause:
        """
        Abstract a clause to this level.

        Maps each literal to its cluster's abstract variable.
        Removes duplicate abstract literals.

        Args:
            clause: Original clause

        Returns:
            Abstract clause with cluster variables
        """
        abstract_literals: Set[Literal] = set()

        for lit in clause.literals:
            cluster = self.get_cluster(lit.variable)
            if cluster:
                # Create abstract literal with cluster variable
                abstract_lit = Literal(cluster.abstract_var, lit.negated)
                abstract_literals.add(abstract_lit)
            else:
                # Variable not in any cluster - keep as is
                abstract_literals.add(lit)

        return Clause(list(abstract_literals))

    def __repr__(self):
        return f"Level {self.level}: {len(self.clusters)} clusters"


class AbstractionHierarchy:
    """
    Hierarchical abstraction of CNF formula.

    Builds multiple abstraction levels from fine-grained to coarse-grained.
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize abstraction hierarchy.

        Args:
            cnf: Original CNF formula
        """
        self.cnf = cnf
        self.levels: List[AbstractionLevel] = []

    def build_hierarchy(self, num_levels: int = 2) -> List[AbstractionLevel]:
        """
        Build abstraction hierarchy.

        Args:
            num_levels: Number of abstraction levels to create

        Returns:
            List of abstraction levels (most abstract first)
        """
        # Start with all variables
        all_vars = set(self.cnf.get_variables())

        # Build variable co-occurrence graph
        cooccurrence = self._build_cooccurrence_graph(all_vars)

        # Build levels (from most abstract to least)
        current_vars = all_vars
        for level_idx in range(num_levels):
            # Cluster variables at this level
            cluster_size = max(2, len(current_vars) // (2 ** (level_idx + 1)))
            clusters = self._cluster_variables(current_vars, cooccurrence, cluster_size)

            # Create abstraction level
            level = AbstractionLevel(level_idx, clusters)
            self.levels.append(level)

            # Next level: individual variables (for final level)
            if level_idx == num_levels - 2:
                # Last iteration - next level is concrete (individual variables)
                break

        return self.levels

    def _build_cooccurrence_graph(self, variables: Set[str]) -> Dict[Tuple[str, str], int]:
        """
        Build co-occurrence graph: how often pairs of variables appear together.

        Args:
            variables: Set of variables

        Returns:
            Map from (var1, var2) → co-occurrence count
        """
        cooccurrence: Dict[Tuple[str, str], int] = defaultdict(int)

        for clause in self.cnf.clauses:
            clause_vars = [lit.variable for lit in clause.literals]

            # Count co-occurrences
            for i, var1 in enumerate(clause_vars):
                for var2 in clause_vars[i+1:]:
                    if var1 in variables and var2 in variables:
                        pair = tuple(sorted([var1, var2]))
                        cooccurrence[pair] += 1

        return cooccurrence

    def _cluster_variables(self,
                          variables: Set[str],
                          cooccurrence: Dict[Tuple[str, str], int],
                          target_cluster_size: int) -> List[VariableCluster]:
        """
        Cluster variables based on co-occurrence.

        Uses greedy clustering: start with variable, add most co-occurring neighbors.

        Args:
            variables: Variables to cluster
            cooccurrence: Co-occurrence counts
            target_cluster_size: Target size for each cluster

        Returns:
            List of variable clusters
        """
        clusters: List[VariableCluster] = []
        unclustered = set(variables)
        cluster_id = 0

        while unclustered:
            # Start new cluster with arbitrary variable
            seed_var = next(iter(unclustered))
            cluster_vars = {seed_var}
            unclustered.remove(seed_var)

            # Grow cluster to target size
            while len(cluster_vars) < target_cluster_size and unclustered:
                # Find most co-occurring variable with cluster
                best_var = None
                best_score = -1

                for var in unclustered:
                    # Score: sum of co-occurrences with cluster members
                    score = sum(
                        cooccurrence.get(tuple(sorted([var, cluster_var])), 0)
                        for cluster_var in cluster_vars
                    )

                    if score > best_score:
                        best_score = score
                        best_var = var

                if best_var is None:
                    break  # No more co-occurring variables

                # Add to cluster
                cluster_vars.add(best_var)
                unclustered.remove(best_var)

            # Create cluster
            cluster = VariableCluster(cluster_id, cluster_vars)
            clusters.append(cluster)
            cluster_id += 1

        return clusters

    def get_abstract_cnf(self, level: int) -> CNFExpression:
        """
        Get abstract CNF at specified level.

        Args:
            level: Abstraction level (0 = most abstract)

        Returns:
            Abstract CNF formula
        """
        if level >= len(self.levels):
            # Beyond abstraction - return original
            return self.cnf

        abstraction = self.levels[level]

        # Abstract each clause
        abstract_clauses = []
        for clause in self.cnf.clauses:
            abstract_clause = abstraction.abstract_clause(clause)

            # Skip tautologies (e.g., C1 | ~C1)
            if not self._is_tautology(abstract_clause):
                abstract_clauses.append(abstract_clause)

        return CNFExpression(abstract_clauses)

    def _is_tautology(self, clause: Clause) -> bool:
        """
        Check if clause is a tautology (contains both p and ~p).

        Args:
            clause: Clause to check

        Returns:
            True if tautology
        """
        positive_vars = {lit.variable for lit in clause.literals if not lit.negated}
        negative_vars = {lit.variable for lit in clause.literals if lit.negated}
        return bool(positive_vars & negative_vars)

    def refine_assignment(self, level: int, abstract_assignment: Dict[str, bool]) -> Dict[str, bool]:
        """
        Refine abstract assignment to more concrete level.

        Maps cluster assignments to individual variable assignments.

        Args:
            level: Current abstraction level
            abstract_assignment: Assignment at abstract level

        Returns:
            Refined assignment (may be partial)
        """
        if level >= len(self.levels):
            return abstract_assignment

        abstraction = self.levels[level]
        refined: Dict[str, bool] = {}

        # Map cluster assignments to variable assignments
        for cluster in abstraction.clusters:
            if cluster.abstract_var in abstract_assignment:
                value = abstract_assignment[cluster.abstract_var]

                # Assign all variables in cluster to same value
                for var in cluster.variables:
                    refined[var] = value

        return refined

    def __repr__(self):
        return f"AbstractionHierarchy({len(self.levels)} levels, {len(self.cnf.clauses)} clauses)"
