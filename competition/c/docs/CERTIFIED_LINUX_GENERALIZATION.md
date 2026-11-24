# Certified prefix policy on a broader Linux corpus — 2026-09-09

Run 34370117115 compares pre-prefix d6601b9 against the prefix-retaining default
on one Ubuntu 24.04 AMD EPYC 9V74 runner. Portable -O3 -flto builds, independently
checked certificates/models, and baseline/candidate/candidate/baseline ordering
are pinned in the report. Reference solvers execute serially on that same runner.

| Set | Pre-prefix checked | Prefix checked | CPU PAR2 before → after |
|---|---:|---:|---:|
| Original four, 60 CPU/query | 94/96 | 94/96 | 2.547238 → 2.552225 s (+0.20%) |
| Expanded four, 10 CPU/query | 60/64 | 60/64 | 1.612472 → 1.550840 s (-3.82%) |

Both frozen regression gates pass with no checked-solve loss. This does **not**
reproduce the Mac cal3 solve within 60 seconds: both variants remain UNKNOWN on
both Linux repetitions. The prefix runs reach 857,826 / 881,756 conflicts at the
cutoff, short of the 1,148,517-conflict completed Mac path. That observation is
consistent with lower throughput but is not a proof of the complete Linux path.
Do not compare absolute timing across different hosted VM models as a controlled
hardware-only experiment.

The expanded unresolved queries are cal100 positive output at depths 4 and 8;
all remaining expanded queries check in both versions. Certified gen23 depth-16
positive takes 1.51–1.60 s before versus 1.83–1.89 s after, retaining the documented
per-query tradeoff. A passed penalized aggregate is not a universal speedup.

The new [assumption-aware LBD prototype](ASSUMPTION_LBD_EXPERIMENT.md) has a
separate matched Linux comparison against the prefix-retaining default. It must
pass independently; these results cannot stand in for its validation.

Raw inputs/tool hashes, exact contexts, compiler/CPU, limits, query work, journal
capacity and checker evidence are in
`benchmark_results/prefix-linux-generalization-20260909/`. The original and
expanded manifests are existing pinned development datasets, not pristine holdouts.
