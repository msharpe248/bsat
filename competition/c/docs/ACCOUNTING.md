# Phase and memory accounting

`bsat --accounting` enables intrusive inclusive CPU timers for parsing,
propagation, analysis, reduction, garbage collection, preprocessing, simplification,
search, model reconstruction/checking and proof encoding/flushing. Search includes
preprocessing and propagation; nested phase times must not be added together.
Default runs leave accounting disabled. Timer overhead and deadlines affect
instrumented searches, so use separate uninstrumented runs for speed comparisons.

Parsed and final checkpoints report requested retained capacities for the arena,
watches, original input, variable arrays, clause references, elimination records,
local-search state and other solver-owned storage. These are not RSS, allocator
metadata or exact transient peaks. Failed partial allocations mark estimates
incomplete. Two additional measurements cover live congruence temporary capacities
and simultaneously owned state during solver reconstruction. They exclude allocator
reallocation overlap and other temporary scratch. Timings and peak diagnostics
survive equivalence substitution and repeated solver rebuilds.

Validation: all 49 C executables pass in release and ASan/UBSan builds, including
accounting lifetime checks. Allocation-failure regressions and 24 independently
checked structured long-search cases pass in release. Twelve fixed-work runs
(20,000 conflicts, two repetitions, diagnosis and station-repacking) match all
selected work counters and binary proof bytes across the previous binary,
accounting disabled, and accounting enabled. These signatures do not record every
trail event. Short CPU samples vary; no speed improvement is claimed for accounting.
Records: `benchmark_results/accounting-work-20260908.json` and its policy file.

Five serial instrumented application runs use ten solver CPU seconds, thirty
external wall seconds and independent proof/model acceptance. The hardware-model-
checking UNSAT answer is verified; the other four cases are UNKNOWN. See
`benchmark_results/accounting-large-20260908.json`. This is diagnostic evidence,
not a performance score. On the largest at-least-two-sol input:

- Congruence temporary capacity reaches 480,641,932 bytes.
- Solver reconstruction overlaps 737,859,885 bytes of owned capacities.
- Final retained capacity is 511,509,242 bytes: watches 178,476,616, arena
  99,332,584, variables 78,643,275, elimination 70,284,339 and input 67,108,608.
- The instrumented ten-second run peaks at 1,064,714,240 bytes process RSS.
  Its shorter/instrumented search differs from the previous 120-second run.

The temporary structures and reconstruction overlap are concrete targets for
memory work. Watch storage is the largest retained category in this sample.
Actual Linux PMU attribution still needs a target host.

After the embedding milestone, clocks use `CLOCK_THREAD_CPUTIME_ID`. Earlier
recorded single-thread measurements used process CPU; artifacts retain their
original provenance.
