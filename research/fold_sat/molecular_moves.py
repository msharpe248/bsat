"""
Molecular Move Operators for FOLD-SAT

Defines conformational change operators (like protein moves).
Each move proposes a new assignment by modifying the current one.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Set
import random
from abc import ABC, abstractmethod

from bsat.cnf import CNFExpression


class MolecularMove(ABC):
    """Abstract base class for move operators."""

    @abstractmethod
    def propose(self, assignment: Dict[str, bool]) -> Dict[str, bool]:
        """
        Propose a new assignment.

        Args:
            assignment: Current assignment

        Returns:
            New proposed assignment
        """
        pass


class SingleFlipMove(MolecularMove):
    """
    Single variable flip (like sidechain rotation).

    Simplest move: flip one variable from True ↔ False.
    """

    def __init__(self, variables: List[str]):
        """
        Initialize move operator.

        Args:
            variables: List of all variables
        """
        self.variables = variables

    def propose(self, assignment: Dict[str, bool]) -> Dict[str, bool]:
        """Flip one random variable."""
        new_assignment = assignment.copy()
        var = random.choice(self.variables)
        new_assignment[var] = not new_assignment[var]
        return new_assignment


class SwapMove(MolecularMove):
    """
    Swap values of two variables.

    Exchange truth values: (x1=T, x2=F) → (x1=F, x2=T)
    """

    def __init__(self, variables: List[str]):
        self.variables = variables

    def propose(self, assignment: Dict[str, bool]) -> Dict[str, bool]:
        """Swap values of two random variables."""
        new_assignment = assignment.copy()

        if len(self.variables) < 2:
            return new_assignment

        # Pick two different variables
        var1, var2 = random.sample(self.variables, 2)

        # Swap their values
        new_assignment[var1], new_assignment[var2] = \
            new_assignment[var2], new_assignment[var1]

        return new_assignment


class ClusterFlipMove(MolecularMove):
    """
    Flip multiple correlated variables together.

    Like flipping a structural motif in protein.
    Useful when variables are coupled.
    """

    def __init__(self, cnf: CNFExpression, cluster_size: int = 3):
        """
        Initialize cluster flip.

        Args:
            cnf: CNF formula (to find correlated variables)
            cluster_size: Number of variables to flip together
        """
        self.cnf = cnf
        self.cluster_size = cluster_size
        self.variables = list(cnf.get_variables())

        # Pre-compute clusters (variables that co-occur in clauses)
        self.clusters = self._identify_clusters()

    def _identify_clusters(self) -> List[Set[str]]:
        """Identify sets of correlated variables."""
        clusters = []

        # Build co-occurrence graph
        from collections import defaultdict
        co_occurrence = defaultdict(set)

        for clause in self.cnf.clauses:
            variables = [lit.variable for lit in clause.literals]

            # All pairs co-occur
            for var1 in variables:
                for var2 in variables:
                    if var1 != var2:
                        co_occurrence[var1].add(var2)

        # Extract clusters (connected components or high-degree subgraphs)
        for var in self.variables:
            # Get neighbors
            neighbors = co_occurrence[var]

            if len(neighbors) >= self.cluster_size - 1:
                # Form cluster
                cluster = {var}
                cluster.update(random.sample(
                    list(neighbors),
                    min(self.cluster_size - 1, len(neighbors))
                ))
                clusters.append(cluster)

        # Add some random clusters for diversity
        for _ in range(5):
            if len(self.variables) >= self.cluster_size:
                cluster = set(random.sample(self.variables, self.cluster_size))
                clusters.append(cluster)

        return clusters if clusters else [set(self.variables[:self.cluster_size])]

    def propose(self, assignment: Dict[str, bool]) -> Dict[str, bool]:
        """Flip all variables in a random cluster."""
        new_assignment = assignment.copy()

        # Choose cluster
        cluster = random.choice(self.clusters)

        # Flip all variables in cluster
        for var in cluster:
            if var in new_assignment:
                new_assignment[var] = not new_assignment[var]

        return new_assignment


class BiasedFlipMove(MolecularMove):
    """
    Flip variable that appears in unsatisfied clauses.

    Biased toward fixing problems (like guided local search).
    """

    def __init__(self, cnf: CNFExpression):
        self.cnf = cnf
        self.variables = list(cnf.get_variables())

    def propose(self, assignment: Dict[str, bool]) -> Dict[str, bool]:
        """Flip variable from an unsatisfied clause."""
        new_assignment = assignment.copy()

        # Find unsatisfied clauses
        unsatisfied_clauses = []
        for clause in self.cnf.clauses:
            if not self._is_satisfied(clause, assignment):
                unsatisfied_clauses.append(clause)

        if unsatisfied_clauses:
            # Pick variable from random unsatisfied clause
            clause = random.choice(unsatisfied_clauses)
            var = random.choice([lit.variable for lit in clause.literals])
            new_assignment[var] = not new_assignment[var]
        else:
            # All satisfied - random flip
            var = random.choice(self.variables)
            new_assignment[var] = not new_assignment[var]

        return new_assignment

    def _is_satisfied(self, clause, assignment):
        """Check if clause satisfied."""
        for lit in clause.literals:
            if lit.variable in assignment:
                value = assignment[lit.variable]
                if (not lit.negated and value) or (lit.negated and not value):
                    return True
        return False


class RandomMutation(MolecularMove):
    """
    Randomize multiple variables (like thermal noise).

    Helps escape local minima.
    """

    def __init__(self, variables: List[str], mutation_rate: float = 0.1):
        """
        Initialize random mutation.

        Args:
            variables: List of all variables
            mutation_rate: Fraction of variables to randomize
        """
        self.variables = variables
        self.mutation_rate = mutation_rate

    def propose(self, assignment: Dict[str, bool]) -> Dict[str, bool]:
        """Randomize some variables."""
        new_assignment = assignment.copy()

        num_mutations = max(1, int(len(self.variables) * self.mutation_rate))

        for _ in range(num_mutations):
            var = random.choice(self.variables)
            new_assignment[var] = random.choice([True, False])

        return new_assignment


class MoveSelector:
    """
    Selects which move type to use.

    Mixes different move types for better exploration.
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize move selector.

        Args:
            cnf: CNF formula
        """
        self.cnf = cnf
        self.variables = list(cnf.get_variables())

        # Create move operators
        self.moves = {
            'single_flip': SingleFlipMove(self.variables),
            'swap': SwapMove(self.variables),
            'cluster_flip': ClusterFlipMove(cnf, cluster_size=3),
            'biased_flip': BiasedFlipMove(cnf),
            'mutation': RandomMutation(self.variables, mutation_rate=0.1),
        }

        # Move probabilities (can be tuned)
        self.move_probs = {
            'single_flip': 0.5,   # Most common
            'swap': 0.1,
            'cluster_flip': 0.2,
            'biased_flip': 0.15,
            'mutation': 0.05,     # Rare but important
        }

    def select_move(self, temperature: float = 1.0) -> MolecularMove:
        """
        Select move type based on probabilities.

        Args:
            temperature: Current temperature (high T = more exploration)

        Returns:
            Move operator
        """
        # At high temperature, use more exploration (mutation, cluster)
        # At low temperature, use more refinement (single flip, biased)

        if temperature > 10.0:
            # High temperature - exploration
            adjusted_probs = {
                'single_flip': 0.3,
                'swap': 0.1,
                'cluster_flip': 0.3,
                'biased_flip': 0.2,
                'mutation': 0.1,
            }
        elif temperature > 1.0:
            # Medium temperature - balanced
            adjusted_probs = self.move_probs
        else:
            # Low temperature - refinement
            adjusted_probs = {
                'single_flip': 0.5,
                'swap': 0.05,
                'cluster_flip': 0.1,
                'biased_flip': 0.3,
                'mutation': 0.05,
            }

        # Weighted random choice
        move_types = list(adjusted_probs.keys())
        probabilities = list(adjusted_probs.values())

        move_type = random.choices(move_types, weights=probabilities)[0]
        return self.moves[move_type]

    def propose(self, assignment: Dict[str, bool], temperature: float = 1.0) -> Dict[str, bool]:
        """
        Propose move using selected operator.

        Args:
            assignment: Current assignment
            temperature: Current temperature

        Returns:
            New proposed assignment
        """
        move = self.select_move(temperature)
        return move.propose(assignment)
