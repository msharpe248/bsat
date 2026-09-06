# Bottom-up variable heap experiment

The existing belpyramid profile attributed 39% of samples to decision selection,
largely heap work. The earlier dense-activity experiment did not produce a robust
speedup. This experiment instead changes the sift algorithm used on extraction.

CPython's [heapq implementation](https://github.com/python/cpython/blob/3.14/Lib/heapq.py)
explains why moving a hole to a leaf and then inserting the replacement upward can
reduce comparisons: a replacement drawn from the last heap position often belongs
near the bottom. That motivates a C experiment, but does not establish a benefit
for BSAT's numeric priorities and inverse-position bookkeeping.

The candidate descends through the larger child, then moves the replacement back
up to its final position. It retains a root comparison to avoid a full traversal
for equal/zero-score heaps. Equal children choose left, and an equal replacement
moves upward, reproducing the original top-down stopping position. Both arrays
and variable-to-position mappings remain identical after extraction.

The regression compares the full heap and every inverse position with an independent
top-down reference after every extraction. Its 128 configurations cover zero,
few-valued and varied activity scores, sizes 1 through 16, and sizes 1023 through
1025 across complete and partial tree levels. The existing heap tests additionally
exercise variable growth, backtracking, score rescaling and API rebuilds.

The baseline is commit `0fc2b182344a71e7fe287e231da7a9103c4d9200`. All timings are
serial on macOS arm64 with no concurrent local builds or tests. They include
startup, parsing and proof output; independent certificate verification is outside
timing. The same 28 development inputs used by previous milestones are reused,
so these results are not a held-out competition evaluation.

The initial comparison uses three repetitions, 10000 conflicts, 20 CPU seconds
and 25 wall seconds. Both versions verify nine of 84 runs (three distinct inputs),
with zero errors. Fourteen search/deadline/target counters match in every paired
run. For 25 inputs whose baseline median CPU exceeds 0.05 seconds, the geometric
mean candidate/baseline ratio is 0.9864. Belpyramid is essentially unchanged at
0.9977; the largest improvements are on two shorter inputs. There is no observed
solved-count gain.

## Longer comparison and decision

At 30000 conflicts and two repetitions, the 25 nontrivial inputs have a geometric
mean CPU ratio of 1.0018 (0.2% higher), with per-instance ratios from 0.9735 to
1.0201. Belpyramid improves by 2.6%, but this does not translate into an aggregate
gain. The shorter cases that drove the initial improvement do not repeat it.
Both versions verify six of 56 runs, with zero errors and all fourteen compared
counters matching. The remaining runs are UNKNOWN at the conflict cap.

The optimization is rejected: these samples do not demonstrate a repeatable
aggregate benefit or a solved-count improvement sufficient to justify the added
complexity. The production algorithm is restored. The new exact-order and
inverse-position regressions remain, making future heap changes easier to assess.

## Reproducible evidence

- [Initial comparison](benchmark_results/heap-bottomup-screen-20260906.json)
- [Longer comparison](benchmark_results/heap-bottomup-long-20260906.json)
- [Initial derived summary](benchmark_results/heap-bottomup-screen-summary-20260906.json)
- [Longer derived summary](benchmark_results/heap-bottomup-long-summary-20260906.json)
- [Rejected implementation](benchmark_results/heap-bottomup-rejected-20260906.patch)

The raw reports pin executable and input hashes, invocation arguments, per-run
outcomes, counters and checker identity. The rejected patch applies to the baseline
commit named above. Earlier formula validation of that baseline is recorded in
[TARGET_SAVING.md](TARGET_SAVING.md); the retained production source is identical.

Final retained-code validation passed all thirteen C test executables in release
and ASan/UBSan modes. The release executable exactly matches the SHA256 of the
previously validated target-saving baseline. Production source and build rules
are unchanged; the rebuilt debug executable has a different hash. The formula
sweep was not repeated for this test/documentation-only retained change. See the
[final validation record](benchmark_results/heap-bottomup-final-validation-20260906.json).
