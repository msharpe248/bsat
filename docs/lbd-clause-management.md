# LBD and Clause Database Reduction

A CDCL solver learns one clause per conflict, and it cannot keep them all. Literal block distance (LBD) is a cheap measure of how useful a learned clause is likely to be. The solver uses it to decide which learned clauses to delete when it periodically reduces its clause database.

> **Implemented in** [`src/bsat/cdcl_modern.py`](../src/bsat/cdcl_modern.py): LBD is computed at the end of `ModernCDCLSolver._analyze` and stored in `_Clause.lbd` by `_add_learned`; `_reduce_database` deletes clauses; the schedule is in `solve()`.
> **Options:** `reduce_db` (default `True`), `reduce_interval` (default `2000`), `reduce_increment` (default `300`), `glue_limit` (default `2`).
> **Statistics:** `glue_clauses`, `deleted_clauses`, `reductions`.
> Part of the [Modern CDCL solver](modern-cdcl.md).

---

## The Problem: Too Many Learned Clauses

Every conflict adds a clause. After a million conflicts there would be a million learned clauses, usually far more than the original formula has. That is a problem for two reasons:

- **Propagation slows down.** Every clause sits in two watch lists, and longer watch lists mean more work per assignment.
- **Memory grows without bound.**

Many learned clauses are used only shortly after they are learned, if at all, so keeping all of them buys little. Deleting a learned clause is always sound: it is implied by the original formula, so removing it cannot change the answer. The question is which ones to delete. Early solvers used clause size or a clause activity score similar to [VSIDS](vsids.md). The Glucose solver (Audemard and Simon, 2009) introduced LBD, which was found to predict usefulness better.

## Literal Block Distance

### Definition

The **LBD** of a clause is the number of distinct decision levels among its literals, measured at the moment the clause is learned.

```
Learned clause:  (¬x1 ∨ ¬x2 ∨ ¬x3 ∨ ¬x4 ∨ ¬x5)

Decision levels: x1 → 8    (the asserting literal)
                 x2 → 5
                 x3 → 5
                 x4 → 3
                 x5 → 3

Distinct levels: {8, 5, 3}        LBD = 3
```

The clause has five literals but an LBD of 3.

### Why It Predicts Usefulness

All variables assigned at one decision level were set by a single decision and the propagation that followed it. They form a **block**: a group of assignments that come as a package. A clause's literals from the same block tend to become false together again whenever that decision, or an equivalent one, is repeated.

So what matters for how easily a clause becomes unit is less its number of literals than its number of blocks. A long clause over two blocks needs only two things to happen before it propagates. A clause with the same length over ten blocks needs ten independent decisions to line up, which is rare.

### Glue Clauses

A learned clause always contains one literal from the conflict level, so its LBD is at least 1. An LBD of 1 means the clause is a single literal (all other literals would have to be at the conflict level too, and first-UIP learning leaves only one there).

A clause with **LBD 2** has its asserting literal plus literals from exactly one other block. Once that one block is in place, the clause propagates, and its asserting literal joins the block. Audemard and Simon called these **glue clauses**, because they glue a literal onto an existing block. They are the most valuable learned clauses, and the standard policy is never to delete them.

### The Code

