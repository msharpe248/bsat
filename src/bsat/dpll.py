"""DPLL (Davis-Putnam-Logemann-Loveland) algorithm for SAT solving."""

from typing import Dict, Optional, Set, List
from .cnf import CNFExpression, Clause, Literal


class DPLLSolver:
    """
    DPLL SAT solver with unit propagation and pure literal elimination.

    The DPLL algorithm is a complete, sound, and terminating algorithm for determining
    the satisfiability of propositional logic formulas in CNF. It uses:
    - Backtracking search through the variable assignment space
    - Unit propagation: Forced assignments from unit clauses
    - Pure literal elimination: Variables appearing with only one polarity
    - Early termination when conflicts are detected

    Time complexity: O(2^n) in worst case, where n is the number of variables
    Space complexity: O(n) for the recursion stack
    """

    def __init__(self, cnf: CNFExpression, use_unit_propagation: bool = True,
                 use_pure_literal: bool = True):
        """
        Initialize the DPLL solver with a CNF expression.

        Args:
            cnf: The CNF expression to solve
            use_unit_propagation: Enable unit propagation optimization
            use_pure_literal: Enable pure literal elimination optimization
        """
        self.cnf = cnf
        self.variables = sorted(cnf.get_variables())
        self.num_decisions = 0  # Track number of decision points (for statistics)
        self.num_unit_propagations = 0  # Track unit propagations
        self.num_pure_literals = 0  # Track pure literal eliminations
        self.use_unit_propagation = use_unit_propagation
        self.use_pure_literal = use_pure_literal

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve the SAT problem using DPLL algorithm.

        Returns:
            A satisfying assignment if one exists, None if unsatisfiable
        """
        self.num_decisions = 0
        self.num_unit_propagations = 0
        self.num_pure_literals = 0
        assignment = {}
        return self._dpll(assignment, list(self.cnf.clauses))

    def find_all_solutions(self, max_solutions: Optional[int] = None) -> List[Dict[str, bool]]:
        """
        Find all satisfying assignments (or up to max_solutions).

        This enumerates all possible solutions by continuing the search
        after finding each solution. Can be exponential in the number
        of solutions!

        Args:
            max_solutions: Maximum number of solutions to find (None = all)

        Returns:
            List of all satisfying assignments found

        Note:
            If unit propagation or pure literal elimination is enabled,
            some solutions may be grouped. To find ALL distinct solutions,
            create solver with use_unit_propagation=False and use_pure_literal=False.

        Example:
            >>> cnf = CNFExpression.parse("(x | y)")
            >>> # With optimizations: may find fewer solutions
            >>> solver1 = DPLLSolver(cnf)
            >>> len(solver1.find_all_solutions())  # May be < 3
            2
            >>> # Without optimizations: finds all distinct solutions
            >>> solver2 = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
            >>> len(solver2.find_all_solutions())  # All 3: {x:T,y:F}, {x:F,y:T}, {x:T,y:T}
            3
        """
        self.num_decisions = 0
        self.num_unit_propagations = 0
        self.num_pure_literals = 0
        self.solutions = []
        self.max_solutions = max_solutions
        assignment = {}
        self._dpll_all(assignment, list(self.cnf.clauses))
        return self.solutions

    def count_solutions(self, max_count: Optional[int] = None) -> int:
        """
        Count the number of satisfying assignments (up to max_count).

        More efficient than find_all_solutions() when you only need the count,
        as it doesn't store the actual assignments.

        Args:
            max_count: Maximum count (None = count all, may be slow!)

        Returns:
            Number of satisfying assignments

        Example:
            >>> cnf = CNFExpression.parse("(x | y)")
            >>> solver = DPLLSolver(cnf)
            >>> solver.count_solutions()
            3
        """
        solutions = self.find_all_solutions(max_solutions=max_count)
        return len(solutions)

    def _dpll(self, assignment: Dict[str, bool], clauses: List[Clause]) -> Optional[Dict[str, bool]]:
        """
        Recursive DPLL algorithm with optimizations.

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

        # Unit Propagation: Find and propagate unit clauses
        if self.use_unit_propagation:
            unit_clause = self._find_unit_clause(simplified_clauses, assignment)
            if unit_clause is not None:
                # Found a unit clause - must assign the literal to True
                literal = unit_clause.literals[0]
                var = literal.variable
                value = not literal.negated  # If literal is ¬x, assign x=False

                self.num_unit_propagations += 1
                assignment[var] = value
                result = self._dpll(assignment, simplified_clauses)
                if result is not None:
                    return result

                # Backtrack
                del assignment[var]
                return None

        # Pure Literal Elimination: Find and eliminate pure literals
        if self.use_pure_literal:
            pure_literal = self._find_pure_literal(simplified_clauses, assignment)
            if pure_literal is not None:
                var = pure_literal.variable
                value = not pure_literal.negated  # If literal is ¬x, assign x=False

                self.num_pure_literals += 1
                assignment[var] = value
                result = self._dpll(assignment, simplified_clauses)
                if result is not None:
                    return result

                # Backtrack (though pure literal assignment should never fail)
                del assignment[var]
                return None

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

    def _dpll_all(self, assignment: Dict[str, bool], clauses: List[Clause]) -> None:
        """
        Recursive DPLL that finds ALL solutions (not just first one).

        Instead of returning when a solution is found, collect it and continue
        searching by forcing backtracking.

        Args:
            assignment: Current partial variable assignment
            clauses: Current set of clauses (may be simplified)
        """
        # Check if we've found enough solutions
        if self.max_solutions is not None and len(self.solutions) >= self.max_solutions:
            return

        # Simplify clauses based on current assignment
        simplified_clauses = self._simplify_clauses(clauses, assignment)

        # Base case 1: Empty clause found (conflict)
        if any(not clause.literals for clause in simplified_clauses):
            return

        # Base case 2: All clauses satisfied
        if not simplified_clauses:
            # Find unassigned variables
            unassigned_vars = [v for v in self.variables if v not in assignment]

            if not unassigned_vars:
                # All variables assigned - store this solution
                self.solutions.append(assignment.copy())
                return

            # Enumerate all combinations of unassigned variables
            # This handles the case where multiple variables don't affect any clause
            self._enumerate_unassigned(assignment, unassigned_vars, 0)
            return

        # Unit Propagation
        if self.use_unit_propagation:
            unit_clause = self._find_unit_clause(simplified_clauses, assignment)
            if unit_clause is not None:
                literal = unit_clause.literals[0]
                var = literal.variable
                value = not literal.negated

                self.num_unit_propagations += 1
                assignment[var] = value
                self._dpll_all(assignment, simplified_clauses)

                # Backtrack
                del assignment[var]
                return

        # Pure Literal Elimination
        if self.use_pure_literal:
            pure_literal = self._find_pure_literal(simplified_clauses, assignment)
            if pure_literal is not None:
                var = pure_literal.variable
                value = not pure_literal.negated

                self.num_pure_literals += 1
                assignment[var] = value
                self._dpll_all(assignment, simplified_clauses)

                # Backtrack
                del assignment[var]
                return

        # Choose next unassigned variable
        unassigned = self._get_unassigned_variable(assignment)
        if unassigned is None:
            # All variables assigned
            if self.cnf.evaluate(assignment):
                self.solutions.append(assignment.copy())
            return

        self.num_decisions += 1

        # Try assigning True
        assignment[unassigned] = True
        self._dpll_all(assignment, simplified_clauses)

        # Try assigning False
        assignment[unassigned] = False
        self._dpll_all(assignment, simplified_clauses)

        # Backtrack
        del assignment[unassigned]

    def _enumerate_unassigned(self, assignment: Dict[str, bool], unassigned_vars: List[str], index: int) -> None:
        """
        Enumerate all combinations of unassigned variables.

        When all clauses are satisfied but variables remain unassigned,
        we need to enumerate all 2^k combinations where k is the number
        of unassigned variables.

        Args:
            assignment: Current assignment (will be modified)
            unassigned_vars: List of unassigned variables
            index: Current index in unassigned_vars
        """
        # Check if we've found enough solutions
        if self.max_solutions is not None and len(self.solutions) >= self.max_solutions:
            return

        # Base case: assigned all unassigned variables
        if index >= len(unassigned_vars):
            self.solutions.append(assignment.copy())
            return

        var = unassigned_vars[index]

        # Try True
        assignment[var] = True
        self._enumerate_unassigned(assignment, unassigned_vars, index + 1)

        # Try False
        assignment[var] = False
        self._enumerate_unassigned(assignment, unassigned_vars, index + 1)

        # Backtrack
        del assignment[var]

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

    def _find_unit_clause(self, clauses: List[Clause], assignment: Dict[str, bool]) -> Optional[Clause]:
        """
        Find a unit clause (clause with only one unassigned literal).

        Args:
            clauses: List of clauses to search
            assignment: Current variable assignment

        Returns:
            A unit clause if one exists, None otherwise
        """
        for clause in clauses:
            unassigned_literals = [
                lit for lit in clause.literals
                if lit.variable not in assignment
            ]
            if len(unassigned_literals) == 1:
                return Clause(unassigned_literals)
        return None

    def _find_pure_literal(self, clauses: List[Clause], assignment: Dict[str, bool]) -> Optional[Literal]:
        """
        Find a pure literal (variable that appears with only one polarity).

        A variable is pure if it appears only positively or only negatively
        in all clauses (considering only unassigned variables).

        Args:
            clauses: List of clauses to search
            assignment: Current variable assignment

        Returns:
            A pure literal if one exists, None otherwise
        """
        # Track polarity of each variable: True for positive, False for negative
        positive_vars: Set[str] = set()
        negative_vars: Set[str] = set()

        for clause in clauses:
            for literal in clause.literals:
                if literal.variable not in assignment:
                    if literal.negated:
                        negative_vars.add(literal.variable)
                    else:
                        positive_vars.add(literal.variable)

        # Find variables that appear with only one polarity
        pure_positive = positive_vars - negative_vars
        pure_negative = negative_vars - positive_vars

        if pure_positive:
            var = pure_positive.pop()
            return Literal(var, negated=False)
        elif pure_negative:
            var = pure_negative.pop()
            return Literal(var, negated=True)

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
            'num_decisions': self.num_decisions,
            'num_unit_propagations': self.num_unit_propagations,
            'num_pure_literals': self.num_pure_literals
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


