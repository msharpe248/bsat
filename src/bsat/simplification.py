"""
Formula simplification techniques used by modern SAT solvers.

`bsat.preprocessing` covers the basics (unit propagation, pure literals,
subsumption, connected components). This module adds the stronger techniques
that competitive solvers run before search ("preprocessing") and repeatedly
during it ("inprocessing"):

- Failed literal probing: try a literal, and if propagation hits a
  conflict, its negation must hold.
- Blocked clause elimination (BCE): remove clauses that can never be
  the only obstacle to a solution.
- Bounded variable elimination (BVE): resolve a variable away when that
  does not make the formula larger. This is the Davis-Putnam rule, applied
  selectively, and the single most effective preprocessing technique.
- Vivification: shorten a clause when propagation shows that some of
  its literals are unnecessary.

BCE and BVE produce a formula that is satisfiable exactly when the original is,
but whose solutions are not directly solutions of the original. Each such step
is recorded on a reconstruction stack, and `extend_model()` replays the stack
backwards to turn a solution of the simplified formula into a solution of the
original one.

The implementations favour clarity over speed: propagation is a plain scan
over the clause list. The C solver in `competition/c/` implements the same
techniques with occurrence lists, signatures and work budgets.
"""

from typing import Callable, Dict, List, Optional, Set, Tuple

from .cnf import CNFExpression, Clause, Literal

# A clause is a sorted tuple of non-zero integers: +v for variable number v,
# -v for its negation.
IntClause = Tuple[int, ...]


class SimplificationStats:
    """Statistics for a simplification run."""

    def __init__(self):
        self.original_vars = 0
        self.original_clauses = 0
        self.final_vars = 0
        self.final_clauses = 0
        self.failed_literals = 0        # Literals fixed by probing (and their consequences)
        self.blocked_clauses = 0        # Clauses removed by BCE
        self.eliminated_variables = 0   # Variables removed by BVE
        self.vivified_clauses = 0       # Clauses shortened by vivification
        self.vivified_literals = 0      # Literals removed by vivification

    def __str__(self):
        return (
            f"SimplificationStats(\n"
            f"  Variables: {self.original_vars} → {self.final_vars}\n"
            f"  Clauses: {self.original_clauses} → {self.final_clauses}\n"
            f"  Failed literals: {self.failed_literals}\n"
            f"  Blocked clauses: {self.blocked_clauses}\n"
            f"  Eliminated variables: {self.eliminated_variables}\n"
            f"  Vivified clauses: {self.vivified_clauses}\n"
            f"  Vivified literals: {self.vivified_literals}\n"
            f")"
        )


class SimplificationResult:
    """
    Result of simplification.

    Attributes:
        cnf: The simplified formula (a single empty clause if proved UNSAT)
        is_unsat: True if simplification alone proved the formula unsatisfiable
        stats: Simplification statistics
    """

    def __init__(self, cnf: CNFExpression, is_unsat: bool, stats: SimplificationStats,
                 extend: Callable[[Dict[str, bool]], Dict[str, bool]]):
        self.cnf = cnf
        self.is_unsat = is_unsat
        self.stats = stats
        self._extend = extend

    def extend_model(self, model: Dict[str, bool]) -> Dict[str, bool]:
        """
        Turn a solution of the simplified formula into a solution of the original.

        Args:
            model: Satisfying assignment of `self.cnf`

        Returns:
            Satisfying assignment of the original formula, covering all its variables
        """
        return self._extend(model)


def _propagate(clauses: List[IntClause], assigned: Set[int]) -> bool:
    """
    Unit propagation by repeated scanning.

    Args:
        clauses: The formula
        assigned: Set of true literals; extended in place with everything implied

    Returns:
        False if a clause became empty (conflict), True otherwise
    """
    changed = True
    while changed:
        changed = False
        for clause in clauses:
            unassigned = None
            open_count = 0
            satisfied = False
            for lit in clause:
                if lit in assigned:
                    satisfied = True
                    break
                if -lit not in assigned:
                    open_count += 1
                    unassigned = lit
            if satisfied:
                continue
            if open_count == 0:
                return False
            if open_count == 1:
                assigned.add(unassigned)
                changed = True
    return True


