# VSIDS: Variable State Independent Decaying Sum

VSIDS is the heuristic that chooses which variable to decide next. Every variable has an activity score that is increased ("bumped") when the variable takes part in a conflict, and all scores fade over time, so the solver keeps branching on the variables involved in the most recent conflicts.

> **Implemented in** [`src/bsat/cdcl_modern.py`](../src/bsat/cdcl_modern.py): `ModernCDCLSolver._bump`, `_decay`, `_pick_branch_variable` and `_rebuild_heap`; state in `activity`, `activity_inc` and `heap`.
> **Options:** `vsids_decay` (default `0.95`). There is no option to switch VSIDS off.
> **Statistics:** `decisions`.
> Part of the [Modern CDCL solver](modern-cdcl.md).

---

## The Problem: Which Variable Next?

Whenever propagation stops without a conflict, the solver must pick an unassigned variable and decide it. The choice has an enormous effect on the size of the search. Obvious strategies have clear weaknesses:

- **Fixed order** ignores everything the search has discovered.
- **Static measures**, such as "the variable occurring in the most clauses", are computed once and do not adapt.
- **Dynamic measures over the current formula**, such as "the variable occurring in the most unsatisfied clauses", adapt but are expensive: they require looking at the clauses at every decision.

VSIDS, introduced by Chaff in 2001, is dynamic and cheap. Its scores depend only on the history of conflicts, not on the current assignment, which is what "variable state independent" refers to.

## The Idea

1. Every variable has a score, initially 0.
2. When a conflict is analyzed, the variables involved get their scores increased.
3. Scores decay over time, so that recent conflicts count for more than old ones.
4. The next decision variable is the unassigned variable with the highest score.

The reasoning is that conflicts are where the solver learns something. Variables that keep appearing in conflicts are the ones the current part of the problem hinges on; deciding them leads quickly to further conflicts and further learned clauses, or to a solution.

## Bumping

Bumps happen inside [conflict analysis](conflict-analysis.md). `_analyze` bumps every variable it marks: the variables of the conflicting clause and of every reason clause it resolves with, except those assigned at level 0. This is a wider set than the variables of the final learned clause, because it includes the current-level variables that were resolved away.

```python
def _bump(self, v: int):
    self.activity[v] += self.activity_inc
    if self.activity[v] > 1e100:
        # Rescale everything to avoid floating-point overflow
        for u in range(1, self.num_vars + 1):
            self.activity[u] *= 1e-100
        self.activity_inc *= 1e-100
        self._rebuild_heap()
    elif self.value[v] == 0:
        heapq.heappush(self.heap, (-self.activity[v], v))
```

## Decay Without Touching Every Score

The direct way to make old conflicts count less is to multiply every score by a factor below 1 after each conflict. That costs time proportional to the number of variables per conflict.

`ModernCDCLSolver` uses the trick from MiniSat instead: leave the scores alone and make the *bump* bigger.

```python
def _decay(self):
    self.activity_inc /= self.vsids_decay
```

`_decay` runs once after every analyzed conflict. With `vsids_decay = 0.95`:

| Conflict | Bump amount |
|---|---|
| 1st | 1.0 |
| 2nd | 1 / 0.95 ≈ 1.053 |
| 3rd | 1 / 0.95² ≈ 1.108 |
| 101st | 1 / 0.95¹⁰⁰ ≈ 169 |

Only the order of the scores matters for choosing a variable, and dividing all scores by the current increment shows that the two schemes produce the same order: a bump made `n` conflicts ago is worth `0.95ⁿ` of a bump made now. With 0.95, a bump loses half its weight in about 13.5 conflicts (`0.95^13.5 ≈ 0.5`).

### Rescaling

The increment grows exponentially, so it would eventually overflow a floating-point number. When any score exceeds `1e100`, `_bump` multiplies all scores and the increment by `1e-100`. This preserves the order. With the default decay the increment grows by a factor of `1e100` about every 4,500 conflicts.

