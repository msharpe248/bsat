# Conflict Analysis and Clause Learning

Conflict analysis is what a CDCL solver does when unit propagation falsifies a clause: it works out which earlier assignments caused the conflict, records that as a new **learned clause**, and uses the clause to decide how far to backtrack. This page covers first-UIP learning, learned-clause minimization and backjumping.

> **Implemented in** [`src/bsat/cdcl_modern.py`](../src/bsat/cdcl_modern.py): `ModernCDCLSolver._analyze` (first-UIP learning, backjump level, LBD), `_minimize` and `_is_redundant` (clause minimization), `_add_learned` and `_backtrack`.
> **Options:** `minimize_learned` (default `True`).
> **Statistics:** `conflicts`, `learned_clauses`, `learned_literals`, `minimized_literals`.
> Part of the [Modern CDCL solver](modern-cdcl.md).

---

## Why Learn from Conflicts

DPLL reacts to a conflict by undoing the most recent decision and trying the opposite value. It keeps no record of *why* the conflict happened, so the same combination of assignments can lead to the same dead end again in a different part of the search tree.

CDCL records the cause. After each conflict it:

1. **Analyzes** the conflict to find a small set of assignments that is sufficient to cause it.
2. **Learns** a clause forbidding that combination.
3. **Backjumps** to the level where the learned clause becomes unit, which may be many levels up.
4. **Asserts** the one literal the learned clause now forces, and continues.

The learned clause is implied by the formula, so adding it changes no solutions; it only lets unit propagation rule the bad combination out immediately in the future.

## The Running Example

The rest of this page uses one small formula:

```
C1: (¬a ∨ b)
C2: (¬c ∨ d)
C3: (¬b ∨ ¬d ∨ e)
C4: (¬d ∨ f)
C5: (¬a ∨ ¬e ∨ ¬f)
```

The solver decides `a = True` and then `c = True`:

```
Level 1:  a = True   (decision)
          b = True   (C1: a is true, so b must be)

Level 2:  c = True   (decision)
          d = True   (C2)
          e = True   (C3: b and d are true)
          f = True   (C4)
          C5 is falsified: a, e and f are all true    → CONFLICT
```

## The Implication Graph

The **implication graph** records how the assignments came about. Each assigned variable is a node. A decision has no incoming edges. A propagated variable has an incoming edge from every other variable of its **reason** clause, the clause that forced it.

```
 a@1 ──────C1─────▶ b@1 ───────────┐
  │                                C3
  │                                ▼
  │    c@2 ──C2──▶ d@2 ──C3──────▶ e@2 ──┐
  │                 │                    C5
  │                 └────C4──────▶ f@2 ──┼──▶ conflict
  │                                      │
  └──────────────────C5──────────────────┘
```

(`x@n` means `x` was assigned at decision level `n`.)

The solver never builds this graph explicitly. It is stored implicitly in three per-variable arrays and the trail:

| Attribute | Example after the conflict |
|---|---|
| `trail` | `a, b, c, d, e, f` in assignment order |
| `level[v]` | 1 for `a`, `b`; 2 for `c`, `d`, `e`, `f` |
| `reason[v]` | `None` for the decisions `a`, `c`; `C1` for `b`, `C2` for `d`, `C3` for `e`, `C4` for `f` |

## First-UIP Learning

### Unique Implication Points

A **unique implication point (UIP)** is a node at the current decision level that lies on *every* path from the current decision to the conflict. In the example, every path from `c` to the conflict passes through `d`, so `d` is a UIP. The decision `c` itself is always a UIP.

The **first UIP** is the one closest to the conflict: here `d`.

A UIP is useful because it yields a clause with exactly one literal from the current decision level. After backtracking, that literal is unassigned while all the others are still false, so the clause is unit and forces a new assignment. Such a clause is called **asserting**.

Stopping at the first UIP, rather than going all the way back to the decision, needs the fewest resolution steps and was found to work best in practice (Zhang et al., 2001). It is the standard learning scheme.

### The Algorithm

Start with the conflicting clause. Repeatedly pick the most recently assigned current-level literal in it and replace it by the other literals of its reason clause. This is a **resolution** step between the two clauses. Stop when only one literal of the current level is left.

