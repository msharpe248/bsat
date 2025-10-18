"""
Genetic Operators for CEGP-SAT

Implements genetic programming operators for clause evolution:
- Crossover: Combine two clauses
- Mutation: Modify literals in clause
- Selection: Choose clauses based on fitness
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

import random
from typing import List, Set, Tuple
from bsat.cnf import Clause, Literal


class GeneticOperators:
    """
    Genetic operators for evolving learned clauses.

    Applies crossover and mutation to create new clause variants.
    """

    def __init__(self,
                 crossover_rate: float = 0.7,
                 mutation_rate: float = 0.1,
                 max_clause_length: int = 10):
        """
        Initialize genetic operators.

        Args:
            crossover_rate: Probability of crossover (0.0 to 1.0)
            mutation_rate: Probability of mutation per literal (0.0 to 1.0)
            max_clause_length: Maximum clause length
        """
        self.crossover_rate = crossover_rate
        self.mutation_rate = mutation_rate
        self.max_clause_length = max_clause_length

    def crossover(self, parent1: Clause, parent2: Clause) -> Tuple[Clause, Clause]:
        """
        Perform one-point crossover on two clauses.

        Combines literals from both parents to create two offspring.

        Args:
            parent1: First parent clause
            parent2: Second parent clause

        Returns:
            Two offspring clauses
        """
        if random.random() > self.crossover_rate:
            # No crossover - return parents
            return parent1, parent2

        # Get literals from both parents
        lits1 = list(parent1.literals)
        lits2 = list(parent2.literals)

        if len(lits1) == 0 or len(lits2) == 0:
            return parent1, parent2

        # One-point crossover
        point1 = random.randint(0, len(lits1))
        point2 = random.randint(0, len(lits2))

        # Create offspring
        offspring1_lits = lits1[:point1] + lits2[point2:]
        offspring2_lits = lits2[:point2] + lits1[point1:]

        # Remove duplicates and limit length
        offspring1 = self._make_clause(offspring1_lits)
        offspring2 = self._make_clause(offspring2_lits)

        return offspring1, offspring2

    def mutate(self, clause: Clause, available_vars: Set[str]) -> Clause:
        """
        Mutate a clause by modifying literals.

        Operations:
        - Flip polarity (negate literal)
        - Replace variable
        - Add random literal
        - Remove random literal

        Args:
            clause: Clause to mutate
            available_vars: Available variables for replacement

        Returns:
            Mutated clause
        """
        mutated_lits = list(clause.literals)

        # Mutate each literal with probability mutation_rate
        for i, lit in enumerate(mutated_lits):
            if random.random() < self.mutation_rate:
                # Choose mutation type
                mutation_type = random.choice(['flip', 'replace'])

                if mutation_type == 'flip':
                    # Flip polarity
                    mutated_lits[i] = Literal(lit.variable, not lit.negated)

                elif mutation_type == 'replace' and available_vars:
                    # Replace with random variable
                    new_var = random.choice(list(available_vars))
                    mutated_lits[i] = Literal(new_var, random.choice([True, False]))

        # With small probability, add or remove literal
        if random.random() < self.mutation_rate:
            if len(mutated_lits) > 1 and random.random() < 0.5:
                # Remove random literal
                mutated_lits.pop(random.randint(0, len(mutated_lits) - 1))
            elif len(mutated_lits) < self.max_clause_length and available_vars:
                # Add random literal
                new_var = random.choice(list(available_vars))
                mutated_lits.append(Literal(new_var, random.choice([True, False])))

        return self._make_clause(mutated_lits)

    def _make_clause(self, literals: List[Literal]) -> Clause:
        """
        Create clause from literals, removing duplicates and limiting length.

        Args:
            literals: List of literals

        Returns:
            Clause
        """
        # Remove duplicates (keep first occurrence)
        seen = set()
        unique_lits = []
        for lit in literals:
            key = (lit.variable, lit.negated)
            if key not in seen:
                seen.add(key)
                unique_lits.append(lit)

        # Limit length
        if len(unique_lits) > self.max_clause_length:
            unique_lits = unique_lits[:self.max_clause_length]

        # Ensure at least one literal
        if not unique_lits:
            unique_lits = literals[:1] if literals else []

        return Clause(unique_lits)

    def tournament_selection(self,
                            population: List[Tuple[Clause, float]],
                            tournament_size: int = 3) -> Clause:
        """
        Select clause using tournament selection.

        Randomly sample tournament_size clauses and return the best.

        Args:
            population: List of (clause, fitness) tuples
            tournament_size: Number of candidates in tournament

        Returns:
            Selected clause
        """
        if not population:
            raise ValueError("Empty population")

        # Sample random tournament
        tournament = random.sample(population, min(tournament_size, len(population)))

        # Return clause with highest fitness
        best = max(tournament, key=lambda x: x[1])
        return best[0]

    def __repr__(self):
        return (
            f"GeneticOperators("
            f"crossover={self.crossover_rate}, "
            f"mutation={self.mutation_rate})"
        )
