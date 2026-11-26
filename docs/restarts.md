# Restarts

A restart abandons the current partial assignment and begins the search again from decision level 0, keeping everything the solver has learned. This page covers the two restart policies in the modern solver: the fixed Luby sequence and the Glucose-style dynamic policy based on LBD.

> **Implemented in** [`src/bsat/cdcl_modern.py`](../src/bsat/cdcl_modern.py): `ModernCDCLSolver._should_restart`, `_restart` and `_luby`; state in `conflicts_since_restart`, `luby_index`, `recent_lbds` and `lbd_sum`.
> **Options:** `restart_strategy` (`"glucose"` by default, or `"luby"` or `"none"`), `restart_base` (default `100`), `glucose_window` (default `50`), `glucose_k` (default `0.8`).
> **Statistics:** `restarts`.
> Part of the [Modern CDCL solver](modern-cdcl.md).

---

## Why Restart?

The first few decisions of a search are made when the solver knows least: all [VSIDS](vsids.md) scores are zero and nothing has been learned. If those decisions are poor, the solver can spend a very long time below them, because backjumping returns to the top of the search tree only when a learned clause forces it to.

Run times of backtracking search also vary enormously with the order of decisions. On the same instance one ordering may finish at once and another may take orders of magnitude longer (the "heavy-tailed" behaviour described by Gomes, Selman and Kautz, 1998). Cutting a run short and trying again with a different ordering avoids being stuck in one of the bad runs.

In a CDCL solver a restart is cheap and loses little, because only the trail above level 0 is discarded. What is kept:

- **Learned clauses**, so nothing that was refuted needs to be refuted again.
- **VSIDS scores**, so the new attempt starts by deciding the variables that mattered in recent conflicts, which is usually a different order from the first attempt.
- **Saved phases**, so the new attempt re-creates much of the previous assignment in that new order. See [Phase saving](phase-saving.md).

A restart is best thought of as a reordering of the decisions, not as starting over.

## What a Restart Does

```python
def _restart(self):
    self.stats.restarts += 1
    self.conflicts_since_restart = 0
    self.luby_index += 1
    self.recent_lbds.clear()
    self._backtrack(0)
```

`_backtrack(0)` unassigns everything above level 0 and saves the phases. Assignments at level 0 (unit clauses of the input and learned unit clauses, with their consequences) are permanent and stay.

The main loop considers a restart after a propagation that ended without a conflict, and only when there is something to undo:

```python
if self.decision_level > 0 and self._should_restart():
    self._restart()
    continue
```

`_should_restart` implements the policy selected by `restart_strategy`:

```python
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
```

## Luby Restarts

### The Sequence

The Luby sequence is

```
1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, 1, 1, 2, ...
```

It is built by repetition: take the sequence so far, repeat it, and append the next power of two. Formally, for `i ≥ 1`:

```
luby(i) = 2^(k-1)                  if i = 2^k - 1
luby(i) = luby(i - 2^(k-1) + 1)    if 2^(k-1) ≤ i < 2^k - 1
```

```python
@staticmethod
def _luby(i: int) -> int:
    k = 1
    while (1 << k) - 1 < i:
        k += 1
    if i == (1 << k) - 1:
        return 1 << (k - 1)
    return ModernCDCLSolver._luby(i - (1 << (k - 1)) + 1)
```

```python
from bsat import ModernCDCLSolver

print([ModernCDCLSolver._luby(i) for i in range(1, 16)])
```

Output:

```
[1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8]
```

### The Policy

The i-th restart happens once `luby(i) * restart_base` conflicts have occurred since the previous restart. With the default `restart_base=100` the intervals are 100, 100, 200, 100, 100, 200, 400, ... conflicts.

Most runs are short, and occasionally a long one is allowed. Luby, Sinclair and Zuckerman (1993) proved that for a randomized algorithm whose run-time distribution is unknown, restarting according to this sequence is within a logarithmic factor of the best possible fixed restart schedule, and that no universal schedule can do better by more than a constant factor. A CDCL solver is not the memoryless algorithm of that theorem, since it keeps learned clauses across restarts, but the sequence works well in practice and is simple and predictable.

The basic [`CDCLSolver`](cdcl-solver.md) uses the same policy.

## Glucose-Style Restarts

### The Idea

A fixed schedule restarts regardless of how the search is going. The Glucose solver (Audemard and Simon, 2009) restarts based on the quality of the clauses currently being learned, measured by their [LBD](lbd-clause-management.md):

- If recent learned clauses have a **low LBD** compared with the run as a whole, the search is producing strong clauses. Leave it alone.
- If recent learned clauses have a **high LBD**, the search has drifted into a region where conflicts involve many unrelated decision levels and teach little. Restart.

### The Mechanics

After every analyzed conflict, whatever the restart strategy, the main loop records the LBD of the new clause:

```python
learned, backjump, lbd = self._analyze(conflict)
self.recent_lbds.append(lbd)
self.lbd_sum += lbd
```

- `recent_lbds` is a `deque` with `maxlen=glucose_window`, so it always holds the LBDs of at most the last `glucose_window` conflicts.
- `lbd_sum` is the sum over the whole run, so `lbd_sum / stats.conflicts` is the overall average.

The solver restarts when

```
average(recent_lbds) * glucose_k  >  overall average
```

With the default `glucose_k = 0.8`, this means the recent average exceeds the overall average by more than 25%.