On the example:

```
Start:            C5 = (¬a ∨ ¬e ∨ ¬f)           current-level variables: e, f
Resolve f (C4):        (¬a ∨ ¬e ∨ ¬d)           current-level variables: e, d
Resolve e (C3):        (¬a ∨ ¬b ∨ ¬d)           current-level variables: d      ← first UIP
```

The learned clause `(¬a ∨ ¬b ∨ ¬d)` says: `a`, `b` and `d` cannot all be true.

### The Code

`_analyze` does this without building intermediate clauses. `seen[v]` marks the variables already in the clause under construction; `pending` counts how many of them belong to the current level and have not been resolved yet. Literals from lower levels go straight into `learned`. Literals assigned at level 0 are permanently false and are left out.

```python
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
```

Walking the trail backwards visits the marked current-level variables in reverse order of assignment, which guarantees that a variable is resolved only after everything it helped to imply. When `pending` drops to zero, the literal just taken from the trail is the only current-level literal left: the first UIP. Its negation becomes `learned[0]`.

Traced on the example:

| Step | Clause processed | Newly marked | `pending` | `learned` |
|---|---|---|---|---|
| 1 | conflict `C5` | `f`, `e` (level 2), `a` (level 1) | 2 | `[_, ¬a]` |
| 2 | take `f` from the trail; its reason `C4` | `d` (level 2) | 1, then 2 | `[_, ¬a]` |
| 3 | take `e` from the trail; its reason `C3` | `b` (level 1) | 1 | `[_, ¬a, ¬b]` |
| 4 | take `d` from the trail | | 0: stop | `[¬d, ¬a, ¬b]` |

Every variable marked along the way also has its [VSIDS](vsids.md) activity bumped (`self._bump(v)`), which is how conflicts steer future decisions.

## Clause Minimization

### Redundant Literals

A first-UIP clause often contains literals that add nothing. In the example the learned clause is `(¬d ∨ ¬a ∨ ¬b)`, but `b` was implied by `a` alone through `C1 = (¬a ∨ b)`. Whenever `a` is true, `b` is true as well, so forbidding "`d` and `a`" already forbids "`d` and `a` and `b`". Formally, resolving the learned clause with `C1` on `b` gives the shorter clause:

```
(¬d ∨ ¬a ∨ ¬b)   resolved with   (¬a ∨ b)   gives   (¬d ∨ ¬a)
```

In general, a literal of the learned clause is **redundant** if its variable was implied by a reason clause whose other variables are all either in the learned clause already, fixed at level 0, or themselves redundant by the same rule. Checking only the literal's own reason clause is *local* minimization; following the reasons further back is *recursive* minimization (Sörensson and Biere, 2009), which is what `ModernCDCLSolver` implements.

A shorter clause is a stronger constraint: it becomes unit sooner and rules out more assignments.

### The Code

`_minimize` checks every literal except the asserting one. Decisions can never be redundant, because they have no reason.

```python
def _minimize(self, learned: List[int], to_clear: List[int]) -> List[int]:
    levels = {self.level[abs(q)] for q in learned[1:]}
    kept = [learned[0]]
    for q in learned[1:]:
        if self.reason[abs(q)] is None or not self._is_redundant(q, levels, to_clear):
            kept.append(q)
    return kept
```

`_is_redundant` searches backwards through the implication graph from one literal, using an explicit stack instead of recursion:

```python
def _is_redundant(self, lit: int, levels: set, to_clear: List[int]) -> bool:
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
```

How it works:

- At this point `seen[v]` is true exactly for the variables of the learned clause below the current level. Reaching a `seen` variable ends that branch of the search successfully.
- Reaching a decision ends the whole search with failure: a decision is not implied by anything.
- **Early exit by level.** If a variable was assigned at a decision level that does not occur in the learned clause, following its reasons back always ends at that level's decision, which is not in the clause. `levels` holds the levels that do occur, so such a search is abandoned at once instead of being followed to the decision.
- **Caching successes.** Variables proven to be implied are left marked in `seen`, so later searches stop when they reach them. If a search fails, the marks it added are removed again (`to_clear[mark:]`), because nothing has been proven about them.
- `to_clear` lists every variable whose `seen` mark must be reset before the next conflict; `_analyze` resets them after minimization.

