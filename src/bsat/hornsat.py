"""Horn-SAT solver using unit propagation."""

from typing import Dict, Optional, Set, List
from .cnf import CNFExpression, Clause, Literal


class HornSATSolver:
    """
    Horn-SAT solver using unit propagation.

    A Horn clause has at most one positive literal. Horn-SAT can be solved
    in polynomial time O(n+m) where n is variables and m is clauses.

    The algorithm uses unit propagation exclusively:
    1. Start with all variables False
    2. Find unit clauses (single literal)
    3. If positive literal: set variable True
    4. Repeat until no more unit clauses
    5. Check if all clauses are satisfied

    Time complexity: O(n+m) - linear in formula size
    Space complexity: O(n) - variable assignments
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize the Horn-SAT solver.

        Args:
            cnf: The CNF expression to solve (should be Horn formula)
        """
        self.cnf = cnf
        self.variables = sorted(cnf.get_variables())
        self.num_unit_propagations = 0

        # Verify it's a Horn formula (at most 1 positive literal per clause)
        if not self._is_horn_formula():
            raise ValueError("Formula is not Horn-SAT (contains clause with >1 positive literals)")

    def _is_horn_formula(self) -> bool:
        """
        Check if the formula is a valid Horn formula.

        Returns:
            True if every clause has at most one positive literal
        """
        for clause in self.cnf.clauses:
            positive_count = sum(1 for lit in clause.literals if not lit.negated)
            if positive_count > 1:
                return False
        return True

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve the Horn-SAT problem using unit propagation.

        Returns:
            A satisfying assignment if one exists, None if unsatisfiable
        """
        self.num_unit_propagations = 0

        # Start with all variables set to False
        assignment = {var: False for var in self.variables}

        # Keep propagating unit clauses until fixpoint
        changed = True
        while changed:
            changed = False

            for clause in self.cnf.clauses:
                # Skip if clause is already satisfied
                if clause.evaluate(assignment):
                    continue

                # Find literals that could still make this clause true
                # (i.e., literals that are not currently false)
                candidate_literals = []

                for literal in clause.literals:
                    var_value = assignment[literal.variable]
                    lit_value = (not var_value) if literal.negated else var_value

                    if not lit_value:
                        # This literal is currently false
                        # For Horn-SAT, if it's positive and var is False, it could be made True
                        if not literal.negated and var_value == False:
                            candidate_literals.append(literal)
                        # Negative literals stay false (we only increase variables from False to True)
                    else:
                        # Literal is already true, clause is satisfied
                        candidate_literals = None
                        break

                # If no candidates and clause not satisfied, it's UNSAT
                if candidate_literals is not None and len(candidate_literals) == 0:
                    return None

                # Unit clause - exactly one literal can make it true
                if candidate_literals is not None and len(candidate_literals) == 1:
                    literal = candidate_literals[0]
                    # Must be positive (we only set False→True)
                    if not literal.negated:
                        assignment[literal.variable] = True
                        self.num_unit_propagations += 1
                        changed = True

        # Check if all clauses are satisfied
        if self.cnf.evaluate(assignment):
            return assignment

        # If not satisfied, check each clause to find conflicts
        for clause in self.cnf.clauses:
            if not clause.evaluate(assignment):
                # Found unsatisfied clause
                return None

        return assignment

    def get_statistics(self) -> Dict[str, int]:
        """
        Get solver statistics.

        Returns:
            Dictionary with statistics about the last solve attempt
        """
        return {
            'num_variables': len(self.variables),
            'num_clauses': len(self.cnf.clauses),
            'num_unit_propagations': self.num_unit_propagations
        }


def is_horn_formula(cnf: CNFExpression) -> bool:
    """
    Check if a CNF formula is a Horn formula.

    A Horn formula has at most one positive literal per clause.

    Args:
        cnf: The CNF expression to check

    Returns:
        True if the formula is Horn, False otherwise
    """
    for clause in cnf.clauses:
        positive_count = sum(1 for lit in clause.literals if not lit.negated)
        if positive_count > 1:
            return False
    return True


def solve_horn_sat(cnf: CNFExpression) -> Optional[Dict[str, bool]]:
    """
    Convenience function to solve a Horn-SAT problem.

    Args:
        cnf: The CNF expression to solve (must be Horn formula)

    Returns:
        A satisfying assignment if one exists, None if unsatisfiable

    Raises:
        ValueError: If the formula is not a Horn formula
    """
    solver = HornSATSolver(cnf)
    return solver.solve()
