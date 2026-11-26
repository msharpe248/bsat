# Phase Saving

[VSIDS](vsids.md) chooses *which* variable to decide; the phase heuristic chooses *which value* to try first. Phase saving remembers the last value each variable had and uses it again the next time the variable is decided. This page also covers the optional random phases.

> **Implemented in** [`src/bsat/cdcl_modern.py`](../src/bsat/cdcl_modern.py): `ModernCDCLSolver._backtrack` (saves the phases) and `_pick_phase` (uses them); state in `saved_phase`.
> **Options:** `phase_saving` (default `True`), `initial_phase` (default `False`), `random_phase_freq` (default `0.0`), `seed` (default `None`).
> **Statistics:** none of its own; compare `conflicts` and `decisions` with the option on and off.
> Part of the [Modern CDCL solver](modern-cdcl.md).

---

## The Problem

A CDCL solver throws away a lot of work. A backjump from level 30 to level 5 unassigns everything decided and propagated on levels 6 to 30, although the conflict may have involved only a few of those variables. A [restart](restarts.md) unassigns everything above level 0.

Much of what is discarded was fine. Suppose the formula contains a group of clauses that the assignments on levels 6 to 20 happened to satisfy completely, and the conflict was caused elsewhere. After the backjump the solver has to satisfy that group again. If it now tries different values, it may run into conflicts there that it had already avoided once.

A fixed rule such as "always try `False` first" has exactly this problem: the values that worked are forgotten.

## The Idea

Keep one remembered value per variable. Whenever a variable is unassigned, store the value it had. Whenever the variable is decided, use the stored value.

The solver then tends to rebuild the assignment it had before the backjump or restart, except where the newly learned clause forces something different. Solved parts of the formula are re-solved the same way at once, and the search continues in the neighbourhood it was already exploring, with one more clause to guide it.

The technique was introduced as "progress saving" in the RSat solver (Pipatsrisawat and Darwiche, 2007) and is used in essentially every CDCL solver. It costs one stored value per variable and one write per unassignment.

## The Code

Phases are saved in `_backtrack`, as each assignment is undone. This covers every variable on the undone part of the trail, whether it was decided or propagated:

```python
def _backtrack(self, level: int):
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
```

They are used in `_pick_phase`, called from the main loop once [VSIDS](vsids.md) has chosen the variable `v`:

```python
def _pick_phase(self, v: int) -> bool:
    if self.random_phase_freq > 0 and self.rng.random() < self.random_phase_freq:
        return self.rng.random() < 0.5
    return self.saved_phase[v] if self.phase_saving else self.initial_phase
```

```python
self._enqueue(v if self._pick_phase(v) else -v, None)
```

The saved phase only affects decisions. A propagated variable gets whatever value its reason clause forces.

### The Initial Phase

`saved_phase` starts out as `initial_phase` for every variable, so that is the value used the first time a variable is decided. The default is `False`, which is also MiniSat's default. With `phase_saving=False` nothing is ever saved and **every** decision uses `initial_phase`.

```python
from bsat import CNFExpression, ModernCDCLSolver

cnf = CNFExpression.parse("(a | b) & (c | d)")

solver = ModernCDCLSolver(cnf)
print(solver.solve())

solver = ModernCDCLSolver(cnf, initial_phase=True)
print(solver.solve())
```

Output:

```
{'a': False, 'b': True, 'c': False, 'd': True}
{'a': True, 'b': True, 'c': True, 'd': True}
```

In the first run `a` and `c` are decided `False`, and `b` and `d` are then forced by propagation. In the second run every variable is decided `True`.

Which initial phase is better depends on the encoding. Many encodings of practical problems have solutions in which most variables are false (most cells do not hold a given value, most actions are not taken at a given step), which is one reason `False` is the usual default.

## Phase Saving and Restarts

Phase saving changes what a restart does. Without it, a restart begins the search again with only the learned clauses and the VSIDS scores carried over. With it, the solver walks back to nearly the same assignment, but takes the decisions in the current VSIDS order, which has changed since the previous attempt. The restart reorders the decisions without discarding the assignment, and this is what makes frequent restarts affordable. See [Restarts](restarts.md).

## Random Phases

### Why

Phase saving is persistent by design, and that can work against it. If the saved values keep the solver in a region with no solution, every backjump and restart leads straight back into it. Learned clauses eventually force a way out, but that can take a long time.

