"""
Energy Landscape for FOLD-SAT

Maps SAT problem to protein folding energy function.
Defines Hamiltonian and energy computation.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Tuple, Set
from collections import defaultdict
import math

from bsat.cnf import CNFExpression, Clause, Literal


class EnergyLandscape:
    """
    Energy landscape for SAT as protein folding.

    Maps SAT assignment to protein conformation energy:
    - Satisfied clause → favorable contact (negative energy)
    - Unsatisfied clause → unfavorable clash (positive energy)
    - Correlated variables → pairwise interaction
    """

    def __init__(self,
                 cnf: CNFExpression,
                 clause_penalty: float = 10.0,
                 clause_reward: float = -1.0,
                 pair_strength: float = -0.1):
        """
        Initialize energy landscape.

        Args:
            cnf: CNF formula
            clause_penalty: Energy cost for unsatisfied clause (positive)
            clause_reward: Energy reward for satisfied clause (negative)
            pair_strength: Pairwise interaction strength
        """
        self.cnf = cnf
        self.clause_penalty = clause_penalty
        self.clause_reward = clause_reward
        self.pair_strength = pair_strength

        # Compute variable correlations
        self.pair_interactions = self._compute_pair_interactions()

        # Compute clause weights (unit clauses more important)
        self.clause_weights = self._compute_clause_weights()

    def _compute_pair_interactions(self) -> Dict[Tuple[str, str], float]:
        """
        Compute pairwise variable interactions.

        Variables that co-occur in many clauses have stronger interaction.
        Like residues that are close in protein structure.
        """
        interactions = defaultdict(float)

        for clause in self.cnf.clauses:
            variables = [lit.variable for lit in clause.literals]

            # All pairs in this clause interact
            for i, var1 in enumerate(variables):
                for var2 in variables[i+1:]:
                    # Canonical order
                    pair = tuple(sorted([var1, var2]))
                    interactions[pair] += self.pair_strength

        return dict(interactions)

    def _compute_clause_weights(self) -> Dict[Clause, float]:
        """
        Compute importance weight for each clause.

        Unit clauses get higher weight (more important).
        """
        weights = {}

        for clause in self.cnf.clauses:
            clause_size = len(clause.literals)

            if clause_size == 1:
                # Unit clause - very important
                weights[clause] = self.clause_penalty * 10.0
            elif clause_size == 2:
                # Binary clause - important
                weights[clause] = self.clause_penalty * 2.0
            else:
                # Larger clause - standard weight
                weights[clause] = self.clause_penalty

        return weights

    def compute_energy(self, assignment: Dict[str, bool]) -> float:
        """
        Compute total energy of assignment.

        Energy = Σ E_clause + Σ E_pair

        Low energy = good (many satisfied clauses)
        High energy = bad (many unsatisfied clauses)

        Args:
            assignment: Variable assignment

        Returns:
            Total energy (minimize this!)
        """
        total_energy = 0.0

        # Clause energies (main contribution)
        for clause in self.cnf.clauses:
            if self._is_clause_satisfied(clause, assignment):
                # Satisfied = favorable (negative energy)
                total_energy += self.clause_reward
            else:
                # Unsatisfied = unfavorable (positive energy)
                total_energy += self.clause_weights[clause]

        # Pairwise interactions (coupling)
        for (var1, var2), strength in self.pair_interactions.items():
            if var1 in assignment and var2 in assignment:
                # Same value = favorable (if strength negative)
                # Different value = unfavorable
                if assignment[var1] == assignment[var2]:
                    total_energy += strength  # Negative = stabilizing
                else:
                    total_energy -= strength  # Positive = destabilizing

        return total_energy

    def _is_clause_satisfied(self, clause: Clause, assignment: Dict[str, bool]) -> bool:
        """Check if clause is satisfied by assignment."""
        for literal in clause.literals:
            if literal.variable not in assignment:
                continue

            value = assignment[literal.variable]

            # Literal satisfied if:
            # - Positive literal and value is True
            # - Negative literal and value is False
            if (not literal.negated and value) or (literal.negated and not value):
                return True

        return False

    def compute_delta_energy(self,
                            assignment: Dict[str, bool],
                            var: str,
                            new_value: bool) -> float:
        """
        Efficiently compute energy change from flipping one variable.

        ΔE = E_new - E_old

        Only need to check clauses containing this variable!
        Much faster than recomputing full energy.

        Args:
            assignment: Current assignment
            var: Variable to flip
            new_value: New value for variable

        Returns:
            Energy change (negative = favorable)
        """
        delta = 0.0
        old_value = assignment[var]

        # Find clauses containing this variable
        affected_clauses = self._get_clauses_containing(var)

        for clause in affected_clauses:
            # Energy before flip
            satisfied_before = self._is_clause_satisfied(clause, assignment)

            # Simulate flip
            assignment[var] = new_value
            satisfied_after = self._is_clause_satisfied(clause, assignment)
            assignment[var] = old_value  # Restore

            # Energy change from this clause
            if satisfied_before and not satisfied_after:
                # Became unsatisfied (bad!)
                delta += self.clause_weights[clause] - self.clause_reward
            elif not satisfied_before and satisfied_after:
                # Became satisfied (good!)
                delta += self.clause_reward - self.clause_weights[clause]

        # Pairwise interaction changes
        for (var1, var2), strength in self.pair_interactions.items():
            if var == var1 or var == var2:
                other_var = var2 if var == var1 else var1
                if other_var not in assignment:
                    continue

                # Check if relationship changes
                same_before = (assignment[var] == assignment[other_var])
                same_after = (new_value == assignment[other_var])

                if same_before != same_after:
                    if same_after:
                        delta += 2 * strength  # Becoming aligned
                    else:
                        delta -= 2 * strength  # Becoming different

        return delta

    def _get_clauses_containing(self, var: str) -> Set[Clause]:
        """Get all clauses containing variable."""
        clauses = set()

        for clause in self.cnf.clauses:
            for literal in clause.literals:
                if literal.variable == var:
                    clauses.add(clause)
                    break

        return clauses

    def get_ground_state_energy(self) -> float:
        """
        Get theoretical ground state energy (all clauses satisfied).

        This is the target energy for SAT solution.
        """
        return self.clause_reward * len(self.cnf.clauses)

    def is_ground_state(self, assignment: Dict[str, bool]) -> bool:
        """Check if assignment is at ground state (all satisfied)."""
        energy = self.compute_energy(assignment)
        return abs(energy - self.get_ground_state_energy()) < 1e-6

    def get_num_unsatisfied(self, assignment: Dict[str, bool]) -> int:
        """Get number of unsatisfied clauses."""
        count = 0
        for clause in self.cnf.clauses:
            if not self._is_clause_satisfied(clause, assignment):
                count += 1
        return count

    def get_energy_distribution(self, num_samples: int = 1000) -> Dict[float, int]:
        """
        Sample random assignments to understand energy landscape.

        Returns distribution of energies (landscape topology).
        """
        import random

        energy_counts = defaultdict(int)
        variables = list(self.cnf.get_variables())

        for _ in range(num_samples):
            # Random assignment
            assignment = {var: random.choice([True, False]) for var in variables}
            energy = self.compute_energy(assignment)

            # Bin energy (round to nearest integer)
            energy_bin = round(energy)
            energy_counts[energy_bin] += 1

        return dict(sorted(energy_counts.items()))

    def __repr__(self):
        return (f"EnergyLandscape(variables={len(self.cnf.get_variables())}, "
                f"clauses={len(self.cnf.clauses)}, "
                f"ground_state={self.get_ground_state_energy():.1f})")
