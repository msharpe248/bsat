# Investigating repeated search decisions

After gate congruence and equivalence substitution, BSAT solves the original
hardware-model-checking development input in about 12 CPU seconds with roughly
120 million decisions. Congruence improves the formula substantially, but the
remaining decision count motivates investigating backtracking.

## Measured evidence

An untimed diagnostic on `65ac319` stopped after one conflict with congruence,
equivalence substitution and alternating VMTF enabled. Among 160,903 variables,
38,613 were root assigned, 50,852 were unassigned and eliminated, 2,319 occurred
only in currently satisfied live clauses, and 69,119 occurred in currently
unsatisfied clauses. No unassigned, non-eliminated variable was absent from the
live original core. Simply skipping absent variable IDs does not explain this
case. The diagnostic does not establish the cause of repeated decisions.

A serial Kissat ablation on the same development input, seed 20261230, used two
repetitions with a 15-second external wall limit. Every answer had an externally
verified UNSAT proof. No build, validation or diagnostic ran concurrently.

| Pinned Kissat configuration | Process CPU seconds | Decisions | Conflicts |
| --- | --- | ---: | ---: |
| Default | 0.987, 1.010 | 886,431 | 8,399 |
| `--chrono=0` | 6.059, 6.090 | 12,923,600 | 10,674 |

Disabling chronological backtracking substantially increased decisions and
runtime in Kissat on this case. This supports a BSAT experiment; it does not
prove that the same policy will have the same effect in BSAT or other families.

## Implementation constraints for the next experiment

BSAT currently backjumps to the highest remaining level in the learned clause.
Its trail is ordered by decision level; backtracking clears a suffix. Unit
learning relies on backtracking to root before enqueuing a reasonless unit.
Conflict analysis assumes it can find literals at the current decision level.

Chronological backtracking therefore requires more than replacing the target
level with `current_level - 1`. The experiment must specify and test assignment
levels, root-unit persistence, propagation after backtracking, conflicts below
the current level, reason validity, assumption boundaries and restart behavior.
It must preserve independently checkable learned clauses and original models.
Small exhaustive formula/assumption tests, targeted trail regressions, mutation
checks and external proof validation should precede performance comparisons.

The pinned Kissat implementation chooses chronological backtracking when the
ordinary jump would skip more than its `chronolevels` allowance (default 100).
Its conflict analysis also detects conflicts whose maximum assignment level is
below the current decision level. These associated mechanisms must be understood
before transferring the policy into BSAT.

Primary references:

- Nadel and Ryvchin, [Chronological Backtracking, SAT 2018](https://doi.org/10.1007/978-3-319-94144-8_7).
- Kissat at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`:
  [learn.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/learn.c),
  [analyze.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/analyze.c),
  and [backtrack.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/backtrack.c).

Raw evidence is preserved in
`benchmark_results/backtracking-active-diagnostic-20260907.json` (including the
diagnostic source) and `backtracking-kissat-ablation-20260907.json` (commands,
hashes, timings, counters and proof-check results). No BSAT runtime changes are
part of this investigation milestone.

The subsequent [ordered-trail prototype](CHRONOLOGICAL_BACKTRACKING.md) passed
independent correctness checks but regressed performance, especially on
belpyramid. It was archived and removed from the active solver. The next
experiment needs to address actual implication levels and retention during trail
compaction; changing the backtrack target and replaying retained watches was
insufficient.

The [logical-level implementation](LOGICAL_CHRONOLOGICAL_BACKTRACKING.md) now
addresses those invariants and has a repeated hardware speedup, with independent
model/proof checks. Its broader results and opt-in status are documented there.
