# Compact assignment trail

## Change and invariant

The C trail stored both a literal and a decision level, but the level member
was only written, never read. Conflict analysis obtains assignment levels from
`vars[var(lit)].level`; backtracking uses `trail_lims` to locate level boundaries.
Removing the duplicate `Trail.level` leaves one literal per entry: four bytes
instead of eight on the supported build ABI. Allocation already uses
`sizeof(Trail)`, so the trail buffer shrinks by half without a second code path.
This saves four bytes per allocated trail slot, not half of total solver memory.

Decision insertion, binary and long-clause propagation, and local-search model
transfer stop writing the duplicate level. Test and benchmark fixtures use
single-field trail initializers. Authoritative variable levels, reason storage,
watch order, phase saving, queue ordering and restart policy are unchanged.
Embedding code that accesses internal `Trail.level` must instead use the variable
level; code using the internal structures must be recompiled.

A compile-time regression requires a trail entry to occupy one literal. The
variable-array growth regression now checks that root and decision levels in
variable metadata survive growth while assignments remain live. Existing
backjump, minimization, target-phase, local-search, assumptions and restart-reuse
tests exercise the consumers of the trail and level boundaries.

The pinned Kissat source likewise declares its assignment trail as an
[unsigned array](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/internal.h),
with the element type defined in
[array.h](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/array.h).
That source comparison supports the representation choice, not a performance
claim for BSAT.

## Evaluation

Compare the preserved executable at `9c887c7` with the compact-trail executable.
Use the existing fixed-work binary propagation/backtracking microbenchmark and
a corpus comparison with conflict caps so unchanged search can be checked
alongside runtime. Timing is serial and separate from builds and validation.
Independently check every completed SAT model and UNSAT proof. Record hashes,
commands and limits with the results. Reused development inputs cannot establish
competition-level performance by themselves.

## Validation

Both release and ASan/UBSan builds passed all 20 C test executables, 2,709
independently validated solves (63 configurations, 13 fixed and 30 random
formulas, seed 20261003), and 30 short-deadline cases. Formula validation checks
truth-table answers, original-input SAT models, text/binary RUP and external
DRAT proofs. Maximum observed CPU overrun was 0.000 seconds at printed precision
in both builds.

## Propagation microbenchmark

The existing tagged-binary benchmark propagates a chain of 50,000 variables and
backtracks to root 1,000 times, for exactly 50,000,000 propagations and 49,999,000
watch inspections. Five runs per executable, in shuffled serial order, give
median CPU times of 0.461340 seconds before and 0.427439 seconds after: 7.35% less
CPU in this isolated workload. Startup and construction are outside its internal
timer. This is not a full-solver or competition speedup claim.

[Complete microbenchmark record](benchmark_results/compact-trail-micro-20260907.json).
The retained driver is `tests/benchmark_binary_watches.c`, invoked with `tagged`;
its only source change is removing the obsolete field from a fixture initializer.

## Fixed-work solver comparison

The [fixed 28-input manifest](benchmark_results/compact-trail-corpus-20260907.json)
is reused from the earlier development sample, selected before timing. Heap and
VMTF-with-trail-reuse modes each compare the preserved and compact executables,
twice per input (224 total jobs), capped at 10,000 conflicts with 20 solving CPU
seconds and a 25-second wall timeout. [Complete record](benchmark_results/compact-trail-fixed-20260907.json) and
[paired analysis](benchmark_results/compact-trail-fixed-summary-20260907.json).

All 112 baseline/candidate pairs match status, verification and every printed
statistic except PID, CPU time, rates and deadline-clock reads (which are excluded
from comparison). Heap profiles both verify 6/56 runs; VMTF profiles both verify
4/56, with zero errors throughout. Unfinished runs hit the conflict cap and do
not count as verified solves.

For each mode, take the median CPU time per input and exclude baseline medians
below 0.05 seconds. Both modes retain 26 inputs. The geometric mean compact /
baseline ratio is 1.01517 for heap (1.52% slower) and 0.98715 for VMTF (1.28%
faster). These modest, mixed results do not establish an aggregate full-solver
speedup. PAR-2 is dominated by the conflict-capped runs and is not a useful
throughput measure here. Peak process RSS also varies between profiles and is
not evidence that total memory falls by half.

## Repeated full solves and decision

The recent development targets were run three times per executable without a
conflict cap, using ten solving CPU seconds and fifteen wall seconds. All 24
runs were certified, with zero errors. All 12 baseline/candidate pairs match
status and the same non-timing statistics used above.

| Mode and input | Baseline median CPU | Compact median CPU |
| --- | ---: | ---: |
| Heap: battleship | 2.431453 s | 2.428690 s |
| VMTF + reuse: belpyramid | 2.898282 s | 2.872010 s |
| VMTF + reuse: Hamiltonian | 0.580853 s | 0.585778 s |
| VMTF + reuse: multiplier circuit | 0.816396 s | 0.813027 s |

These small differences are effectively flat and do not establish a general
runtime gain. Process wall times vary more than CPU times in these runs; no
wall-time speedup is claimed. See the
[heap record](benchmark_results/compact-trail-heap-targeted-20260907.json),
[VMTF record](benchmark_results/compact-trail-vmtf-targeted-20260907.json), and
[paired summary](benchmark_results/compact-trail-targeted-summary-20260907.json).

Retain the compact representation: it removes unused state and redundant writes,
halves the trail allocation, and preserves the measured search behavior. The
microbenchmark improvement does not imply competition-level performance. The
full-solver results remain near the prior baseline, including the substantial
reference-solver gaps documented in the restart-reuse evaluation. No new option
or search-policy default is introduced.

Final release executable SHA256:
`5c1564d046a1843ffb0c06297594c9008277681453f8d75ee34f269ff0acbd50`.
