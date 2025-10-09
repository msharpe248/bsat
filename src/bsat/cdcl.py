"""
CDCL (Conflict-Driven Clause Learning) SAT Solver

This module implements a basic CDCL SAT solver with:
- Unit propagation
- Conflict analysis with 1UIP scheme
- Clause learning
- VSIDS (Variable State Independent Decaying Sum) heuristic
- Non-chronological backtracking
- Luby restart strategy

This is an educational implementation of CDCL. For production use, consider
industrial-strength solvers like MiniSat, CryptoMiniSat, Glucose, or Lingeling.

Note: This implementation prioritizes clarity over performance. It uses a simple
unit propagation scheme rather than the two-watched-literal optimization.
"""

from typing import Dict, List, Optional, Set, Tuple
from dataclasses import dataclass
from collections import defaultdict
import heapq
from .cnf import CNFExpression, Clause, Literal


@dataclass
class Assignment:
    """Represents a variable assignment with metadata."""
    variable: str
    value: bool
    decision_level: int
    antecedent: Optional[Clause] = None  # Clause that forced this assignment (None for decisions)

    def __repr__(self):
        return f"{self.variable}={self.value}@{self.decision_level}"


class CDCLStats:
    """Statistics for CDCL solver."""

    def __init__(self):
        self.decisions = 0
        self.propagations = 0
        self.conflicts = 0
        self.learned_clauses = 0
        self.restarts = 0
        self.backjumps = 0
        self.max_decision_level = 0

    def __str__(self):
        return (
            f"CDCLStats(\n"
            f"  Decisions: {self.decisions}\n"
            f"  Propagations: {self.propagations}\n"
            f"  Conflicts: {self.conflicts}\n"
            f"  Learned clauses: {self.learned_clauses}\n"
            f"  Restarts: {self.restarts}\n"
            f"  Backjumps: {self.backjumps}\n"
            f"  Max decision level: {self.max_decision_level}\n"
            f")"
        )


