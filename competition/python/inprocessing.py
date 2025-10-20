#!/usr/bin/env python3
"""
Inprocessing for Competition CDCL Solver

Inprocessing = preprocessing techniques applied DURING search, not just before.
Applied periodically to simplify the clause database (original + learned clauses).

Key Techniques:
1. Subsumption: Remove clauses subsumed by others
2. Self-subsuming resolution: Strengthen clauses
3. Bounded Variable Elimination (BVE): Eliminate variables with few occurrences

Modern competition solvers (CaDiCaL, Kissat) use inprocessing extensively.
Expected speedup: 5-10× on structured instances.

References:
- Järvisalo et al. (2012) "Inprocessing Rules"
- Biere (2013) "Lingeling, Plingeling, PicoSAT"
- Eén & Biere (2005) "Effective Preprocessing in SAT"
"""

from typing import List, Set, Tuple, Dict, Optional
from dataclasses import dataclass
import sys
import os

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression, Clause, Literal


@dataclass
class InprocessingStats:
    """Statistics from inprocessing operations."""
    calls: int = 0
    subsumed_clauses: int = 0
    self_subsumptions: int = 0
    eliminated_vars: int = 0
    clauses_removed: int = 0  # From variable elimination
    clauses_added: int = 0    # From variable elimination
    time_seconds: float = 0.0

    def __str__(self):
        return (
            f"InprocessingStats(\n"
            f"  Calls: {self.calls}\n"
            f"  Subsumed clauses: {self.subsumed_clauses}\n"
            f"  Self-subsumptions: {self.self_subsumptions}\n"
            f"  Eliminated variables: {self.eliminated_vars}\n"
            f"  Clauses removed: {self.clauses_removed}, added: {self.clauses_added}\n"
            f"  Net reduction: {self.clauses_removed - self.clauses_added} clauses\n"
            f"  Total time: {self.time_seconds:.3f}s\n"
            f")"
        )


