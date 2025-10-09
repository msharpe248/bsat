"""
WalkSAT Solver

This module implements the WalkSAT randomized local search algorithm for SAT.

WalkSAT is an incomplete but often very fast algorithm for finding satisfying assignments.
It works by:
1. Starting with a random assignment
2. Repeatedly flipping variables to satisfy unsatisfied clauses
3. Using a noise parameter to escape local minima

Key characteristics:
- Incomplete: May not find a solution even if one exists
- Probabilistic: Different runs may give different results
- Fast: Often finds solutions much quicker than complete solvers
- Good for SAT instances: Not suitable for proving UNSAT

Time Complexity: Varies (depends on problem and luck)
- Best case: O(n) if lucky
- Average case: Problem-dependent
- Worst case: May never terminate (set max_flips limit)

References:
- Selman, Kautz, Cohen (1994): "Noise Strategies for Improving Local Search"
  AAAI-94: Proceedings of the Twelfth National Conference on Artificial Intelligence
- Hoos (2002): "An Adaptive Noise Mechanism for WalkSAT"
"""

from typing import Dict, Optional, Set, List
import random
from .cnf import CNFExpression, Clause


class WalkSATSolver:
    """
    WalkSAT randomized local search SAT solver.

    Algorithm:
    1. Start with random assignment
    2. While unsatisfied clauses exist and flips < max_flips:
       a. Pick a random unsatisfied clause
       b. With probability p (noise): flip random variable from clause
       c. With probability 1-p: flip variable that minimizes breaks
    3. Return solution if found, None otherwise

    The noise parameter p controls exploration vs exploitation:
    - p=0: Greedy (always minimize breaks) - can get stuck
    - p=1: Random walk - poor performance
    - p≈0.3-0.5: Good balance for most problems
    """

    def __init__(self, cnf: CNFExpression, noise: float = 0.5, max_flips: int = 100000,
                 max_tries: int = 10, seed: Optional[int] = None):
        """
        Initialize WalkSAT solver.

        Args:
            cnf: CNFExpression to solve
            noise: Probability of random flip (0.0 to 1.0, default 0.5)
            max_flips: Maximum flips per try (default 100000)
            max_tries: Maximum number of random restarts (default 10)
            seed: Random seed for reproducibility (optional)
        """
        self.cnf = cnf
        self.noise = noise
        self.max_flips = max_flips
        self.max_tries = max_tries

        if seed is not None:
            random.seed(seed)

        self.stats = {
            'total_flips': 0,
            'total_tries': 0,
            'unsatisfied_clauses_history': [],
            'num_variables': 0,
            'num_clauses': 0
        }

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve the SAT problem using WalkSAT.

        Returns:
            A satisfying assignment if found within max_tries and max_flips,
            None otherwise (note: None doesn't mean UNSAT, just not found)
        """
        if not self.cnf.clauses:
            return {}

        variables = sorted(self.cnf.get_variables())
        if not variables:
            return {}

        self.stats['num_variables'] = len(variables)
        self.stats['num_clauses'] = len(self.cnf.clauses)

        # Try multiple random restarts
        for try_num in range(self.max_tries):
            self.stats['total_tries'] += 1

            # Random initial assignment
            assignment = {var: random.choice([True, False]) for var in variables}

            # Local search
            for flip_num in range(self.max_flips):
                self.stats['total_flips'] += 1

                # Find unsatisfied clauses
                unsatisfied = self._get_unsatisfied_clauses(assignment)
                self.stats['unsatisfied_clauses_history'].append(len(unsatisfied))

                # Check if solved
                if not unsatisfied:
                    return assignment

                # Pick random unsatisfied clause
                clause = random.choice(unsatisfied)

                # Decide whether to make random or greedy move
                if random.random() < self.noise:
                    # Random walk: flip random variable from clause
                    var = random.choice([lit.variable for lit in clause.literals])
                else:
                    # Greedy: flip variable that minimizes break count
                    var = self._pick_best_flip(clause, assignment)

                # Flip the variable
                assignment[var] = not assignment[var]

        # No solution found within limits
        return None

    def _get_unsatisfied_clauses(self, assignment: Dict[str, bool]) -> List[Clause]:
        """
        Get list of clauses that are not satisfied by the current assignment.

        Args:
            assignment: Current variable assignment

        Returns:
            List of unsatisfied clauses
        """
        unsatisfied = []
        for clause in self.cnf.clauses:
            if not self._is_clause_satisfied(clause, assignment):
                unsatisfied.append(clause)
        return unsatisfied

    def _is_clause_satisfied(self, clause: Clause, assignment: Dict[str, bool]) -> bool:
        """
        Check if a clause is satisfied by the assignment.

        Args:
            clause: Clause to check
            assignment: Variable assignment

        Returns:
            True if clause is satisfied
        """
        for literal in clause.literals:
            var_value = assignment.get(literal.variable, False)
            literal_value = var_value if not literal.negated else not var_value
            if literal_value:
                return True
        return False

    def _pick_best_flip(self, clause: Clause, assignment: Dict[str, bool]) -> str:
        """
        Pick the variable from the clause that minimizes break count when flipped.

        Break count = number of currently satisfied clauses that become unsatisfied.

        Args:
            clause: The unsatisfied clause to pick from
            assignment: Current assignment

        Returns:
            Variable name to flip
        """
        variables = [lit.variable for lit in clause.literals]
        min_breaks = float('inf')
        best_var = variables[0]  # Default to first variable

        for var in variables:
            # Count how many clauses would break if we flip this variable
            breaks = self._count_breaks(var, assignment)

            if breaks < min_breaks:
                min_breaks = breaks
                best_var = var

        return best_var

    def _count_breaks(self, var: str, assignment: Dict[str, bool]) -> int:
        """
        Count how many currently satisfied clauses would become unsatisfied
        if we flip the given variable.

        Args:
            var: Variable to flip
            assignment: Current assignment

        Returns:
            Number of breaks (satisfied → unsatisfied)
        """
        # Create temporary assignment with var flipped
        temp_assignment = assignment.copy()
        temp_assignment[var] = not temp_assignment[var]

        breaks = 0
        for clause in self.cnf.clauses:
            # Count clauses that are currently satisfied but would become unsatisfied
            if self._is_clause_satisfied(clause, assignment):
                if not self._is_clause_satisfied(clause, temp_assignment):
                    breaks += 1

        return breaks


def solve_walksat(cnf: CNFExpression, noise: float = 0.5, max_flips: int = 100000,
                  max_tries: int = 10, seed: Optional[int] = None) -> Optional[Dict[str, bool]]:
    """
    Solve a SAT problem using the WalkSAT algorithm.

    WalkSAT is an incomplete randomized local search algorithm. It's often very fast
    for satisfiable instances but cannot prove unsatisfiability.

    Args:
        cnf: CNFExpression to solve
        noise: Probability of random flip (0.0-1.0, default 0.5)
        max_flips: Maximum variable flips per try (default 100000)
        max_tries: Maximum random restarts (default 10)
        seed: Random seed for reproducibility (optional)

    Returns:
        A satisfying assignment if found, None otherwise
        (Note: None doesn't prove UNSAT, just means solution not found)

    Example:
        >>> from bsat import CNFExpression, solve_walksat
        >>> formula = "(x | y | z) & (~x | y) & (~y | ~z)"
        >>> cnf = CNFExpression.parse(formula)
        >>> result = solve_walksat(cnf)
        >>> if result:
        ...     print(f"SAT: {result}")
        ... else:
        ...     print("No solution found (but may still be SAT)")
    """
    solver = WalkSATSolver(cnf, noise=noise, max_flips=max_flips,
                          max_tries=max_tries, seed=seed)
    return solver.solve()


def get_walksat_stats(cnf: CNFExpression, noise: float = 0.5, max_flips: int = 100000,
                      max_tries: int = 10, seed: Optional[int] = None) -> Dict:
    """
    Solve with WalkSAT and return detailed statistics.

    Args:
        cnf: CNFExpression to solve
        noise: Probability of random flip (0.0-1.0, default 0.5)
        max_flips: Maximum flips per try (default 100000)
        max_tries: Maximum random restarts (default 10)
        seed: Random seed for reproducibility (optional)

    Returns:
        Dictionary with solution and statistics:
        - solution: The assignment (or None)
        - found: Whether a solution was found
        - stats: Detailed statistics (flips, tries, etc.)

    Example:
        >>> result = get_walksat_stats(cnf, noise=0.4, seed=42)
        >>> print(f"Found: {result['found']}")
        >>> print(f"Total flips: {result['stats']['total_flips']}")
    """
    solver = WalkSATSolver(cnf, noise=noise, max_flips=max_flips,
                          max_tries=max_tries, seed=seed)
    solution = solver.solve()

    return {
        'solution': solution,
        'found': solution is not None,
        'stats': solver.stats
    }
