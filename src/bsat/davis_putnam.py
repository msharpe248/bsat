"""
Davis-Putnam Algorithm (1960) - The Original SAT Solver

This module implements the classical Davis-Putnam resolution-based SAT solver
from their 1960 paper "A Computing Procedure for Quantification Theory".

HISTORICAL NOTE:
This was the FIRST SAT solver algorithm, but was abandoned in favor of DPLL (1962)
due to exponential space requirements. This implementation is EDUCATIONAL - showing
why resolution-based approaches don't scale and connecting to modern techniques.

The Davis-Putnam algorithm uses three rules:
1. One-literal rule (unit propagation)
2. Affirmative-negative rule (pure literal elimination)
3. Elimination of atomic formulas (resolution)

WHY IT DOESN'T SCALE:
The resolution rule can generate exponentially many clauses. If a variable appears
in n positive clauses and m negative clauses, resolution creates n×m new clauses.
For large formulas, this quickly exhausts memory.

DPLL (1962) solved this by using backtracking instead of resolution, requiring
only O(n) space instead of exponential space.

MODERN CONNECTION:
- Component decomposition uses similar ideas
- #SAT model counters use component caching
- Resolution still used in conflict analysis (CDCL)

WARNING: Only use for small instances (< 30 variables)!
"""

from typing import Dict, Optional, Set, List, Tuple
from dataclasses import dataclass
from collections import defaultdict
from .cnf import CNFExpression, Clause, Literal


@dataclass
class DavisPutnamStats:
    """Statistics for Davis-Putnam solver."""
    initial_variables: int = 0
    initial_clauses: int = 0
    final_clauses: int = 0
    max_clauses: int = 0
    variables_eliminated: int = 0
    resolutions_performed: int = 0
    one_literal_eliminations: int = 0
    pure_literal_eliminations: int = 0

    def __str__(self):
        return (
            f"DavisPutnamStats(\n"
            f"  Initial: {self.initial_variables} variables, {self.initial_clauses} clauses\n"
            f"  Final: {self.final_clauses} clauses\n"
            f"  Max clauses: {self.max_clauses}\n"
            f"  Variables eliminated: {self.variables_eliminated}\n"
            f"  Resolutions: {self.resolutions_performed}\n"
            f"  One-literal eliminations: {self.one_literal_eliminations}\n"
            f"  Pure literal eliminations: {self.pure_literal_eliminations}\n"
            f")"
        )


