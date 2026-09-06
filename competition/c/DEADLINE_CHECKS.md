# Amortized CPU deadline checks

CPU deadline checks now avoid redundant clock reads during cheap search and
preprocessing polls. Every call still checks the existing error, cancellation
and work-limit state. With a CPU limit enabled, the clock is read on the first
poll, after at most 128 subsequent polls, or when either the general work counter
or minimization counter advances by 1024 inspections, whichever comes first.

The work triggers are evaluated at existing polling sites inside propagation
and minimization. They avoid throttling those already-spaced polls another
128-fold. If the last clock read falls between aligned 1024-inspection checkpoints,
the next read can occur almost 2048 inspections later. Counting calls alone would
have allowed a much larger gap. Searches with no watch work
still poll by call count. Expensive best-phase target copying forces a clock read
because it does not advance either inspection counter. Solver return also forces
a read after model reconstruction/checking and proof flushing. A result that is
then over the CPU limit is reported as UNKNOWN.

Without a CPU limit, the scheduling state is unused and no deadline clock reads
are performed. The ordinary start-time/statistics measurements remain. Repeated
solves reset the scheduler; staged preprocessing replacements start with an
uncached check. `Deadline clock reads` reports actual deadline queries, excluding
start-time and statistics queries.

## Motivation

The preceding [profile](DENSE_ACTIVITY.md) attributed 400 of 2544 belpyramid
samples to `getrusage`, used by CPU-time checks on the measured macOS host. The
old shared helper called `clock()` at every poll when a time limit was set,
including every search-loop iteration. The change targets that measured overhead
without changing branching, retention or propagation policies.

## Tests and limits

`test_deadline` checks exact poll bounds, independent general/minimization work
triggers, immediate error/cancellation/work-limit results, expiration after a
cached reading, and a terminal check when an input contradiction bypasses search.
A 100000-variable empty formula exercises deadline expiration with zero watch
inspections, both with and without target saving, followed by a fresh unlimited
solve. The unlimited reuse disables target saving to avoid testing its unrelated
quadratic copying cost at that milestone. Subsequent
[incremental target saving](TARGET_SAVING.md) removes that cost on uninterrupted
descents while retaining the same forced deadline checks.

`tests/check_deadlines.py` exercises 1 ms, 10 ms and 50 ms CPU limits on pigeonhole
search, iterative minimization, combined preprocessing, and empty-watch search.
Its CPU-time assertion has a deliberately generous 100 ms CI margin; recorded
observations provide the tighter measured values. Parsing/startup is outside the
solver's CPU-limit interval, so wall time and whole-process CPU time are not the
same quantity. This is bounded polling, not a hard real-time guarantee: an
uninterruptible operation or an existing long operation between checkpoints can
still overshoot a deadline. Such operations are not all rewritten here.

The independent formula validator adds three timed configurations, including
iterative minimization with preprocessing and aggressive inprocessing/reduction
with binary proofs. Full results are recorded with the milestone evidence.

## Initial timing comparison

The existing twelve larger competition inputs were compared over three repetitions
per version, with 10000 conflicts, 20 CPU seconds and 25 wall seconds. Recorded
search counters matched on all instances and repetitions. Both versions verified
three of 36 runs, with zero reported errors. The other runs were UNKNOWN at the
conflict cap.

For eleven instances with baseline median CPU above 0.05 seconds, the geometric
mean candidate/baseline CPU ratio was 0.9371 (6.3% less CPU). Belpyramid fell from
3.408 to 2.772 seconds (18.7%); scheduling fell from 0.525 to 0.466 seconds (11.3%).
All eleven per-instance median ratios were below one. The very short preprocessing
UNSAT case is excluded only from the timing aggregate. Both versions' maximum
RSS was 42237952 bytes.

These timings include process startup, parsing and proof output, while certificate
checking is outside timing. Runs were serial without concurrent local tests or
builds. They are development samples, not disjoint held-out competition families.
Conflict-capped PAR-2 is a diagnostic, not a competition score. Matching counters
supports equal-search-work comparisons; UNKNOWN results are not solved instances.

## Broader and no-limit comparisons

The longer comparison uses 28 inputs, two repetitions, 30000 conflicts,
20 CPU seconds and 25 wall seconds. All eleven compared counters match across
versions and repetitions. Both versions verify six of 56 runs, with zero reported
errors. For the 25 inputs with baseline median CPU above 0.05 seconds, the geometric
mean CPU ratio is 0.9208 (7.9% lower). Individual families vary: the multiplier
circuit regresses by about 8.2%, while the larger scheduling case improves by
about 19.3%. The aggregate gain held across the two timed experiments; the small
samples do not establish a universal speedup or increased competition solved count.

The control repeats the twelve larger inputs twice at 10000 conflicts with no
internal CPU limit. Search counters again match, deadline-clock counts are zero,
and both versions verify two of 24 runs. The eleven nontrivial inputs have a
geometric mean CPU ratio of 0.9943: essentially unchanged at this sample size.
This supports attributing the main improvement to timed solves rather than a
new search policy. External wall limits remain 25 seconds in the control.

Both release and ASan/UBSan builds passed twelve short-deadline runs at 1, 10 and
50 ms. The reported maximum CPU overrun was zero at the CLI's millisecond
precision. This observation is narrower than the test's 100 ms failure margin,
and does not imply a hard real-time bound on arbitrary inputs.

## Reproducible evidence

- [Initial timed comparison](benchmark_results/deadline-screen-20260906.json)
- [Longer timed comparison](benchmark_results/deadline-long-20260906.json)
- [No-limit control](benchmark_results/deadline-unlimited-20260906.json)
- [Derived per-instance summaries](benchmark_results/deadline-summary-20260906.json)
- [Release short deadlines](benchmark_results/deadline-limits-release-20260906.json)
- [Sanitizer short deadlines](benchmark_results/deadline-limits-asan-20260906.json)

The baseline came from commit `509b5da90ec691b7866c8f5dd140f2db81bd237c`.
Exact commands, input and executable hashes, counters, per-run outcomes and
proof-checker identity are stored in the raw comparisons. The final executable
hash is recorded in the derived summary. Timing uses macOS arm64; performance
on other platforms is not established by these measurements.

Final validation passed 4520 release and 4520 ASan/UBSan formula solves across
40 configurations (seed 20260913), all twelve C test executables in both modes,
and twelve short-deadline runs per mode. The final release SHA256 is
`2f6f1331d14815146a01fbd96b849bd12804cadbdc0ec2043f05bc26169aed0b`,
matching every measured candidate.
[Validation record](benchmark_results/deadline-validation-20260906.json).