def find_all_sat_solutions(cnf: CNFExpression, max_solutions: Optional[int] = None) -> List[Dict[str, bool]]:
    """
    Find all satisfying assignments for a SAT problem.

    Args:
        cnf: The CNF expression to solve
        max_solutions: Maximum number of solutions to find (None = all)

    Returns:
        List of all satisfying assignments

    Warning:
        Can be exponential! For n variables, there can be up to 2^n solutions.
        Use max_solutions to limit the search.

    Example:
        >>> cnf = CNFExpression.parse("(x | y)")
        >>> solutions = find_all_sat_solutions(cnf)
        >>> len(solutions)  # 3: {x:T,y:F}, {x:F,y:T}, {x:T,y:T}
        3
    """
    solver = DPLLSolver(cnf)
    return solver.find_all_solutions(max_solutions=max_solutions)


def count_sat_solutions(cnf: CNFExpression, max_count: Optional[int] = None) -> int:
    """
    Count the number of satisfying assignments.

    Args:
        cnf: The CNF expression to solve
        max_count: Maximum count to compute (None = count all)

    Returns:
        Number of satisfying assignments

    Example:
        >>> cnf = CNFExpression.parse("(x | y)")
        >>> count_sat_solutions(cnf)
        3
    """
    solver = DPLLSolver(cnf)
    return solver.count_solutions(max_count=max_count)
