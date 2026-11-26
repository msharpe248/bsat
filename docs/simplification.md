# Formula Simplification

`bsat.simplification` implements four techniques that modern SAT solvers use to rewrite a formula into a smaller or easier one: failed literal probing, blocked clause elimination, bounded variable elimination and vivification. It also implements the bookkeeping (a *reconstruction stack*) needed to turn a solution of the rewritten formula back into a solution of the original.

The basic techniques (unit propagation, pure literals, subsumption, self-subsumption, connected components) are in a separate module and are described in [SAT Preprocessing](preprocessing.md).

> **Need terminology help?** See the [Terminology Reference](terminology.md).

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Equivalence and Equisatisfiability](#equivalence-and-equisatisfiability)
4. [Failed Literal Probing](#failed-literal-probing)
5. [Blocked Clause Elimination](#blocked-clause-elimination)
6. [Bounded Variable Elimination](#bounded-variable-elimination)
7. [Vivification](#vivification)
8. [The Reconstruction Stack](#the-reconstruction-stack)
9. [API Reference](#api-reference)
10. [How UNSAT Is Reported](#how-unsat-is-reported)
11. [Effect on Different Kinds of Formulas](#effect-on-different-kinds-of-formulas)
12. [Complexity and Limitations](#complexity-and-limitations)
13. [Relation to `bsat.preprocessing`](#relation-to-bsatpreprocessing)
14. [References](#references)

---

## Overview

A CDCL solver spends its time propagating over clauses and branching on variables. Anything that removes clauses, shortens clauses or removes variables without changing the answer makes that work cheaper, and sometimes settles the answer outright. Three terms are used for this:

- **Simplification** is the general term: any rewriting of the formula that keeps the answer (SAT or UNSAT) the same.
- **Preprocessing** is simplification done once, before search starts.
- **Inprocessing** is simplification interleaved with search: the solver pauses (typically after a restart, when no decisions are active), simplifies the formula together with the clauses it has learned so far, and resumes. Learned clauses and the units found during search give the simplifier more to work with than the original formula did.

This module is a **preprocessor**. Its techniques are the ones solvers also use for inprocessing, but here they run on a formula before any solver is called; nothing in BSAT's Python solvers calls them during search. See [Complexity and Limitations](#complexity-and-limitations).

The four techniques:

| Technique | Method | What it does |
|-----------|--------|--------------|
| Failed literal probing | `probe_failed_literals()` | Fixes variables whose opposite value leads to a conflict |
| Blocked clause elimination (BCE) | `eliminate_blocked_clauses()` | Removes clauses that can never be the only obstacle to a solution |
| Bounded variable elimination (BVE) | `eliminate_variables()` | Resolves a variable away when that does not grow the formula |
| Vivification | `vivify()` | Removes literals from clauses when propagation shows they are unnecessary |

The implementation favours clarity over speed. The C solver in `competition/c/` contains production versions of all four (see [competition/c/README.md](../competition/c/README.md) and [FEATURES.md](../competition/c/FEATURES.md)).

---

## Quick Start

```python
from bsat import CNFExpression, simplify_cnf, solve_cdcl

cnf = CNFExpression.parse(
    "(a | c) & (a | e) & (c | e) & (~e | d) & (~c | ~d) & (b | d)"
)

result = simplify_cnf(cnf)
print(f"Simplified: {result.cnf}")
print(result.stats)

if result.is_unsat:
    print("UNSAT")
else:
    model = solve_cdcl(result.cnf)          # solve what is left
    if model is None:
        print("UNSAT")
    else:
        full = result.extend_model(model)   # map back to the original
        print(f"Model of simplified formula: {model}")
        print(f"Model of original formula:   {full}")
        print(f"Satisfies original: {cnf.evaluate(full)}")
```

Output:

```
Simplified: ⊤
SimplificationStats(
  Variables: 5 → 0
  Clauses: 6 → 0
  Failed literals: 1
  Blocked clauses: 1
  Eliminated variables: 2
  Vivified clauses: 0
  Vivified literals: 0
)
Model of simplified formula: {}
Model of original formula:   {'a': True, 'b': True, 'c': True, 'd': False, 'e': False}
Satisfies original: True
```

Here simplification removed every clause, so the "solver" had nothing to do and returned the empty model. This is common for small satisfiable formulas and not typical of hard ones (see [Effect on Different Kinds of Formulas](#effect-on-different-kinds-of-formulas)). The important line is `result.extend_model(model)`: the empty model is not a solution of the original formula, and the extended one is. This formula is traced step by step in [The Reconstruction Stack](#the-reconstruction-stack).

`solve_with_simplification(cnf)` does the simplify / solve / extend sequence in one call.

A runnable tour of all four techniques is in [examples/example_simplification.py](../examples/example_simplification.py).

---

## Equivalence and Equisatisfiability

Simplification techniques come in two strengths.

**Equivalence-preserving** techniques produce a formula with exactly the same satisfying assignments. Removing a subsumed clause is an example; so is vivification. Nothing needs to be remembered: any model of the result is a model of the original.

**Satisfiability-preserving** techniques only guarantee that the result is satisfiable exactly when the original is (the two formulas are *equisatisfiable*). The result may have models that are not models of the original, or may not mention some of the original variables at all. A solver run on the result then returns an assignment that can be wrong for the original formula. These techniques must record enough information to repair the assignment afterwards. This repair is called **model reconstruction**.

| Technique | Result compared with the original | Recorded for reconstruction |
|-----------|-----------------------------------|-----------------------------|
| Failed literal probing | Equivalent once the fixed literals are added back as unit clauses | The fixed literals |
| Vivification | Equivalent | Nothing |
| Blocked clause elimination | Equisatisfiable only (gains models) | Each removed clause and its blocking literal |
| Bounded variable elimination | Equisatisfiable only (the variable disappears) | Each eliminated variable and the clauses removed with it |

Probing sits in between: it deduces that a literal must be true and then deletes the variable from the formula, so the variable's value has to be restored at the end, but no model is gained or lost.

---

## Failed Literal Probing

### The idea

Pick a literal `l`, assume it is true, and run unit propagation. If propagation reaches a conflict, no satisfying assignment can make `l` true, so `¬l` is a consequence of the formula. `l` is called a **failed literal**. The formula can then be simplified with `¬l` as if it had been a unit clause all along.

### Worked example

```
(¬a ∨ b) ∧ (¬a ∨ ¬b) ∧ (a ∨ c ∨ d) ∧ (¬c ∨ e)
```

Assume `a`. The first clause forces `b`, the second forces `¬b`: conflict. So `a` is a failed literal and `¬a` holds. Simplifying with `a = False` satisfies the first two clauses and removes `a` from the third, leaving `(c ∨ d) ∧ (¬c ∨ e)`. No other literal fails.

```python
from bsat import CNFExpression, Simplifier

cnf = CNFExpression.parse("(~a | b) & (~a | ~b) & (a | c | d) & (~c | e)")
simplifier = Simplifier(cnf)

print(simplifier.probe_failed_literals())
print(simplifier.reconstruction)
print(simplifier.result().cnf)
```

Output:

```
1
[('unit', -1)]
(c ∨ d) ∧ (¬c ∨ e)
```

(`-1` is the internal encoding of `¬a`; see [Internal representation](#internal-representation).)

### Why it is correct

Unit propagation only derives literals that follow logically from the formula and the assumption. A conflict under the assumption `l` therefore means the formula together with `l` is unsatisfiable, i.e. the formula implies `¬l`. Adding an implied literal does not change the set of models.

### How the code does it

`probe_failed_literals()` goes through the variables in order and probes both polarities of each. When a probe of `l` conflicts:

1. It propagates `¬l` on its own. If that conflicts too, both values of the variable are impossible and the formula is UNSAT.
2. Otherwise `¬l` **and every literal propagation derived from it** are fixed: each gets a `('unit', lit)` entry on the reconstruction stack, satisfied clauses are deleted and false literals are removed from the remaining clauses.

The pass is repeated until a complete pass over the variables fixes nothing, because simplifying the formula can make further literals fail.

Things to note:

- The return value, and `stats.failed_literals`, count **all variables fixed**: the failed literals plus everything implied by their negations.
- A unit clause in the input is found as a failed literal (assuming the opposite value falsifies the unit clause immediately), so probing also performs ordinary unit propagation:

```python
from bsat import CNFExpression, Simplifier

cnf = CNFExpression.parse("a & (~a | b) & (~b | c | d)")
simplifier = Simplifier(cnf)
print(simplifier.probe_failed_literals())
print(simplifier.reconstruction)
print(simplifier.result().cnf)
```

Output:

```
2
[('unit', 1), ('unit', 2)]
(c ∨ d)
```

- A probe that does not conflict teaches this implementation nothing. Production solvers extract more from probing (for example literals implied by both `l` and `¬l`, or new binary clauses by hyper-binary resolution); none of that is implemented here.

---

## Blocked Clause Elimination

### The idea

Take a clause `C` and one of its literals `l`. `C` is **blocked on `l`** if every resolvent of `C` with a clause containing `¬l` is a tautology, that is, if every clause containing `¬l` also contains the negation of some *other* literal of `C`. A blocked clause can be deleted without changing satisfiability.

The simplest case is a clause containing a pure literal `l`: no clause contains `¬l`, so there is nothing to resolve with and the condition holds trivially.

### Worked example

```
(a ∨ b) ∧ (¬a ∨ ¬b ∨ c) ∧ (b ∨ c) ∧ (¬b ∨ ¬c)
```

1. `(a ∨ b)` on `a`: the only clause with `¬a` is `(¬a ∨ ¬b ∨ c)`. The resolvent on `a` is `(b ∨ ¬b ∨ c)`, a tautology. **Blocked on `a`**; remove it.
2. `(¬a ∨ ¬b ∨ c)` on `¬a`: no clause contains `a` any more. **Blocked on `¬a`**; remove it.
3. `(b ∨ c)` on `b`: the only clause with `¬b` is `(¬b ∨ ¬c)`; the resolvent `(c ∨ ¬c)` is a tautology. **Blocked on `b`**; remove it.
4. `(¬b ∨ ¬c)` on `¬c`: no clause contains `c` any more. **Blocked on `¬c`**; remove it.

Every clause is gone. Removing a blocked clause leaves fewer resolution partners for the others, which is why one removal often enables the next.

```python
from bsat import CNFExpression, Simplifier

cnf = CNFExpression.parse("(a | b) & (~a | ~b | c) & (b | c) & (~b | ~c)")
simplifier = Simplifier(cnf)

print(simplifier.eliminate_blocked_clauses())
for step in simplifier.reconstruction:
    print(step)

result = simplifier.result()
print(result.cnf)
print(cnf.evaluate({'a': False, 'b': False, 'c': False}))
print(result.extend_model({}))
```

Output:

```
4
('blocked', 1, (1, 2))
('blocked', -1, (-2, -1, 3))
('blocked', 2, (2, 3))
('blocked', -3, (-3, -2))
⊤
False
{'a': False, 'b': True, 'c': False}
```

The empty formula `⊤` is satisfied by every assignment, including all-False, which does not satisfy the original (the fifth output line). BCE is therefore *not* equivalence-preserving, and `extend_model` is required. It turned the all-False assignment into one that satisfies the original; how it did so is explained under [The Reconstruction Stack](#the-reconstruction-stack).

### Why it is correct

Let `F` be the formula without the blocked clause `C`, and take any model of `F`. If it satisfies `C`, it is a model of `F ∧ C`. If not, every literal of `C` is false, in particular `l`. Flip `l` to true. `C` is now satisfied. The only clauses that could have become false are those containing `¬l`. Each of them contains the negation of some other literal `q` of `C`; `q` is false (all of `C` was), so `¬q` is true and the clause is still satisfied. So `F` satisfiable implies `F ∧ C` satisfiable; the other direction is trivial.

Because satisfiability is preserved, BCE can never delete all clauses of an unsatisfiable formula, and it never reports UNSAT itself.

### How the code does it

`eliminate_blocked_clauses()` scans the clause list. For each clause, `_blocking_literal` tries its literals in turn and, for each, checks every clause containing the complementary literal for a second clashing variable. If a blocking literal is found the clause is removed immediately and `('blocked', lit, clause)` is pushed. Scans are repeated until one removes nothing. The return value is the number of clauses removed, also added to `stats.blocked_clauses`.

---

## Bounded Variable Elimination

### The idea

To eliminate a variable `x`, take all clauses containing `x` and all clauses containing `¬x`, form every resolvent on `x` between the two groups, discard the tautologies, and replace both groups by the resolvents. The new formula does not mention `x` and is satisfiable exactly when the old one is.

Done for every variable in turn this is the 1960 [Davis-Putnam procedure](davis-putnam-solver.md), which can square the number of clauses at each step. **Bounded** variable elimination, introduced in the SatELite preprocessor, performs the step only for variables where the formula does not grow. With `p` clauses containing `x` and `n` containing `¬x`, the code eliminates `x` only if

```
number of distinct non-tautological resolvents  ≤  p + n + max_growth
```

With the default `max_growth=0` the number of clauses never increases.

### Worked example

```
(a ∨ b) ∧ (¬a ∨ c) ∧ (¬b ∨ c) ∧ (¬c ∨ d)
```

Variables are tried in the order `a, b, c, d`.

1. **Eliminate `a`.** `(a ∨ b)` and `(¬a ∨ c)` give one resolvent, `(b ∨ c)`. One clause replaces two. Formula: `(¬b ∨ c) ∧ (¬c ∨ d) ∧ (b ∨ c)`.
2. **Eliminate `b`.** `(b ∨ c)` and `(¬b ∨ c)` give `(c)`. Formula: `(¬c ∨ d) ∧ (c)`.
3. **Eliminate `c`.** `(c)` and `(¬c ∨ d)` give `(d)`. Formula: `(d)`. (Resolving with a unit clause is unit propagation.)
4. **Eliminate `d`.** `d` occurs only positively, so there are no resolvents and its clause is simply dropped. (Eliminating a pure variable is pure literal elimination.)

```python
from bsat import CNFExpression, Simplifier

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")
simplifier = Simplifier(cnf)

print(simplifier.eliminate_variables())
for step in simplifier.reconstruction:
    print(step)

result = simplifier.result()
print(result.cnf)
print(result.extend_model({}))
```

Output:

```
4
('eliminated', 1, [(1, 2), (-1, 3)])
('eliminated', 2, [(2, 3), (-2, 3)])
('eliminated', 3, [(3,), (-3, 4)])
('eliminated', 4, [(4,)])
⊤
{'a': True, 'b': False, 'c': True, 'd': True}
```

Each stack entry holds the eliminated variable and the clauses that were removed with it; they are exactly the clauses listed in the hand trace.

### Why it is correct

Write the clauses with `x` as `(x ∨ Aᵢ)` and those with `¬x` as `(¬x ∨ Bⱼ)`.

- *Old formula satisfiable ⇒ new formula satisfiable.* A resolvent `(Aᵢ ∨ Bⱼ)` is implied by its two parents, so any model of the old formula satisfies all resolvents.
- *New formula satisfiable ⇒ old formula satisfiable.* Take a model of the new formula. If every `Aᵢ` is true in it, set `x = False`: clauses `(x ∨ Aᵢ)` hold through `Aᵢ` and clauses `(¬x ∨ Bⱼ)` through `¬x`. Otherwise some `Aᵢ` is false; set `x = True`. Clauses with `x` now hold. For each `(¬x ∨ Bⱼ)`, either the resolvent `(Aᵢ ∨ Bⱼ)` is in the formula and, `Aᵢ` being false, forces `Bⱼ` true; or the resolvent was a tautology, so `Bⱼ` contains the negation of a literal of `Aᵢ`, and that negation is true. Either way `Bⱼ` is true.

The second part is also the recipe `extend_model` uses to choose the value of `x`.

### How the code does it

```python
simplifier.eliminate_variables(max_occurrences=10, max_growth=0)
```

- `max_occurrences`: a variable is skipped if it occurs in more than this many clauses in *either* polarity. This bounds the work per variable (at most `max_occurrences²` resolvents).
- `max_growth`: allowed increase in the number of clauses **per eliminated variable**.

Variables are tried in increasing order of their internal number, and passes over the variables are repeated until a full pass eliminates nothing. On elimination, `('eliminated', var, clauses)` is pushed with the removed clauses, which are then replaced by the resolvents (duplicates of existing clauses are merged). If the empty clause is among the resolvents, which happens exactly when `(x)` and `(¬x)` are both present, the formula is marked UNSAT.

The return value is the number of variables eliminated, also added to `stats.eliminated_variables`.

There is no limit on the *length* of resolvents, and resolvents are not checked for subsumption against the rest of the formula.

`max_growth` trades clauses for variables. On a random 3-SAT instance near the satisfiability threshold nothing can be eliminated for free, but allowing growth changes that (run from the repository root; the generator is in `tests/benchmarks.py`):

```python
import sys
sys.path.insert(0, "tests")
from bsat import Simplifier
from benchmarks import random_3sat

cnf = random_3sat(30, 100, seed=45)
for growth in (0, 2, 4, 8):
    simplifier = Simplifier(cnf)
    eliminated = simplifier.eliminate_variables(max_growth=growth)
    stats = simplifier.result().stats
    print(f"max_growth={growth}: eliminated {eliminated} variables, "
          f"{stats.original_clauses} -> {stats.final_clauses} clauses")
```

Output:

```
max_growth=0: eliminated 0 variables, 100 -> 100 clauses
max_growth=2: eliminated 2 variables, 100 -> 104 clauses
max_growth=4: eliminated 4 variables, 100 -> 111 clauses
max_growth=8: eliminated 6 variables, 100 -> 124 clauses
```

---

## Vivification

### The idea

Vivification asks, for each clause, whether unit propagation through the *rest* of the formula shows that some of its literals are unnecessary. For a clause `(l₁ ∨ l₂ ∨ ... ∨ lₖ)`, assume `¬l₁`, propagate, assume `¬l₂`, propagate, and so on. Before each literal is assumed, look at what propagation has already done to it:

| Situation | Conclusion | Action |
|-----------|------------|--------|
| Propagation hits a conflict after assuming `¬l₁ ... ¬lᵢ` | `(l₁ ∨ ... ∨ lᵢ)` is implied by the other clauses | Keep `l₁ ... lᵢ`, drop the rest |
| The next literal `lⱼ` is already **true** | `(kept literals ∨ lⱼ)` is implied by the other clauses | Keep those, drop the rest |
| The next literal `lⱼ` is already **false** | `lⱼ` cannot help satisfy the clause when the kept literals are false | Drop `lⱼ`, continue |

### Worked example

```
(a ∨ ¬b) ∧ (a ∨ b ∨ c ∨ d) ∧ (¬c ∨ e) ∧ (¬e ∨ a)
```

Take `(a ∨ b ∨ c ∨ d)` and propagate through the other three clauses.

1. Assume `¬a`. `(a ∨ ¬b)` forces `¬b`. `(¬e ∨ a)` forces `¬e`, and then `(¬c ∨ e)` forces `¬c`. No conflict.
2. `b` is already false: drop it.
3. `c` is already false: drop it.
4. `d` is unassigned: keep it and assume `¬d`. No conflict, and no literals are left.

The clause becomes `(a ∨ d)`. None of the other clauses can be shortened.

```python
from bsat import CNFExpression, Simplifier

cnf = CNFExpression.parse("(a | ~b) & (a | b | c | d) & (~c | e) & (~e | a)")
simplifier = Simplifier(cnf)
print(simplifier.vivify())
result = simplifier.result()
print(result.cnf)
print(result.stats.vivified_clauses, result.stats.vivified_literals)
```

Output:

```
1
(¬b ∨ a) ∧ (a ∨ d) ∧ (¬c ∨ e) ∧ (¬e ∨ a)
1 2
```

(One clause shortened, two literals removed. Clauses are printed with their literals in internal order, so `(a ∨ ¬b)` appears as `(¬b ∨ a)`.)

One small formula for each row of the table, plus one where nothing happens:

```python
from bsat import CNFExpression, Simplifier

for formula in [
    "(a | b | c) & (a | e) & (a | ~e)",      # conflict
    "(a | b | c) & (a | e) & (~e | b)",      # later literal implied true
    "(a | b | c) & (a | ~b)",                # later literal implied false
    "(a | b | c) & (~a | b)",                # depends on literal order
]:
    simplifier = Simplifier(CNFExpression.parse(formula))
    shortened = simplifier.vivify()
    result = simplifier.result()
    print(f"{formula}")
    print(f"  -> {result.cnf}   "
          f"(clauses shortened: {shortened}, literals removed: {result.stats.vivified_literals})")
```

Output:

```
(a | b | c) & (a | e) & (a | ~e)
  -> a ∧ (¬e ∨ a)   (clauses shortened: 2, literals removed: 3)
(a | b | c) & (a | e) & (~e | b)
  -> (a ∨ b) ∧ (a ∨ e) ∧ (¬e ∨ b)   (clauses shortened: 1, literals removed: 1)
(a | b | c) & (a | ~b)
  -> (a ∨ c) ∧ (¬b ∨ a)   (clauses shortened: 1, literals removed: 1)
(a | b | c) & (~a | b)
  -> (a ∨ b ∨ c) ∧ (¬a ∨ b)   (clauses shortened: 0, literals removed: 0)
```

- First formula: assuming `¬a` forces both `e` and `¬e`, so `(a ∨ b ∨ c)` shrinks to `(a)`. `(a ∨ e)` then shrinks to `(a)` as well and the duplicate is merged. `(¬e ∨ a)` is left alone even though `(a)` subsumes it: vivification is not subsumption.
- Second formula: assuming `¬a` forces `e` and then `b`, so `(a ∨ b)` is implied and `c` is dropped.
- Third formula: assuming `¬a` forces `¬b`, so `b` is dropped.
- Fourth formula: `(b ∨ c)` *is* implied (resolve the two clauses on `a`), but the literals are visited in the order `a, b, c`, and after assuming `¬a` the clause `(¬a ∨ b)` is already satisfied and propagates nothing. Vivification depends on the order in which literals are visited. This particular strengthening is what self-subsuming resolution finds (see [Relation to `bsat.preprocessing`](#relation-to-bsatpreprocessing)).

### Why it is correct

Call the clause being vivified `C` and the rest of the formula `F`. In the first two rows of the table the shortened clause `C'` is implied by `F` alone (assuming all its literals false leads `F` to a contradiction), and `C'` is a subset of `C`, so `C'` implies `C`. In the third row, `F` together with the negated kept literals implies `¬lⱼ`; any model of `F ∧ C` in which the kept literals are false must satisfy `C` through some literal other than `lⱼ`, so `C` without `lⱼ` holds in every model. In all cases `F ∧ C'` has exactly the models of `F ∧ C`. Vivification is equivalence-preserving and pushes nothing on the reconstruction stack.

### How the code does it

`vivify()` walks the clause list by position. Clauses with fewer than two literals are skipped. For the others it propagates through all *other* clauses as described above, visiting the literals in the clause's internal order (negative literals first, then positive literals by variable number). If the clause got shorter it is replaced, duplicates are merged, and the same position is examined again; otherwise the walk moves on.

The return value is the number of shortenings, added to `stats.vivified_clauses`; `stats.vivified_literals` counts the literals removed.

`vivify()` never sets the UNSAT flag itself, even if it produces contradictory unit clauses (see [How UNSAT Is Reported](#how-unsat-is-reported)).

---

## The Reconstruction Stack

### Step kinds

`Simplifier.reconstruction` is a list of steps in the order they were applied. There are three kinds:

| Step | Pushed by | Meaning |
|------|-----------|---------|
| `('unit', lit)` | `probe_failed_literals` | `lit` was found to be forced and its variable was removed from the formula |
| `('blocked', lit, clause)` | `eliminate_blocked_clauses` | `clause` was removed; it was blocked on `lit` |
| `('eliminated', var, clauses)` | `eliminate_variables` | `var` was resolved away; `clauses` are the clauses containing `var` or `¬var` that were removed |

Vivification pushes nothing.

### Replaying the stack

`extend_model(model)` starts from the given model of the simplified formula, with every original variable that the model does not mention set to False. It then walks the stack **from last to first** and undoes each step:

| Step | Undo |
|------|------|
| `('unit', lit)` | Make `lit` true |
| `('blocked', lit, clause)` | If no literal of `clause` is true, make `lit` true |
| `('eliminated', var, clauses)` | Set `var` to True if some saved clause `(var ∨ A)` has every literal of `A` false; otherwise set it to False |

The result assigns every variable of the original formula.

### Why reverse order is correct

Call the original formula `F₀` and the formula after step `i` `Fᵢ`; the simplified formula is `Fₙ`. Each undo rule has this property: *given an assignment that satisfies `Fᵢ`, it produces an assignment that satisfies `Fᵢ₋₁`*. This is exactly what the correctness arguments for the individual techniques show:

- **Unit**: `Fᵢ` is `Fᵢ₋₁` simplified under the fixed literals, so an assignment satisfying `Fᵢ` that also makes those literals true satisfies `Fᵢ₋₁`.
- **Blocked**: `Fᵢ₋₁` is `Fᵢ` plus the blocked clause. Flipping the blocking literal when the clause is false satisfies it and breaks nothing in `Fᵢ`.
- **Eliminated**: the value chosen for the variable satisfies all the removed clauses, given that the resolvents (which are in `Fᵢ`) hold.

The assignment handed to `extend_model` satisfies `Fₙ`. Undoing step `n` gives one satisfying `Fₙ₋₁`, undoing step `n-1` gives one satisfying `Fₙ₋₂`, and so on down to `F₀`. The order matters because each undo rule is only valid against the formula that existed when its step was applied: a clause blocked late in the run may not have been blocked in the original formula.

Note that undoing a blocked clause can flip a variable that still occurs in the simplified formula, so the extended model does not necessarily agree with the solver's model on the variables they share.

### Worked example

This is the formula from the Quick Start, simplified one technique at a time:

```python
from bsat import CNFExpression, Simplifier

cnf = CNFExpression.parse(
    "(a | c) & (a | e) & (c | e) & (~e | d) & (~c | ~d) & (b | d)"
)
simplifier = Simplifier(cnf)
print("names:", simplifier.names)

simplifier.probe_failed_literals()
print("after probing:", simplifier.result().cnf)
simplifier.eliminate_blocked_clauses()
print("after BCE:    ", simplifier.result().cnf)
simplifier.eliminate_variables()
print("after BVE:    ", simplifier.result().cnf)

print("reconstruction stack:")
for step in simplifier.reconstruction:
    print("  ", step)

result = simplifier.result()
full = result.extend_model({})
print(full)
print(cnf.evaluate(full))
```

Output:

```
names: ['a', 'b', 'c', 'd', 'e']
after probing: (c ∨ e) ∧ (¬e ∨ d) ∧ (¬d ∨ ¬c) ∧ (b ∨ d)
after BCE:     (c ∨ e) ∧ (¬e ∨ d) ∧ (¬d ∨ ¬c)
after BVE:     ⊤
reconstruction stack:
   ('unit', 1)
   ('blocked', 2, (2, 4))
   ('eliminated', 3, [(3, 5), (-4, -3)])
   ('eliminated', 4, [(-5, 4), (-4, 5)])
{'a': True, 'b': True, 'c': True, 'd': False, 'e': False}
True
```

What happened going forward (variables are numbered `a`=1 ... `e`=5):

1. **Probing.** Assume `¬a`: `(a ∨ c)` forces `c`, `(a ∨ e)` forces `e`, `(¬e ∨ d)` forces `d`, and `(¬c ∨ ¬d)` is falsified. So `a` is forced: `('unit', 1)`. The two clauses containing `a` are satisfied and removed.
2. **BCE.** `b` is pure, so `(b ∨ d)` is blocked on `b`: `('blocked', 2, (2, 4))`. No other clause is blocked.
3. **BVE on `c`.** `(c ∨ e)` and `(¬c ∨ ¬d)` are replaced by their resolvent `(¬d ∨ e)`: `('eliminated', 3, [(3, 5), (-4, -3)])`.
4. **BVE on `d`.** The remaining clauses are `(¬e ∨ d)` and `(¬d ∨ e)`. Their resolvent `(¬e ∨ e)` is a tautology, so both are removed and nothing is added: `('eliminated', 4, [(-5, 4), (-4, 5)])`.

The formula is now empty and its model is `{}`. Replaying backwards, starting with every variable False:

| Undo | Check | Effect |
|------|-------|--------|
| eliminated `d`, clauses `(d ∨ ¬e)`, `(¬d ∨ e)` | The only clause containing `d` is `(d ∨ ¬e)`; its other literal `¬e` is true (`e` is False) | `d = False` |
| eliminated `c`, clauses `(c ∨ e)`, `(¬c ∨ ¬d)` | `(c ∨ e)` has its other literal `e` false | `c = True` |
| blocked `(b ∨ d)` on `b` | `b` and `d` are both False, so the clause is false | flip: `b = True` |
| unit `a` | | `a = True` |

Result: `a=True, b=True, c=True, d=False, e=False`, as printed.

---

## API Reference

All names below are exported from `bsat`.

### `Simplifier`

```python
Simplifier(cnf: CNFExpression)
```

Holds a working copy of the formula; the `CNFExpression` passed in is not modified. Each technique is a method that can be called on its own, in any order, any number of times:

| Method | Returns |
|--------|---------|
| `probe_failed_literals()` | Number of variables fixed |
| `eliminate_blocked_clauses()` | Number of clauses removed |
| `eliminate_variables(max_occurrences=10, max_growth=0)` | Number of variables eliminated |
| `vivify()` | Number of clauses shortened |
| `result()` | A `SimplificationResult` for the current state |
| `extend_model(model)` | The model extended to the original formula |

Call `result()` after the last technique. Once the formula has been found UNSAT, `probe_failed_literals`, `eliminate_blocked_clauses`, `eliminate_variables` and `vivify` return without doing anything.

Attributes that are useful for study: `names` (variable names, sorted), `clauses` (current clauses in internal form), `reconstruction` (the stack), `unsat` (the UNSAT flag) and `stats`.

#### Internal representation

Variable names are sorted (as strings, so `x10` comes before `x2`) and numbered from 1. A literal is `+v` or `-v`; a clause is a sorted tuple of such integers, for example `(-2, 1)` for `(a ∨ ¬b)`. On construction, tautological clauses and duplicate clauses are dropped and repeated literals within a clause are merged.

One visible consequence: clauses in `result.cnf` have their literals in sorted integer order (negative literals first), which may differ from the order in the input.

### `simplify_cnf`

```python
simplify_cnf(cnf,
             probing=True,
             blocked_clauses=True,
             variable_elimination=True,
             vivification=True,
             max_occurrences=10) -> SimplificationResult
```

Runs the enabled techniques **once each, in this fixed order**:

1. `probe_failed_literals()`
2. `vivify()`
3. `eliminate_blocked_clauses()`
4. `eliminate_variables(max_occurrences=max_occurrences)`

`max_occurrences` is passed to variable elimination. `max_growth` is not exposed here and stays at 0; use `Simplifier` directly to change it. The sequence is not repeated to a fixpoint, and a second call on `result.cnf` often simplifies further, because variable elimination creates new opportunities for the earlier techniques. If you do that, apply both results' `extend_model` functions, the later one first.

Because the techniques overlap, the order decides which one gets the credit:

```python
from bsat import CNFExpression, simplify_cnf

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")
for options in [
    {},
    {"probing": False},
    {"probing": False, "blocked_clauses": False},
    {"probing": False, "blocked_clauses": False, "variable_elimination": False},
]:
    stats = simplify_cnf(cnf, **options).stats
    print(options)
    print(f"  failed={stats.failed_literals} blocked={stats.blocked_clauses} "
          f"eliminated={stats.eliminated_variables} clauses left={stats.final_clauses}")
```

Output:

```
{}
  failed=2 blocked=1 eliminated=0 clauses left=0
{'probing': False}
  failed=0 blocked=4 eliminated=0 clauses left=0
{'probing': False, 'blocked_clauses': False}
  failed=0 blocked=0 eliminated=4 clauses left=0
{'probing': False, 'blocked_clauses': False, 'variable_elimination': False}
  failed=0 blocked=0 eliminated=0 clauses left=4
```

### `solve_with_simplification`

```python
solve_with_simplification(cnf, solver=None, **options) -> Optional[Dict[str, bool]]
```

Calls `simplify_cnf(cnf, **options)`; returns `None` if that proves UNSAT; otherwise calls `solver(result.cnf)`, returns `None` if the solver does, and otherwise returns `result.extend_model(model)`.

`solver` is any function from a `CNFExpression` to a model or `None`. The default is `solve_modern_cdcl` (see [Modern CDCL Solver](modern-cdcl.md)).

```python
from bsat import CNFExpression, solve_with_simplification, solve_sat

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | c) & (~c | d)")

solution = solve_with_simplification(cnf)
print(solution, cnf.evaluate(solution))

# Any function CNFExpression -> dict-or-None can be the solver; options go to simplify_cnf
solution = solve_with_simplification(cnf, solver=solve_sat, blocked_clauses=False)
print(solution, cnf.evaluate(solution))
```

Output:

```
{'a': True, 'b': False, 'c': True, 'd': True} True
{'a': True, 'b': False, 'c': True, 'd': True} True
```

If `solver` is an incomplete solver such as WalkSAT, a `None` result means "no solution found", as it does without simplification.

### `SimplificationResult`

| Member | Meaning |
|--------|---------|
| `cnf` | The simplified formula. A single empty clause (printed `⊥`) if simplification proved UNSAT; no clauses (printed `⊤`) if everything was removed |
| `is_unsat` | `True` if simplification alone proved the formula unsatisfiable |
| `stats` | A `SimplificationStats` |
| `extend_model(model)` | Takes a satisfying assignment of `cnf`, returns a satisfying assignment of the original formula covering all its variables |

`extend_model` assumes its argument really satisfies `result.cnf`; it does not check. Passing anything else gives a meaningless result.

### `SimplificationStats`

| Field | Meaning |
|-------|---------|
| `original_vars` | Number of variables in the input formula |
| `original_clauses` | Number of clauses in the input formula, as given (before duplicates and tautologies are dropped) |
| `final_vars` | Number of variables occurring in `result.cnf` |
| `final_clauses` | Number of clauses left; 0 if the formula was proved UNSAT |
| `failed_literals` | Variables fixed by probing (failed literals and everything their negations imply) |
| `blocked_clauses` | Clauses removed by BCE |
| `eliminated_variables` | Variables removed by BVE |
| `vivified_clauses` | Number of times vivification shortened a clause |
| `vivified_literals` | Literals removed by vivification |

Counters accumulate over repeated calls on the same `Simplifier`. `print(stats)` gives the multi-line summary shown in the Quick Start.

---

## How UNSAT Is Reported

Simplification can prove a formula unsatisfiable in three ways:

- the input contains an empty clause;
- probing finds a variable for which both polarities fail;
- variable elimination resolves `(x)` with `(¬x)` and obtains the empty clause.

In each case `result.is_unsat` is `True` and `result.cnf` consists of a single empty clause, which every BSAT solver reports as UNSAT. `stats.final_vars` and `stats.final_clauses` are both 0 in this case, even though `len(result.cnf.clauses)` is 1.

`is_unsat == False` does **not** mean the formula is satisfiable; it only means simplification did not settle the question. In particular, vivification and blocked clause elimination never set the flag:

```python
from bsat import CNFExpression, simplify_cnf, solve_with_simplification, solve_sat

cnf = CNFExpression.parse("(a | b) & (a | ~b) & (~a | b) & (~a | ~b)")

result = simplify_cnf(cnf)
print(result.is_unsat)
print(result.cnf, len(result.cnf.clauses))
print(result.stats.final_vars, result.stats.final_clauses)
print(solve_with_simplification(cnf))

# Vivification alone does not notice the contradiction it leaves behind
result = simplify_cnf(cnf, probing=False, blocked_clauses=False, variable_elimination=False)
print(result.is_unsat, result.cnf)
print(solve_sat(result.cnf))
```

Output:

```
True
⊥ 1
0 0
None
False a ∧ ¬b ∧ ¬a
None
```

So the pattern is always: check `is_unsat`, otherwise solve `result.cnf`, and treat an UNSAT answer from the solver as UNSAT for the original formula.

---

## Effect on Different Kinds of Formulas

How much simplification achieves depends heavily on the formula. The following runs `simplify_cnf` with default settings on instances from the generators in `tests/benchmarks.py` (run from the repository root):

```python
import sys
sys.path.insert(0, "tests")
from bsat import simplify_cnf
from benchmarks import (graph_coloring_hard, pigeon_hole, xor_chain,
                        xor_chain_contradiction, random_3sat)

instances = [
    ("graph_coloring_hard(8, 4, density=0.3, seed=49)", graph_coloring_hard(8, 4, density=0.3, seed=49)),
    ("graph_coloring_hard(12, 3, density=0.5, seed=1)", graph_coloring_hard(12, 3, density=0.5, seed=1)),
    ("xor_chain(20)", xor_chain(20)),
    ("xor_chain_contradiction(10)", xor_chain_contradiction(10)),
    ("pigeon_hole(5)", pigeon_hole(5)),
    ("random_3sat(20, 60, seed=1)", random_3sat(20, 60, seed=1)),
    ("random_3sat(30, 100, seed=45)", random_3sat(30, 100, seed=45)),
    ("random_3sat(100, 426, seed=1)", random_3sat(100, 426, seed=1)),
]
for name, cnf in instances:
    r = simplify_cnf(cnf)
    s = r.stats
    print(f"{name}: vars {s.original_vars}->{s.final_vars}, clauses {s.original_clauses}->{s.final_clauses}, "
          f"failed {s.failed_literals}, blocked {s.blocked_clauses}, eliminated {s.eliminated_variables}, "
          f"vivified {s.vivified_clauses}, is_unsat {r.is_unsat}")
```

Output, arranged as a table:

| Instance | Variables | Clauses | Failed | Blocked | Eliminated | Vivified | `is_unsat` |
|----------|-----------|---------|--------|---------|------------|----------|------------|
| `graph_coloring_hard(8, 4, density=0.3, seed=49)` | 32 → 0 | 88 → 0 | 0 | 50 | 21 | 0 | False |
| `graph_coloring_hard(12, 3, density=0.5, seed=1)` | 36 → 24 | 156 → 108 | 0 | 36 | 12 | 0 | False |
| `xor_chain(20)` | 38 → 0 | 74 → 0 | 0 | 74 | 0 | 0 | False |
| `xor_chain_contradiction(10)` | 26 → 0 | 68 → 0 | 0 | 0 | 26 | 0 | True |
| `pigeon_hole(5)` | 20 → 15 | 45 → 40 | 0 | 0 | 5 | 0 | False |
| `random_3sat(20, 60, seed=1)` | 20 → 12 | 60 → 46 | 0 | 7 | 7 | 5 | False |
| `random_3sat(30, 100, seed=45)` | 30 → 30 | 100 → 100 | 0 | 0 | 0 | 0 | False |
| `random_3sat(100, 426, seed=1)` | 100 → 100 | 426 → 426 | 0 | 0 | 0 | 0 | False |

What these eight instances show (they are single instances, not a benchmark):

- Encodings with definitional structure can collapse completely. The satisfiable XOR chain is removed entirely by BCE; the contradictory one is refuted by BVE alone, without search.
- The 4-colour graph colouring instance disappears; the 3-colour one loses about a third of its variables and clauses and still needs a solver (which finds it unsatisfiable).
- `pigeon_hole(5)` is unsatisfiable, but simplification removes only a few variables and does not detect it (`is_unsat` is False). The remaining formula is still UNSAT and still needs a solver.
- Random 3-SAT near the satisfiability threshold (ratio ≈ 4.26) is untouched in these two instances: nothing fails, nothing is blocked, and no variable can be eliminated without adding clauses. Under-constrained random 3-SAT (ratio 3) does shrink.

---

## Complexity and Limitations

**Propagation is a naive scan.** The helper `_propagate` re-reads every clause until nothing changes: one pass costs time proportional to the total number of literals `L`, and up to `n` passes may be needed (`n` variables), so a single propagation is `O(n · L)` in the worst case. Real solvers use [two watched literals](two-watched-literals.md) and occurrence lists to avoid this.

Every technique is built on that scan or on scans of the whole clause list:

| Technique | Work per pass |
|-----------|---------------|
| Probing | Two propagations per variable; the pass is repeated as long as the previous one fixed something |
| Vivification | Up to one propagation per literal of every clause |
| BCE | For each literal of each clause, a scan of all clauses |
| BVE | For each variable, a scan of all clauses plus at most `max_occurrences²` resolvents |

In practice the cost grows roughly quadratically with formula size. As a rough indication, on the machine used to write this page, `simplify_cnf` on `random_3sat(n, int(4.26 * n), seed=1)` (generator from `tests/benchmarks.py`) took about 0.03 s for n = 50, 0.10 s for n = 100, 0.38 s for n = 200 and 1.67 s for n = 400, while removing less than 2% of the clauses in each case. Timings will differ on other machines. The module is meant for learning and for small formulas.

**Other limitations:**

- **No inprocessing.** Simplification runs before search and is not interleaved with it. BSAT's Python solvers, including [`ModernCDCLSolver`](modern-cdcl.md), search on the formula they are given and never call this module.
- **One pass per technique** in `simplify_cnf`, in a fixed order, with no work budget. Production solvers schedule techniques repeatedly and limit the effort spent on each.
- **Probing only uses conflicts.** No equivalent-literal detection, no hyper-binary resolution, no lifting of literals implied by both polarities.
- **BVE** has no resolvent-length limit, no gate detection and no subsumption check on resolvents.
- **Vivification** depends on the fixed literal order described above and does not remove subsumed clauses.
- **No subsumption or self-subsumption** in this module; see the next section.
- **No proof output.** Nothing is recorded that would let an UNSAT result be checked independently.
- **Not incremental.** BCE and BVE are only valid for the formula as it stands. Adding clauses to `result.cnf` afterwards (for example a blocking clause to enumerate further solutions) and then calling `extend_model` is not sound in general, because a new clause could mention an eliminated variable or invalidate a blocking argument. Enumerating or counting solutions on `result.cnf` is likewise meaningless after BCE or BVE, since those steps change the set of models.

---

## Relation to `bsat.preprocessing`

The two modules are independent and cover different techniques:

| Technique | `bsat.preprocessing` | `bsat.simplification` |
|-----------|----------------------|-----------------------|
| Connected component decomposition | yes | no |
| Unit propagation | yes | as a by-product of probing (and of BVE) |
| Pure literal elimination | yes | as a special case of BCE and BVE |
| Subsumption | yes | no |
| Self-subsumption | yes | no |
| Failed literal probing | no | yes |
| Blocked clause elimination | no | yes |
| Bounded variable elimination | no | yes |
| Vivification | no | yes |

They also report results differently. [`preprocess_cnf`](preprocessing.md) returns the simplified formula together with a dictionary of forced `assignments` that the caller merges into the solver's model. `simplify_cnf` returns a formula and an `extend_model` function, because for BCE and BVE the values of the removed variables are not known until the rest of the formula has been solved.

---

## References

- Davis, M. and Putnam, H. (1960). "A Computing Procedure for Quantification Theory". *Journal of the ACM* 7(3). The resolution-based elimination rule that BVE applies selectively. See also [Davis-Putnam Solver](davis-putnam-solver.md).
- Freeman, J. W. (1995). *Improvements to Propositional Satisfiability Search Algorithms*. PhD thesis, University of Pennsylvania. Failed literals.
- Le Berre, D. (2001). "Exploiting the Real Power of Unit Propagation Lookahead". LICS 2001 Workshop on Theory and Applications of Satisfiability Testing. Failed literal probing as a simplification technique.
- Kullmann, O. (1999). "On a Generalization of Extended Resolution". *Discrete Applied Mathematics* 96-97. Defines blocked clauses.
- Eén, N. and Biere, A. (2005). "Effective Preprocessing in SAT Through Variable and Clause Elimination". SAT 2005. The SatELite preprocessor: bounded variable elimination.
- Piette, C., Hamadi, Y. and Saïs, L. (2008). "Vivifying Propositional Clausal Formulae". ECAI 2008.
- Järvisalo, M., Biere, A. and Heule, M. (2010). "Blocked Clause Elimination". TACAS 2010.
- Järvisalo, M., Heule, M. and Biere, A. (2012). "Inprocessing Rules". IJCAR 2012. A general framework for inprocessing and model reconstruction.
- Biere, A., Järvisalo, M. and Kiesl, B. (2021). "Preprocessing in SAT Solving". In *Handbook of Satisfiability*, 2nd edition, IOS Press.

### Related pages

- [SAT Preprocessing](preprocessing.md) - unit propagation, pure literals, subsumption, self-subsumption, decomposition
- [Davis-Putnam Solver](davis-putnam-solver.md) - unbounded variable elimination as a complete decision procedure
- [Modern CDCL Solver](modern-cdcl.md) - the default solver used by `solve_with_simplification`
- [examples/example_simplification.py](../examples/example_simplification.py) - runnable examples for every technique