class Simplifier:
    """
    Applies simplification techniques to a formula and records how to undo them.

    Each technique is a method that can be called on its own, in any order and
    any number of times. Call `result()` at the end.

    Example:
        >>> simplifier = Simplifier(cnf)
        >>> simplifier.probe_failed_literals()
        >>> simplifier.eliminate_variables()
        >>> result = simplifier.result()
        >>> model = solve_cdcl(result.cnf)
        >>> if model is not None:
        ...     model = result.extend_model(model)
    """

    def __init__(self, cnf: CNFExpression):
        """
        Args:
            cnf: Formula to simplify (not modified)
        """
        self.names: List[str] = sorted(cnf.get_variables())
        index = {name: i + 1 for i, name in enumerate(self.names)}

        self.stats = SimplificationStats()
        self.stats.original_vars = len(self.names)
        self.stats.original_clauses = len(cnf.clauses)

        self.unsat = False
        self.clauses: List[IntClause] = []
        # Reconstruction stack, in the order the steps were applied:
        #   ('unit', lit)                  lit was found to be forced
        #   ('blocked', lit, clause)       clause was removed, blocked on lit
        #   ('eliminated', var, clauses)   var was resolved away; clauses are those removed
        self.reconstruction: List[tuple] = []

        seen: Set[IntClause] = set()
        for clause in cnf.clauses:
            lits = {(-index[l.variable] if l.negated else index[l.variable])
                    for l in clause.literals}
            if any(-lit in lits for lit in lits):
                continue                    # Tautology
            if not lits:
                self.unsat = True
            key = tuple(sorted(lits))
            if key not in seen:
                seen.add(key)
                self.clauses.append(key)

    # ------------------------------------------------------------------
    # Failed literal probing
    # ------------------------------------------------------------------

    def probe_failed_literals(self) -> int:
        """
        Fix every literal whose assumption leads to a conflict.

        If assuming `x` and propagating produces a conflict, then `¬x` is a
        consequence of the formula. It is added as a fact and the formula is
        simplified with it, which may make further literals fail.

        Returns:
            Number of variables fixed (failed literals plus everything they imply)
        """
        fixed_before = self.stats.failed_literals
        changed = True
        while changed and not self.unsat:
            changed = False
            for var in sorted(self._variables()):
                for lit in (var, -var):
                    if _propagate(self.clauses, {lit}):
                        continue
                    # `lit` failed, so its negation is forced
                    forced = {-lit}
                    if not _propagate(self.clauses, forced):
                        self.unsat = True       # Both polarities fail
                        return self.stats.failed_literals - fixed_before
                    self._fix(forced)
                    changed = True
                    break
                if self.unsat:
                    break
        return self.stats.failed_literals - fixed_before

    def _fix(self, literals: Set[int]):
        """Record literals as true and simplify the formula with them."""
        for lit in sorted(literals, key=abs):
            self.reconstruction.append(('unit', lit))
            self.stats.failed_literals += 1

        remaining = []
        for clause in self.clauses:
            if any(lit in literals for lit in clause):
                continue                    # Satisfied
            reduced = tuple(lit for lit in clause if -lit not in literals)
            if not reduced:
                self.unsat = True
            remaining.append(reduced)
        self.clauses = self._unique(remaining)

    # ------------------------------------------------------------------
    # Blocked clause elimination
    # ------------------------------------------------------------------

    def eliminate_blocked_clauses(self) -> int:
        """
        Remove blocked clauses.

        A clause C is blocked on one of its literals `l` if every resolvent of
        C with a clause containing `¬l` is a tautology. Such a clause can be
        removed: any solution of the rest that falsifies C can be repaired by
        flipping `l` to true, and the tautology condition guarantees that this
        breaks no other clause. A clause containing a pure literal is the
        simplest case (there is nothing to resolve with).

        Returns:
            Number of clauses removed
        """
        removed = 0
        changed = True
        while changed and not self.unsat:
            changed = False
            for clause in list(self.clauses):
                lit = self._blocking_literal(clause)
                if lit is not None:
                    self.clauses.remove(clause)
                    self.reconstruction.append(('blocked', lit, clause))
                    removed += 1
                    changed = True      # Removing a clause can block others
        self.stats.blocked_clauses += removed
        return removed

    def _blocking_literal(self, clause: IntClause) -> Optional[int]:
        """Return a literal on which the clause is blocked, or None."""
        literals = set(clause)
        for lit in clause:
            blocked = True
            for other in self.clauses:
                if -lit not in other:
                    continue
                # The resolvent on `lit` is a tautology if the two clauses
                # clash on some second variable as well
                if not any(-q in literals for q in other if q != -lit):
                    blocked = False
                    break
            if blocked:
                return lit
        return None

    # ------------------------------------------------------------------
    # Bounded variable elimination
    # ------------------------------------------------------------------

    def eliminate_variables(self, max_occurrences: int = 10, max_growth: int = 0) -> int:
        """
        Eliminate variables by resolution where that does not grow the formula.

        Eliminating x replaces all clauses containing x or ¬x by all their
        non-tautological resolvents on x. Unrestricted, this is the 1960
        Davis-Putnam procedure and blows up exponentially. Bounded variable
        elimination (SatELite, Eén & Biere 2005) performs the step only when
        the number of clauses does not increase by more than `max_growth`.

        Args:
            max_occurrences: Skip variables occurring more often than this in
                either polarity (keeps the work bounded)
            max_growth: Allowed increase in the number of clauses per elimination

        Returns:
            Number of variables eliminated
        """
        eliminated = 0
        changed = True
        while changed and not self.unsat:
            changed = False
            for var in sorted(self._variables()):
                positive = [c for c in self.clauses if var in c]
                negative = [c for c in self.clauses if -var in c]
                if not positive and not negative:
                    continue                # Already gone with an earlier elimination
                if len(positive) > max_occurrences or len(negative) > max_occurrences:
                    continue

                resolvents = set()
                for p in positive:
                    for n in negative:
                        merged = set(p) | set(n)
                        merged.discard(var)
                        merged.discard(-var)
                        if any(-lit in merged for lit in merged):
                            continue        # Tautological resolvent
                        resolvents.add(tuple(sorted(merged)))

                if len(resolvents) > len(positive) + len(negative) + max_growth:
                    continue

                removed = positive + negative
                removed_set = set(removed)
                self.reconstruction.append(('eliminated', var, removed))
                self.clauses = self._unique(
                    [c for c in self.clauses if c not in removed_set] + sorted(resolvents)
                )
                eliminated += 1
                changed = True
                if () in resolvents:
                    self.unsat = True       # Resolving (x) with (¬x)
                    break
        self.stats.eliminated_variables += eliminated
        return eliminated

    # ------------------------------------------------------------------
    # Vivification
    # ------------------------------------------------------------------

    def vivify(self) -> int:
        """
        Shorten clauses using unit propagation.

        For a clause (l1 ∨ l2 ∨ ... ∨ lk), assume ¬l1, ¬l2, ... one at a time
        and propagate through the rest of the formula:

        - If a conflict occurs, the literals assumed so far already form an
          implied clause, and the remaining ones can be dropped.
        - If a later literal lj becomes true, the clause (assumed ∨ lj) is
          implied, and the others can be dropped.
        - If a later literal lj becomes false, lj itself is redundant.

        The result is equivalent to the original formula, so no reconstruction
        is needed.

        Returns:
            Number of clauses shortened
        """
        shortened = 0
        position = 0
        while position < len(self.clauses) and not self.unsat:
            clause = self.clauses[position]
            if len(clause) < 2:
                position += 1
                continue

            others = self.clauses[:position] + self.clauses[position + 1:]
            assigned: Set[int] = set()
            kept: List[int] = []
            for lit in clause:
                if lit in assigned:
                    kept.append(lit)        # Implied by the negated prefix
                    break
                if -lit in assigned:
                    continue                # Redundant literal
                kept.append(lit)
                assigned.add(-lit)
                if not _propagate(others, assigned):
                    break                   # The prefix alone is an implied clause

            if len(kept) < len(clause):
                shortened += 1
                self.stats.vivified_literals += len(clause) - len(kept)
                self.clauses[position] = tuple(sorted(kept))
                self.clauses = self._unique(self.clauses)
                # Do not advance: the shorter clause (or whatever now sits at
                # this position) may be shortened further
                continue
            position += 1
        self.stats.vivified_clauses += shortened
        return shortened

    # ------------------------------------------------------------------
    # Results
    # ------------------------------------------------------------------

    def _variables(self) -> Set[int]:
        return {abs(lit) for clause in self.clauses for lit in clause}

    @staticmethod
    def _unique(clauses: List[IntClause]) -> List[IntClause]:
        """Remove duplicate clauses, keeping the first occurrence of each."""
        seen: Set[IntClause] = set()
        result = []
        for clause in clauses:
            if clause not in seen:
                seen.add(clause)
                result.append(clause)
        return result

    def _to_cnf(self) -> CNFExpression:
        if self.unsat:
            return CNFExpression([Clause([])])
        return CNFExpression([
            Clause([Literal(self.names[abs(lit) - 1], lit < 0) for lit in clause])
            for clause in self.clauses
        ])

    def extend_model(self, model: Dict[str, bool]) -> Dict[str, bool]:
        """
        Turn a solution of the simplified formula into a solution of the original.

        The reconstruction stack is replayed in reverse. Undoing one step turns
        a solution of the formula after that step into a solution of the
        formula before it, so at the end the assignment satisfies the original.

        Args:
            model: Satisfying assignment of the simplified formula

        Returns:
            Satisfying assignment of the original formula
        """
        # Variables that disappeared entirely start out False
        value = {i + 1: model.get(name, False) for i, name in enumerate(self.names)}

        def is_true(lit: int) -> bool:
            return value[abs(lit)] == (lit > 0)

        for step in reversed(self.reconstruction):
            kind = step[0]
            if kind == 'unit':
                lit = step[1]
                value[abs(lit)] = lit > 0
            elif kind == 'blocked':
                _, lit, clause = step
                if not any(is_true(q) for q in clause):
                    value[abs(lit)] = lit > 0
            else:
                _, var, clauses = step
                # x must be true exactly if some clause (x ∨ A) has A false.
                # The resolvents guarantee that every (¬x ∨ B) then has B true.
                value[var] = any(
                    var in clause and not any(is_true(q) for q in clause if q != var)
                    for clause in clauses
                )

        return {name: value[i + 1] for i, name in enumerate(self.names)}

    def result(self) -> SimplificationResult:
        """Return the simplified formula together with statistics and model extension."""
        cnf = self._to_cnf()
        self.stats.final_vars = len(cnf.get_variables())
        self.stats.final_clauses = 0 if self.unsat else len(self.clauses)
        return SimplificationResult(cnf, self.unsat, self.stats, self.extend_model)