## Finding the Maximum: A Lazy Heap

The basic [`CDCLSolver`](cdcl-solver.md) finds the best variable by scanning all unassigned variables at every decision. `ModernCDCLSolver` keeps a priority queue so that a decision does not have to look at every variable.

The queue is Python's `heapq`, a binary min-heap, holding `(-activity, variable)` tuples so that the highest activity comes out first. `heapq` has no operation to change the priority of an entry already in the heap, so the solver uses **lazy deletion**: it pushes a new entry whenever a variable needs one, and discards out-of-date entries when they reach the top.

```python
def _pick_branch_variable(self) -> int:
    if len(self.heap) > 4 * self.num_vars + 1000:
        self._rebuild_heap()            # Too many stale entries
    while self.heap:
        negated_activity, v = heapq.heappop(self.heap)
        if self.value[v] == 0 and -negated_activity == self.activity[v]:
            return v
    return 0
```

An entry is **stale**, and skipped, if its variable is assigned or its recorded activity is no longer the variable's current activity. The heap is kept correct by one rule: *every unassigned variable has an entry carrying its current activity*. Entries are pushed in three places:

- **At construction**, every variable is entered with activity 0.
- **In `_backtrack`**, each variable that becomes unassigned is pushed with its current activity. This is the source of nearly all entries: a variable is assigned whenever conflict analysis bumps it, so its fresh entry is created when it is next unassigned.
- **In `_bump`**, if the bumped variable is unassigned.

Since a variable's score only changes while it is assigned, and unassigning it pushes a fresh entry, the rule holds. Stale entries accumulate, so the heap is rebuilt from scratch when it grows beyond `4 * num_vars + 1000` entries, and after every rescaling (which changes all scores at once).

`_pick_branch_variable` returns 0 when the heap runs out, which means every variable is assigned and the formula is satisfied.

### Ties

Heap entries are compared as tuples, so among variables with equal activity the one with the lowest number wins. Variables are numbered in sorted order of their names, so the very first decision is always the variable whose name sorts first. (The sort is a string sort: `x10` comes before `x2`.)

## Looking at the Scores

```python
from bsat import ModernCDCLSolver
from benchmarks import pigeon_hole

solver = ModernCDCLSolver(pigeon_hole(6))
solver.solve()

ranked = sorted(range(1, solver.num_vars + 1), key=lambda v: -solver.activity[v])
print("conflicts:", solver.stats.conflicts)
print(f"bump increment now: {solver.activity_inc:.1f}")
for v in ranked[:3] + ranked[-1:]:
    print(f"{solver.names[v - 1]:<6} {solver.activity[v]:.1f}")
```

Output (run from the repository root with `PYTHONPATH=src:tests`):

```
conflicts: 146
bump increment now: 1698.5
p_5_2  22282.4
p_1_3  21941.2
p_0_2  21568.4
p_0_4  4.0
```

After 145 analyzed conflicts the increment is `1 / 0.95¹⁴⁵ ≈ 1698.5`. The three most active variables have scores worth about 13 current bumps each. The least active one, `p_0_4`, was last bumped early in the run, when a bump was worth about 1; those early bumps now count for almost nothing.

## The Decay Factor

`vsids_decay` controls how long conflicts are remembered:

- **Lower (for example 0.80):** short memory. The ordering follows the last few conflicts closely.
- **Higher (for example 0.99):** long memory. The ordering changes slowly.
- **`1.0`:** no decay. A score is simply the number of times the variable was bumped.

MiniSat uses 0.95; some later solvers vary the factor during the run. The following runs the same instances with five values:

```python
from bsat import get_modern_cdcl_stats
from benchmarks import pigeon_hole, phase_transition_3sat

instances = [("pigeon_hole(8)", pigeon_hole(8))]
instances += [(f"phase_transition_3sat(100, seed={seed})", phase_transition_3sat(100, seed=seed))
              for seed in (1, 2, 3, 4)]
for label, cnf in instances:
    conflicts = [get_modern_cdcl_stats(cnf, vsids_decay=decay)[1].conflicts
                 for decay in (0.8, 0.9, 0.95, 0.99, 1.0)]
    print(label, conflicts)
```

Conflicts until the instance was solved:

| Instance | 0.80 | 0.90 | 0.95 | 0.99 | 1.0 |
|---|---|---|---|---|---|
| `pigeon_hole(8)` | 6044 | 4506 | 3199 | 2170 | 2643 |
| `phase_transition_3sat(100, seed=1)` | 772 | 723 | 724 | 726 | 838 |
| `phase_transition_3sat(100, seed=2)` | 146 | 154 | 134 | 105 | 176 |
| `phase_transition_3sat(100, seed=3)` | 528 | 453 | 432 | 563 | 475 |
| `phase_transition_3sat(100, seed=4)` | 639 | 495 | 450 | 479 | 528 |

No value wins everywhere, and on instances this small the differences are mostly noise. The table shows how to run the experiment, not which value is best.

## Compared with the Basic Solver

`CDCLSolver` in `src/bsat/cdcl.py` also uses VSIDS with a growing increment. The differences are:

| | `CDCLSolver` | `ModernCDCLSolver` |
|---|---|---|
| Finding the maximum | `max()` over all unassigned variables at every decision | Lazy heap |
| Variables bumped per conflict | Those of the current decision level met during analysis | All those met during analysis, except level 0 |
| Overflow protection | None | Rescaling at `1e100` |

## Not Implemented Here

- **Switching VSIDS off.** There is no fixed-order or random decision mode to compare against. `vsids_decay=1.0` removes the decay but still orders variables by conflict counts.
- **An indexed heap.** MiniSat and its descendants store each variable's position in the heap, so a bump can move the existing entry instead of pushing a duplicate. The lazy heap gives the same decisions with simpler code and more memory.
- **Other decision heuristics.** VMTF (variable move-to-front) keeps the variables in a queue ordered by when they were last bumped and needs no scores at all; LRB and CHB treat branching as a learning problem. Many current solvers alternate between VSIDS and VMTF during a run.
- **Reason-side bumping**, which also bumps variables in the reason clauses of the learned clause's literals.

The C solver in `competition/c/` implements VSIDS on an indexed 4-ary heap, VMTF, the alternation between the two, LRB/CHB and reason-side bumping; see [competition/c/README.md](../competition/c/README.md) and [competition/c/FEATURES.md](../competition/c/FEATURES.md).

## A Note on Variants

The original VSIDS in Chaff kept a counter per *literal*, incremented the counters of the literals in each learned clause, and periodically divided all counters by a constant. The version described here, with one score per variable, a bump for every variable seen during analysis, and decay after every conflict through a growing increment, comes from MiniSat. It is sometimes called EVSIDS (exponential VSIDS).

## References

- Moskewicz, Madigan, Zhao, Zhang, Malik. "Chaff: Engineering an Efficient SAT Solver" (DAC 2001). The original VSIDS.
- Eén, Sörensson. "An Extensible SAT-solver" (SAT 2003). Per-variable activities with a growing increment, as used here.
- Biere, Fröhlich. "Evaluating CDCL Variable Scoring Schemes" (SAT 2015). Compares VSIDS variants and VMTF.
- Liang, Ganesh, Poupart, Czarnecki. "Learning Rate Based Branching Heuristic for SAT Solvers" (SAT 2016). LRB.

## See Also

- [Modern CDCL solver](modern-cdcl.md)
- [Conflict analysis](conflict-analysis.md) - where variables are bumped
- [Phase saving](phase-saving.md) - VSIDS chooses the variable; phase saving chooses its value
- [Terminology: VSIDS](terminology.md#vsids-variable-state-independent-decaying-sum)
