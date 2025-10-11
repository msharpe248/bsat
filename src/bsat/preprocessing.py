"""
SAT Preprocessing and Simplification Techniques

This module provides various preprocessing techniques that can dramatically
simplify SAT instances before solving:

1. Connected Component Decomposition - Split independent subproblems
2. Unit Propagation - Propagate forced assignments
3. Pure Literal Elimination - Remove variables appearing with single polarity
4. Subsumption - Remove redundant clauses
5. Self-Subsumption - Simplify clauses
6. Backbone Detection - Find forced variables

These techniques can:
- Reduce problem size significantly
- Find trivial solutions/conflicts early
- Decompose into independent subproblems
- Speed up subsequent solving

Reference:
"Effective Preprocessing in SAT Through Variable and Clause Elimination"
Eén & Biere (2005)
"""

from typing import Dict, List, Set, Tuple, Optional
from dataclasses import dataclass
from collections import defaultdict, deque
from .cnf import CNFExpression, Clause, Literal


@dataclass
class PreprocessingStats:
    """Statistics from preprocessing operations."""
    original_vars: int = 0
    original_clauses: int = 0
    final_vars: int = 0
    final_clauses: int = 0
    unit_propagations: int = 0
    pure_literals: int = 0
    subsumed_clauses: int = 0
    self_subsumptions: int = 0
    components: int = 1

    def __str__(self):
        return (
            f"PreprocessingStats(\n"
            f"  Variables: {self.original_vars} → {self.final_vars} "
            f"({self.original_vars - self.final_vars} removed)\n"
            f"  Clauses: {self.original_clauses} → {self.final_clauses} "
            f"({self.original_clauses - self.final_clauses} removed)\n"
            f"  Unit propagations: {self.unit_propagations}\n"
            f"  Pure literals: {self.pure_literals}\n"
            f"  Subsumed clauses: {self.subsumed_clauses}\n"
            f"  Self-subsumptions: {self.self_subsumptions}\n"
            f"  Connected components: {self.components}\n"
            f")"
        )


@dataclass
class PreprocessingResult:
    """Result of preprocessing operation."""
    simplified: CNFExpression
    assignments: Dict[str, bool]  # Forced variable assignments
    stats: PreprocessingStats
    is_sat: Optional[bool] = None  # True if trivially SAT, False if UNSAT, None if unknown
    components: Optional[List[CNFExpression]] = None  # Independent subproblems


