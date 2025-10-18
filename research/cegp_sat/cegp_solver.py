"""
CEGP-SAT: Clause Evolution with Genetic Programming

Evolves learned clauses using genetic operators to discover high-quality clauses.

Educational/Experimental approach demonstrating genetic programming for SAT.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional
from bsat.cnf import CNFExpression, Clause
from bsat.cdcl import CDCLSolver, CDCLStats

from .genetic_operators import GeneticOperators
from .fitness_evaluator import FitnessEvaluator


class CEGPStats(CDCLStats):
    """Extended statistics for CEGP-SAT solver."""

    def __init__(self):
        super().__init__()
        self.evolutions = 0
        self.evolved_clauses = 0
        self.crossovers = 0
        self.mutations = 0

    def __str__(self):
        base_stats = super().__str__()
        cegp_stats = (
            f"  Evolutions: {self.evolutions}\n"
            f"  Evolved clauses: {self.evolved_clauses}\n"
            f"  Crossovers: {self.crossovers}\n"
            f"  Mutations: {self.mutations}\n"
        )
        return base_stats.rstrip(')') + '\n' + cegp_stats + ')'


class CEGPSATSolver(CDCLSolver):
    """
    CDCL solver with genetic programming for clause evolution.

    Evolves learned clauses using crossover and mutation to discover
    high-quality clauses for propagation.

    Educational/experimental approach.
    """

    def __init__(self,
                 cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000,
                 # CEGP-specific parameters
                 use_evolution: bool = True,
                 evolution_frequency: int = 500,  # Evolve every N conflicts
                 population_size: int = 20,  # Max evolved clauses to keep
                 crossover_rate: float = 0.7,
                 mutation_rate: float = 0.1):
        """
        Initialize CEGP-SAT solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: VSIDS decay factor
            restart_base: Base interval for restarts
            learned_clause_limit: Max learned clauses
            use_evolution: Enable clause evolution
            evolution_frequency: Conflicts between evolution rounds
            population_size: Max evolved clauses in population
            crossover_rate: Probability of crossover
            mutation_rate: Probability of mutation
        """
        # Initialize base CDCL solver
        super().__init__(cnf, vsids_decay, restart_base, learned_clause_limit)

        # CEGP parameters
        self.use_evolution = use_evolution
        self.evolution_frequency = evolution_frequency
        self.population_size = population_size

        # Genetic components
        if self.use_evolution:
            self.genetic_ops = GeneticOperators(
                crossover_rate=crossover_rate,
                mutation_rate=mutation_rate,
                max_clause_length=10
            )
            self.fitness_eval = FitnessEvaluator()

        # Extended statistics
        self.stats = CEGPStats()

    def _add_learned_clause(self, clause: Clause):
        """
        Override to track clause fitness.

        Args:
            clause: Learned clause to add
        """
        # Call parent
        super()._add_learned_clause(clause)

        # Register for fitness tracking
        if self.use_evolution:
            self.fitness_eval.register_clause(clause, self.stats.conflicts)

    def _evolve_clauses(self):
        """
        Evolve learned clauses using genetic programming.

        Creates new clause variants through crossover and mutation.
        """
        if not self.use_evolution:
            return

        # Get current population fitness
        population = self.fitness_eval.get_population_fitness(self.stats.conflicts)

        if len(population) < 2:
            return  # Need at least 2 clauses to evolve

        # Limit population to top performers
        population.sort(key=lambda x: x[1], reverse=True)
        population = population[:self.population_size]

        # Get available variables for mutation
        available_vars = set(self.variables)

        # Generate new clauses
        new_clauses = []

        # Crossover: Create offspring from top parents
        for i in range(min(5, len(population) // 2)):
            parent1 = self.genetic_ops.tournament_selection(population)
            parent2 = self.genetic_ops.tournament_selection(population)

            offspring1, offspring2 = self.genetic_ops.crossover(parent1, parent2)
            new_clauses.append(offspring1)
            new_clauses.append(offspring2)
            self.stats.crossovers += 1

        # Mutation: Mutate top clauses
        for clause, _ in population[:min(5, len(population))]:
            mutated = self.genetic_ops.mutate(clause, available_vars)
            new_clauses.append(mutated)
            self.stats.mutations += 1

        # Add evolved clauses to learned clause database
        for clause in new_clauses:
            if len(clause.literals) > 0 and not self._is_tautology(clause):
                # Add to clause database
                if len(self.clauses) < self.learned_clause_limit:
                    self.clauses.append(clause)
                    self.stats.evolved_clauses += 1

                    # Register for fitness
                    self.fitness_eval.register_clause(clause, self.stats.conflicts)

                    # Note: Evolved clauses will be automatically handled by
                    # CDCL's propagation mechanism (no need to manually update watch lists)

        self.stats.evolutions += 1

    def _is_tautology(self, clause: Clause) -> bool:
        """Check if clause is a tautology."""
        positive_vars = {lit.variable for lit in clause.literals if not lit.negated}
        negative_vars = {lit.variable for lit in clause.literals if lit.negated}
        return bool(positive_vars & negative_vars)

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using clause evolution.

        Args:
            max_conflicts: Maximum conflicts before giving up

        Returns:
            Solution if SAT, None if UNSAT
        """
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

            # Evolve clauses periodically
            if (self.use_evolution and
                self.stats.conflicts > 0 and
                self.stats.conflicts % self.evolution_frequency == 0):
                self._evolve_clauses()

            # Pick branching variable (VSIDS)
            var = self._pick_branching_variable()

            if var is None:
                # All variables assigned - SAT!
                return dict(self.assignment)

            # Make decision (phase: True by default)
            phase = True
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

                # Analyze conflict
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

    def get_evolution_statistics(self) -> dict:
        """Get detailed evolution statistics."""
        stats = {
            'enabled': self.use_evolution,
            'evolutions': self.stats.evolutions,
            'evolved_clauses': self.stats.evolved_clauses,
            'crossovers': self.stats.crossovers,
            'mutations': self.stats.mutations,
        }

        if self.use_evolution:
            stats['fitness'] = self.fitness_eval.get_statistics()

        return stats


def solve_cegp_sat(cnf: CNFExpression,
                   use_evolution: bool = True,
                   evolution_frequency: int = 500,
                   max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using CEGP-SAT (Clause Evolution with Genetic Programming).

    Educational/experimental approach using genetic operators.

    Args:
        cnf: CNF formula to solve
        use_evolution: Enable clause evolution
        evolution_frequency: Conflicts between evolution rounds
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Solution if SAT, None if UNSAT
    """
    solver = CEGPSATSolver(
        cnf,
        use_evolution=use_evolution,
        evolution_frequency=evolution_frequency
    )
    return solver.solve(max_conflicts=max_conflicts)
