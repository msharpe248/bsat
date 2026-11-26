# Two-Watched Literals

Two-watched literals is a data structure for unit propagation. Instead of examining every clause after every assignment, the solver examines only the clauses in which one of two designated ("watched") literals has just become false. It was introduced by the Chaff solver in 2001 and has been used by essentially every CDCL solver since.

> **Implemented in** [`src/bsat/cdcl_modern.py`](../src/bsat/cdcl_modern.py): `ModernCDCLSolver._propagate` and `_attach`, the `watches` dictionary, and the literal order inside `_Clause.lits`.
> **Options:** none; this technique cannot be switched off.
> **Statistics:** `clause_visits`, `blocker_skips`, `propagations`.
> Part of the [Modern CDCL solver](modern-cdcl.md).

---

## The Problem

Unit propagation is where a CDCL solver spends most of its time. The simplest implementation, used by the basic [`CDCLSolver`](cdcl-solver.md) in `src/bsat/cdcl.py`, loops over every clause, evaluates every literal, and repeats until nothing changes. The cost of each pass is proportional to the size of the whole formula, including every learned clause, even though a single assignment affects very few clauses.

## The Idea

A clause needs attention in only two situations:

- **It is unit:** all literals but one are false, and the remaining one is unassigned. That literal must be made true.
- **It is conflicting:** all literals are false.

Both situations require all but at most one literal to be false. So as long as a clause has **two literals that are not false**, it is neither unit nor conflicting, and it does not matter what happens to its other literals.

The solver therefore picks two literals in each clause to watch and maintains this invariant:

> In every clause, the two watched literals are not false, unless the clause is satisfied or unit.

When a literal becomes false, only the clauses watching that literal are examined. For each one the solver tries to restore the invariant by watching a different, non-false literal. If there is none, the clause is unit or conflicting.

A clause with 20 literals is visited when one of its 2 watched literals becomes false, and ignored when any of the other 18 do.

## Data Structures

A literal is a non-zero integer: `+v` for variable `v`, `-v` for its negation.

A clause is a `_Clause` object whose `lits` list holds the literals. No separate record of the watches is kept: **the watched literals are simply `lits[0]` and `lits[1]`**, and changing a watch means swapping literals inside the list.

```python
class _Clause:
    __slots__ = ("lits", "learned", "lbd", "deleted")
```

