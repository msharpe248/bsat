# Rejected experiment: growing reduction intervals

The tested profiles were rejected after a repeatable loss on
maximum-constraint-partition. Production solver options and behavior are restored
to the baseline. The archived implementation includes its integration fix and tests.

The prototype grows the reduction interval as a search continues. The existing
LBD/activity ranking, keep fraction, maximum-LBD cutoff and protection of binary,
glue and locked clauses are unchanged. Larger intervals let more learned clauses
remain available between reductions, at the cost of propagation work and memory.

Kissat's pinned [reduction code](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/reduce.c)
updates its conflict deadline using a square-root scale. Its
[limit helper](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/kimits.c)
uses the completed reduction count, and its
[options](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/options.h)
define a 1000-conflict initial and base interval. This experiment adopts that
scaling rule while retaining BSAT's existing reduction policy and adding a cap.
It is not a port of Kissat's tiering and retention policy.

`--growing-reduce` initially schedules a reduction after `--reduce-interval`
conflicts. After completed pass k, it schedules the next one after
`floor(base * sqrt(min(k, 1024)))` further conflicts. The cap is 32 times the base
interval; it does not bound total memory because protected clauses can accumulate.
Deadline addition and the pass counter saturate safely. Scheduling precedes the
score-array allocation, so allocation failure cannot cause a retry every conflict.
The fixed-period default remains available when the option is absent.

## Validation

The initial prototype passed 18 C test executables and 3339 truth-table/model/proof
checks per release and ASan/UBSan build (53 configurations, 13 fixed and 50 random
formulas, seed 20260925). Each build passed 21 short-deadline cases.

Review found that successful SCC substitution replaces the solver object without
preserving the new reduction deadline. A regression combining actual equivalence
substitution and growing reductions failed at its deadline assertion before the
fix. Copying the deadline with the solver statistics fixes the issue. The final
prototype passes all 18 C test executables in both build modes, 1782 further
checked solves per build (54 configurations, 13 fixed and 20 fresh random
formulas, seed 20260926), and 21 deadline cases per build. New tests cover the
first deadlines, cap, overflow boundaries, API rebuilds, and actual heap/VMTF
search scheduling, including the terminal root-conflict boundary. The initial
performance runs did not enable SCC; the 1000-base comparison uses the fixed build.

## Initial 2000-base comparison

Baseline executable uses `794a1438c83bcf8635aafcd857a4bc31c3fa714e`.
Both versions use VMTF, ten solving CPU seconds and fifteen wall seconds per run.
The targeted comparison repeats belpyramid, hgen and multiplier-circuits twice.
Both verify 4/6 with zero errors. Mean wall PAR-2 improves from 12.6553 to 11.4096.
Belpyramid takes 3.176–3.242 process CPU seconds at baseline versus 2.947–2.971
with growing reductions. Multiplier drops from 4.240–4.860 to 1.072–1.356 seconds,
and from 102842 conflicts to 34942. Hgen remains UNKNOWN. Targeted peak process
RSS rises from 32.8 MB to 56.9 MB (decimal units).

On all 28 reused development inputs, once each, baseline verifies 6/28 and growing
verifies 5/28. There are no gained solves; maximum-constraint-partition is lost.
Mean wall PAR-2 worsens from 24.1710 to 24.7938. Both have zero errors. Peak RSS
rises from 91.9 MB to 144.1 MB. The targeted speedup therefore does not justify
making this profile the default.

Runs were serial without concurrent local BSAT builds or tests. SAT models and
UNSAT proofs were independently checked. Process CPU includes parsing/startup
and proof output; the solving limit excludes parsing. Independent checking is
outside solver timing. UNKNOWN and unverified answers receive twice the wall
timeout as PAR-2. These are short, reused macOS arm64 development measurements,
not a held-out competition evaluation. Heap branching performance was not measured.

## 1000-base comparison and longer-limit control

The corrected prototype was compared at base 1000 on the original three inputs
plus maximum-constraint-partition, twice each. Both versions verify 4/8 with
zero errors. Mean wall PAR-2 is 17.0422 for baseline and 16.3210 for growing.
Multiplier improves from 4.834–4.907 process CPU seconds to 1.955–1.964;
belpyramid is roughly unchanged. Neither version finishes maximum-constraint-partition within ten solving CPU
seconds in this repeat. Baseline's result near
the earlier cutoff therefore needed a longer-limit check.

That input was then run twice with each of baseline, growing base 2000, and
growing base 1000, using twenty solving CPU seconds and twenty-five wall seconds.
Baseline verifies SAT twice (8.221–10.264 process CPU seconds); both growing profiles return UNKNOWN twice. There
are zero reported errors. Baseline mean wall PAR-2 is 9.2757 seconds; both growing
profiles receive 50.0 seconds. The regression persists with double the CPU budget,
so it is not adequately explained by the earlier ten-second boundary.

The multiplier benefit is substantial, but neither tested schedule avoids the
lost solve. Neither option nor schedule is retained. This result does not rule
out other adaptive retention policies; it rejects these profiles as a general
improvement over the current baseline.

After restoration, all 17 production C test executables pass in release and
debug builds. The release executable is byte-identical to the saved baseline
(SHA-256 `c5d8b9ab52cc50e9322028aee8112b25cb489afb3f6ab633969e6142e8387592`). The archived patch passes
`git apply --check --unidiff-zero` against the restored source.

## Evidence and reproduction

- [Corrected prototype and regressions](benchmark_results/growing-reduce-prototype-20260907.patch)
- [Validation and deadline record](benchmark_results/growing-reduce-validation-20260907.json)
- [2000-base targeted comparison](benchmark_results/growing-reduce-2000-targeted-20260907.json)
- [2000-base broad comparison](benchmark_results/growing-reduce-2000-broad-20260907.json)
- [1000-base targeted comparison](benchmark_results/growing-reduce-1000-targeted-20260907.json)
- [Longer-limit control](benchmark_results/growing-reduce-boundary-20260907.json)

Apply the zero-context patch independently to `794a143` with
`git apply --unidiff-zero <patch>` from the repository root. Run
`make -C competition/c -j4 all test` and
`make -C competition/c -j4 MODE=debug all test`. The final validator run uses
`tests/validate.py --solver <binary> --checker <drat-trim> --cases 20 --seed 20260926`;
the initial run used cases 50, seed 20260925, before the added SCC configuration.
Use `tests/check_deadlines.py` for deadline checks and `tests/benchmark.py` with
the reports' exact inputs, solver templates, limits and repetition counts.

The initial 2000-base timings precede the one-line transfer of `next_reduce` in
`src/equiv.c`; they do not enable equivalence substitution. Removing that transfer
reconstructs the initial solver implementation, but intentionally fails the new
SCC scheduling regression. The 1000-base timings and longer-limit control use
the corrected source archived here. Reports pin actual executable and input
hashes; temporary executable and dataset paths must be recreated elsewhere.