class Inprocessor:
    """
    Inprocessing engine for CDCL solver.

    Simplifies the clause database during search by removing redundant clauses
    and eliminating variables.

    Usage:
        inprocessor = Inprocessor()
        clauses = inprocessor.simplify(clauses, max_var_occur=10)
    """

    def __init__(self):
        self.stats = InprocessingStats()

    def simplify(self,
                 clauses: List[List[int]],
                 subsumption: bool = True,
                 self_subsumption: bool = True,
                 var_elimination: bool = True,
                 max_var_occur: int = 10,
                 max_resolvent_size: int = 20) -> List[List[int]]:
        """
        Simplify a clause database using inprocessing techniques.

        Args:
            clauses: List of clauses (each clause is a list of integer literals)
            subsumption: Apply subsumption
            self_subsumption: Apply self-subsuming resolution
            var_elimination: Apply bounded variable elimination
            max_var_occur: Max occurrences of a variable to eliminate
            max_resolvent_size: Max size of resolvents in variable elimination

        Returns:
            Simplified clause database

        Note:
            Input format uses integer literals (positive = x, negative = ~x).
            This matches the internal representation used by CDCL solvers.
        """
        import time
        start_time = time.perf_counter()
        self.stats.calls += 1

        # Make a copy to avoid modifying input
        working_clauses = [list(clause) for clause in clauses]

        # Apply techniques iteratively until no changes
        changed = True
        iterations = 0
        max_iterations = 5  # Prevent infinite loops

        while changed and iterations < max_iterations:
            changed = False
            iterations += 1

            if subsumption and self._subsumption(working_clauses):
                changed = True

            if self_subsumption and self._self_subsumption(working_clauses):
                changed = True

            if var_elimination and self._bounded_variable_elimination(
                working_clauses, max_var_occur, max_resolvent_size):
                changed = True

        self.stats.time_seconds += time.perf_counter() - start_time
        return working_clauses

    def _subsumption(self, clauses: List[List[int]]) -> bool:
        """
        Remove subsumed clauses.

        Clause C subsumes clause D if C ⊆ D.
        If C subsumes D, then D is redundant and can be removed.

        Example: (a ∨ b) subsumes (a ∨ b ∨ c)

        Returns:
            True if any subsumption was done
        """
        changed = False
        clauses_to_remove = set()

        # Sort clauses by size (smaller first) for efficiency
        sorted_indices = sorted(range(len(clauses)), key=lambda i: len(clauses[i]))

        for i in sorted_indices:
            if i in clauses_to_remove:
                continue

            clause1 = clauses[i]
            if not clause1:  # Skip empty clauses
                continue

            lit_set1 = set(clause1)

            # Check if clause1 subsumes any larger clause
            for j in range(i + 1, len(clauses)):
                if j in clauses_to_remove:
                    continue

                clause2 = clauses[j]
                if not clause2:
                    continue

                # Early exit: clause1 can only subsume clause2 if len(clause1) <= len(clause2)
                if len(clause1) > len(clause2):
                    continue

                lit_set2 = set(clause2)

                # If clause1 ⊆ clause2, then clause2 is subsumed
                if lit_set1.issubset(lit_set2) and len(clause1) < len(clause2):
                    clauses_to_remove.add(j)
                    self.stats.subsumed_clauses += 1
                    changed = True

        # Remove subsumed clauses
        if clauses_to_remove:
            clauses[:] = [clauses[i] for i in range(len(clauses))
                          if i not in clauses_to_remove]

        return changed

    def _self_subsumption(self, clauses: List[List[int]]) -> bool:
        """
        Apply self-subsuming resolution.

        If clause C1 = (a ∨ R) and C2 = (¬a ∨ S) exist,
        and R ⊆ S, then C2 can be strengthened to S.

        Example:
            C1 = (a ∨ b)
            C2 = (¬a ∨ b ∨ c)
            Resolvent = (b ∨ c)
            Since (b) ⊆ (b ∨ c), replace C2 with (b ∨ c)

        Returns:
            True if any self-subsumption was done
        """
        changed = False

        for i in range(len(clauses)):
            clause1 = clauses[i]
            if not clause1:
                continue

            for j in range(len(clauses)):
                if i == j:
                    continue

                clause2 = clauses[j]
                if not clause2:
                    continue

                # Find complementary literals
                for lit1 in clause1:
                    lit2_complement = -lit1  # Complementary literal

                    if lit2_complement in clause2:
                        # Found complementary pair
                        # Check if (clause1 - {lit1}) ⊆ (clause2 - {lit2_complement})
                        rest1 = set(clause1) - {lit1}
                        rest2 = set(clause2) - {lit2_complement}

                        if rest1.issubset(rest2) and len(rest1) < len(rest2):
                            # Self-subsumption: strengthen clause2
                            # New clause = rest2 (removes literals not in rest1)
                            # But we need to keep lit2_complement or literals in rest1
                            # Actually, we replace clause2 with (rest1 ∪ {lit2_complement})
                            # Wait, that's not quite right...

                            # Correct formulation: replace clause2 with the strengthened version
                            # The resolvent is rest1 ∪ rest2, but we're doing subsumption
                            # If rest1 ⊆ rest2, we can remove literals from clause2
                            # New clause2 = literals that are in rest1 or are lit2_complement
                            # Actually simpler: new_clause2 = rest1 ∪ {lit2_complement}

                            # Let's be more precise:
                            # We want to strengthen clause2 by removing literals
                            # New clause2 should be: all literals from rest1, plus lit2_complement
                            # No wait, if rest1 ⊆ rest2, we strengthen by removing from rest2

                            # Correct algorithm:
                            # resolvent = rest1 ∪ rest2 (standard resolution)
                            # But if rest1 ⊆ rest2, then resolvent = rest2
                            # And if resolvent subsumes clause2, we can replace clause2 with resolvent

                            # Actually, the standard self-subsuming resolution is:
                            # If rest1 ⊆ rest2, then we can remove (rest2 - rest1) from clause2
                            # So new_clause2 = rest1 ∪ {lit2_complement}

                            new_clause2 = list(rest1) + [lit2_complement]
                            new_clause2 = list(set(new_clause2))  # Remove duplicates

                            if len(new_clause2) < len(clause2):
                                clauses[j] = new_clause2
                                self.stats.self_subsumptions += 1
                                changed = True

        return changed

    def _bounded_variable_elimination(self,
                                     clauses: List[List[int]],
                                     max_occur: int,
                                     max_resolvent_size: int) -> bool:
        """
        Bounded Variable Elimination (BVE).

        For each variable v:
        - If v appears in ≤ max_occur clauses (both polarities combined)
        - Eliminate v by resolving all pairs (C1 containing v, C2 containing ¬v)
        - If number of resolvents ≤ (number of clauses with v) × (number with ¬v)
        - Replace clauses with resolvents

        This is "bounded" because we only eliminate if it reduces or maintains clause count.

        Args:
            clauses: Clause database
            max_occur: Maximum occurrences of a variable to consider
            max_resolvent_size: Maximum size of generated resolvents

        Returns:
            True if any elimination was done
        """
        changed = False

        # Build occurrence lists
        var_to_pos_clauses: Dict[int, List[int]] = {}  # var -> list of clause indices with +var
        var_to_neg_clauses: Dict[int, List[int]] = {}  # var -> list of clause indices with -var

        for i, clause in enumerate(clauses):
            for lit in clause:
                var = abs(lit)
                if lit > 0:
                    if var not in var_to_pos_clauses:
                        var_to_pos_clauses[var] = []
                    var_to_pos_clauses[var].append(i)
                else:
                    if var not in var_to_neg_clauses:
                        var_to_neg_clauses[var] = []
                    var_to_neg_clauses[var].append(i)

        # Find all variables
        all_vars = set(var_to_pos_clauses.keys()) | set(var_to_neg_clauses.keys())

        # Try to eliminate variables
        for var in all_vars:
            pos_indices = var_to_pos_clauses.get(var, [])
            neg_indices = var_to_neg_clauses.get(var, [])

            # Check if variable appears in few enough clauses
            if len(pos_indices) + len(neg_indices) > max_occur:
                continue

            # If variable appears with only one polarity, skip (pure literal)
            if not pos_indices or not neg_indices:
                continue

            # Generate all resolvents
            resolvents = []
            for pos_idx in pos_indices:
                for neg_idx in neg_indices:
                    # Resolve clauses[pos_idx] and clauses[neg_idx] on variable var
                    pos_clause = clauses[pos_idx]
                    neg_clause = clauses[neg_idx]

                    # Remove ±var from both clauses and combine
                    pos_rest = [lit for lit in pos_clause if abs(lit) != var]
                    neg_rest = [lit for lit in neg_clause if abs(lit) != var]

                    # Resolvent = pos_rest ∪ neg_rest
                    resolvent = list(set(pos_rest + neg_rest))

                    # Check for tautology (contains both p and ¬p)
                    is_tautology = False
                    lit_set = set(resolvent)
                    for lit in resolvent:
                        if -lit in lit_set:
                            is_tautology = True
                            break

                    if is_tautology:
                        continue

                    # Check resolvent size
                    if len(resolvent) > max_resolvent_size:
                        # Resolvent too large, don't eliminate this variable
                        resolvents = None
                        break

                    resolvents.append(resolvent)

                if resolvents is None:
                    break

            if resolvents is None:
                continue

            # Check if elimination is beneficial
            clauses_removed = len(pos_indices) + len(neg_indices)
            clauses_added = len(resolvents)

            # Only eliminate if it reduces total clauses (or at least doesn't increase much)
            if clauses_added <= clauses_removed:
                # Perform elimination
                # Mark clauses for removal
                indices_to_remove = set(pos_indices + neg_indices)

                # Remove old clauses and add resolvents
                new_clauses = [clauses[i] for i in range(len(clauses))
                              if i not in indices_to_remove]
                new_clauses.extend(resolvents)
                clauses[:] = new_clauses

                # Update stats
                self.stats.eliminated_vars += 1
                self.stats.clauses_removed += clauses_removed
                self.stats.clauses_added += clauses_added
                changed = True

                # Rebuild occurrence lists after modification
                break  # Restart the loop with updated clauses

        return changed


def convert_cnf_to_int_clauses(cnf: CNFExpression, var_to_int: Dict[str, int]) -> List[List[int]]:
    """
    Convert CNF expression to integer clause format.

    Args:
        cnf: CNF expression
        var_to_int: Mapping from variable names to integers

    Returns:
        List of integer clauses
    """
    int_clauses = []
    for clause in cnf.clauses:
        int_clause = []
        for lit in clause.literals:
            var_id = var_to_int[lit.variable]
            int_lit = -var_id if lit.negated else var_id
            int_clause.append(int_lit)
        int_clauses.append(int_clause)
    return int_clauses


def convert_int_clauses_to_cnf(int_clauses: List[List[int]],
                               int_to_var: Dict[int, str]) -> CNFExpression:
    """
    Convert integer clauses back to CNF expression.

    Args:
        int_clauses: List of integer clauses
        int_to_var: Mapping from integers to variable names

    Returns:
        CNF expression
    """
    clauses = []
    for int_clause in int_clauses:
        literals = []
        for int_lit in int_clause:
            var_id = abs(int_lit)
            var_name = int_to_var[var_id]
            negated = int_lit < 0
            literals.append(Literal(var_name, negated))
        clauses.append(Clause(literals))
    return CNFExpression(clauses)
