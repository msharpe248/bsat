"""
Schöning's Randomized Algorithm for k-SAT

A beautiful randomized algorithm discovered by Uwe Schöning in 1999.
For 3SAT, achieves expected runtime O(1.334^n), much better than
the trivial O(2^n) brute force.

Key Insights:
1. Start with random assignment
2. If unsatisfied, pick random unsatisfied clause
3. Flip random variable from that clause
4. Repeat for 3n steps (random walk)
5. Try multiple times with different random starts

Theoretical Results:
- 2SAT: Expected O(n) time (polynomial!)
- 3SAT: Expected O(1.334^n) time
- k-SAT: Expected O((2(k-1)/k)^n) time

The algorithm is remarkably simple yet theoretically important,
showing the power of randomization in algorithm design.

Reference:
Schöning, U. (1999). "A probabilistic algorithm for k-SAT and
constraint satisfaction problems". FOCS 1999.
"""

from typing import Dict, Optional, List, Set
from dataclasses import dataclass
import random
from .cnf import CNFExpression, Clause, Literal


@dataclass
class SchoeningStats:
    """Statistics from Schöning's algorithm execution."""
    tries: int = 0              # Number of random restarts
    total_flips: int = 0        # Total variable flips across all tries
    flips_per_try: List[int] = None  # Flips in each try

    def __post_init__(self):
        if self.flips_per_try is None:
            self.flips_per_try = []

    def __str__(self):
        avg_flips = sum(self.flips_per_try) / len(self.flips_per_try) if self.flips_per_try else 0
        return (
            f"SchoeningStats(\n"
            f"  Tries: {self.tries}\n"
            f"  Total flips: {self.total_flips}\n"
            f"  Average flips per try: {avg_flips:.1f}\n"
            f")"
        )


class SchoeningSolver:
    """
    Schöning's randomized algorithm for k-SAT.

    This elegant algorithm uses random walks to find satisfying assignments.
    For 3SAT, it achieves expected runtime O(1.334^n), significantly better
    than brute force O(2^n).

    Algorithm:
    1. Start with random assignment
    2. While not all clauses satisfied:
       a. Pick random unsatisfied clause
       b. Pick random variable from that clause
       c. Flip that variable
    3. Repeat step 2 for at most max_flips steps
    4. If solution not found, restart from step 1
    5. Try at most max_tries restarts

    The key insight: If we're k flips away from a solution, we have
    at least 1/k chance of getting closer with each random flip from
    an unsatisfied clause.

    Example:
        >>> from bsat import CNFExpression, SchoeningSolver
        >>> cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")
        >>> solver = SchoeningSolver(cnf)
        >>> solution = solver.solve()
    """

    def __init__(self, cnf: CNFExpression, seed: Optional[int] = None):
        """
        Initialize Schöning's algorithm.

        Args:
            cnf: CNF formula to solve
            seed: Random seed for reproducibility
        """
        self.cnf = cnf
        self.variables = list(cnf.get_variables())
        self.n = len(self.variables)
        self.stats = SchoeningStats()

        if seed is not None:
            random.seed(seed)

    def _random_assignment(self) -> Dict[str, bool]:
        """Generate random initial assignment."""
        return {var: random.choice([True, False]) for var in self.variables}

    def _get_unsatisfied_clauses(self, assignment: Dict[str, bool]) -> List[Clause]:
        """Get list of clauses not satisfied by current assignment."""
        return [clause for clause in self.cnf.clauses
                if not clause.evaluate(assignment)]

    def _random_walk_attempt(self, max_flips: int) -> Optional[Dict[str, bool]]:
        """
        Single attempt: random walk for at most max_flips steps.

        Args:
            max_flips: Maximum number of variable flips

        Returns:
            Satisfying assignment if found, None otherwise
        """
        # Start with random assignment
        assignment = self._random_assignment()
        flips = 0

        for _ in range(max_flips):
            # Check if we have a solution
            if self.cnf.evaluate(assignment):
                self.stats.flips_per_try.append(flips)
                return assignment

            # Pick random unsatisfied clause
            unsat_clauses = self._get_unsatisfied_clauses(assignment)
            if not unsat_clauses:
                # Should not happen, but just in case
                self.stats.flips_per_try.append(flips)
                return assignment

            clause = random.choice(unsat_clauses)

            # Pick random variable from that clause and flip it
            variables_in_clause = [lit.variable for lit in clause.literals]
            var_to_flip = random.choice(variables_in_clause)

            assignment[var_to_flip] = not assignment[var_to_flip]
            flips += 1

        self.stats.flips_per_try.append(flips)
        return None

    def solve(self, max_tries: int = 1000, max_flips: Optional[int] = None) -> Optional[Dict[str, bool]]:
        """
        Run Schöning's algorithm to find satisfying assignment.

        Args:
            max_tries: Maximum number of random restarts
            max_flips: Maximum flips per try (default: 3*n for 3SAT)

        Returns:
            Satisfying assignment if found, None otherwise

        Complexity:
            For 3SAT: Expected O(1.334^n)
            For k-SAT: Expected O((2(k-1)/k)^n)
        """
        # Default: 3*n flips per attempt (optimal for 3SAT)
        if max_flips is None:
            max_flips = 3 * self.n if self.n > 0 else 10

        self.stats = SchoeningStats()

        for try_num in range(max_tries):
            self.stats.tries = try_num + 1

            solution = self._random_walk_attempt(max_flips)

            if solution is not None:
                self.stats.total_flips = sum(self.stats.flips_per_try)
                return solution

        self.stats.total_flips = sum(self.stats.flips_per_try)
        return None

    def get_stats(self) -> SchoeningStats:
        """Get statistics from last solve() call."""
        return self.stats


