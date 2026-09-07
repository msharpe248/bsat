# Experimental conflict-recency branching

`--vmtf` enables a variable-move-to-front queue. Conflict analysis moves each
bumped variable to the front; selection scans from the most recent available
variable, skipping assigned and eliminated variables. Backtracking restores the
search cursor using monotonically increasing timestamps. New variables are added
lazily in ascending identifier order, so initial selection prefers higher IDs.
Timestamp renumbering preserves order on overflow. The default remains VSIDS.

Queue storage is allocated only when enabled, with a 16-byte node per variable
at allocated capacity. The existing activity heap is still maintained. This
first implementation isolates the branching policy; it does not claim to remove
heap overhead. `--vmtf` takes precedence over heap selection with `--lrb` or
`--alternating`, including stable mode. Phase saving, random phase selection,
target phases, restart scheduling, and clause reduction retain their policies.
Repeated API solves rebuild the queue along with the rest of the solver.

## Existing work and experiment design

Kissat's [decision implementation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/decide.c)
uses a recency queue in focused mode and an activity heap in stable mode.
Its phase selection also differs from BSAT's per-decision random phase override.
This experiment implements a basic recency queue, not Kissat's complete mode,
bumping, random-decision, or phase machinery. The comparison executable was built
from that pinned Kissat revision.
In particular, Kissat's [bump implementation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/bump.c)
sorts analyzed variables by their previous queue timestamps before moving them.
BSAT currently moves them immediately in conflict-analysis traversal order.
A subsequent experiment should isolate that ordering difference before drawing
conclusions about the effectiveness of Kissat-style VMTF.

Before implementing the queue, a serial screen compared BSAT at
`c2d01be8596610ea71440e511e79f8b754a3114b`, the same executable with
`--no-random-phase`, and Kissat. At three solving CPU seconds and eight wall
seconds, each BSAT variant verified 3 of 28 inputs; Kissat verified 6.
Kissat's extra solves were belpyramid (UNSAT), hgen (SAT), and
multiplier-circuits (SAT). Disabling random phase selection did not close the
gap, so the default phase policy is unchanged.

The corrected VMTF screen also verified 3 of 28 inputs for both baseline and
VMTF, with zero errors. Mean PAR-2 was 14.2874 seconds for baseline and 14.2893
for VMTF. There is no demonstrated solved-count or speed improvement. The queue
remains opt-in as an experimental policy for subsequent controlled comparisons.

All inputs are reused development inputs from the minimization and large-values
corpora. Each screen uses one repetition and independently checks SAT models and
UNSAT proofs. UNKNOWN, errors, and unverified answers receive the harness's
16-second PAR-2 penalty, based on the wall timeout. Solver CPU limits exclude
parsing. Certificate checking is outside solver timing. Local builds and tests
were not run concurrently with timing. This short screen does not establish
competition-scale performance, held-out generalization, or small speed changes.

## Correctness and deadlines

Unit coverage compares 1000 randomized queue operations against a simple ordered
array, including assignment, backtracking cursor recovery, variable growth,
exhaustion, and timestamp rollover. Integration coverage exercises actual
conflict-analysis bumps, learned units, models, assumptions, API rebuilds,
default-mode allocation, and deadline polls during initialization and scanning.
The independent validator includes VMTF combinations with preprocessing,
inprocessing, local search, minimization, aggressive reduction, and binary DRAT.

The initial timed VMTF screen exposed seven errors: variable selection could stop
at the deadline, but its caller treated an unsuccessful selection as a completed
SAT assignment. Model validation then correctly rejected the incomplete model.
The search loop now distinguishes interruption/error from exhaustion before
requesting model validation. Short-deadline coverage includes both a hard UNSAT
input and an empty SAT formula with VMTF. No unverified SAT answer is counted as
a success.
As a negative control, restoring only the faulty unconditional SAT transition
in a temporary executable caused the expanded deadline suite to fail on its
exit-code assertion. The suite therefore detects this integration regression,
in addition to checking the queue's polling primitives directly.

Final validation passed 5311 release and 5311 ASan/UBSan formula solves across
47 configurations (13 fixed and 100 randomized formulas, seed 20260919), with
truth-table comparison, original-input model checks, and independent text/binary
proof checks. All 15 C test executables and all 18 short-deadline runs passed in
each build mode. Allocation-growth coverage explicitly crosses the queue's
allocated capacity. These finite tests are evidence, not a proof of solver
correctness on arbitrary competition inputs.

## Evidence

- [Phase policy and Kissat screen](benchmark_results/phase-screen-20260906.json)
- [Corrected VMTF screen](benchmark_results/vmtf-screen-20260906.json)
- [Superseded initial screen with deadline errors](benchmark_results/vmtf-initial-errors-20260906.json)
- [Release deadline checks](benchmark_results/vmtf-limits-release-20260906.json)
- [Sanitizer deadline checks](benchmark_results/vmtf-limits-asan-20260906.json)

Reports pin input and executable hashes, commands, limits, and per-run results.
Reproduce with `tests/benchmark.py`, using the solver templates and input paths
in each report after building each revision with `make -j4 all`.
