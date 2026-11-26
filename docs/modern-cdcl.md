# Modern CDCL Solver

`ModernCDCLSolver` is a second CDCL solver in BSAT. It implements the same algorithm as [`CDCLSolver`](cdcl-solver.md) and adds the standard engineering techniques that practical solvers have used since Chaff (2001), MiniSat (2003) and Glucose (2009), written to be read rather than to be fast.

> **Need terminology help?** See the [Terminology Reference](terminology.md).

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Options](#options)
4. [Statistics](#statistics)
5. [Result Status](#result-status)
6. [Measuring One Technique (Ablation)](#measuring-one-technique-ablation)
7. [How the Solver Is Organized](#how-the-solver-is-organized)
8. [Limitations](#limitations)
9. [Technique Pages](#technique-pages)

---

## Overview

BSAT has two CDCL solvers, and they are meant to be read in order:

| | `CDCLSolver` ([cdcl-solver.md](cdcl-solver.md)) | `ModernCDCLSolver` (this page) |
|---|---|---|
| Source | `src/bsat/cdcl.py` | `src/bsat/cdcl_modern.py` |
| Purpose | The core idea of clause learning in as little code as possible | The techniques that make clause learning practical |
| Unit propagation | Scans every clause | [Two-watched literals](two-watched-literals.md) with blocking literals |
| Learned clauses | First-UIP | First-UIP plus [recursive minimization](conflict-analysis.md#clause-minimization) |
| Decision variable | VSIDS, linear scan for the maximum | [VSIDS](vsids.md) with a priority queue |
| Decision value | Always `True` | [Phase saving](phase-saving.md), optional random phases |
| Clause deletion | Keep the most recent half when a size limit is hit | [LBD-ranked reduction](lbd-clause-management.md), glue clauses kept |
| Restarts | Luby | [Glucose-style, Luby or none](restarts.md) |
| Literals | `Literal` objects | Non-zero integers (`+v` / `-v`) |

Every technique that has an on/off option can be disabled independently, so its effect can be measured by comparing statistics on the same formula. Two parts have no switch: two-watched literals (the data structure the rest is built on) and VSIDS (only its decay factor can be changed).

Internally the solver numbers the variables 1..n in sorted name order and represents a literal as a non-zero integer, `+v` for the variable and `-v` for its negation (the DIMACS convention). The public interface is unchanged: it takes a `CNFExpression` and returns a `Dict[str, bool]` or `None`.

The solver is deterministic: the same formula and options always give the same run, unless `random_phase_freq > 0` is used without a `seed`.

---

## Quick Start

### Basic Usage

```python
from bsat import CNFExpression, solve_modern_cdcl

cnf = CNFExpression.parse("(x | y | z) & (~x | y | ~z) & (x | ~y | z) & (~x | ~y)")
result = solve_modern_cdcl(cnf)

if result:
    print(f"SAT: {result}")
    print(f"Verifies: {cnf.evaluate(result)}")
else:
    print("UNSAT (or conflict limit reached)")
```

Output:

```
SAT: {'x': False, 'y': False, 'z': True}
Verifies: True
```

### With Statistics

The examples on this page and on the technique pages that import `benchmarks` use the instance generators in `tests/benchmarks.py`. Run them from the repository root with `PYTHONPATH=src:tests python3 your_script.py`.

```python
from bsat import get_modern_cdcl_stats
from benchmarks import pigeon_hole

cnf = pigeon_hole(6)            # 6 pigeons into 5 holes: unsatisfiable
result, stats = get_modern_cdcl_stats(cnf)
print(result)
print(stats)
```

Output:

```
None
ModernCDCLStats(
  Status: UNSAT
  Decisions: 190
  Propagations: 1771
  Clause visits: 4887
  Blocker skips: 3012
  Conflicts: 146
  Learned clauses: 145
  Learned literals: 910
  Minimized literals: 156
  Glue clauses: 28
  Deleted clauses: 0
  Restarts: 0
  Max decision level: 11
)
```

### Using the Solver Class

```python
from bsat import ModernCDCLSolver
from benchmarks import pigeon_hole

solver = ModernCDCLSolver(
    pigeon_hole(6),
    restart_strategy="luby",    # 'glucose' (default), 'luby' or 'none'
    restart_base=50,
    minimize_learned=True,
    phase_saving=True,
)
result = solver.solve(max_conflicts=100000)
print(solver.stats.status, solver.stats.conflicts, solver.stats.restarts)
```

Output:

```
UNSAT 152 2
```

`solve_modern_cdcl(cnf, max_conflicts=1000000, **options)` and `get_modern_cdcl_stats(cnf, max_conflicts=1000000, **options)` accept every constructor option listed below as a keyword argument.

---

## Options

All options are keyword arguments of `ModernCDCLSolver(cnf, ...)`.

| Option | Default | Meaning | Details |
|---|---|---|---|
| `restart_strategy` | `"glucose"` | `"glucose"`, `"luby"` or `"none"`. Any other value raises `ValueError`. | [Restarts](restarts.md) |
| `restart_base` | `100` | Luby only: conflicts per unit of the Luby sequence. | [Restarts](restarts.md) |
| `glucose_window` | `50` | Glucose only: number of recent conflicts in the short-term LBD average. Must be at least 1. | [Restarts](restarts.md) |
| `glucose_k` | `0.8` | Glucose only: restart when `recent_average * glucose_k > overall_average`. | [Restarts](restarts.md) |
| `phase_saving` | `True` | Decide a variable to the value it had when it was last unassigned. | [Phase saving](phase-saving.md) |
| `initial_phase` | `False` | Value tried first for a variable with no saved phase; the only value ever decided when `phase_saving=False`. | [Phase saving](phase-saving.md) |
| `random_phase_freq` | `0.0` | Probability that a decision uses a random value instead. `0` disables it. | [Phase saving](phase-saving.md#random-phases) |
| `seed` | `None` | Seed for the random phases. Only matters when `random_phase_freq > 0`. | [Phase saving](phase-saving.md#random-phases) |
| `minimize_learned` | `True` | Remove redundant literals from each learned clause. | [Conflict analysis](conflict-analysis.md#clause-minimization) |
| `reduce_db` | `True` | Periodically delete the least useful learned clauses. | [LBD](lbd-clause-management.md) |
| `reduce_interval` | `2000` | Conflicts before the first database reduction. | [LBD](lbd-clause-management.md) |
| `reduce_increment` | `300` | After the k-th reduction, the next one is due `reduce_interval + k * reduce_increment` conflicts later. | [LBD](lbd-clause-management.md) |
| `glue_limit` | `2` | Learned clauses with LBD at most this are never deleted (and are counted in `stats.glue_clauses`). | [LBD](lbd-clause-management.md) |
| `vsids_decay` | `0.95` | VSIDS decay factor. `1.0` disables the decay. | [VSIDS](vsids.md) |

`solve()` takes one argument:

| Argument | Default | Meaning |
|---|---|---|
| `max_conflicts` | `1000000` | Stop with status `UNKNOWN` once this many conflicts have occurred. |

---

## Statistics

`solver.stats` (also returned by `get_modern_cdcl_stats` and `solver.get_stats()`) is a `ModernCDCLStats` object with these fields:

| Field | What it counts |
|---|---|
| `status` | `"SAT"`, `"UNSAT"` or `"UNKNOWN"`. See [Result Status](#result-status). |
| `decisions` | Branching decisions made. |
| `propagations` | Literals assigned with a reason clause: unit propagations, plus the asserting literal of each learned clause of two or more literals. Decisions, unit clauses of the input and learned unit clauses are not counted. |
| `clause_visits` | Clauses actually inspected during propagation. |
| `blocker_skips` | Watch-list entries skipped without touching the clause, because the entry's blocking literal was true. |
| `conflicts` | Conflicts found by propagation, including the final conflict at decision level 0 of an UNSAT run. |
| `learned_clauses` | Clauses learned, including learned unit clauses. |
| `learned_literals` | Total number of literals in the learned clauses, after minimization. `learned_literals / learned_clauses` is the average learned clause length. |
| `minimized_literals` | Literals removed from learned clauses by minimization. |
| `glue_clauses` | Learned clauses whose LBD was at most `glue_limit` when they were learned. |
| `deleted_clauses` | Learned clauses removed by database reductions. |
| `reductions` | Number of database reductions performed. This field is not included in `print(stats)`. |
| `restarts` | Restarts performed. |
| `max_decision_level` | Deepest decision level reached. |

In an UNSAT run that ends with a conflict at level 0, `conflicts` is one more than `learned_clauses` (146 and 145 above), because nothing is learned from the final conflict.

---

## Result Status

`solve()` returns `None` in two different situations, and `stats.status` tells them apart:

| `stats.status` | `solve()` returns | Meaning |
|---|---|---|
| `"SAT"` | a `Dict[str, bool]` covering every variable | The assignment satisfies the formula. |
| `"UNSAT"` | `None` | The formula has no satisfying assignment. |
| `"UNKNOWN"` | `None` | `max_conflicts` was reached before an answer was found. |

```python
from bsat import ModernCDCLSolver
from benchmarks import pigeon_hole

solver = ModernCDCLSolver(pigeon_hole(8))
result = solver.solve(max_conflicts=100)
print(result, solver.stats.status, solver.stats.conflicts)
```

Output:

```
None UNKNOWN 100
```

Always check `stats.status` before treating `None` as a proof of unsatisfiability. The convenience function `solve_modern_cdcl` returns only the result, so use `get_modern_cdcl_stats` or the class when the limit might be reached.

---

## Measuring One Technique (Ablation)

To see what a technique contributes, solve the same formula twice, once with the technique switched off, and compare the statistics.

```python
from bsat import get_modern_cdcl_stats
from benchmarks import pigeon_hole

cnf = pigeon_hole(8)
configurations = [
    ("default", {}),
    ("minimize_learned=False", {"minimize_learned": False}),
    ("phase_saving=False", {"phase_saving": False}),
    ("reduce_db=False", {"reduce_db": False}),
    ("restart_strategy='luby'", {"restart_strategy": "luby"}),
    ("restart_strategy='none'", {"restart_strategy": "none"}),
]
print(f"{'configuration':<26} {'conflicts':>9} {'avg len':>8} {'deleted':>8} {'restarts':>8}")
for label, options in configurations:
    _, stats = get_modern_cdcl_stats(cnf, **options)
    average = stats.learned_literals / stats.learned_clauses
    print(f"{label:<26} {stats.conflicts:>9} {average:>8.1f} "
          f"{stats.deleted_clauses:>8} {stats.restarts:>8}")
```

Output:

```
configuration              conflicts  avg len  deleted restarts
default                         3199     15.2      997        8
minimize_learned=False          3534     18.9      999       13
phase_saving=False              2687     16.1      997        8
reduce_db=False                 3043     15.2        0        8
restart_strategy='luby'         5705     15.2     2644       29
restart_strategy='none'         2785     14.2      991        0
```

How to read a comparison like this:

- **Look at the statistic the technique is meant to change.** Minimization shortens learned clauses (`avg len` 18.9 → 15.2). Database reduction deletes clauses. The restart strategy changes `restarts`.
- **One instance proves little.** Here the default configuration needs more conflicts than three of the ablations: on this particular pigeonhole instance, phase saving, clause deletion and restarts each cost a few hundred conflicts. These techniques earn their place on average over many large, structured instances, not on every small one. Changing any option changes the whole search path, so conflict counts on a single instance move by tens of percent for no deeper reason.
- **Use several instances and seeds** before concluding anything, for example `phase_transition_3sat(n, seed=s)` for a range of `s`.
- **Prefer counts over time.** Conflicts, decisions and clause visits are reproducible; wall-clock time in Python is not.

### Compared with the Basic Solver

The same comparison can be made against `CDCLSolver`:

```python
import time
from bsat import get_cdcl_stats, get_modern_cdcl_stats
from benchmarks import pigeon_hole, phase_transition_3sat

instances = [
    ("pigeon_hole(6)", pigeon_hole(6)),
    ("pigeon_hole(7)", pigeon_hole(7)),
    ("phase_transition_3sat(60, seed=1)", phase_transition_3sat(60, seed=1)),
    ("phase_transition_3sat(80, seed=2)", phase_transition_3sat(80, seed=2)),
]
for label, cnf in instances:
    row = [label]
    for solve in (get_cdcl_stats, get_modern_cdcl_stats):
        start = time.perf_counter()
        result, stats = solve(cnf)
        elapsed = time.perf_counter() - start
        row.append(f"{'SAT' if result else 'UNSAT'} {stats.conflicts} conflicts {elapsed:.2f}s")
    print(" | ".join(row))
```

One run on one laptop (Python 3.12):

| Instance | Result | `CDCLSolver` | `ModernCDCLSolver` |
|---|---|---|---|
| `pigeon_hole(6)` | UNSAT | 177 conflicts, 0.03s | 146 conflicts, 0.00s |
| `pigeon_hole(7)` | UNSAT | 1342 conflicts, 1.63s | 654 conflicts, 0.03s |
| `phase_transition_3sat(60, seed=1)` | UNSAT | 143 conflicts, 0.06s | 159 conflicts, 0.01s |
| `phase_transition_3sat(80, seed=2)` | SAT | 146 conflicts, 0.08s | 122 conflicts, 0.01s |

The conflict counts are of the same order, because both solvers learn clauses from conflicts. The times are not: the basic solver rescans every clause after each assignment, and that cost grows with every learned clause. The timings will differ on another machine; the conflict counts will not.

---

## How the Solver Is Organized

The main loop is `ModernCDCLSolver.solve()`:

```
loop:
    conflict = _propagate()                  # two-watched literals
    if conflict:
        if decision level is 0:  return UNSAT
        learned, backjump, lbd = _analyze(conflict)   # first-UIP, minimization, LBD
        _backtrack(backjump)                 # saves phases
        _add_learned(learned, lbd)           # asserts the first literal
        _decay()                             # VSIDS
        if conflicts >= max_conflicts:  return UNKNOWN
        continue
    if decision level > 0 and _should_restart():
        _restart()
        continue
    if reduce_db and conflicts >= next_reduce:
        _reduce_database()
    v = _pick_branch_variable()              # VSIDS heap
    if no unassigned variable:  return SAT
    decide v with the value from _pick_phase(v)
```

The state it works on:

| Attribute | Contents |
|---|---|
| `names` | Variable names in sorted order; variable `v` is `names[v - 1]` |
| `value[v]` | `+1` true, `-1` false, `0` unassigned |
| `level[v]`, `reason[v]` | Decision level of the assignment, and the clause that forced it (`None` for a decision) |
| `trail`, `trail_lim` | Assigned literals in order; `trail_lim[d]` is the trail position where level `d + 1` starts |
| `clauses`, `learned` | Lists of `_Clause` objects (fields `lits`, `learned`, `lbd`, `deleted`) |
| `watches[lit]` | List of `(blocker, clause)` for every clause watching `lit` |
| `activity[v]`, `heap` | VSIDS scores and the lazy priority queue |
| `saved_phase[v]` | Value used the next time `v` is decided |

Duplicate literals are removed from input clauses, tautologies are dropped, and unit clauses of the input are assigned at level 0 while the formula is loaded.

---

## Limitations

- **Pure Python.** The solver is far slower than a compiled solver. As a rough guide, random 3-SAT at the phase transition (`phase_transition_3sat`) took about a second with 200 variables and 10 to 15 seconds with 250 variables on one laptop. Competition benchmarks are out of reach.
- **No proofs.** An UNSAT answer is not accompanied by a DRAT or LRAT certificate.
- **No preprocessing or inprocessing.** The solver searches on the formula as given. Simplification techniques are implemented separately; see [Simplification](simplification.md).
- **No incremental interface.** There are no assumptions and no way to add clauses after construction.
- **Simplified variants of some techniques.** There is no special handling of binary clauses, no clause activity, the Glucose-style restart uses plain averages with no restart blocking, and LBD values are never updated after a clause is learned. Each technique page lists what is left out.

For a solver that implements these techniques and many more in C, with proof output, see [competition/c/README.md](../competition/c/README.md).

---

## Technique Pages

Each page explains one technique and shows the code that implements it:

1. [Two-Watched Literals](two-watched-literals.md) - fast unit propagation, blocking literals
2. [Conflict Analysis](conflict-analysis.md) - first-UIP learning, clause minimization, backjumping
3. [VSIDS](vsids.md) - choosing the decision variable
4. [Phase Saving](phase-saving.md) - choosing the decision value, random phases
5. [LBD and Clause Database Reduction](lbd-clause-management.md) - which learned clauses to keep
6. [Restarts](restarts.md) - Luby and Glucose-style restarts

Related:

- [Simplification](simplification.md) - preprocessing techniques that run before search
- [CDCL Solver](cdcl-solver.md) - the basic solver; read this first
- [examples/example_cdcl_modern.py](../examples/example_cdcl_modern.py) - runnable examples, including the comparisons above
- [tests/test_cdcl_modern.py](../tests/test_cdcl_modern.py) - tests

## References

- Moskewicz, Madigan, Zhao, Zhang, Malik. "Chaff: Engineering an Efficient SAT Solver" (DAC 2001)
- Eén, Sörensson. "An Extensible SAT-solver" (SAT 2003)
- Audemard, Simon. "Predicting Learnt Clauses Quality in Modern SAT Solvers" (IJCAI 2009)
- [BSAT Reading List](reading-list.md)