Two details:

- No restart happens until the window is full. Since `_restart` clears the window, there are always at least `glucose_window` conflicts between two restarts.
- The overall average is not reset by a restart.

### The Parameters

| Option | Effect of a larger value |
|---|---|
| `glucose_k` | **More** restarts: a smaller excess of the recent average is enough. At `1.0` any window that is worse than average triggers a restart. |
| `glucose_window` | Smoother recent average and a longer minimum distance between restarts. Must be at least 1. |

The defaults, a window of 50 and a factor of 0.8, are those of Glucose 2 and later.

## No Restarts

`restart_strategy="none"` never restarts. It exists for comparison.

## Measuring It

The following compares four policies. Each cell shows conflicts / restarts.

```python
from bsat import get_modern_cdcl_stats
from benchmarks import pigeon_hole, phase_transition_3sat

configurations = [
    {"restart_strategy": "glucose"},
    {"restart_strategy": "glucose", "glucose_k": 1.0},
    {"restart_strategy": "luby"},
    {"restart_strategy": "none"},
]
instances = [("pigeon_hole(8)", pigeon_hole(8))]
instances += [(f"phase_transition_3sat(150, seed={seed})", phase_transition_3sat(150, seed=seed))
              for seed in (1, 2, 3)]
for label, cnf in instances:
    cells = []
    for options in configurations:
        _, stats = get_modern_cdcl_stats(cnf, **options)
        cells.append(f"{stats.conflicts} / {stats.restarts}")
    print(label, stats.status, cells)
```

Result (run from the repository root with `PYTHONPATH=src:tests`; takes a few seconds). All four instances are unsatisfiable.

| Instance | `glucose` (default) | `glucose`, `glucose_k=1.0` | `luby` | `none` |
|---|---|---|---|---|
| `pigeon_hole(8)` | 3199 / 8 | 6724 / 51 | 5705 / 29 | 2785 / 0 |
| `phase_transition_3sat(150, seed=1)` | 1672 / 0 | 1090 / 3 | 1145 / 6 | 1672 / 0 |
| `phase_transition_3sat(150, seed=2)` | 4030 / 0 | 2725 / 20 | 2960 / 14 | 4030 / 0 |
| `phase_transition_3sat(150, seed=3)` | 3658 / 1 | 3337 / 25 | 2837 / 14 | 3917 / 0 |

What this shows, and what it does not:

- **The default policy rarely fires on these instances.** On these random 3-SAT formulas the LBD of learned clauses fluctuates little: in the runs for seeds 1 and 2, the 50-conflict average never rose above 1.14 times the overall average, and a restart needs more than 1.25 times. With zero restarts the run is identical to `"none"`. The Glucose condition was designed for large structured instances, where LBD varies strongly between regions of the search.
- **Restarts are not always a gain.** On the pigeonhole instance, every policy that restarts needs more conflicts than not restarting. On the three random instances, the policies that do restart need fewer.
- **Four instances are an illustration, not a benchmark.** Conflict counts on single instances vary by tens of percent with any change to the search.

## Not Implemented Here

`ModernCDCLSolver` implements the original, simplest form of the Glucose policy. Later refinements that are not included:

- **Exponential moving averages.** Instead of an exact average over a window and an exact average over the whole run, keep a fast-moving and a slow-moving exponential average of the LBD and restart when the fast one exceeds the slow one by a margin (Biere and Fröhlich, 2015). This needs no window and lets the long-term average follow the search.
- **Restart blocking** (also called postponing). Glucose 2.1 cancels a pending restart when the trail is unusually long compared with its recent average, on the grounds that the solver may be close to a satisfying assignment (Audemard and Simon, 2012).
- **Reluctant doubling**, a way of computing the Luby sequence incrementally, used by solvers that alternate between a phase with frequent dynamic restarts and a phase with rare Luby-scheduled ones.
- **Trail reuse.** When a restart would immediately re-make the same first decisions, keep that part of the trail instead of undoing it.

The C solver in `competition/c/` implements exponential-moving-average restarts, reluctant doubling, the alternation between the two, and trail reuse; see [competition/c/FEATURES.md](../competition/c/FEATURES.md).

## References

- Gomes, Selman, Kautz. "Boosting Combinatorial Search Through Randomization" (AAAI 1998). Heavy-tailed run times and the case for restarts.
- Luby, Sinclair, Zuckerman. "Optimal Speedup of Las Vegas Algorithms" (Information Processing Letters, 1993). The Luby sequence.
- Huang. "The Effect of Restarts on the Efficiency of Clause Learning" (IJCAI 2007). Luby restarts in a CDCL solver.
- Audemard, Simon. "Predicting Learnt Clauses Quality in Modern SAT Solvers" (IJCAI 2009). LBD and LBD-based restarts.
- Audemard, Simon. "Refining Restarts Strategies for SAT and UNSAT" (CP 2012). Restart blocking.
- Biere, Fröhlich. "Evaluating CDCL Restart Schemes" (Pragmatics of SAT workshop, 2015). Exponential moving averages.

## See Also

- [Modern CDCL solver](modern-cdcl.md)
- [LBD and clause database reduction](lbd-clause-management.md) - the quantity the Glucose policy averages
- [Phase saving](phase-saving.md) - what makes frequent restarts cheap
- [Terminology: Restart](terminology.md#restart)