def simplify_cnf(cnf: CNFExpression,
                 probing: bool = True,
                 blocked_clauses: bool = True,
                 variable_elimination: bool = True,
                 vivification: bool = True,
                 max_occurrences: int = 10) -> SimplificationResult:
    """
    Simplify a CNF formula with the selected techniques.

    Args:
        cnf: Formula to simplify
        probing: Apply failed literal probing
        blocked_clauses: Apply blocked clause elimination
        variable_elimination: Apply bounded variable elimination
        vivification: Apply vivification
        max_occurrences: Occurrence limit for variable elimination

    Returns:
        SimplificationResult; use `result.extend_model(model)` to map a solution
        of `result.cnf` back to the original formula

    Example:
        >>> from bsat import CNFExpression, simplify_cnf, solve_cdcl
        >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")
        >>> result = simplify_cnf(cnf)
        >>> model = solve_cdcl(result.cnf)
        >>> full_model = result.extend_model(model)
    """
    simplifier = Simplifier(cnf)
    if probing:
        simplifier.probe_failed_literals()
    if vivification:
        simplifier.vivify()
    if blocked_clauses:
        simplifier.eliminate_blocked_clauses()
    if variable_elimination:
        simplifier.eliminate_variables(max_occurrences=max_occurrences)
    return simplifier.result()


def solve_with_simplification(cnf: CNFExpression,
                              solver: Optional[Callable[[CNFExpression],
                                                        Optional[Dict[str, bool]]]] = None,
                              **options) -> Optional[Dict[str, bool]]:
    """
    Simplify a formula, solve what is left, and reconstruct a full solution.

    Args:
        cnf: Formula to solve
        solver: Function used on the simplified formula (default: solve_modern_cdcl)
        **options: Passed to simplify_cnf()

    Returns:
        Satisfying assignment of the original formula, or None if UNSAT

    Example:
        >>> from bsat import CNFExpression, solve_with_simplification
        >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")
        >>> solve_with_simplification(cnf)
    """
    if solver is None:
        from .cdcl_modern import solve_modern_cdcl
        solver = solve_modern_cdcl

    result = simplify_cnf(cnf, **options)
    if result.is_unsat:
        return None
    model = solver(result.cnf)
    if model is None:
        return None
    return result.extend_model(model)
