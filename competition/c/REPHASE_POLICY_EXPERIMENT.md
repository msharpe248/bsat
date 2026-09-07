# Rejected rephasing policy experiments

The retained default continues to restore its deepest historical partial target
every 1,000 conflicts. Disabling or delaying rephasing lost a solved case in a
probe; resetting the best-trail threshold after each rephase produced repeated
regressions without new solves. Runtime source is restored to `3703a4a`.

## Frequency probe

The [frequency probe](benchmark_results/rephase-policy-probe-20260907.json)
compares the default, `--no-rephase`, and `--rephase-interval 10000` on battleship,
belpyramid, the larger `ktf` case and hardware verification. Each runs once, with
ten solving CPU seconds, fifteen external wall seconds and seed 20261102.
The default verifies **2/4**, versus **1/4** for each alternative. Both alternatives
lose battleship within the limit: default verifies SAT in 2.365 process CPU
seconds and 107,461 conflicts; disabled/delayed rephasing return UNKNOWN at
445,379/456,130 conflicts. All profiles verify belpyramid UNSAT and leave the two
larger targets UNKNOWN. There are zero checker errors. This probe motivated
keeping the frequency rather than promoting either alternative.

## Fresh-target candidate

After copying a target into saved phases, reset `best_trail_size` to zero and
invalidate its prefix. The next shorter branch can then replace a stale deeper
target. Invalidating the prefix makes the next capture clear unknown entries,
rather than leaving old values outside the new partial assignment.

The motivation comes from pinned Kissat revision
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`, `src/rephase.c`: after its best-phase
step, it resets the best-assigned threshold. Kissat also uses a multi-step phase
schedule, growing limits and stable-mode gating. This BSAT candidate only tries
the threshold reset, not the complete Kissat policy. The historical BSAT policy
is a heuristic choice, not a soundness bug.

The [paired comparison](benchmark_results/fresh-target-pilot-20260907.json)
repeats the same four inputs twice per profile, with the same time limits and
seed 20261103. Both verify **4/8** with zero errors. Median process CPU and
counters for the completed cases:

| Input | Historical CPU | Fresh CPU | Historical / fresh conflicts | Historical / fresh target literals copied |
| --- | ---: | ---: | --- | --- |
| battleship | 2.099676 s | 3.850834 s | 107,461 / 209,352 | 4,164 / 426,027 |
| belpyramid | 5.010482 s | 6.059408 s | 30,460 / 33,599 | 4,657,462 / 8,830,955 |

Both conflict and copying counts repeat exactly on the completed cases.
Hardware verification and `ktf` remain UNKNOWN in all repetitions. The fresh
policy copies tens of millions of target literals there, versus hundreds of
thousands with the historical policy. Mean wall PAR-2 worsens from 17.1672 to
18.1123 seconds. Maximum process RSS is 62,423,040 versus 62,259,200 bytes.
Reject the policy: it increases search effort and target copying without a new
solve on this screen. This does not rule out a different complete rephase policy.

Performance jobs are serial and separate from builds/tests. SAT models are
checked against original inputs and UNSAT proofs with external drat-trim outside
solver timing. Process CPU includes startup, parsing and proof output; the
internal solve limit excludes parsing. Unverified results receive twice the
external wall limit as PAR-2. This is reused development material, not held-out
competition evidence. UNKNOWN provides no soundness verdict.

## Regression coverage and restoration

The candidate passed all 38 release C test executables. Its fresh-target regression
checks a shorter branch replacing an older target across heap/VMTF and
focused/stable configurations, both at and before the rephase boundary. Candidate
source and this specific test are archived in the
[patch](benchmark_results/fresh-target-rejected-20260907.patch), applicable with
`git apply --unidiff-zero`. The patch applies cleanly to the restored source.

A separate retained `test_rephase.c` checks **16** combinations of heap/queue,
focused/stable, enabled/disabled and due/not-due rephasing. It verifies that only
known target entries replace saved polarities, unknown target entries preserve
saved values, stable target selection has the expected phase, and a decision
budget returns UNKNOWN with the correct trail and reason state. This regression
checks phase application without requiring the rejected capture policy.

The external validator adds four profiles: rephase every conflict with probing
disabled; rephasing disabled with probing disabled; rephase every conflict in
alternating mode; and rephase every conflict with VMTF and binary proofs. This
raises the matrix from 69 to 73 configurations and exercises rephasing on the
small independently decidable formulas.

Both restored builds pass all **38 C test executables**. Each also passes **3,139 independent validation solves** (73 configurations,
13 fixed and 30 random formulas, seed 20261104), checking truth-table answers,
original models and text/binary proofs with external drat-trim. These finite
checks strengthen regression evidence; they are not a universal soundness proof. The restored release binary matches the preserved
baseline hash; the archived candidate matches its measured hash. No rejected
runtime change remains.

## Reproduction

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 30 --seed 20261104
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug --checker /tmp/bsat-drat-trim --cases 30 --seed 20261104
```

For performance reproduction, preserve the `3703a4a` executable, apply the archived
patch, build the candidate and use the exact inputs, templates, seeds and limits
in the raw JSON records. Remove the patch to return to the retained policy.
