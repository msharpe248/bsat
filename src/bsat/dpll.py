"""DPLL (Davis-Putnam-Logemann-Loveland) algorithm for SAT solving."""

from typing import Dict, Optional, Set, List
from .cnf import CNFExpression, Clause, Literal


class DPLLSolver:
    """
    Basic DPLL SAT solver using backtracking search.

    The DPLL algorithm is a complete, sound, and terminating algorithm for determining
    the satisfiability of propositional logic formulas in CNF. It uses:
    - Backtracking search through the variable assignment space
    - Early termination when conflicts are detected

    Time complexity: O(2^n) in worst case, where n is the number of variables
    Space complexity: O(n) for the recursion stack
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize the DPLL solver with a CNF expression.

        Args:
            cnf: The CNF expression to solve
        """
        self.cnf = cnf
        self.variables = sorted(cnf.get_variables())
        self.num_decisions = 0  # Track number of decision points (for statistics)

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve the SAT problem using DPLL algorithm.

        Returns:
            A satisfying assignment if one exists, None if unsatisfiable
        """
        self.num_decisions = 0
        assignment = {}
        return self._dpll(assignment, list(self.cnf.clauses))

    def _dpll(self, assignment: Dict[str, bool], clauses: List[Clause]) -> Optional[Dict[str, bool]]:
        """
        Recursive DPLL algorithm.

        Args:
            assignment: Current partial variable assignment
            clauses: Current set of clauses (may be simplified)

        Returns:
            A satisfying assignment if one exists, None otherwise
        """
        # Simplify clauses based on current assignment
        simplified_clauses = self._simplify_clauses(clauses, assignment)

        # Base case 1: Empty clause found (conflict)
        if any(not clause.literals for clause in simplified_clauses):
            return None

        # Base case 2: All clauses satisfied
        if not simplified_clauses:
            # Complete the assignment for any unassigned variables
            for var in self.variables:
                if var not in assignment:
                    assignment[var] = True  # Arbitrary choice since all clauses are satisfied
            return assignment

        # Choose next unassigned variable (simple ordering for basic DPLL)
        unassigned = self._get_unassigned_variable(assignment)
        if unassigned is None:
            # All variables assigned but we haven't returned yet - shouldn't happen
            # but let's verify the assignment
            if self.cnf.evaluate(assignment):
                return assignment
            return None

        self.num_decisions += 1

        # Try assigning True
        assignment[unassigned] = True
        result = self._dpll(assignment, simplified_clauses)
        if result is not None:
            return result

        # Backtrack and try False
        assignment[unassigned] = False
        result = self._dpll(assignment, simplified_clauses)
        if result is not None:
            return result

        # Both assignments failed, backtrack
        del assignment[unassigned]
        return None

    def _simplify_clauses(self, clauses: List[Clause], assignment: Dict[str, bool]) -> List[Clause]:
        """
        Simplify clauses based on current assignment.

        - Remove clauses that are satisfied by the assignment
        - Remove literals that are falsified by the assignment

        Args:
            clauses: List of clauses to simplify
            assignment: Current variable assignment

        Returns:
            Simplified list of clauses
        """
        simplified = []

        for clause in clauses:
            # Check if clause is already satisfied by checking each literal
            clause_satisfied = False
            new_literals = []

            for literal in clause.literals:
                if literal.variable not in assignment:
                    # Unassigned variable, keep the literal
                    new_literals.append(literal)
                else:
                    # Variable is assigned, check if literal is satisfied
                    value = assignment[literal.variable]
                    literal_value = (not value) if literal.negated else value
                    if literal_value:
                        # This literal is true, so the whole clause is satisfied
                        clause_satisfied = True
                        break
                    # else: literal is false, don't include it in new_literals

            # Add simplified clause only if not satisfied
            if not clause_satisfied:
                simplified.append(Clause(new_literals))

        return simplified

    def _get_unassigned_variable(self, assignment: Dict[str, bool]) -> Optional[str]:
        """
        Get the next unassigned variable.

        Uses simple ordering for basic DPLL. More sophisticated heuristics
        will be added in later versions (VSIDS, etc.).

        Args:
            assignment: Current variable assignment

        Returns:
            Name of an unassigned variable, or None if all are assigned
        """
        for var in self.variables:
            if var not in assignment:
                return var
        return None

    def get_statistics(self) -> Dict[str, int]:
        """
        Get solver statistics.

        Returns:
            Dictionary with statistics about the last solve attempt
        """
        return {
            'num_variables': len(self.variables),
            'num_clauses': len(self.cnf.clauses),
            'num_decisions': self.num_decisions
        }


def solve_sat(cnf: CNFExpression) -> Optional[Dict[str, bool]]:
    """
    Convenience function to solve a SAT problem using DPLL.

    Args:
        cnf: The CNF expression to solve

    Returns:
        A satisfying assignment if one exists, None if unsatisfiable
    """
    solver = DPLLSolver(cnf)
    return solver.solve()