### Trying It

This script reproduces the running example by driving the solver's internal methods directly (useful for experiments, but not part of the public API). It analyzes the same conflict with minimization off and on.

```python
from bsat import CNFExpression, ModernCDCLSolver

cnf = CNFExpression.parse(
    "(~a | b) & (~c | d) & (~b | ~d | e) & (~d | f) & (~a | ~e | ~f)"
)

def name(lit):
    return ("" if lit > 0 else "~") + solver.names[abs(lit) - 1]

def decide(lit):
    solver.trail_lim.append(len(solver.trail))   # open a new decision level
    solver._enqueue(lit, None)
    return solver._propagate()

for minimize in (False, True):
    solver = ModernCDCLSolver(cnf, minimize_learned=minimize)
    a, c = 1, 3                                   # variables are numbered in sorted name order
    decide(a)
    conflict = decide(c)
    print("trail:   ", [f"{name(lit)}@{solver.level[abs(lit)]}" for lit in solver.trail])
    print("conflict:", [name(lit) for lit in conflict.lits])
    learned, backjump, lbd = solver._analyze(conflict)
    print(f"minimize_learned={minimize}: learned {[name(lit) for lit in learned]}, "
          f"backjump to level {backjump}, LBD {lbd}")
```

Output:

```
trail:    ['a@1', 'b@1', 'c@2', 'd@2', 'e@2', 'f@2']
conflict: ['~f', '~e', '~a']
minimize_learned=False: learned ['~d', '~a', '~b'], backjump to level 1, LBD 2
trail:    ['a@1', 'b@1', 'c@2', 'd@2', 'e@2', 'f@2']
conflict: ['~f', '~e', '~a']
minimize_learned=True: learned ['~d', '~a'], backjump to level 1, LBD 2
```

(The conflicting clause is `C5` with its literals in a different order: [propagation](two-watched-literals.md) reorders literals inside a clause as it moves the watches.)

### How Much It Removes

The statistics `learned_literals` (total length of all learned clauses after minimization) and `minimized_literals` (literals removed) show the effect. The following compares `minimize_learned=True` and `False` on five instances from `tests/benchmarks.py`:

```python
from bsat import get_modern_cdcl_stats
from benchmarks import pigeon_hole, phase_transition_3sat

instances = [("pigeon_hole(7)", pigeon_hole(7)), ("pigeon_hole(8)", pigeon_hole(8))]
instances += [(f"phase_transition_3sat(120, seed={seed})", phase_transition_3sat(120, seed=seed))
              for seed in (1, 2, 3)]
for label, cnf in instances:
    _, on = get_modern_cdcl_stats(cnf)
    _, off = get_modern_cdcl_stats(cnf, minimize_learned=False)
    print(f"{label:<36} {on.status:<5} "
          f"on: {on.conflicts:>5} conflicts, {on.minimized_literals:>5} removed, "
          f"avg {on.learned_literals / on.learned_clauses:>4.1f}   "
          f"off: {off.conflicts:>5} conflicts, avg {off.learned_literals / off.learned_clauses:>4.1f}")
```

Result (run from the repository root with `PYTHONPATH=src:tests`):

| Instance | Result | Conflicts, on | Literals removed | Avg. length, on | Conflicts, off | Avg. length, off |
|---|---|---|---|---|---|---|
| `pigeon_hole(7)` | UNSAT | 654 | 1455 | 9.8 | 796 | 12.8 |
| `pigeon_hole(8)` | UNSAT | 3199 | 10834 | 15.2 | 3534 | 18.9 |
| `phase_transition_3sat(120, seed=1)` | UNSAT | 1348 | 3135 | 7.1 | 1455 | 9.7 |
| `phase_transition_3sat(120, seed=2)` | SAT | 235 | 344 | 8.5 | 275 | 10.3 |
| `phase_transition_3sat(120, seed=3)` | SAT | 1193 | 2719 | 7.3 | 1457 | 10.2 |

On these five instances minimization shortened the average learned clause by about two to four literals, and the run with minimization also needed fewer conflicts. The two runs follow different search paths once the first clause differs, so the conflict counts are not a controlled comparison, and five instances are a small sample.

