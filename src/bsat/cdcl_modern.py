"""
Modern CDCL: the techniques that turn textbook CDCL into a practical solver.

`bsat.cdcl.CDCLSolver` shows the core idea of conflict-driven clause learning
in as little code as possible. This module adds the engineering that every
competitive solver since Chaff (2001), MiniSat (2003) and Glucose (2009) is
built on, written to be read:

- Two-watched literals with blocking literals (fast unit propagation)
- First-UIP learning followed by recursive learned-clause minimization
- VSIDS with exponentially growing bumps and a priority queue
- Phase saving (and optional random phases)
- LBD ("glue") scoring of learned clauses and periodic clause-database reduction
- Luby or Glucose-style dynamic restarts

Clause minimization, phase saving, database reduction and restarts can each be
switched off, so their effect can be measured by comparing statistics on the
same formula. (Watched literals, first-UIP learning and VSIDS are always on.)

Internally variables are numbered 1..n and a literal is a non-zero integer:
+v for the variable, -v for its negation (the DIMACS convention). This is the
first optimization any serious solver makes, because integer comparison and
list indexing replace object hashing in the innermost loops.

The same techniques are implemented in C, with many more on top, in
`competition/c/`.
"""

import heapq
import random
from collections import deque
from typing import Deque, Dict, List, Optional, Tuple

from .cnf import CNFExpression


class ModernCDCLStats:
    """Statistics for a ModernCDCLSolver run."""

    def __init__(self):
        self.status = "UNKNOWN"        # "SAT", "UNSAT" or "UNKNOWN" (conflict limit reached)
        self.decisions = 0
        self.propagations = 0          # Literals assigned by unit propagation
        self.clause_visits = 0         # Clauses actually inspected during propagation
        self.blocker_skips = 0         # Watchers skipped because the blocking literal was true
        self.conflicts = 0
        self.learned_clauses = 0
        self.learned_literals = 0      # Total size of learned clauses after minimization
        self.minimized_literals = 0    # Literals removed by clause minimization
        self.glue_clauses = 0          # Learned clauses with LBD <= glue_limit (default 2)
        self.deleted_clauses = 0       # Learned clauses removed by database reduction
        self.reductions = 0            # Number of database reductions
        self.restarts = 0
        self.max_decision_level = 0

    def __str__(self):
        return (
            f"ModernCDCLStats(\n"
            f"  Status: {self.status}\n"
            f"  Decisions: {self.decisions}\n"
            f"  Propagations: {self.propagations}\n"
            f"  Clause visits: {self.clause_visits}\n"
            f"  Blocker skips: {self.blocker_skips}\n"
            f"  Conflicts: {self.conflicts}\n"
            f"  Learned clauses: {self.learned_clauses}\n"
            f"  Learned literals: {self.learned_literals}\n"
            f"  Minimized literals: {self.minimized_literals}\n"
            f"  Glue clauses: {self.glue_clauses}\n"
            f"  Deleted clauses: {self.deleted_clauses}\n"
            f"  Restarts: {self.restarts}\n"
            f"  Max decision level: {self.max_decision_level}\n"
            f")"
        )


class _Clause:
    """
    A clause in the solver's database.

    The first two literals of `lits` are the watched ones. For a clause that is
    the reason for an assignment, the assigned (true) literal is `lits[0]`.
    """

    __slots__ = ("lits", "learned", "lbd", "deleted")

    def __init__(self, lits: List[int], learned: bool = False, lbd: int = 0):
        self.lits = lits
        self.learned = learned
        self.lbd = lbd
        self.deleted = False


