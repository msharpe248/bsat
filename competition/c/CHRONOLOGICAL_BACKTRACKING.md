# Ordered-trail chronological backtracking: rejected prototype


The prototype is **not retained**. Both profiles solved all four development
runs with verified UNSAT proofs, but the candidate slowed belpyramid by about
11 times and did not improve the hardware case. Runtime sources and active tests
were restored to `c8ab1eb`; the patch, hashes and full validation evidence remain
available for reproducibility.
Both restored builds pass their 45 C test executables, and the restored release
binary is byte-identical to the baseline. See
`benchmark_results/chrono-restoration-20260907.json`.

## Measured outcome

Seed 20261301 shuffled two repetitions per profile on two existing development
inputs. Both profiles enabled congruence, equivalence substitution with a
100,000,000-work allowance, alternating search and VMTF. The candidate additionally
enabled chronological backtracking with its default threshold of 100. Each job
had 30 solving CPU seconds and a 35-second external wall limit. Proof checking
was outside timing, and no builds or validation ran concurrently.

| Input | Baseline process CPU seconds | Prototype process CPU seconds |
| --- | --- | --- |
| Hardware model checking | 9.771, 12.172 | 13.838, 13.904 |
| Belpyramid puzzle | 1.985, 1.999 | 21.578, 22.534 |

All eight runs returned externally verified UNSAT. Hardware CPU variation in the
baseline is substantial, but neither candidate repetition was faster. Belpyramid
is a clear regression. On hardware the prototype still made 117,351,338 decisions
and incurred 163,774 conflicts, with 149,459 chronological backtracks and
211,170,802 summed replay-head rewind positions. Keeping the trail ordered by
assigning implications at the current level did not deliver the intended search
reduction. The counters suggest investigating assignment levels and retained
implications; they do not isolate a single cause of the regression.

Raw measurements are in `benchmark_results/chrono-initial-20260907.json`.
`chrono-ordered-prototype-20260907.json` links the patch, source hashes, validation
and rejection decision.


The archived experiment adds `--chrono` and `--chrono-levels N` (default 100). For a
non-unit learned clause, if ordinary backjumping would discard more than N
levels below the previous decision level, search instead keeps the prefix up to
the previous level. These options are not retained in the current solver. In the prototype, the
option is disabled by default; setting the threshold alone does not enable it.

To reproduce, apply `benchmark_results/chrono-ordered-prototype-20260907.patch`
to baseline `c8ab1eb` with `git apply --unidiff-zero`, then build and test. The
patch has been checked against that baseline. Prototype invocation:

```
bin/bsat --chrono --congruence --equiv --equiv-budget 100000000 \
  --alternating --vmtf --time 30 --proof answer.drat input.cnf
drat-trim input.cnf answer.drat
```

## Ordered trail and propagation

Assignments retain the current decision level, so the trail remains ordered and
backtracking still removes a suffix. This avoids introducing out-of-order
assignment levels into the existing analysis, minimization and restart code.
It requires explicit handling of late implications: a learned clause can imply
a literal at a higher level than all its antecedents.

When a suffix is removed, the pass inspects each removed implication's retained
false watch. If that watch remains in the prefix, the propagation head is
rewound to its trail position. Reprocessing the watch rediscovers the implication
if it is still unit; subsequent propagation rediscovers dependent implications.
Implicit binaries use their stored antecedent literal; arena-backed binary and
long reasons use watch position one. Locked reasons keep the implied literal
in position zero. This can replay extra work, which must be evaluated alongside
any reduction in repeated decisions.

A replayed watch can expose a conflict below the current decision level. Before
first-UIP analysis, the solver finds the maximum assignment level of the
conflicting literals and backtracks there. A root-level conflict terminates
UNSAT; an interrupted or exhausted preparation returns UNKNOWN. Learned units
always backjump to root so reasonless root facts cannot be lost. A restart that
retains a prefix processes any scheduled replay before making another decision.

The existing clause derivation and proof logging remain authoritative. No
learned clause is justified merely by the chosen backtrack level. SAT answers
still require validation against the immutable input; benchmark UNSAT answers
still require external proof checking. Assumption calls retain the existing
restriction against unconditional proof output.

## Counters and tests

`Chronological backtracks` counts selected chronological jumps. `Chronological
replay positions` sums propagation-head rewind distances; it is not the number
of distinct literals or a direct CPU-cost measurement. `Lower-level conflicts`
counts conflict normalizations that shorten the trail.

The dedicated C tests construct delayed implications for implicit binaries,
arena binaries and long clauses, including transitive consequences and a later
root restart. They compact the arena while reasons are locked before replay.
They check deep learned-unit persistence, lower binary/long/root
conflicts, preparation budget exhaustion, heap and VMTF modes, and exact
seven-variable formula/assumption answers under frequent reductions and retained
restart prefixes. Independent Python validation additionally exercises text and
binary proofs, congruence, equivalence substitution, portfolio search and
preprocessing combinations. Mutation checks remove replay, permit learned units
to remain above root, omit conflict normalization, or replay the implied watch.

## Motivation

The prior [backtracking investigation](BACKTRACKING_INVESTIGATION.md) found that
disabling chronological backtracking in pinned Kissat increased a hardware
development case from about one CPU second and 886,431 decisions to about six
CPU seconds and 12,923,600 decisions. This motivates an experiment in BSAT;
it does not establish that this implementation improves BSAT.

Primary references are Nadel and Ryvchin's
[Chronological Backtracking](https://doi.org/10.1007/978-3-319-94144-8_7) and the
pinned Kissat source linked in that investigation. This implementation uses
ordered-trail replay; it does not claim to reproduce Kissat's full assignment
and reimplication machinery.

## Validation evidence

The archived prototype release and ASan/UBSan builds each passed 46 C test executables,
including 12,288 exact formula/assumption solves in the chronological tests.
Each build passed 4,524 independent validator solves (87 configurations, seed
20261231) and 63 short-deadline cases, with at most 0.001 seconds observed CPU
overrun. All four deliberate mutations were caught. Twelve default trace cases
matched the prior binary and the disabled-threshold profile in return code,
status, 24 selected counters and proof bytes. Incomplete trace proofs are not
UNSAT certificates.

Commands, source/binary hashes, validation logs and deadline results are in
`benchmark_results/chrono-validation-20260907.json`; mutation and trace details
are in `chrono-mutations-20260907.json` and
`chrono-default-traces-20260907.json`. Initial test development corrected a test
that incorrectly treated a detected contradiction during clause loading as an
allocation failure. Review also added the retained-prefix restart replay guard
before final validation. These changes were not committed as intermediate
runtime versions.

## Next design to investigate

A stronger implementation should give propagated literals the maximum actual
level of their reason's antecedents (the triggering literal's level for a binary,
the maximum false-antecedent level for a long clause). A learned assertion should
receive its ordinary logical jump level even if search retains a higher prefix.
Backtracking must then retain lower-level assignments that appear later on the
trail, compact the suffix, update variable trail positions, and map the old
propagation head to the retained entries. Root facts must survive that compaction.

This also requires checking watch invariants. A lower-level conflict may have
watches below other literals in the clause; conflict preparation should restore
the two highest-level watches before backtracking. Reason locking, garbage
collection, saved-phase prefixes, restart boundaries and assumptions all need
coverage under the changed trail invariant. This is planned work, not an
implemented or validated claim.