class SATPreprocessor:
    """
    SAT preprocessing engine with various simplification techniques.

    Example:
        >>> from bsat import CNFExpression, SATPreprocessor
        >>> cnf = CNFExpression.parse("(a | b) & (c | d) & a")
        >>> preprocessor = SATPreprocessor(cnf)
        >>> result = preprocessor.preprocess()
        >>> print(f"Simplified: {result.simplified}")
        >>> print(f"Forced: {result.assignments}")
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize preprocessor.

        Args:
            cnf: CNF formula to preprocess
        """
        self.original_cnf = cnf
        self.cnf = CNFExpression([Clause(list(clause.literals)) for clause in cnf.clauses])
        self.assignments: Dict[str, bool] = {}
        self.stats = PreprocessingStats(
            original_vars=len(cnf.get_variables()),
            original_clauses=len(cnf.clauses)
        )

    def preprocess(self,
                   unit_propagation: bool = True,
                   pure_literal: bool = True,
                   subsumption: bool = True,
                   self_subsumption: bool = True) -> PreprocessingResult:
        """
        Apply all enabled preprocessing techniques.

        Args:
            unit_propagation: Apply unit propagation
            pure_literal: Apply pure literal elimination
            subsumption: Apply clause subsumption
            self_subsumption: Apply self-subsumption resolution

        Returns:
            PreprocessingResult with simplified formula and statistics
        """
        # Keep applying techniques until no more changes
        changed = True
        while changed:
            changed = False

            if unit_propagation and self._unit_propagation():
                changed = True

            if pure_literal and self._pure_literal_elimination():
                changed = True

            if subsumption and self._subsumption():
                changed = True

            if self_subsumption and self._self_subsumption():
                changed = True

        # Check for trivial SAT/UNSAT
        is_sat = None
        if len(self.cnf.clauses) == 0:
            is_sat = True  # All clauses satisfied
        elif any(len(clause.literals) == 0 for clause in self.cnf.clauses):
            is_sat = False  # Empty clause - UNSAT

        # Update final stats
        self.stats.final_vars = len(self.cnf.get_variables())
        self.stats.final_clauses = len(self.cnf.clauses)

        return PreprocessingResult(
            simplified=self.cnf,
            assignments=self.assignments,
            stats=self.stats,
            is_sat=is_sat
        )

    def _unit_propagation(self) -> bool:
        """
        Propagate unit clauses (clauses with single literal).

        Returns:
            True if any propagation was done
        """
        changed = False

        while True:
            # Find unit clauses
            unit_clauses = [c for c in self.cnf.clauses if len(c.literals) == 1]
            if not unit_clauses:
                break

            for unit_clause in unit_clauses:
                lit = unit_clause.literals[0]

                # Record assignment
                self.assignments[lit.variable] = not lit.negated
                self.stats.unit_propagations += 1
                changed = True

                # Remove satisfied clauses and simplify others
                new_clauses = []
                for clause in self.cnf.clauses:
                    if any(l.variable == lit.variable and l.negated == lit.negated
                           for l in clause.literals):
                        # Clause is satisfied, remove it
                        continue
                    else:
                        # Remove opposite literal if present
                        new_lits = [l for l in clause.literals
                                   if not (l.variable == lit.variable and l.negated != lit.negated)]
                        if len(new_lits) > 0:
                            new_clauses.append(Clause(new_lits))
                        elif len(new_lits) == 0:
                            # Empty clause - UNSAT!
                            new_clauses.append(Clause([]))

                self.cnf = CNFExpression(new_clauses)
                break  # Restart search for unit clauses

        return changed

    def _pure_literal_elimination(self) -> bool:
        """
        Eliminate pure literals (variables appearing with only one polarity).

        Returns:
            True if any elimination was done
        """
        # Find polarity of each variable
        positive = set()
        negative = set()

        for clause in self.cnf.clauses:
            for lit in clause.literals:
                if lit.negated:
                    negative.add(lit.variable)
                else:
                    positive.add(lit.variable)

        # Find pure literals
        pure_vars = (positive - negative) | (negative - positive)

        if not pure_vars:
            return False

        # Assign pure literals
        for var in pure_vars:
            # Assign to satisfy all occurrences
            self.assignments[var] = var in positive
            self.stats.pure_literals += 1

        # Remove all clauses containing pure literals
        new_clauses = []
        for clause in self.cnf.clauses:
            clause_vars = {lit.variable for lit in clause.literals}
            if not clause_vars & pure_vars:  # No pure literals in this clause
                new_clauses.append(clause)

        self.cnf = CNFExpression(new_clauses)
        return True

    def _subsumption(self) -> bool:
        """
        Remove subsumed clauses.
        Clause C subsumes D if C ⊆ D (every literal in C is in D).

        Returns:
            True if any subsumption was done
        """
        changed = False
        clauses_to_keep = []

        for i, clause1 in enumerate(self.cnf.clauses):
            lit_set1 = set((l.variable, l.negated) for l in clause1.literals)
            subsumed = False

            for j, clause2 in enumerate(self.cnf.clauses):
                if i == j:
                    continue

                lit_set2 = set((l.variable, l.negated) for l in clause2.literals)

                # If clause2 ⊆ clause1, then clause1 is subsumed
                if lit_set2.issubset(lit_set1) and lit_set2 != lit_set1:
                    subsumed = True
                    self.stats.subsumed_clauses += 1
                    changed = True
                    break

            if not subsumed:
                clauses_to_keep.append(clause1)

        self.cnf = CNFExpression(clauses_to_keep)
        return changed

    def _self_subsumption(self) -> bool:
        """
        Apply self-subsumption resolution.
        If (a ∨ C) and (¬a ∨ D) exist, and C ⊆ D, replace (¬a ∨ D) with D.

        Returns:
            True if any self-subsumption was done
        """
        changed = False
        new_clauses = list(self.cnf.clauses)

        for i in range(len(new_clauses)):
            for j in range(len(new_clauses)):
                if i == j:
                    continue

                clause1 = new_clauses[i]
                clause2 = new_clauses[j]

                # Find complementary literals
                for lit1 in clause1.literals:
                    for lit2 in clause2.literals:
                        if lit1.variable == lit2.variable and lit1.negated != lit2.negated:
                            # Found complementary pair
                            # Check if clause1 - {lit1} ⊆ clause2 - {lit2}
                            rest1 = {(l.variable, l.negated) for l in clause1.literals if l != lit1}
                            rest2 = {(l.variable, l.negated) for l in clause2.literals if l != lit2}

                            if rest1.issubset(rest2) and rest1 != rest2:
                                # Self-subsumption: clause2 can be simplified
                                new_lits = [l for l in clause2.literals
                                           if (l.variable, l.negated) in rest1 or l == lit2]
                                new_lits = [l for l in new_lits if l != lit2]

                                if new_lits and new_lits != clause2.literals:
                                    new_clauses[j] = Clause(new_lits)
                                    self.stats.self_subsumptions += 1
                                    changed = True

        if changed:
            self.cnf = CNFExpression(new_clauses)

        return changed


