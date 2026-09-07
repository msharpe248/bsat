# Rejected experiment: focused queue and stable heap

The prototype adds `--hybrid-order`: queue branching and ordered queue bumps in
focused mode, numeric heap branching and score bumps in stable mode. The heap
honors the existing VSIDS/LRB selection. `--alternating` enables automatic mode
changes. Existing VMTF remains queue-only; combining VMTF and hybrid ordering is
rejected. The prototype preserves the current alternating schedule, including
its immediate entry into stable mode at zero conflicts. It does not change
restart intervals or target phases.

This follows the mode-specific ordering and activity policy in pinned
[Kissat bump.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/bump.c).
BSAT keeps its heap membership bookkeeping in both modes and restores the queue
cursor on backtracking in both modes. These are necessary when the ordering is
inactive during assignments or when a mode switch precedes backtracking.

## Correctness

Both release and ASan/UBSan builds passed 18 C test executables. A new regression
checks actual focused/stable decisions, a switch before backtracking, queue
cursor restoration, mode-specific conflict updates, existing VMTF behavior,
incompatible options, and solver rebuilds with assumptions. Each build passed
21 short-deadline cases, with at most 0.001 seconds observed CPU overrun.

The correctness matrix adds five hybrid configurations covering focused-only,
alternating, LRB, binary proofs, SCC/BVE/BCE and aggressive inprocessing/reduction.
There are 52 configurations and 63 formulas (13 fixed, 50 random; seed 20260927),
for 3276 solves per build. Answers are compared with truth tables, SAT models
with original clauses, and UNSAT proofs with independent checking.

## Performance method

The four reused development inputs are belpyramid, hgen,
maximum-constraint-partition and multiplier-circuits, repeated twice per profile.
The controls are retained VMTF and retained alternating VMTF; the candidate uses
hybrid ordering with alternating modes. Each run has ten solving CPU seconds
and fifteen wall seconds. Runs are serial after local builds/tests finish.
SAT models and UNSAT proofs are independently checked outside solver timing.
PAR-2 uses twice the wall timeout for unsolved runs. Process CPU includes parsing
and proof output; the solving limit excludes parsing. This small reused sample
is a development screen, not a competition or held-out performance claim.

Baseline source is 794a1438c83bcf8635aafcd857a4bc31c3fa714e, executable SHA256
c5d8b9ab52cc50e9322028aee8112b25cb489afb3f6ab633969e6142e8387592.

## Result and decision

| Profile | Verified runs | Mean wall PAR-2 | Peak RSS (MB) |
| --- | ---: | ---: | ---: |
| VMTF | 5/8 | 15.1399 | 91.8 |
| Alternating VMTF | 4/8 | 17.8297 | 81.1 |
| Hybrid alternating | 2/8 | 23.6254 | 94.0 |

All profiles had zero correctness/checker errors. Hybrid gained no solves and
lost multiplier-circuits in both repetitions. VMTF solved that case in
4.802–5.032 process CPU seconds; alternating VMTF in 5.440–5.443; hybrid reached
the ten-second solving limit twice. Belpyramid also worsened: VMTF took
3.164–3.370 process CPU seconds, alternating VMTF 3.396–3.419, and hybrid
3.652–3.653. Maximum-constraint-partition was solved once by VMTF and by neither
alternating profile; its baseline result is close to the limit and variable.
Hgen remained unsolved in all runs.

Reject this profile: the multiplier regression is repeated against both
controls, with no compensating gain on this sample. No broad benchmark was
needed to reject it. This does not rule out hybrid ordering with a different
mode schedule, score initialization or phase policy. The immediate stable-mode
entry is an existing schedule property, not a new change in this experiment.

The implementation, regression and validator changes are archived in
[the patch](benchmark_results/hybrid-order-rejected-20260907.patch), applicable
with `git apply --unidiff-zero`. Complete measurements and binary/input hashes
are in [the result](benchmark_results/hybrid-order-targeted-20260907.json).
Runtime source and test configuration are restored to the retained baseline.
The restored release and ASan/UBSan builds each pass all 17 retained C test
executables. The rebuilt release executable has the exact baseline SHA256 above;
there is no runtime source difference from 794a143. The archived patch passes
`git apply --check --unidiff-zero` against the restored tree.