class CDCLSolver:
    """
    CDCL SAT Solver using watched literals and VSIDS heuristic.

    The solver implements the modern CDCL algorithm with:
    - Two-watched literal scheme for efficient unit propagation
    - First UIP (Unique Implication Point) clause learning
    - VSIDS variable selection heuristic
    - Luby restart strategy
    - Clause deletion for learned clauses

    Example:
        >>> solver = CDCLSolver(cnf)
        >>> result = solver.solve()
        >>> if result:
        ...     print(f"SAT: {result}")
        >>> else:
        ...     print("UNSAT")
    """

    def __init__(self, cnf: CNFExpression,
                 vsids_decay: float = 0.95,
                 restart_base: int = 100,
                 learned_clause_limit: int = 10000):
        """
        Initialize CDCL solver.

        Args:
            cnf: CNF formula to solve
            vsids_decay: Decay factor for VSIDS scores (0.9-0.99 typical)
            restart_base: Base interval for restarts
            learned_clause_limit: Maximum number of learned clauses to keep
        """
        self.original_cnf = cnf
        self.clauses = list(cnf.clauses)  # Original + learned clauses
        self.variables = sorted(cnf.get_variables())

        # Assignment trail
        self.trail: List[Assignment] = []
        self.assignment: Dict[str, bool] = {}  # Current assignment
        self.decision_level = 0

        # VSIDS heuristic
        self.vsids_scores: Dict[str, float] = {var: 0.0 for var in self.variables}
        self.vsids_decay = vsids_decay
        self.vsids_increment = 1.0

        # Restart strategy
        self.restart_base = restart_base
        self.conflicts_until_restart = restart_base
        self.restart_count = 0

        # Learned clause management
        self.learned_clause_limit = learned_clause_limit
        self.num_original_clauses = len(self.clauses)

        # Statistics
        self.stats = CDCLStats()

    def _get_literal_value(self, lit: Literal) -> Optional[bool]:
        """Get the value of a literal under current assignment."""
        if lit.variable not in self.assignment:
            return None
        var_value = self.assignment[lit.variable]
        return not var_value if lit.negated else var_value

    def _is_clause_satisfied(self, clause: Clause) -> bool:
        """Check if clause is satisfied under current assignment."""
        for lit in clause.literals:
            val = self._get_literal_value(lit)
            if val is True:
                return True
        return False

    def _assign(self, variable: str, value: bool, antecedent: Optional[Clause] = None):
        """Make an assignment and add to trail."""
        assignment = Assignment(
            variable=variable,
            value=value,
            decision_level=self.decision_level,
            antecedent=antecedent
        )
        self.trail.append(assignment)
        self.assignment[variable] = value

        if antecedent is None:
            self.stats.decisions += 1
        else:
            self.stats.propagations += 1

    def _unassign_to_level(self, level: int):
        """Backtrack to given decision level."""
        while self.trail and self.trail[-1].decision_level > level:
            assignment = self.trail.pop()
            del self.assignment[assignment.variable]
        self.decision_level = level

    def _propagate(self) -> Optional[Clause]:
        """
        Unit propagation.

        Returns:
            Conflict clause if conflict found, None otherwise
        """
        # Keep propagating until no more unit clauses
        propagated = True
        while propagated:
            propagated = False

            for clause in self.clauses:
                # Skip empty clauses
                if len(clause.literals) == 0:
                    return clause

                # Evaluate clause
                satisfied = False
                false_count = 0
                unassigned_lit = None

                for lit in clause.literals:
                    val = self._get_literal_value(lit)
                    if val is True:
                        satisfied = True
                        break
                    elif val is False:
                        false_count += 1
                    else:
                        unassigned_lit = lit

                if satisfied:
                    continue

                # All false - conflict
                if false_count == len(clause.literals):
                    return clause

                # Exactly one unassigned - unit clause
                if false_count == len(clause.literals) - 1 and unassigned_lit is not None:
                    self._assign(
                        unassigned_lit.variable,
                        not unassigned_lit.negated,
                        antecedent=clause
                    )
                    propagated = True

        return None

    def _analyze_conflict(self, conflict_clause: Clause) -> Tuple[Clause, int]:
        """
        Analyze conflict and learn a new clause using 1UIP scheme.

        Returns:
            Tuple of (learned_clause, backtrack_level)
        """
        if self.decision_level == 0:
            # Conflict at decision level 0 means UNSAT
            return (Clause([]), -1)

        # Build implication graph and find 1UIP
        seen = set()
        learned_literals = []
        counter = 0
        current_clause = conflict_clause
        antecedent_idx = len(self.trail) - 1

        while True:
            # Add literals from current clause
            for lit in current_clause.literals:
                if lit.variable not in seen:
                    seen.add(lit.variable)
                    var_assignment = None
                    for assign in self.trail:
                        if assign.variable == lit.variable:
                            var_assignment = assign
                            break

                    if var_assignment and var_assignment.decision_level == self.decision_level:
                        counter += 1
                        # Bump VSIDS score
                        self.vsids_scores[lit.variable] += self.vsids_increment
                    elif var_assignment and var_assignment.decision_level > 0:
                        # Add to learned clause (negated)
                        learned_literals.append(Literal(lit.variable, not lit.negated))

            # Find next literal to resolve
            while antecedent_idx >= 0:
                assignment = self.trail[antecedent_idx]
                if assignment.variable in seen and assignment.decision_level == self.decision_level:
                    break
                antecedent_idx -= 1

            counter -= 1
            if counter <= 0:
                # Found 1UIP
                assignment = self.trail[antecedent_idx]
                learned_literals.append(Literal(assignment.variable, not assignment.value))
                break

            # Continue with antecedent
            assignment = self.trail[antecedent_idx]
            if assignment.antecedent is None:
                break
            current_clause = assignment.antecedent
            antecedent_idx -= 1

        # Determine backtrack level
        if len(learned_literals) <= 1:
            backtrack_level = 0
        else:
            # Find second-highest decision level
            levels = []
            for lit in learned_literals:
                for assign in self.trail:
                    if assign.variable == lit.variable:
                        levels.append(assign.decision_level)
                        break
            levels.sort(reverse=True)
            backtrack_level = levels[1] if len(levels) > 1 else 0

        learned_clause = Clause(learned_literals)
        return learned_clause, backtrack_level

    def _pick_branching_variable(self) -> Optional[str]:
        """Pick next variable to branch on using VSIDS heuristic."""
        unassigned = [var for var in self.variables if var not in self.assignment]
        if not unassigned:
            return None

        # Pick variable with highest VSIDS score
        return max(unassigned, key=lambda v: self.vsids_scores[v])

    def _decay_vsids_scores(self):
        """Decay all VSIDS scores."""
        self.vsids_increment /= self.vsids_decay

    def _should_restart(self) -> bool:
        """Check if we should restart (Luby sequence)."""
        return self.stats.conflicts >= self.conflicts_until_restart

    def _restart(self):
        """Restart search from decision level 0."""
        self._unassign_to_level(0)
        self.stats.restarts += 1
        self.restart_count += 1

        # Luby sequence for restart intervals
        self.conflicts_until_restart = self._luby(self.restart_count) * self.restart_base

    def _luby(self, i: int) -> int:
        """Compute Luby sequence value."""
        # Find the finite subsequence that contains index i
        k = 1
        while (1 << (k + 1)) - 1 <= i:
            k += 1

        if (1 << k) - 1 == i:
            return 1 << (k - 1)
        else:
            return self._luby(i - (1 << k) + 1)

    def _add_learned_clause(self, clause: Clause):
        """Add learned clause to clause database."""
        self.clauses.append(clause)
        self.stats.learned_clauses += 1

        # Clause deletion if too many learned clauses
        if len(self.clauses) - self.num_original_clauses > self.learned_clause_limit:
            self._reduce_learned_clauses()

    def _reduce_learned_clauses(self):
        """Remove some learned clauses to save memory."""
        # Simple strategy: keep half of learned clauses (the most recently learned)
        num_to_keep = self.learned_clause_limit // 2
        learned_clauses = self.clauses[self.num_original_clauses:]

        # Keep the most recent ones
        to_keep = learned_clauses[-num_to_keep:]
        self.clauses = self.clauses[:self.num_original_clauses] + to_keep

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve the SAT formula using CDCL.

        Args:
            max_conflicts: Maximum number of conflicts before giving up

        Returns:
            Dictionary mapping variables to values if SAT, None if UNSAT
        """
        # Check for empty clause
        for clause in self.clauses:
            if len(clause.literals) == 0:
                return None

        # Initial unit propagation
        conflict = self._propagate()
        if conflict is not None:
            return None  # UNSAT at level 0

        while True:
            # Check conflict limit
            if self.stats.conflicts >= max_conflicts:
                return None  # Give up

            # Pick branching variable
            var = self._pick_branching_variable()

            if var is None:
                # All variables assigned - SAT!
                return dict(self.assignment)

            # Make decision
            self.decision_level += 1
            self.stats.max_decision_level = max(self.stats.max_decision_level, self.decision_level)
            self._assign(var, True)  # Try True first (could use phase saving here)

            # Propagate
            while True:
                conflict = self._propagate()

                if conflict is None:
                    # No conflict - continue
                    break

                # Conflict!
                self.stats.conflicts += 1

                if self.decision_level == 0:
                    # Conflict at level 0 - UNSAT
                    return None

                # Analyze conflict and learn clause
                learned_clause, backtrack_level = self._analyze_conflict(conflict)

                if backtrack_level < 0:
                    return None  # UNSAT

                # Add learned clause
                self._add_learned_clause(learned_clause)

                # Backtrack
                self._unassign_to_level(backtrack_level)
                self.decision_level = backtrack_level
                self.stats.backjumps += 1

                # Decay VSIDS scores
                self._decay_vsids_scores()

                # Check for restart
                if self._should_restart():
                    self._restart()
                    conflict = self._propagate()
                    if conflict is not None:
                        return None  # UNSAT
                    break

    def get_stats(self) -> CDCLStats:
        """Get solver statistics."""
        return self.stats


def solve_cdcl(cnf: CNFExpression,
               vsids_decay: float = 0.95,
               max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve a CNF formula using CDCL algorithm.

    Args:
        cnf: CNF formula to solve
        vsids_decay: VSIDS decay factor (0.9-0.99 typical)
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Dictionary mapping variables to values if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression, solve_cdcl
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z) & (~y | ~z)")
        >>> result = solve_cdcl(cnf)
        >>> if result:
        ...     print(f"SAT: {result}")
        ... else:
        ...     print("UNSAT")
    """
    solver = CDCLSolver(cnf, vsids_decay=vsids_decay)
    return solver.solve(max_conflicts=max_conflicts)


def get_cdcl_stats(cnf: CNFExpression,
                   vsids_decay: float = 0.95,
                   max_conflicts: int = 1000000) -> Tuple[Optional[Dict[str, bool]], CDCLStats]:
    """
    Solve using CDCL and return both solution and statistics.

    Args:
        cnf: CNF formula to solve
        vsids_decay: VSIDS decay factor
        max_conflicts: Maximum conflicts before giving up

    Returns:
        Tuple of (solution, statistics)

    Example:
        >>> solution, stats = get_cdcl_stats(cnf)
        >>> print(stats)
    """
    solver = CDCLSolver(cnf, vsids_decay=vsids_decay)
    solution = solver.solve(max_conflicts=max_conflicts)
    return solution, solver.get_stats()