Occasionally deciding a random value instead is a simple form of **diversification**: it pushes the search off its habitual path now and then, while the saved phases are still used for the great majority of decisions.

### How

With `random_phase_freq = p`, each decision uses a random value with probability `p` and the usual rule otherwise. In `_pick_phase` above, the first draw from `self.rng` decides whether this decision is random, and the second one picks the value, `True` or `False` with equal probability.

When phase saving is on, a randomly chosen value is saved like any other when the variable is later unassigned, so it persists until something changes it.

```python
from bsat import CNFExpression, ModernCDCLSolver

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
solver = ModernCDCLSolver(cnf, random_phase_freq=0.05, seed=0)
result = solver.solve()
print(solver.stats.status, cnf.evaluate(result))
```

Output:

```
SAT True
```

- `random_phase_freq` defaults to `0.0`: no randomness, and the random generator is never consulted.
- `seed` initialises the solver's own `random.Random`. With a seed the run is reproducible. Without one (`seed=None`), a run with `random_phase_freq > 0` differs from one execution to the next.
- The probability is fixed for the whole run. The solver does not raise it when the search appears to be stuck.

Random phases are off by default because on most instances they are noise: a random value is, on average, a worse guess than a value that recently worked. Treat the option as something to experiment with on an instance where the default configuration does badly.

## Measuring It

The following solves six random 3-SAT instances at the phase transition (100 variables, 426 clauses) with four phase policies:

```python
from bsat import get_modern_cdcl_stats
from benchmarks import phase_transition_3sat

configurations = [
    {},                                             # phase saving (default)
    {"phase_saving": False},                        # always decide False
    {"phase_saving": False, "initial_phase": True}, # always decide True
    {"random_phase_freq": 0.05, "seed": 0},         # phase saving + 5% random
]
for seed in range(1, 7):
    cnf = phase_transition_3sat(100, seed=seed)
    stats = [get_modern_cdcl_stats(cnf, **options)[1] for options in configurations]
    print(seed, stats[0].status, [s.conflicts for s in stats])
```

Conflicts until solved (run from the repository root with `PYTHONPATH=src:tests`):

| Instance seed | Result | Phase saving | Always `False` | Always `True` | Saving + 5% random |
|---|---|---|---|---|---|
| 1 | UNSAT | 724 | 650 | 697 | 707 |
| 2 | SAT | 134 | 190 | 239 | 168 |
| 3 | UNSAT | 432 | 492 | 394 | 485 |
| 4 | UNSAT | 450 | 471 | 594 | 517 |
| 5 | UNSAT | 281 | 337 | 393 | 301 |
| 6 | UNSAT | 453 | 660 | 522 | 659 |

Phase saving needs the fewest conflicts on four of the six instances, but the margins are small and it is behind on the other two. This is the expected picture for small, uniformly random formulas: they have no sub-structure for saved phases to preserve, five of the six are unsatisfiable (where the choice of value tends to matter less, since both values of a decision have to be refuted in the end), and the default restart policy performed no restarts in any of the default runs. The benefit reported in the literature is on large structured instances with frequent restarts, which a pure-Python solver cannot run in reasonable time. As with every table of this kind, a handful of instances shows how to measure, not what is true in general.

## Not Implemented Here

- **Target phases and rephasing.** Current solvers additionally remember the values of the longest conflict-free assignment found so far and prefer those ("target" or "best" phases), and periodically reset all saved phases to the original values, inverted values, or values found by a short local search ("rephasing").
- **Adaptive random phases**, where the probability changes with the state of the search.

The C solver in `competition/c/` implements target phases and rephasing, and has random phases as an option that is off by default; see [competition/c/FEATURES.md](../competition/c/FEATURES.md).

## References

- Pipatsrisawat, Darwiche. "A Lightweight Component Caching Scheme for Satisfiability Solvers" (SAT 2007). Introduces phase saving, under the name progress saving.
- Biere, Fleury. "Chasing Target Phases" (Pragmatics of SAT workshop, 2020). Target phases and rephasing.

## See Also

- [Modern CDCL solver](modern-cdcl.md)
- [VSIDS](vsids.md) - choosing the variable
- [Restarts](restarts.md) - why phase saving and restarts belong together
