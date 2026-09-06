# Binary equivalence substitution experiment

`--equiv` enables bounded binary implication SCC substitution after failed-literal
probing and before BCE/BVE. It remains disabled by default: the measured development
instances showed no solved-count improvement and several substantial regressions.

## Implementation

Two iterative graph traversals identify strongly connected components without C
recursion. Reverse edges follow from binary contraposition. Complementary literals
use complementary representatives of the smallest variable in their class. A
component containing both polarities proves UNSAT. Root-false literals can expose
binary consequences inside longer clauses; these are recorded as RUP additions.

`--equiv-budget N` independently limits inspected work (default 1000000; zero skips
the pass), preserving the remaining budget for subsequent preprocessing. Traversal
checks cancellation and CPU deadlines. A replacement solver is constructed before
committing a substitution. Budget exhaustion discards that replacement; sound,
redundant binary clauses and proof additions already produced may remain.

Every existing root assignment is explicitly preserved, including probing units
without arena records. Rewritten clauses are RUP consequences of the retained
proof database. Proof output keeps original clauses and logs additions, so staged
aborts need no rollback of the proof stream. The replacement does not reopen the
proof file. Original input and proof ownership transfer only at commit.

Signed equivalences use the existing model-extension stack. Subsequent BVE/BCE
records are restored first, followed by the equivalence assignments. Every SAT
model is checked against the original input. Assumption calls skip substitution;
repeated solves rebuild the original formula. Successful substitution creates
elimination state, which disables the existing local-search hybrid through its
usual elimination-state guard.

Old and replacement solvers coexist during construction, increasing transient
memory. Search changes can further affect peak memory; the measurements below do
not isolate these contributions.

## Validation

Unit tests cover signed and positive SCCs, contradictory components, root-hidden
binaries, probing facts without arena units, composition with BCE/BVE, repeated
solves, assumptions and later formula mutations. Both positive and alternating-sign
10000-variable cycles exercise nonrecursive traversal and model reconstruction.
Budgets 0 through 149 exercise staged exits with and without probing facts.

The independent validator now checks RUP additions on SAT proof prefixes as well
as complete UNSAT proofs. It runs 34 option configurations, including text/binary
proofs, equivalence plus BCE/BVE, and small equivalence budgets. Thirteen fixed
formulas supplement generated formulas. Final validation results are recorded in
[HARDENING.md](HARDENING.md).

## Measurements

All timings used serial solver runs on macOS arm64 without concurrent builds or
tests. These are development samples, not disjoint held-out competition results.

| Experiment | Baseline | Equivalence |
|---|---:|---:|
| 28 inputs, two repeats: verified runs | 6/56 | 6/56 |
| Same run: maximum RSS, bytes | 49053696 | 103415808 |
| Grandtour, three longer repeats: verified runs | 0/3 | 0/3 |
| Longer run: maximum RSS, bytes | 8798208 | 132022272 |

The first comparison capped each solve at 30000 conflicts and three CPU seconds
(five wall seconds). Its PAR-2 values, 8.93221 and 8.93211, are diagnostics under
these combined limits, not competition scores or evidence of a speedup. Both
versions had zero reported errors. The longer exploratory Grandtour comparison
used 20 CPU seconds and 25 wall seconds, with all runs returning UNKNOWN.
Grandtour was selected after observing 456 substitutions, so this is a posthoc
follow-up rather than an independent performance test.

The pass found 212 substitutions in the multiplier circuit and 830 in the Fermat
instance, but simplification alone did not predict improvement. Fermat's mean CPU
time rose from approximately 1.15 to 2.99 seconds at the same conflict cap. Other
inputs exhausted the preprocessing budget without committing substitutions.

An earlier search-control screen tried disabling rephasing, randomness or probing,
Luby restarts, and alternating mode on twelve larger instances (two CPU seconds,
four wall seconds, one repeat). Every configuration verified the same one input;
this screen provides no reason to change defaults.

Raw evidence includes exact commands, input and executable hashes, counters and
per-run outcomes:

- [Repeated comparison](benchmark_results/equivalence-comparison-20260906.json)
- [Longer comparison](benchmark_results/equivalence-long-20260906.json)
- [Search-control screen](benchmark_results/search-ablation-20260906.json)
- [Core source delta](benchmark_results/equivalence-source-20260906.patch)

The baseline was a source snapshot after the minimization-limits milestone, not
Git HEAD. Its executable SHA256 was
`21f4aad1cbb7995d5bd376178dc7ee952f514f91d6f6ed198ea795151f60142c`;
the final candidate was
`628331dd10a2ff67ac822716843c05bdae202a3b41f7519e656c93a1c78d6f5e`.
The source delta applies to that snapshot's `src` and `include` directories.

## Existing work reviewed

[Kissat substitution](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/substitute.c)
provides an established reference for SCC-based equivalences, proof support and
model extension. This implementation independently uses two iterative passes and
a staged replacement solver. [Kissat rephasing](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/rephase.c)
was also reviewed when selecting the earlier search ablations; no rephasing policy
was changed on the basis of the inconclusive screen.