LBD is computed in `_analyze`, after the clause has been [minimized](conflict-analysis.md#clause-minimization) and while the decision levels of its literals are still available (the backjump that follows unassigns some of them):

```python
lbd = len({level[abs(q)] for q in learned})
```

`_add_learned` stores it in the clause and counts glue clauses:

```python
def _add_learned(self, lits: List[int], lbd: int):
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
```

A learned unit clause has LBD 1, is counted in `glue_clauses`, and is assigned at level 0 without being stored in `self.learned`.

The LBD is computed once and never updated.

### Looking at the Distribution

```python
from collections import Counter
from bsat import ModernCDCLSolver
from benchmarks import phase_transition_3sat

solver = ModernCDCLSolver(phase_transition_3sat(80, seed=2), reduce_db=False)
solver.solve()

histogram = Counter(clause.lbd for clause in solver.learned)
print(f"{solver.stats.status}, {len(solver.learned)} learned clauses stored")
for lbd in sorted(histogram):
    print(f"LBD {lbd}: {histogram[lbd]:>3} {'#' * histogram[lbd]}")
```

Output (run from the repository root with `PYTHONPATH=src:tests`):

```
SAT, 122 learned clauses stored
LBD 2:   3 ###
LBD 3:  14 ##############
LBD 4:  34 ##################################
LBD 5:  27 ###########################
LBD 6:  30 ##############################
LBD 7:  11 ###########
LBD 8:   3 ###
```

Three of the 122 clauses are glue clauses. The distribution depends heavily on the kind of instance; uniformly random formulas like this one produce few glue clauses.

## Database Reduction

### When

The main loop checks the schedule each time it is about to make a decision:

```python
if self.reduce_db and stats.conflicts >= self.next_reduce:
    self._reduce_database()
    # The interval grows, so the database is allowed to grow too
    self.next_reduce = (stats.conflicts + self.reduce_interval
                        + self.reduce_increment * stats.reductions)
```

`next_reduce` starts at `reduce_interval`. After the k-th reduction, the next one is due `reduce_interval + k * reduce_increment` conflicts later. With the defaults the intervals are 2000, 2300, 2600, 2900, ... conflicts, so reductions fall at about 2000, 4300, 6900 and 9800 conflicts. A run with fewer than 2000 conflicts never reduces.

The growing interval matters. Each reduction removes about half of the removable clauses, so a growing interval lets the database grow slowly over a long run instead of holding it at a fixed size. Hard instances need more learned clauses.

The defaults of 2000 and 300 follow Glucose.

### What Is Deleted

```python
def _reduce_database(self):
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
```

The steps:

1. **Collect the candidates:** learned clauses that are neither glue clauses nor locked.
2. **Rank them** from worst to best: highest LBD first, and among clauses with equal LBD the longest first.
3. **Delete the worse half** of the candidates (rounded down).

Three kinds of clause are never deleted:

- **Original clauses.** They define the problem. Only `self.learned` is examined.
- **Glue clauses**, those with `lbd <= glue_limit`.
- **Locked clauses.** A clause that is currently the reason for an assignment on the trail must stay, because [conflict analysis](conflict-analysis.md) may need to resolve with it. A reason clause always has its implied literal in `lits[0]`, so the test is whether the reason recorded for that variable is this very clause.

Note that half of the *candidates* are deleted, not half of all learned clauses. If most learned clauses are glue clauses, a reduction removes little.

### How a Clause Is Removed

Deleting a clause from the two watch lists it sits in would require searching them. Instead the clause is only marked with `deleted = True` and dropped from `self.learned`. [`_propagate`](two-watched-literals.md#deleted-clauses) discards its watch-list entries the next time it comes across them.

## Measuring It

```python
from bsat import ModernCDCLSolver
from benchmarks import pigeon_hole

for n in (8, 9):
    for reduce_db in (True, False):
        solver = ModernCDCLSolver(pigeon_hole(n), reduce_db=reduce_db)
        solver.solve()
        stats = solver.stats
        print(f"pigeon_hole({n}) reduce_db={reduce_db!s:<5} conflicts={stats.conflicts:>5} "
              f"reductions={stats.reductions} deleted={stats.deleted_clauses:>4} "
              f"glue={stats.glue_clauses} kept at end={len(solver.learned)}")
```

Result:

| Instance | `reduce_db` | Conflicts | Reductions | Deleted | Glue clauses | Learned clauses at the end |
|---|---|---|---|---|---|---|
| `pigeon_hole(8)` | `True` | 3199 | 1 | 997 | 25 | 2197 |
| `pigeon_hole(8)` | `False` | 3043 | 0 | 0 | 22 | 3039 |
| `pigeon_hole(9)` | `True` | 11370 | 4 | 7284 | 26 | 4079 |
| `pigeon_hole(9)` | `False` | 9338 | 0 | 0 | 23 | 9333 |

Reduction does what it is designed to do: on `pigeon_hole(9)` the database ends with 4079 learned clauses instead of 9333. It did not make these runs shorter: both instances needed *more* conflicts with reduction on. A deleted clause that would have been useful later has to be learned again in some form, although on single instances a difference of this size can also be chance.

That is the trade-off: reduction gives up some pruning power to keep propagation fast and memory bounded. It pays off in runs of hundreds of thousands or millions of conflicts, where an unreduced database becomes the bottleneck. Runs of a few thousand conflicts, which is what a pure-Python solver can do comfortably, are too short to show the benefit, and two instances of one family are not a basis for general conclusions.

## Tuning

| Option | Smaller value | Larger value |
|---|---|---|
| `reduce_interval` | Earlier and more frequent reductions; smaller database | Larger database before the first reduction |
| `reduce_increment` | Intervals grow slowly; database stays small | Database grows faster over a long run |
| `glue_limit` | Fewer protected clauses. With `0` no clause is protected by its LBD. | More clauses kept permanently |

`reduce_db=False` keeps every learned clause.

## Not Implemented Here

- **Updating LBD.** When a learned clause takes part in a later conflict or propagation, its literals may span fewer levels than when it was learned. Real solvers recompute the LBD then and keep the smaller value, so clauses can be promoted to glue status.
- **Tiers.** Instead of the two classes used here (glue or not), many solvers use three: clauses kept permanently, clauses kept as long as they have been used recently, and the rest, which are reduced aggressively.
- **Clause activity.** MiniSat-style activity scores for clauses, bumped when a clause is used in conflict analysis, used as the main criterion or as a tie-breaker. Here ties in LBD are broken by clause length only.
- **Physical removal.** Solvers with their own clause memory compact it after a reduction (garbage collection). In Python the memory is reclaimed automatically once the watch lists have dropped the clause.

The C solver in `competition/c/` implements LBD updates and a three-tier database; see [competition/c/FEATURES.md](../competition/c/FEATURES.md).

## References

- Audemard, Simon. "Predicting Learnt Clauses Quality in Modern SAT Solvers" (IJCAI 2009). Introduces LBD and glue clauses.
- Eén, Sörensson. "An Extensible SAT-solver" (SAT 2003). Activity-based clause deletion, the approach LBD replaced.
- Oh. "Between SAT and UNSAT: The Fundamental Difference in CDCL SAT" (SAT 2015). The tiered clause database.

## See Also

- [Modern CDCL solver](modern-cdcl.md)
- [Conflict analysis](conflict-analysis.md) - where learned clauses and their LBD come from
- [Restarts](restarts.md) - the Glucose-style restart policy also uses LBD
- [Two-watched literals](two-watched-literals.md) - how deleted clauses leave the watch lists
