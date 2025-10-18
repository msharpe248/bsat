"""
Fitness Evaluator for CEGP-SAT

Evaluates fitness of evolved clauses based on:
- Propagation effectiveness (how many times clause propagates)
- Conflict participation (how often used in conflict analysis)
- Clause size (shorter clauses preferred)
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict
from bsat.cnf import Clause


class ClauseFitness:
    """Tracks fitness metrics for a clause."""

    def __init__(self, clause: Clause):
        """
        Initialize clause fitness.

        Args:
            clause: The clause being tracked
        """
        self.clause = clause
        self.propagation_count = 0
        self.conflict_participation = 0
        self.age = 0  # Conflicts since creation

    def compute_fitness(self, current_conflict: int) -> float:
        """
        Compute overall fitness score.

        Higher fitness = better clause.

        Args:
            current_conflict: Current conflict count

        Returns:
            Fitness score (0.0 to 1.0+)
        """
        # Age of clause
        self.age = current_conflict

        # Size penalty (shorter clauses preferred)
        size_score = max(0.0, 1.0 - (len(self.clause.literals) / 10.0))

        # Propagation effectiveness (normalized)
        prop_score = min(1.0, self.propagation_count / 10.0)

        # Conflict participation (helpful in learning)
        conflict_score = min(1.0, self.conflict_participation / 5.0)

        # Combined fitness
        fitness = (
            size_score * 0.3 +        # 30%: prefer short clauses
            prop_score * 0.5 +        # 50%: propagation effectiveness
            conflict_score * 0.2      # 20%: conflict participation
        )

        return max(0.0, fitness)

    def __repr__(self):
        return (
            f"ClauseFitness("
            f"props={self.propagation_count}, "
            f"conflicts={self.conflict_participation}, "
            f"age={self.age})"
        )


class FitnessEvaluator:
    """
    Evaluates fitness of clauses in clause population.

    Tracks propagation and conflict statistics to score clauses.
    """

    def __init__(self):
        """Initialize fitness evaluator."""
        self.clause_fitness: Dict[Clause, ClauseFitness] = {}

    def register_clause(self, clause: Clause, current_conflict: int = 0):
        """
        Register a clause for fitness tracking.

        Args:
            clause: Clause to track
            current_conflict: Current conflict count
        """
        if clause not in self.clause_fitness:
            self.clause_fitness[clause] = ClauseFitness(clause)
            self.clause_fitness[clause].age = current_conflict

    def record_propagation(self, clause: Clause):
        """
        Record that clause propagated a unit literal.

        Args:
            clause: Clause that propagated
        """
        if clause in self.clause_fitness:
            self.clause_fitness[clause].propagation_count += 1

    def record_conflict_participation(self, clause: Clause):
        """
        Record that clause participated in conflict analysis.

        Args:
            clause: Clause that participated
        """
        if clause in self.clause_fitness:
            self.clause_fitness[clause].conflict_participation += 1

    def get_fitness(self, clause: Clause, current_conflict: int) -> float:
        """
        Get fitness score for clause.

        Args:
            clause: Clause to evaluate
            current_conflict: Current conflict count

        Returns:
            Fitness score
        """
        if clause not in self.clause_fitness:
            self.register_clause(clause, current_conflict)

        return self.clause_fitness[clause].compute_fitness(current_conflict)

    def get_population_fitness(self, current_conflict: int) -> list:
        """
        Get fitness scores for all tracked clauses.

        Args:
            current_conflict: Current conflict count

        Returns:
            List of (clause, fitness) tuples
        """
        return [
            (clause, fitness.compute_fitness(current_conflict))
            for clause, fitness in self.clause_fitness.items()
        ]

    def get_top_clauses(self, k: int, current_conflict: int) -> list:
        """
        Get top-k clauses by fitness.

        Args:
            k: Number of clauses to return
            current_conflict: Current conflict count

        Returns:
            List of (clause, fitness) tuples
        """
        population = self.get_population_fitness(current_conflict)
        population.sort(key=lambda x: x[1], reverse=True)
        return population[:k]

    def get_statistics(self) -> dict:
        """Get fitness statistics."""
        if not self.clause_fitness:
            return {
                'total_clauses': 0,
                'avg_propagations': 0.0,
                'avg_conflicts': 0.0,
            }

        total = len(self.clause_fitness)
        avg_props = sum(f.propagation_count for f in self.clause_fitness.values()) / total
        avg_conflicts = sum(f.conflict_participation for f in self.clause_fitness.values()) / total

        return {
            'total_clauses': total,
            'avg_propagations': avg_props,
            'avg_conflicts': avg_conflicts,
        }

    def __repr__(self):
        return f"FitnessEvaluator({len(self.clause_fitness)} clauses tracked)"