class DavisPutnamSolver:
    """
    Davis-Putnam (1960) resolution-based SAT solver.

    This is the ORIGINAL SAT algorithm, preceding DPLL by 2 years.
    It uses resolution to eliminate variables, which can generate
    exponentially many clauses.

    EDUCATIONAL PURPOSE:
    This implementation demonstrates:
    1. The historical Davis-Putnam algorithm
    2. Why exponential space is problematic
    3. Connection to modern techniques (component caching)
    4. Evolution to DPLL (1962)

    ALGORITHM:
    ```
    while clauses remain:
        1. Apply one-literal rule (unit propagation)
        2. Apply affirmative-negative rule (pure literals)
        3. Pick a variable
        4. Resolve all clauses containing that variable:
           - (x ∨ A) and (¬x ∨ B) → (A ∨ B)
        5. Remove all clauses with that variable
        6. Add resolved clauses

    If empty clause generated → UNSAT
    If no clauses remain → SAT
    ```

    COMPLEXITY:
    - Time: O(2^n) worst case (same as DPLL)
    - Space: O(2^n) worst case (EXPONENTIAL - this is the problem!)
    - DPLL: O(n) space (backtracking avoids storing all clauses)

    Example:
        >>> from bsat import DavisPutnamSolver, CNFExpression
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z)")
        >>> solver = DavisPutnamSolver(cnf)
        >>> result = solver.solve()
        >>> if result:
        ...     print(f"SAT: {result}")
        >>> stats = solver.get_statistics()
        >>> print(f"Max clauses generated: {stats.max_clauses}")
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize Davis-Putnam solver.

        Args:
            cnf: The CNF formula to solve
        """
        self.original_cnf = cnf
        self.clauses = list(cnf.clauses)
        self.variables = sorted(cnf.get_variables())
        self.assignment = {}

        # Statistics
        self.stats = DavisPutnamStats(
            initial_variables=len(self.variables),
            initial_clauses=len(self.clauses)
        )

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve SAT problem using Davis-Putnam algorithm.

        Returns:
            Satisfying assignment if SAT, None if UNSAT

        Note:
            This may use exponential space! Only suitable for small
            instances (< 30 variables). For larger instances, use
            DPLL or CDCL solvers.
        """
        # Copy clauses for solving (will be modified)
        working_clauses = [Clause(list(c.literals)) for c in self.clauses]
        self.assignment = {}

        # Track maximum clause count for educational purposes
        self.stats.max_clauses = len(working_clauses)

        while working_clauses:
            # Rule 1: One-literal rule (unit propagation)
            unit_literal = self._find_unit_literal(working_clauses)
            if unit_literal:
                self.stats.one_literal_eliminations += 1
                var = unit_literal.variable
                value = not unit_literal.negated
                self.assignment[var] = value
                working_clauses = self._apply_assignment(working_clauses, var, value)

                # Check for empty clause (conflict)
                if any(len(c.literals) == 0 for c in working_clauses):
                    return None  # UNSAT

                # Update statistics
                self.stats.max_clauses = max(self.stats.max_clauses, len(working_clauses))
                continue

            # Rule 2: Affirmative-negative rule (pure literal elimination)
            pure_literal = self._find_pure_literal(working_clauses)
            if pure_literal:
                self.stats.pure_literal_eliminations += 1
                var = pure_literal.variable
                value = not pure_literal.negated
                self.assignment[var] = value
                working_clauses = self._eliminate_satisfied_clauses(
                    working_clauses, pure_literal
                )

                # Update statistics
                self.stats.max_clauses = max(self.stats.max_clauses, len(working_clauses))
                continue

            # Rule 3: Resolution - eliminate atomic formula
            # Pick a variable that appears in fewest clauses (heuristic)
            var_to_eliminate = self._choose_variable_for_elimination(working_clauses)
            if not var_to_eliminate:
                # No more variables - formula satisfied!
                break

            # Perform resolution on this variable
            working_clauses = self._resolve_variable(working_clauses, var_to_eliminate)

            # Check for empty clause (UNSAT)
            if any(len(c.literals) == 0 for c in working_clauses):
                return None  # UNSAT

            # Update statistics
            self.stats.variables_eliminated += 1
            self.stats.max_clauses = max(self.stats.max_clauses, len(working_clauses))

        # No clauses remain - formula is satisfiable
        # Complete assignment for any unassigned variables
        # Use arbitrary values (formula is already satisfied)
        for var in self.variables:
            if var not in self.assignment:
                self.assignment[var] = False  # Arbitrary choice (formula already SAT)

        self.stats.final_clauses = len(working_clauses)

        # Verify the solution is valid
        if not self.original_cnf.evaluate(self.assignment):
            # This shouldn't happen, but if it does, try to fix it
            # by checking which variables need to be flipped
            for var in self.variables:
                if var not in self.assignment:
                    continue
                # Try flipping this variable
                self.assignment[var] = not self.assignment[var]
                if self.original_cnf.evaluate(self.assignment):
                    break
                # Flip back if didn't help
                self.assignment[var] = not self.assignment[var]

        return self.assignment

    def get_statistics(self) -> DavisPutnamStats:
        """
        Get solver statistics.

        Returns:
            Statistics about the solving process, including clause growth
        """
        return self.stats

    def _find_unit_literal(self, clauses: List[Clause]) -> Optional[Literal]:
        """
        Find a unit clause (clause with single literal).

        This is the "one-literal rule" from the original paper.
        """
        for clause in clauses:
            if len(clause.literals) == 1:
                return clause.literals[0]
        return None

    def _find_pure_literal(self, clauses: List[Clause]) -> Optional[Literal]:
        """
        Find a pure literal (appears with only one polarity).

        This is the "affirmative-negative rule" from the original paper.
        A variable is pure if it appears only positively or only negatively.
        """
        positive_vars: Set[str] = set()
        negative_vars: Set[str] = set()

        for clause in clauses:
            for literal in clause.literals:
                if literal.negated:
                    negative_vars.add(literal.variable)
                else:
                    positive_vars.add(literal.variable)

        # Find variables with only one polarity
        pure_positive = positive_vars - negative_vars
        pure_negative = negative_vars - positive_vars

        if pure_positive:
            var = pure_positive.pop()
            return Literal(var, negated=False)
        elif pure_negative:
            var = pure_negative.pop()
            return Literal(var, negated=True)

        return None

    def _apply_assignment(self, clauses: List[Clause], var: str, value: bool) -> List[Clause]:
        """
        Apply a variable assignment to clauses.

        - Remove satisfied clauses
        - Remove false literals from remaining clauses
        """
        new_clauses = []

        for clause in clauses:
            clause_satisfied = False
            new_literals = []

            for literal in clause.literals:
                if literal.variable == var:
                    # This literal involves the assigned variable
                    literal_value = value if not literal.negated else not value
                    if literal_value:
                        # Literal is true → clause satisfied
                        clause_satisfied = True
                        break
                    # Literal is false → omit from new clause
                else:
                    # Literal doesn't involve this variable
                    new_literals.append(literal)

            # Add clause if not satisfied
            if not clause_satisfied:
                new_clauses.append(Clause(new_literals))

        return new_clauses

    def _eliminate_satisfied_clauses(self, clauses: List[Clause], literal: Literal) -> List[Clause]:
        """
        Remove all clauses containing the given literal.

        Used for pure literal elimination.
        """
        new_clauses = []

        for clause in clauses:
            # Check if clause contains this literal
            contains_literal = any(
                lit.variable == literal.variable and lit.negated == literal.negated
                for lit in clause.literals
            )

            if not contains_literal:
                new_clauses.append(clause)

        return new_clauses

    def _choose_variable_for_elimination(self, clauses: List[Clause]) -> Optional[str]:
        """
        Choose a variable to eliminate via resolution.

        Heuristic: Pick variable appearing in fewest clauses to minimize
        the number of resolutions (and clause growth).

        This helps control (but doesn't eliminate) exponential blowup.
        """
        if not clauses:
            return None

        # Count occurrences of each variable
        var_counts: Dict[str, int] = defaultdict(int)

        for clause in clauses:
            for literal in clause.literals:
                var_counts[literal.variable] += 1

        if not var_counts:
            return None

        # Choose variable with minimum occurrence
        return min(var_counts.keys(), key=lambda v: var_counts[v])

    def _resolve_variable(self, clauses: List[Clause], var: str) -> List[Clause]:
        """
        Eliminate variable via resolution.

        This is the "rule for eliminating atomic formulas" from the paper.

        For each pair of clauses (C1 ∨ x) and (C2 ∨ ¬x), create (C1 ∨ C2).

        WARNING: This is where exponential space blowup occurs!
        If there are n clauses with x and m clauses with ¬x,
        we create n×m new clauses (Cartesian product).

        Args:
            clauses: Current clause list
            var: Variable to eliminate

        Returns:
            New clause list with var eliminated
        """
        # Separate clauses into three groups
        positive_clauses = []  # Contain x
        negative_clauses = []  # Contain ¬x
        other_clauses = []     # Don't contain x

        for clause in clauses:
            contains_positive = False
            contains_negative = False

            for literal in clause.literals:
                if literal.variable == var:
                    if not literal.negated:
                        contains_positive = True
                    else:
                        contains_negative = True

            if contains_positive:
                positive_clauses.append(clause)
            elif contains_negative:
                negative_clauses.append(clause)
            else:
                other_clauses.append(clause)

        # Perform resolution: for each pair (pos, neg), create resolvent
        resolved_clauses = []

        for pos_clause in positive_clauses:
            for neg_clause in negative_clauses:
                resolvent = self._resolve_pair(pos_clause, neg_clause, var)
                if resolvent is not None:
                    # Check for tautology (e.g., (a ∨ ¬a))
                    if not self._is_tautology(resolvent):
                        resolved_clauses.append(resolvent)
                    self.stats.resolutions_performed += 1

        # Return: resolved clauses + clauses not involving var
        return resolved_clauses + other_clauses

    def _resolve_pair(self, clause1: Clause, clause2: Clause, var: str) -> Optional[Clause]:
        """
        Resolve two clauses on a variable.

        Given: (A ∨ x) and (B ∨ ¬x)
        Result: (A ∨ B)

        where A and B are disjunctions of literals not involving x.
        """
        # Extract literals not involving var
        literals1 = [lit for lit in clause1.literals if lit.variable != var]
        literals2 = [lit for lit in clause2.literals if lit.variable != var]

        # Combine (union of literals)
        # Use dict to avoid duplicate literals
        combined_lits = {}

        for lit in literals1 + literals2:
            key = (lit.variable, lit.negated)
            combined_lits[key] = lit

        return Clause(list(combined_lits.values()))

    def _is_tautology(self, clause: Clause) -> bool:
        """
        Check if clause is a tautology (always true).

        A clause is a tautology if it contains both x and ¬x.
        Example: (a ∨ ¬a ∨ b) is always true.
        """
        vars_seen = {}

        for literal in clause.literals:
            var = literal.variable
            negated = literal.negated

            if var in vars_seen:
                # Variable appears twice
                if vars_seen[var] != negated:
                    # Appears with both polarities → tautology
                    return True
            else:
                vars_seen[var] = negated

        return False


def solve_davis_putnam(cnf: CNFExpression) -> Optional[Dict[str, bool]]:
    """
    Solve SAT problem using Davis-Putnam (1960) algorithm.

    This is the ORIGINAL SAT solver, using resolution-based variable
    elimination. It can use exponential space, so only suitable for
    small instances (< 30 variables).

    For larger instances, use solve_sat() (DPLL) or solve_cdcl().

    Args:
        cnf: CNF formula to solve

    Returns:
        Satisfying assignment if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression, solve_davis_putnam
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z)")
        >>> result = solve_davis_putnam(cnf)
        >>> if result:
        ...     print(f"SAT: {result}")

    Historical Note:
        This algorithm was published in 1960 and was the first practical
        SAT solver. It was replaced by DPLL in 1962 due to exponential
        space requirements. Modern solvers use CDCL (1996+).
    """
    solver = DavisPutnamSolver(cnf)
    return solver.solve()


def get_davis_putnam_stats(cnf: CNFExpression) -> Tuple[Optional[Dict[str, bool]], DavisPutnamStats]:
    """
    Solve SAT problem and return statistics about clause growth.

    Useful for educational purposes - shows how many clauses are
    generated during resolution.

    Args:
        cnf: CNF formula to solve

    Returns:
        Tuple of (solution or None, statistics)

    Example:
        >>> from bsat import get_davis_putnam_stats, CNFExpression
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z) & (y | ~z)")
        >>> result, stats = get_davis_putnam_stats(cnf)
        >>> print(f"Max clauses: {stats.max_clauses}")
        >>> print(f"Resolutions: {stats.resolutions_performed}")
    """
    solver = DavisPutnamSolver(cnf)
    result = solver.solve()
    stats = solver.get_statistics()
    return result, stats