class ModernCDCLSolver:
    """
    CDCL solver with the standard modern techniques, several individually switchable.

    Example:
        >>> solver = ModernCDCLSolver(cnf)
        >>> result = solver.solve()
        >>> print(solver.stats)

        Measure what one technique contributes:

        >>> without = ModernCDCLSolver(cnf, minimize_learned=False)
        >>> without.solve()
        >>> print(without.stats.learned_literals)
    """

    RESTART_STRATEGIES = ("glucose", "luby", "none")

    def __init__(self, cnf: CNFExpression,
                 restart_strategy: str = "glucose",
                 restart_base: int = 100,
                 glucose_window: int = 50,
                 glucose_k: float = 0.8,
                 phase_saving: bool = True,
                 initial_phase: bool = False,
                 random_phase_freq: float = 0.0,
                 minimize_learned: bool = True,
                 reduce_db: bool = True,
                 reduce_interval: int = 2000,
                 reduce_increment: int = 300,
                 glue_limit: int = 2,
                 vsids_decay: float = 0.95,
                 seed: Optional[int] = None):
        """
        Initialize the solver.

        Args:
            cnf: CNF formula to solve
            restart_strategy: 'glucose' (restart when recent learned clauses are
                worse than average), 'luby' (fixed Luby sequence) or 'none'
            restart_base: Conflicts per unit of the Luby sequence
            glucose_window: Number of recent conflicts in the short-term LBD average
            glucose_k: Restart when recent_average * glucose_k > overall_average
            phase_saving: Re-use a variable's last value when deciding on it again
            initial_phase: Value tried first for a variable with no saved phase
            random_phase_freq: Probability of deciding a random value instead (0 disables)
            minimize_learned: Remove redundant literals from learned clauses
            reduce_db: Periodically delete the least useful learned clauses
            reduce_interval: Conflicts before the first database reduction
            reduce_increment: Added to the interval after each reduction
            glue_limit: Learned clauses with LBD <= this are never deleted
            vsids_decay: VSIDS decay factor (0.9-0.99 typical)
            seed: Seed for random phases (only used if random_phase_freq > 0)
        """
        if restart_strategy not in self.RESTART_STRATEGIES:
            raise ValueError(
                f"restart_strategy must be one of {self.RESTART_STRATEGIES}, got {restart_strategy!r}"
            )
        if glucose_window < 1:
            raise ValueError(f"glucose_window must be at least 1, got {glucose_window}")

        self.restart_strategy = restart_strategy
        self.restart_base = restart_base
        self.glucose_k = glucose_k
        self.phase_saving = phase_saving
        self.initial_phase = initial_phase
        self.random_phase_freq = random_phase_freq
        self.minimize_learned = minimize_learned
        self.reduce_db = reduce_db
        self.reduce_interval = reduce_interval
        self.reduce_increment = reduce_increment
        self.glue_limit = glue_limit
        self.vsids_decay = vsids_decay
        self.rng = random.Random(seed)
        self.stats = ModernCDCLStats()

        # Variable numbering: names sorted so that runs are reproducible
        self.names: List[str] = sorted(cnf.get_variables())
        self.num_vars = len(self.names)
        index = {name: i + 1 for i, name in enumerate(self.names)}
        n = self.num_vars

        # Per-variable state, indexed by variable number (slot 0 unused)
        self.value: List[int] = [0] * (n + 1)          # +1 true, -1 false, 0 unassigned
        self.level: List[int] = [0] * (n + 1)          # Decision level of the assignment
        self.reason: List[Optional[_Clause]] = [None] * (n + 1)  # None for decisions
        self.saved_phase: List[bool] = [initial_phase] * (n + 1)
        self.activity: List[float] = [0.0] * (n + 1)   # VSIDS score
        self.seen: List[bool] = [False] * (n + 1)      # Scratch marks for conflict analysis

        # Trail: assigned literals in order; trail_lim[d] is where level d+1 starts
        self.trail: List[int] = []
        self.trail_lim: List[int] = []
        self.qhead = 0                                 # Next trail position to propagate

        # Clause database and watch lists.
        # watches[lit] holds (blocker, clause) for every clause watching `lit`.
        self.clauses: List[_Clause] = []
        self.learned: List[_Clause] = []
        self.watches: Dict[int, List[Tuple[int, _Clause]]] = {}
        for v in range(1, n + 1):
            self.watches[v] = []
            self.watches[-v] = []

        # VSIDS: lazy max-heap of (-activity, variable); stale entries are skipped
        self.activity_inc = 1.0
        self.heap: List[Tuple[float, int]] = [(0.0, v) for v in range(1, n + 1)]

        # Restart state
        self.conflicts_since_restart = 0
        self.luby_index = 1
        self.recent_lbds: Deque[int] = deque(maxlen=glucose_window)
        self.lbd_sum = 0

        # Database reduction schedule
        self.next_reduce = reduce_interval

        # Load the formula. `ok` becomes False if it is trivially unsatisfiable.
        self.ok = True
        for clause in cnf.clauses:
            lits = []
            for literal in clause.literals:
                v = index[literal.variable]
                lits.append(-v if literal.negated else v)
            self._add_original_clause(lits)

    # ------------------------------------------------------------------
    # Basic assignment handling
    # ------------------------------------------------------------------

    def _val(self, lit: int) -> int:
        """Value of a literal: +1 true, -1 false, 0 unassigned."""
        return self.value[lit] if lit > 0 else -self.value[-lit]

    @property
    def decision_level(self) -> int:
        return len(self.trail_lim)

    def _enqueue(self, lit: int, reason: Optional[_Clause]):
        """Make `lit` true at the current decision level."""
        v = abs(lit)
        self.value[v] = 1 if lit > 0 else -1
        self.level[v] = self.decision_level
        self.reason[v] = reason
        self.trail.append(lit)
        if reason is not None:
            self.stats.propagations += 1

    def _backtrack(self, level: int):
        """Undo all assignments above `level`."""
        if self.decision_level <= level:
            return
        start = self.trail_lim[level]
        for lit in self.trail[start:]:
            v = abs(lit)
            if self.phase_saving:
                # Phase saving: remember the value so the next decision on v
                # re-enters the part of the search space we were just in
                self.saved_phase[v] = lit > 0
            self.value[v] = 0
            self.reason[v] = None
            heapq.heappush(self.heap, (-self.activity[v], v))
        del self.trail[start:]
        del self.trail_lim[level:]
        self.qhead = len(self.trail)

    # ------------------------------------------------------------------
    # Clause database
    # ------------------------------------------------------------------

    def _attach(self, clause: _Clause):
        """Watch the first two literals of a clause; each is the other's blocker."""
        lits = clause.lits
        self.watches[lits[0]].append((lits[1], clause))
        self.watches[lits[1]].append((lits[0], clause))

    def _add_original_clause(self, lits: List[int]):
        """Add an input clause, handling duplicates, tautologies and units."""
        unique = []
        present = set()
        for lit in lits:
            if -lit in present:
                return                      # Tautology: always satisfied
            if lit not in present:
                present.add(lit)
                unique.append(lit)

        if not unique:
            self.ok = False                 # Empty clause
        elif len(unique) == 1:
            value = self._val(unique[0])
            if value == -1:
                self.ok = False             # Contradicts an earlier unit clause
            elif value == 0:
                self._enqueue(unique[0], None)
        else:
            clause = _Clause(unique)
            self.clauses.append(clause)
            self._attach(clause)

    # ------------------------------------------------------------------
    # Unit propagation with two watched literals
    # ------------------------------------------------------------------

    def _propagate(self) -> Optional[_Clause]:
        """
        Propagate all pending assignments. Returns a conflicting clause or None.

        Invariant: in every clause, the two watched literals (lits[0], lits[1])
        are not false, unless the clause is satisfied or unit. A clause therefore
        only needs attention when one of its two watched literals becomes false,
        and nothing has to be undone when the solver backtracks.
        """
        value = self.value
        watches = self.watches
        stats = self.stats

        while self.qhead < len(self.trail):
            false_lit = -self.trail[self.qhead]
            self.qhead += 1

            watchers = watches[false_lit]
            kept: List[Tuple[int, _Clause]] = []
            i = 0
            count = len(watchers)
            while i < count:
                watcher = watchers[i]
                i += 1
                blocker, clause = watcher
                if clause.deleted:
                    continue                # Dropped lazily after database reduction

                # Blocking literal: if some literal of the clause is known to be
                # true, the clause is satisfied and need not even be looked at.
                if (value[blocker] if blocker > 0 else -value[-blocker]) == 1:
                    kept.append(watcher)
                    stats.blocker_skips += 1
                    continue

                stats.clause_visits += 1
                lits = clause.lits
                # Put the false watched literal in position 1
                if lits[0] == false_lit:
                    lits[0], lits[1] = lits[1], lits[0]
                first = lits[0]
                first_value = value[first] if first > 0 else -value[-first]
                if first_value == 1:
                    kept.append((first, clause))    # Satisfied by the other watch
                    continue

                # Look for a non-false literal to watch instead
                for k in range(2, len(lits)):
                    other = lits[k]
                    if (value[other] if other > 0 else -value[-other]) != -1:
                        lits[1], lits[k] = other, false_lit
                        watches[other].append((first, clause))
                        break
                else:
                    # Every other literal is false: the clause is unit or conflicting
                    kept.append((first, clause))
                    if first_value == -1:
                        kept.extend(watchers[i:])
                        watches[false_lit] = kept
                        self.qhead = len(self.trail)
                        return clause
                    self._enqueue(first, clause)

            watches[false_lit] = kept

        return None

    # ------------------------------------------------------------------
    # Conflict analysis
    # ------------------------------------------------------------------

    def _analyze(self, conflict: _Clause) -> Tuple[List[int], int, int]:
        """
        Derive a learned clause from a conflict using the first-UIP scheme.

        Starting from the conflicting clause, literals assigned at the current
        decision level are replaced by their reasons (resolution), walking the
        trail backwards, until exactly one current-level literal remains. That
        literal is the first unique implication point (UIP).

        Returns:
            (learned clause with the asserting literal first, backjump level, LBD)
        """
        seen = self.seen
        level = self.level
        current = self.decision_level

        learned = [0]                       # Slot 0 is filled with the asserting literal
        pending = 0                         # Current-level literals still to resolve
        lit = 0
        index = len(self.trail) - 1
        clause = conflict

        while True:
            for q in clause.lits:
                if q == lit:
                    continue                # The literal this reason clause implied
                v = abs(q)
                if not seen[v] and level[v] > 0:
                    seen[v] = True
                    self._bump(v)
                    if level[v] == current:
                        pending += 1
                    else:
                        learned.append(q)
            # Next marked literal on the trail
            while not seen[abs(self.trail[index])]:
                index -= 1
            lit = self.trail[index]
            index -= 1
            seen[abs(lit)] = False
            pending -= 1
            if pending == 0:
                break
            clause = self.reason[abs(lit)]

        learned[0] = -lit

        to_clear = [abs(q) for q in learned[1:]]
        if self.minimize_learned and len(learned) > 1:
            before = len(learned)
            learned = self._minimize(learned, to_clear)
            self.stats.minimized_literals += before - len(learned)
        for v in to_clear:
            seen[v] = False

        # Backjump to the second-highest level in the clause, and move a literal
        # of that level to position 1 so that it can be watched.
        backjump = 0
        for k in range(1, len(learned)):
            if level[abs(learned[k])] > backjump:
                backjump = level[abs(learned[k])]
                learned[1], learned[k] = learned[k], learned[1]

        # LBD (literal block distance): number of distinct decision levels.
        # A low LBD means the clause links few "blocks" of propagation, which
        # is the best known predictor of how useful a learned clause will be.
        lbd = len({level[abs(q)] for q in learned})
        return learned, backjump, lbd

    def _minimize(self, learned: List[int], to_clear: List[int]) -> List[int]:
        """
        Recursive clause minimization (Sörensson & Biere 2009).

        A literal is redundant if it is implied by the other literals of the
        clause: that is, if following its reason clause backwards always ends
        in literals that are already in the clause (or fixed at level 0).
        """
        levels = {self.level[abs(q)] for q in learned[1:]}
        kept = [learned[0]]
        for q in learned[1:]:
            if self.reason[abs(q)] is None or not self._is_redundant(q, levels, to_clear):
                kept.append(q)
        return kept

    def _is_redundant(self, lit: int, levels: set, to_clear: List[int]) -> bool:
        """Check whether `lit` is implied by the marked literals of the clause."""
        seen = self.seen
        mark = len(to_clear)
        stack = [lit]
        while stack:
            p = stack.pop()
            for q in self.reason[abs(p)].lits:
                v = abs(q)
                if v == abs(p) or seen[v] or self.level[v] == 0:
                    continue
                # A decision, or a level the clause does not mention, can never
                # be traced back into the clause
                if self.reason[v] is None or self.level[v] not in levels:
                    for u in to_clear[mark:]:
                        seen[u] = False
                    del to_clear[mark:]
                    return False
                seen[v] = True              # Known to be implied: no need to re-check
                to_clear.append(v)
                stack.append(q)
        return True

    def _add_learned(self, lits: List[int], lbd: int):
        """Store a learned clause and assert its first literal."""
        self.stats.learned_clauses += 1
        self.stats.learned_literals += len(lits)
        if lbd <= self.glue_limit:
            self.stats.glue_clauses += 1

        if len(lits) == 1:
            self._enqueue(lits[0], None)    # A learned fact, true at level 0
            return
        clause = _Clause(lits, learned=True, lbd=lbd)
        self.learned.append(clause)
        self._attach(clause)
        self._enqueue(lits[0], clause)

    # ------------------------------------------------------------------
    # VSIDS decision heuristic
    # ------------------------------------------------------------------

    def _bump(self, v: int):
        """Increase the activity of a variable involved in the current conflict."""
        self.activity[v] += self.activity_inc
        if self.activity[v] > 1e100:
            # Rescale everything to avoid floating-point overflow
            for u in range(1, self.num_vars + 1):
                self.activity[u] *= 1e-100
            self.activity_inc *= 1e-100
            self._rebuild_heap()
        elif self.value[v] == 0:
            heapq.heappush(self.heap, (-self.activity[v], v))

    def _decay(self):
        """
        Decay all activities. Rather than multiplying every score by the decay
        factor, the bump amount grows instead, which has the same effect on the
        ordering at constant cost.
        """
        self.activity_inc /= self.vsids_decay

    def _rebuild_heap(self):
        self.heap = [(-self.activity[v], v)
                     for v in range(1, self.num_vars + 1) if self.value[v] == 0]
        heapq.heapify(self.heap)

    def _pick_branch_variable(self) -> int:
        """Return the unassigned variable with the highest activity, or 0 if none."""
        if len(self.heap) > 4 * self.num_vars + 1000:
            self._rebuild_heap()            # Too many stale entries
        while self.heap:
            negated_activity, v = heapq.heappop(self.heap)
            if self.value[v] == 0 and -negated_activity == self.activity[v]:
                return v
        return 0

    def _pick_phase(self, v: int) -> bool:
        if self.random_phase_freq > 0 and self.rng.random() < self.random_phase_freq:
            return self.rng.random() < 0.5
        return self.saved_phase[v] if self.phase_saving else self.initial_phase

    # ------------------------------------------------------------------
    # Restarts
    # ------------------------------------------------------------------

    @staticmethod
    def _luby(i: int) -> int:
        """The i-th element (1-based) of the Luby sequence 1,1,2,1,1,2,4,..."""
        k = 1
        while (1 << k) - 1 < i:
            k += 1
        if i == (1 << k) - 1:
            return 1 << (k - 1)
        return ModernCDCLSolver._luby(i - (1 << (k - 1)) + 1)

    def _should_restart(self) -> bool:
        if self.restart_strategy == "luby":
            return self.conflicts_since_restart >= self._luby(self.luby_index) * self.restart_base
        if self.restart_strategy == "glucose":
            # Restart when the clauses learned recently are clearly worse
            # (higher LBD) than the average over the whole run
            if len(self.recent_lbds) < self.recent_lbds.maxlen:
                return False
            recent = sum(self.recent_lbds) / len(self.recent_lbds)
            overall = self.lbd_sum / self.stats.conflicts
            return recent * self.glucose_k > overall
        return False

    def _restart(self):
        self.stats.restarts += 1
        self.conflicts_since_restart = 0
        self.luby_index += 1
        self.recent_lbds.clear()
        self._backtrack(0)

    # ------------------------------------------------------------------
    # Learned clause database reduction
    # ------------------------------------------------------------------

    def _reduce_database(self):
        """
        Delete the less useful half of the deletable learned clauses.

        Glue clauses and clauses that are currently the reason for an
        assignment are always kept. The rest are ranked by LBD (then by size)
        and the worse half is deleted.
        """
        self.stats.reductions += 1
        candidates = []
        for clause in self.learned:
            locked = self.reason[abs(clause.lits[0])] is clause
            if clause.lbd > self.glue_limit and not locked:
                candidates.append(clause)

        candidates.sort(key=lambda c: (c.lbd, len(c.lits)), reverse=True)
        for clause in candidates[:len(candidates) // 2]:
            clause.deleted = True           # Watch lists drop it lazily
            self.stats.deleted_clauses += 1

        self.learned = [c for c in self.learned if not c.deleted]

    # ------------------------------------------------------------------
    # Main loop
    # ------------------------------------------------------------------

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve the formula.

        Args:
            max_conflicts: Give up after this many conflicts

        Returns:
            A satisfying assignment, or None if the formula is unsatisfiable or
            the conflict limit was reached. `stats.status` tells the two apart.
        """
        stats = self.stats
        if not self.ok:
            stats.status = "UNSAT"
            return None

        while True:
            conflict = self._propagate()

            if conflict is not None:
                stats.conflicts += 1
                self.conflicts_since_restart += 1
                if self.decision_level == 0:
                    self.ok = False
                    stats.status = "UNSAT"
                    return None

                learned, backjump, lbd = self._analyze(conflict)
                self.recent_lbds.append(lbd)
                self.lbd_sum += lbd
                self._backtrack(backjump)
                self._add_learned(learned, lbd)
                self._decay()

                if stats.conflicts >= max_conflicts:
                    stats.status = "UNKNOWN"
                    return None
                continue

            if self.decision_level > 0 and self._should_restart():
                self._restart()
                continue

            if self.reduce_db and stats.conflicts >= self.next_reduce:
                self._reduce_database()
                # The interval grows, so the database is allowed to grow too
                self.next_reduce = (stats.conflicts + self.reduce_interval
                                    + self.reduce_increment * stats.reductions)

            v = self._pick_branch_variable()
            if v == 0:
                stats.status = "SAT"
                return {name: self.value[i + 1] == 1 for i, name in enumerate(self.names)}

            stats.decisions += 1
            self.trail_lim.append(len(self.trail))
            if self.decision_level > stats.max_decision_level:
                stats.max_decision_level = self.decision_level
            self._enqueue(v if self._pick_phase(v) else -v, None)

    def get_stats(self) -> ModernCDCLStats:
        """Get solver statistics."""
        return self.stats


def solve_modern_cdcl(cnf: CNFExpression,
                      max_conflicts: int = 1000000,
                      **options) -> Optional[Dict[str, bool]]:
    """
    Solve a CNF formula with the modern CDCL solver.

    Args:
        cnf: CNF formula to solve
        max_conflicts: Maximum conflicts before giving up
        **options: Any ModernCDCLSolver option (restart_strategy, phase_saving, ...)

    Returns:
        Dictionary mapping variables to values if SAT; None if UNSAT or if the
        conflict limit was reached (get_modern_cdcl_stats() and `stats.status`
        tell these apart)

    Example:
        >>> from bsat import CNFExpression, solve_modern_cdcl
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z) & (~y | ~z)")
        >>> result = solve_modern_cdcl(cnf)
    """
    return ModernCDCLSolver(cnf, **options).solve(max_conflicts=max_conflicts)


def get_modern_cdcl_stats(cnf: CNFExpression,
                          max_conflicts: int = 1000000,
                          **options) -> Tuple[Optional[Dict[str, bool]], ModernCDCLStats]:
    """
    Solve with the modern CDCL solver and return both solution and statistics.

    Args:
        cnf: CNF formula to solve
        max_conflicts: Maximum conflicts before giving up
        **options: Any ModernCDCLSolver option

    Returns:
        Tuple of (solution, statistics)

    Example:
        >>> solution, stats = get_modern_cdcl_stats(cnf, restart_strategy='luby')
        >>> print(stats)
    """
    solver = ModernCDCLSolver(cnf, **options)
    solution = solver.solve(max_conflicts=max_conflicts)
    return solution, solver.get_stats()