def solve_schoening(cnf: CNFExpression, max_tries: int = 1000,
                    max_flips: Optional[int] = None,
                    seed: Optional[int] = None) -> Optional[Dict[str, bool]]:
    """
    Solve SAT using Schöning's randomized algorithm.

    This is a simple functional interface to SchoeningSolver.

    Args:
        cnf: CNF formula to solve
        max_tries: Maximum number of random restarts (default: 1000)
        max_flips: Maximum flips per try (default: 3*n)
        seed: Random seed for reproducibility

    Returns:
        Satisfying assignment if found, None otherwise

    Example:
        >>> from bsat import CNFExpression, solve_schoening
        >>> cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c)")
        >>> solution = solve_schoening(cnf)
        >>> if solution:
        ...     print(f"SAT: {solution}")
        ... else:
        ...     print("No solution found (but may exist)")

    Note:
        This is an INCOMPLETE algorithm - it may not find a solution
        even if one exists. However, for satisfiable instances it is
        very fast in practice.

        For 3SAT: Expected runtime O(1.334^n)
        For 2SAT: Expected runtime O(n) - but use solve_2sat() instead!
    """
    solver = SchoeningSolver(cnf, seed=seed)
    return solver.solve(max_tries=max_tries, max_flips=max_flips)


def get_schoening_stats(cnf: CNFExpression, max_tries: int = 1000,
                        max_flips: Optional[int] = None,
                        seed: Optional[int] = None) -> tuple[Optional[Dict[str, bool]], SchoeningStats]:
    """
    Solve SAT with Schöning's algorithm and return detailed statistics.

    Args:
        cnf: CNF formula to solve
        max_tries: Maximum number of random restarts
        max_flips: Maximum flips per try (default: 3*n)
        seed: Random seed for reproducibility

    Returns:
        Tuple of (solution, statistics)

    Example:
        >>> solution, stats = get_schoening_stats(cnf)
        >>> print(f"Tries: {stats.tries}")
        >>> print(f"Total flips: {stats.total_flips}")
    """
    solver = SchoeningSolver(cnf, seed=seed)
    solution = solver.solve(max_tries=max_tries, max_flips=max_flips)
    return solution, solver.get_stats()
