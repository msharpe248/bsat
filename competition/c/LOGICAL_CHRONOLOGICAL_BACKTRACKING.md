# Chronological backtracking with logical assignment levels

This experiment replaces the rejected
[ordered-trail replay design](CHRONOLOGICAL_BACKTRACKING.md). It assigns a
propagated literal the maximum assignment level of the antecedents in its
chosen reason, instead of the current decision level. A binary implication
inherits its antecedent's level; a long implication includes every false
antecedent, including non-watched literals. A learned assertion receives its
ordinary logical jump level, even when chronological search keeps a higher
decision prefix. Learned units therefore remain level-zero facts.

`--chrono` enables the experiment. `--chrono-levels N` defaults to 100 and
controls how many levels an ordinary jump may skip before using the previous
decision level instead. Supplying the threshold alone does not enable the mode.
The mode is retained as opt-in; defaults are unchanged.

```
bin/bsat --chrono --congruence --equiv --equiv-budget 100000000 \
  --alternating --vmtf --time 30 --proof answer.drat input.cnf
drat-trim input.cnf answer.drat
```

## Trail, watches and conflict analysis

Assignment levels may now appear out of order on the trail. Backtracking keeps
the unchanged prefix, filters the suffix by assignment level, compacts retained
entries in order and updates their variable trail positions. Higher-level
assignments lose their values and reasons and return to the decision order.
Retained suffix assignments are repropagated, since watches may have changed
while higher-level assignments were present. Saved-phase prefixes are invalidated
when compaction can change their contents. Restart decisions wait for pending
propagation.

Conflict preparation finds the highest actual assignment level and normalizes
the current decision level before analysis. For a long conflict, it repairs the
two watches to cover the highest-level literals before backtracking. The original
clause and its proof meaning are unchanged. First-UIP traversal explicitly skips
seen lower-level entries, even if they occur later on the trail. This last
requirement was caught by the initial exact tests and fixed before independent
validation or performance timing.

The mode does not implement eager lowering of an already assigned literal when
another reason becomes available. The level is derived from the chosen reason,
not claimed to be the globally smallest possible level. The default solver's
backtracking path remains separate.

## Correctness checks

The C tests cover delayed implicit and arena-backed binary implications, long
reasons with higher-level non-watched antecedents, transitive implications,
root-unit retention, compacted trail positions, garbage collection with locked
reasons, repropagation, repaired conflict watches, lower-level conflicts and
current-level UIP selection. Exact seven-variable formula/assumption tests also
exercise heap/VMTF search, frequent reductions and retained restart prefixes.

Eight deliberate mutations test those invariants: assigning a binary implication
at the current level, omitting long-reason maxima, discarding retained
implications, leaving stale trail positions, skipping retained propagation,
assigning learned assertions at the current level, selecting a lower-level seen
literal during analysis, and leaving a stale conflict watch.

Independent validation checks original SAT models, truth-table answers and
text/binary RUP proofs, with external `drat-trim` checks for UNSAT. It includes
aggressive threshold-zero search, assumptions in the C suite, retained-prefix
restarts, congruence, equivalence substitution, portfolio search and elimination.
Timeouts remain UNKNOWN. These checks are evidence, not formal verification of
all executions.

The implementation follows the assignment-level and suffix-retention mechanisms
in pinned Kissat `8af8e56f174b778aef3aa45af9f739b2a5f492c2`, notably
[inlineassign.h](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/inlineassign.h),
[backtrack.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/backtrack.c)
and [analyze.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/analyze.c).
The broader motivation is recorded in
[BACKTRACKING_INVESTIGATION.md](BACKTRACKING_INVESTIGATION.md).

## Validation and initial measurements

Both release and ASan/UBSan builds passed 46 C test executables. The dedicated
test completed 12,288 exact formula/assumption solves, exercising 936
chronological backtracks and 148 lower-level conflicts. Each build also passed
4,524 independent validator solves (87 configurations, seed 20261302) and 63
deadline cases, with at most 0.001 seconds observed CPU overrun. All eight
mutations were detected. Twelve default trace cases matched the baseline and
disabled-threshold profile in status, exit code, 24 selected counters and proof
bytes. This establishes trace compatibility, not zero runtime overhead.

The initial serial comparison used two repetitions on two existing development
inputs, seed 20261303, 30 solving CPU seconds and a 35-second external wall
limit. All profiles enabled congruence, equivalence substitution with a
100,000,000-work allowance, alternating search and VMTF. Both prototypes added
chronological backtracking with threshold 100. No builds or validation ran
concurrently; proof checking was outside solver timing. All 12 answers were
externally verified UNSAT.

| Input | Baseline CPU seconds | Ordered-trail prototype | Logical-level design |
| --- | --- | --- | --- |
| Hardware model checking | 12.447, 13.060 | 15.680, 14.720 | 1.992, 2.003 |
| Belpyramid puzzle | 2.558, 2.605 | 24.219, 23.592 | 2.470, 2.460 |

On hardware, decisions fell from 119,937,348 to 585,483. The new design made
19,119 conflicts versus 17,752 in the baseline, while retaining 1,595,269
assignments across 10,388 chronological backtracks. Mean process CPU fell by
about 84%. This is a repeated gain on a development input, not a competition-wide
claim. Belpyramid CPU was close to baseline; its wall times were worse
(3.568–3.675 seconds versus 3.027–3.117), so no general speedup is claimed there.

Reproduction records are under `benchmark_results/`: `chrono-logical-validation`,
`chrono-logical-mutations`, `chrono-logical-default-traces` and
`chrono-logical-initial`, all dated `20260907`. They preserve commands, hashes,
validation logs and per-run results. Incomplete trace proofs are not UNSAT
certificates.

A four-input development guard (seed 20261304, one repetition, 15 solving CPU
seconds and 20 external wall seconds) improved verified solves from **1/4 to
3/4**. The candidate produced valid original-input SAT models for battleship
(1.759 process CPU seconds) and KTF (14.289), where the matched baseline timed
out. Both solved a second hardware instance: candidate 2.198 CPU seconds versus
baseline 13.211. Random circuits remained UNKNOWN in both. No errors or incorrect
completed answers were reported. See `chrono-logical-guard-20260907.json`.

Because the first KTF solve was close to the 15-second CPU allowance, a separate
confirmation used two repetitions at 30 solving CPU seconds and 35 external wall
seconds (seed 20261305). The baseline timed out twice; the candidate returned
independently validated SAT models in 14.006 and 14.461 process CPU seconds.
See `chrono-logical-ktf-confirm-20260907.json`.

Peak RSS on KTF rose from 114.2 MB to 118.8 MB and on the second hardware input
from 191.7 MB to 198.8 MB; these are whole-run, time-dependent measurements.
This development evidence supports keeping the option, not promoting it to the
default or claiming competition parity. A new frozen held-out comparison and
longer competition-scale budgets remain necessary.