## Backjumping

### The Backjump Level

The learned clause has one literal from the current level (the asserting literal, in position 0). The solver backtracks to the **highest level among the remaining literals**, the second-highest level in the clause. That is the earliest point at which all the other literals are still false, so the clause is unit there.

```python
backjump = 0
for k in range(1, len(learned)):
    if level[abs(learned[k])] > backjump:
        backjump = level[abs(learned[k])]
        learned[1], learned[k] = learned[k], learned[1]
```

The loop also moves a literal of the backjump level into position 1. Positions 0 and 1 are the clause's [watched literals](two-watched-literals.md), and these two are the right ones to watch: the literal about to be asserted, and the one that will be unassigned first on later backtracking.

A learned clause with a single literal has no "remaining literals", so the backjump level is 0 and the literal becomes a permanent fact.

This is **non-chronological backtracking**: the jump can skip any number of levels. If a clause learned at level 12 has its other literals at levels 2 and 5, the solver returns directly to level 5, discarding the decisions of levels 6 to 12 because they played no part in the conflict.

### Asserting the Learned Clause

The main loop then runs:

```python
learned, backjump, lbd = self._analyze(conflict)
self.recent_lbds.append(lbd)
self.lbd_sum += lbd
self._backtrack(backjump)
self._add_learned(learned, lbd)
self._decay()
```

`_add_learned` stores the clause, attaches its watches and assigns `learned[0]` with the new clause as its reason. A unit learned clause is assigned at level 0 with no reason and is not stored.

In the example, the solver backjumps to level 1, where `a` and `b` are still true. The learned clause `(¬d ∨ ¬a)` is unit and forces `d = False`; `C2 = (¬c ∨ d)` then forces `c = False`. The solver has learned that its second decision was wrong without ever trying the opposite value explicitly.

### Conflicts at Level 0

If propagation finds a conflict when no decision has been made, there is nothing to undo: the formula is unsatisfiable. `solve()` checks for this before calling `_analyze` and returns with `stats.status == "UNSAT"`.

## LBD

`_analyze` also returns the clause's **literal block distance**, the number of distinct decision levels among its literals, computed while the levels are still known:

```python
lbd = len({level[abs(q)] for q in learned})
```

It is used to rank learned clauses for deletion and to trigger restarts. See [LBD and clause database reduction](lbd-clause-management.md) and [Restarts](restarts.md).

## Not Implemented Here

These refinements of conflict analysis exist in real solvers but not in `ModernCDCLSolver`. All four are implemented in the C solver in `competition/c/`; see [competition/c/FEATURES.md](../competition/c/FEATURES.md).

- **All-UIP shrinking:** after minimization, try to replace all literals of one decision level in the learned clause by a single UIP of that level.
- **Minimization with binary clauses:** use the binary clauses that contain the asserting literal to remove further literals.
- **On-the-fly subsumption:** notice during analysis that an intermediate resolvent subsumes one of the clauses it was derived from, and strengthen that clause.
- **Chronological backtracking:** when the backjump would undo a very large number of levels, backtrack only one level instead and keep the rest of the trail.

## References

- Marques-Silva, Sakallah. "GRASP: A Search Algorithm for Propositional Satisfiability" (IEEE Transactions on Computers, 1999). Conflict analysis, clause learning and non-chronological backtracking.
- Zhang, Madigan, Moskewicz, Malik. "Efficient Conflict Driven Learning in a Boolean Satisfiability Solver" (ICCAD 2001). Compares learning schemes; first-UIP comes out best.
- Eén, Sörensson. "An Extensible SAT-solver" (SAT 2003). The trail-walking formulation of first-UIP analysis used here.
- Sörensson, Biere. "Minimizing Learned Clauses" (SAT 2009). Local and recursive minimization.

## See Also

- [Modern CDCL solver](modern-cdcl.md)
- [CDCL solver](cdcl-solver.md) - the basic solver's first-UIP analysis, without minimization
- [VSIDS](vsids.md) - what the activity bumps in `_analyze` are for
- [Terminology: Implication Graph](terminology.md#implication-graph)