`watches` maps each literal to the clauses watching it. Each entry is a `(blocker, clause)` pair; the blocker is explained [below](#blocking-literals).

```python
self.watches: Dict[int, List[Tuple[int, _Clause]]] = {}
for v in range(1, n + 1):
    self.watches[v] = []
    self.watches[-v] = []
```

A clause is registered by `_attach`, which watches its first two literals and makes each the other's blocker:

```python
def _attach(self, clause: _Clause):
    lits = clause.lits
    self.watches[lits[0]].append((lits[1], clause))
    self.watches[lits[1]].append((lits[0], clause))
```

Only clauses with at least two literals are stored. A unit clause of the input is assigned directly at decision level 0 when the formula is loaded, and a learned unit clause is assigned at level 0 after backjumping; neither needs watches.

## Propagation

Assignments are appended to `trail`, and `qhead` is the position of the first trail literal that has not been propagated yet. `_propagate` processes them in order. For each trail literal, its negation `false_lit` has just become false, so the watch list of `false_lit` is walked:

```python
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
```

(`value`, `watches` and `stats` are local aliases of the solver attributes. The expression `value[x] if x > 0 else -value[-x]` is the value of literal `x`: `1` true, `-1` false, `0` unassigned.)

For each clause watching `false_lit` there are five possible outcomes:

| Case | Condition | Action |
|---|---|---|
| Deleted | `clause.deleted` | Drop the entry from the watch list. |
| Blocker true | The cached blocking literal is true | Keep the entry. The clause is not touched. |
| Other watch true | `lits[0]` is true | Keep the entry, with `lits[0]` as the new blocker. |
| New watch found | Some `lits[k]`, `k >= 2`, is not false | Swap it into position 1 and add the clause to that literal's watch list. The entry is not kept in this list. |
| No new watch | All of `lits[1:]` are false | If `lits[0]` is unassigned the clause is unit: assign `lits[0]` with this clause as its reason. If `lits[0]` is false the clause is conflicting: return it. |

Three details of the code are worth noticing:

- The watch list is rebuilt into `kept` rather than edited in place, so removing an entry costs nothing.
- When a conflict is found, the entries not yet examined (`watchers[i:]`) are copied into `kept` so that no watch is lost, and `qhead` is moved to the end of the trail because the remaining assignments are about to be undone.
- A reason clause always has its implied (true) literal in `lits[0]`. Database reduction relies on this to recognise clauses that are currently in use.

### A Trace

This script follows the single clause `(a ∨ b ∨ c ∨ d)` through three decisions. It drives the solver's internal methods directly, which is useful for experiments but is not part of the public API.

```python
from bsat import CNFExpression, ModernCDCLSolver

solver = ModernCDCLSolver(CNFExpression.parse("(a | b | c | d)"))
clause = solver.clauses[0]

def name(lit):
    return ("" if lit > 0 else "~") + solver.names[abs(lit) - 1]

def decide(lit):
    solver.trail_lim.append(len(solver.trail))   # open a new decision level
    solver._enqueue(lit, None)
    solver._propagate()

def show(event):
    lits = [name(lit) for lit in clause.lits]
    print(f"{event:<14} watched: {lits[0]}, {lits[1]}   others: {', '.join(lits[2:])}"
          f"   visits: {solver.stats.clause_visits}")

a, b, c, d = 1, 2, 3, 4          # variables are numbered in sorted name order
show("start")
decide(-a); show("a = False")
decide(-d); show("d = False")
decide(-b); show("b = False")
print("trail:", [name(lit) for lit in solver.trail])
solver._backtrack(0); show("backtrack")
```

Output:

```
start          watched: a, b   others: c, d   visits: 0
a = False      watched: b, c   others: a, d   visits: 1
d = False      watched: b, c   others: a, d   visits: 1
b = False      watched: c, b   others: a, d   visits: 2
trail: ['~a', '~d', '~b', 'c']
backtrack      watched: c, b   others: a, d   visits: 2
```

Step by step:

1. **`a = False`.** `a` is watched, so the clause is visited. `c` is not false, so it replaces `a` as a watch.
2. **`d = False`.** `d` is not watched. The clause is not visited at all (`visits` stays at 1), although one of its literals changed.
3. **`b = False`.** `b` is watched. The other watch `c` is unassigned and the remaining literals `a` and `d` are false, so the clause is unit and `c` is propagated: it appears on the trail.
4. **Backtrack.** The watches stay exactly where they were.

## Nothing to Undo on Backtracking

Step 4 is the second half of what makes the scheme efficient. `_backtrack` unassigns variables and does not touch any watch list.

This is safe because of the order of assignments. `_propagate` leaves a false literal watched in only two situations: every non-watched literal of the clause was already false (the clause is unit or conflicting), or the clause was already satisfied by its other watch or its blocker. In both, whatever justifies keeping the false watch was assigned *before* the watch became false. Backtracking undoes assignments in reverse order, so the false watch is always unassigned no later than the assignments that justified it. The invariant therefore still holds after any backtrack. In the trace, the clause ends up watching `c` and `b`, both unassigned, without any work.

The same reasoning fixes the literal order of a learned clause. `_analyze` returns it with the asserting literal in position 0 and a literal of the backjump level (the highest level among the rest) in position 1. After the backjump these are the unassigned literal and the most recently falsified one, which are the right two to watch.

## Blocking Literals

Visiting a clause means following a reference to its literal list, which in a compiled solver is usually a cache miss. Very often the visit only discovers that the clause is already satisfied. A **blocking literal** avoids many of these visits: each watch-list entry carries a copy of one other literal of the clause, and if that literal is true the clause is satisfied and is skipped without being touched.

```python
if (value[blocker] if blocker > 0 else -value[-blocker]) == 1:
    kept.append(watcher)
    stats.blocker_skips += 1
    continue
```

The blocker is initially the other watched literal. Whenever a clause is visited, the entry that is written back uses `lits[0]` as its blocker. Any literal of the clause would be a correct blocker; the other watch is simply a convenient choice.

In Python the saving per skip is small, since everything is a pointer dereference anyway, but the counters show how often the shortcut applies.

## Deleted Clauses

When [database reduction](lbd-clause-management.md) removes a learned clause, it only sets `clause.deleted = True`. The clause's two watch-list entries are left in place and are dropped the next time `_propagate` walks those lists (the first case in the table above). This avoids searching the watch lists at deletion time.

## Measuring It

```python
from bsat import ModernCDCLSolver
from benchmarks import phase_transition_3sat

cnf = phase_transition_3sat(100, seed=3)
solver = ModernCDCLSolver(cnf)
solver.solve()
stats = solver.stats

assignments = stats.decisions + stats.propagations
clauses = len(solver.clauses) + stats.learned_clauses
print("status:                ", stats.status)
print("assignments:           ", assignments)
print("clauses inspected:     ", stats.clause_visits)
print("blocking-literal skips:", stats.blocker_skips)
print("inspected / assignment:", round(stats.clause_visits / assignments, 1))
print("clauses at the end:    ", clauses)
```

Output (run from the repository root with `PYTHONPATH=src:tests`):

```
status:                 UNSAT
assignments:            13345
clauses inspected:      36497
blocking-literal skips: 17021
inspected / assignment: 2.7
clauses at the end:     857
```

The formula starts with 426 clauses and ends with 857 (no learned clause was deleted in this run). A propagation pass that scans the whole clause list would look at 426 to 857 clauses each time. With watched literals the solver examined about 4 watch-list entries per assignment (36497 + 17021 over 13345 assignments) and inspected 2.7 clauses; about a third of the entries were dismissed by the blocking literal alone.

The gap widens as formulas grow: the cost of a full scan grows with the number of clauses, while the length of a watch list grows only with the number of clauses watching one particular literal.

## Cost

- **Per assignment:** proportional to the length of one watch list, plus the search for a replacement watch in the clauses that are visited. The replacement search is linear in the clause length in the worst case.
- **Per backtrack:** nothing for the watch lists.
- **Memory:** two watch-list entries per clause.

## Not Implemented Here

Real solvers refine the scheme further. These refinements are not in `ModernCDCLSolver`:

- **Special handling of binary clauses.** In a two-literal clause the blocker is the only other literal, so the implication can be made straight from the watch-list entry without visiting the clause. The C solver in `competition/c/` does this; see [competition/c/FEATURES.md](../competition/c/FEATURES.md).
- **Compact memory layout.** Compiled solvers store clauses contiguously in an arena and keep watch lists as flat arrays, because cache behaviour dominates propagation speed. Python lists of objects give no control over layout.
- **Remembering the search position.** Some solvers store where the last replacement watch was found in long clauses, to avoid rescanning false literals.

## References

- Moskewicz, Madigan, Zhao, Zhang, Malik. "Chaff: Engineering an Efficient SAT Solver" (DAC 2001). Introduced two-watched literals.
- Eén, Sörensson. "An Extensible SAT-solver" (SAT 2003). MiniSat; the layout with the watches in the first two positions of the clause.
- Chu, Harwood, Stuckey. "Cache Conscious Data Structures for Boolean Satisfiability Solvers" (JSAT 2009). Blocking literals and the memory behaviour of propagation.

## See Also

- [Modern CDCL solver](modern-cdcl.md)
- [Conflict analysis](conflict-analysis.md) - what happens with the conflicting clause that `_propagate` returns
- [LBD and clause database reduction](lbd-clause-management.md) - where clauses get marked as deleted
