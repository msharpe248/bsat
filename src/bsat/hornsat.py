"""Horn-SAT solver using unit propagation."""

from typing import Dict, Optional, Set, List
from .cnf import CNFExpression, Clause, Literal


class HornSATSolver:
    """
    Horn-SAT solver using unit propagation.

    A Horn clause has at most one positive literal. Horn-SAT can be solved
    in polynomial time O(n+m) where n is variables and m is clauses.

    The algorithm is forward chaining (unit propagation):
    1. Start with all variables False
    2. Fire every clause whose negated variables are all True
    3. Firing sets the clause's positive variable True, or reports UNSAT if
       the clause has no positive literal
    4. Repeat until nothing more fires

    A per-clause counter makes each literal occurrence cost O(1) overall.

    Time complexity: O(n+m) - linear in formula size
    Space complexity: O(n+m) - assignments plus occurrence lists
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
        Solve the Horn-SAT problem using linear-time unit propagation.

        Each Horn clause (¬b1 ∨ ... ∨ ¬bk ∨ h) is read as the implication
        (b1 ∧ ... ∧ bk) → h. A clause with no positive literal is a goal
        clause (b1 ∧ ... ∧ bk) → ⊥.

        Every clause keeps a counter of body variables that are not yet True.
        When a variable becomes True, only the clauses whose body mentions it
        are visited and their counters decremented; a counter reaching zero
        fires the clause. Each literal occurrence is therefore touched a
        constant number of times (Dowling & Gallier, 1984).

        Returns:
            A satisfying assignment if one exists, None if unsatisfiable
        """
        self.num_unit_propagations = 0

        # Start with all variables False; variables only ever go False → True,
        # so the result is the minimal model.
        assignment = {var: False for var in self.variables}

        heads: List[Optional[str]] = []            # Positive variable of each clause (or None)
        remaining: List[int] = []                  # Body variables not yet True
        body_occurrences: Dict[str, List[int]] = {var: [] for var in self.variables}
        ready: List[int] = []                      # Clauses whose body is fully True

        for clause in self.cnf.clauses:
            head = None
            body: Set[str] = set()
            for literal in clause.literals:
                if literal.negated:
                    body.add(literal.variable)
                else:
                    head = literal.variable

            index = len(heads)
            heads.append(head)

            if head is not None and head in body:
                # (¬h ∨ h ∨ ...) is a tautology: it can never fire
                remaining.append(-1)
                continue

            remaining.append(len(body))
            for var in body:
                body_occurrences[var].append(index)
            if not body:
                ready.append(index)

        while ready:
            index = ready.pop()
            head = heads[index]

            if head is None:
                # Body is entirely True and there is no head: conflict
                return None

            if assignment[head]:
                continue

            assignment[head] = True
            self.num_unit_propagations += 1

            for other in body_occurrences[head]:
                remaining[other] -= 1
                if remaining[other] == 0:
                    ready.append(other)

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