def decompose_into_components(cnf: CNFExpression) -> List[CNFExpression]:
    """
    Decompose CNF into independent connected components.

    Two clauses are connected if they share a variable. This creates
    independent subproblems that can be solved separately.

    Args:
        cnf: CNF formula to decompose

    Returns:
        List of independent CNF subproblems

    Example:
        >>> cnf = CNFExpression.parse("(a | b) & (c | d)")
        >>> components = decompose_into_components(cnf)
        >>> len(components)
        2
    """
    if not cnf.clauses:
        return [cnf]

    # Build variable-to-clause mapping
    var_to_clauses: Dict[str, Set[int]] = defaultdict(set)
    for i, clause in enumerate(cnf.clauses):
        for lit in clause.literals:
            var_to_clauses[lit.variable].add(i)

    # Find connected components using BFS
    visited = set()
    components = []

    for start_idx in range(len(cnf.clauses)):
        if start_idx in visited:
            continue

        # BFS to find connected component
        component_clauses = []
        queue = deque([start_idx])
        visited.add(start_idx)

        while queue:
            clause_idx = queue.popleft()
            component_clauses.append(cnf.clauses[clause_idx])

            # Find all clauses connected through shared variables
            for lit in cnf.clauses[clause_idx].literals:
                for connected_idx in var_to_clauses[lit.variable]:
                    if connected_idx not in visited:
                        visited.add(connected_idx)
                        queue.append(connected_idx)

        components.append(CNFExpression(component_clauses))

    return components


def preprocess_cnf(cnf: CNFExpression,
                   unit_propagation: bool = True,
                   pure_literal: bool = True,
                   subsumption: bool = True,
                   self_subsumption: bool = True) -> PreprocessingResult:
    """
    Preprocess CNF formula with simplification techniques.

    Args:
        cnf: CNF formula to preprocess
        unit_propagation: Apply unit propagation
        pure_literal: Apply pure literal elimination
        subsumption: Apply clause subsumption
        self_subsumption: Apply self-subsumption

    Returns:
        PreprocessingResult with simplified formula

    Example:
        >>> cnf = CNFExpression.parse("(a | b) & a & (b | c)")
        >>> result = preprocess_cnf(cnf)
        >>> print(result.assignments)  # {'a': True}
        >>> print(result.simplified)   # (b | c)
    """
    preprocessor = SATPreprocessor(cnf)
    return preprocessor.preprocess(
        unit_propagation=unit_propagation,
        pure_literal=pure_literal,
        subsumption=subsumption,
        self_subsumption=self_subsumption
    )


def decompose_and_preprocess(cnf: CNFExpression,
                             **preprocess_args) -> Tuple[List[CNFExpression],
                                                          Dict[str, bool],
                                                          PreprocessingStats]:
    """
    Decompose CNF into components and preprocess each one.

    This is the recommended workflow: first decompose into independent
    subproblems, then preprocess each one separately.

    Args:
        cnf: CNF formula to process
        **preprocess_args: Arguments to pass to preprocessing

    Returns:
        Tuple of (component_list, forced_assignments, combined_stats)

    Example:
        >>> cnf = CNFExpression.parse("(a | b) & a & (c | d) & c")
        >>> components, assignments, stats = decompose_and_preprocess(cnf)
        >>> len(components)  # 2 independent components
        2
        >>> assignments  # {'a': True, 'c': True}
        {'a': True, 'c': True}
    """
    # First decompose
    components = decompose_into_components(cnf)

    # Preprocess each component
    simplified_components = []
    all_assignments = {}
    combined_stats = PreprocessingStats(
        original_vars=len(cnf.get_variables()),
        original_clauses=len(cnf.clauses),
        components=len(components)
    )

    for component in components:
        result = preprocess_cnf(component, **preprocess_args)

        # Only add non-trivial components
        if result.is_sat is None and len(result.simplified.clauses) > 0:
            simplified_components.append(result.simplified)

        # Collect assignments
        all_assignments.update(result.assignments)

        # Aggregate stats
        combined_stats.unit_propagations += result.stats.unit_propagations
        combined_stats.pure_literals += result.stats.pure_literals
        combined_stats.subsumed_clauses += result.stats.subsumed_clauses
        combined_stats.self_subsumptions += result.stats.self_subsumptions

    # Update final counts
    total_vars = set()
    total_clauses = 0
    for comp in simplified_components:
        total_vars.update(comp.get_variables())
        total_clauses += len(comp.clauses)

    combined_stats.final_vars = len(total_vars)
    combined_stats.final_clauses = total_clauses

    return simplified_components, all_assignments, combined_stats
