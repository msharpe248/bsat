# Phase feedback from improving local-search walks

## Motivation and implementation

Kissat's [walk implementation](https://github.com/arminbiere/kissat/blob/master/src/walk.c)
exports its best assignment to saved phases when the walk improves its initial
unsatisfied-clause count. BSAT previously discarded every unsuccessful walk's
assignment. This experiment tests whether phase feedback improves the existing
WalkSAT/CDCL combination, without changing variable selection inside a walk.

The experimental `--ls-save-phases` option starts off. With local search and phase
saving enabled, each strict improvement of the current walk's unsatisfied-clause
minimum saves non-root phases. Equal or worse assignments do not overwrite the
best hints. A completed satisfying assignment continues through the existing
model-transfer path. Root phases, live values, reasons, decision levels, and the
trail are not replaced by a partial local-search assignment.

The prototype writes saved phases directly at each improvement, avoiding a new
snapshot allocation. It still needs a variable scan per improvement and may
write many phase hints over one walk. Copying polls the CPU deadline every 1,024
variables. Cancellation can leave a prefix of phase hints updated; those hints
are not a claimed model. Disabling phase saving disables feedback as well. This
is a simpler implementation than Kissat's best-value/trail machinery, not a
reproduction of it.

## Verification

The prototype passes 26 C executable suites in release and ASan/UBSan builds.
Its independent prefix regression reconstructs each walk without feedback,
records the first strict minimum, and compares the hinted phases against it.
There are 6,336 feedback cases across 32 seeds, greedy/mixed/random walks,
budgets zero through 32, and enabled/disabled phase saving. The contradictory
core keeps walks active, including cases that worsen after finding their best
assignment. Root values override contradictory saved phases. Each feedback case also starts
with an active non-root decision and checks live values, reasons, levels, trail
positions, queue position, and the complete trail against their pre-walk state.

Independent validation passes 832 model/proof checks per build across sixteen
option combinations and 52 formulas. Both builds pass 42 short-deadline cases;
maximum observed CPU overrun rounds to 0.000 seconds in each. This is observed
coverage rather than a worst-case bound for initialization or every flip.

## Targeted measurements

The four recent competition development targets were run twice per profile.
Both use VMTF, trail reuse, local search every 5,000 conflicts, 10,000 flips per
attempt, and noise 0.5. Limits are ten solving CPU seconds and fifteen wall
seconds. Timing is serial and separate from builds and validation, with original
SAT models and UNSAT proofs independently checked outside timing.

[Raw targeted results](benchmark_results/walk-feedback-hybrid-20260907.json):

| Profile | Verified runs | Mean wall PAR-2 | Errors |
| --- | ---: | ---: | ---: |
| Baseline | 8/8 | 1.4932 s | 0 |
| Phase feedback | 8/8 | 1.2720 s | 0 |

The change is mixed and materially alters search:

- Hamiltonian `a497d784`: conflicts fall from 30,661 to 9,118; process CPU changes
  from 0.507–0.601 to 0.162–0.163 seconds.
- Multiplier `90bec6dc`: conflicts fall from 18,183 to 14,991; CPU changes from
  0.697–0.796 to 0.573–0.580 seconds.
- Belpyramid `5831f356`: conflicts rise from 35,136 to 35,694; CPU worsens from
  2.705–3.098 to 3.377–3.391 seconds.
- Battleship `ed6d842f`: both profiles keep the first-walk SAT win at 5,001
  conflicts, with CPU approximately 0.13 seconds.

Only battleship records a local-search win. The Hamiltonian and multiplier gains
therefore come from subsequent CDCL search, rather than a walk returning a model.
The targeted sample establishes no additional solves and is insufficient for a
default change.

## Broader evaluation

The fixed 44-input development corpus compares the same profiles, one run per
profile/input with the same limits and independent checks. The
[raw comparison](benchmark_results/walk-feedback-broad-20260907.json) records
commands, input and executable hashes, timings, and verification outcomes.

| Profile | Verified solves | Mean wall PAR-2 | Errors |
| --- | ---: | ---: | ---: |
| Baseline | 8/44 | 24.6689 s | 0 |
| Phase feedback | 8/44 | 24.6513 s | 0 |

The solved sets are identical, with no gains or losses. Overall PAR-2 differs by
about 0.07%, which does not establish a general speedup. The larger run repeats
the Hamiltonian conflict reduction (30,661 to 9,118) and multiplier reduction
(18,183 to 14,991). Belpyramid still uses more conflicts with feedback, but its
single CPU measurement is lower here, unlike the first batch. This inconsistency
reinforces the need to keep search counts and timing observations distinct.

These are selected development inputs with one run per input/profile, not a
held-out competition result. They do not justify enabling feedback by default.

## Pinned reference and retention

A further two repetitions per target compare both BSAT profiles with the pinned
Kissat build at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`, using the same limits
and independent certification. All three profiles verify 8/8 runs with no errors.
[Complete reference comparison](benchmark_results/walk-feedback-reference-20260907.json).

| Target | BSAT baseline median CPU | BSAT feedback median CPU | Kissat median CPU |
| --- | ---: | ---: | ---: |
| hamiltonian `a497d784` | 0.602418 s | 0.167870 s | 0.058635 s |
| belpyramid-puzzle `5831f356` | 3.204804 s | 3.373671 s | 1.514435 s |
| multiplier-circuits `90bec6dc` | 0.797746 s | 0.593334 s | 0.543284 s |
| battleship `ed6d842f` | 0.132794 s | 0.130167 s | 0.189277 s |

Feedback again reduces the targeted Hamiltonian and multiplier search costs,
while belpyramid remains a tradeoff. Kissat is still faster overall on these
four selected targets; none of these experiments proves competition parity.

Retain `--ls-save-phases` as an experimental, opt-in capability because its
substantial conflict reductions reproduce across batches without losing a solve
in the broader sample. Keep it off by default, keep local search off by default,
and do not change the existing default flip budget. A measured example is:

```sh
competition/c/bin/bsat --vmtf --reuse-trail --local-search --ls-max-flips 10000 --ls-save-phases input.cnf
```

Saved phases remain subject to normal CDCL assignments, random phases, and
rephasing; the option does not guarantee that every hint becomes a decision.
