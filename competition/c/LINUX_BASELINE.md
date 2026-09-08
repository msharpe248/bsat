# Uniform Linux baseline

The manual `C solver Linux performance baseline` workflow runs on Ubuntu 24.04.
It builds portable `-O3 -flto` executables, uses a pinned Kissat source revision
with embedded input options disabled, and independently checks conclusive
answers through the existing model/DRAT-to-LRAT/CakeML chain.

Two exact development inputs from the audited industrial campaign are stored
compressed under `tests/fixtures/linux_baseline`, with original/compressed hashes
and DIMACS dimensions. They are the known random-CSP and planning gaps, not a
fresh representative sample. `prepare_linux_baseline.py` verifies bytes before
launching any solver. Keeping this small pinned corpus makes the experiment
reproducible without the local untracked competition dataset.

Three profiles (control, no-rephase, Kissat), two inputs and two repetitions give
12 serial trials, shuffled with a fixed seed and pinned to one allowed CPU. Every
profile receives the same external 60-second process deadline including parsing
and proof output. No native solver time flag is used. `--external-wall-only`
rejects known BSAT/Kissat stopping options; arbitrary wrapper programs still
require review. Original formula bytes, commands and executable hashes are
written before the first trial; reports carry an explicit completion field.

Each solver child also receives a 4 GiB address-space ceiling and 1 GiB per-file
ceiling. Checker timing is separate with a 600-second limit. Artifacts retain
wall/CPU time, per-worker child peak RSS, verification time, outcomes, platform,
affinity, compiler and checker identities. A deadline kill stays UNKNOWN even if
the child printed a result before exiting. Protocol mismatches count as errors.

This is a hosted Linux baseline, not a dedicated deployment-server measurement.
CPU pinning cannot eliminate host contention or frequency changes. The workflow
records `perf_event_paranoid`; it does not claim to have PMU measurements.
`tests/profile_linux.py` remains the explicit hardware-counter workflow for an
environment that grants counter access.

## Completed baseline

[Workflow 34257988158](https://github.com/msharpe248/bsat/actions/runs/34257988158)
completed successfully at revision `6c65322`. The artifact reports Linux
6.17/Azure, glibc 2.39, AMD EPYC 7763, four allowed logical CPUs, with this serial
experiment pinned to CPU 0. Linux address-space denial, file ceilings and timeout
protocol tests passed before measurements. All 12 trials completed with four
independently verified SAT models, eight external-deadline UNKNOWNs and no errors.

| Profile | Verified trials | Random-CSP SAT wall seconds | Planning | Maximum child RSS |
|---|---:|---:|---|---:|
| Control | 0/4 | both UNKNOWN | both UNKNOWN | 72.20 MB |
| No rephase | 2/4 | 47.337, 49.899 | both UNKNOWN | 63.18 MB |
| Kissat | 2/4 | 25.643, 24.532 | both UNKNOWN | 47.33 MB |

SAT validation takes 0.048–0.051 seconds separately. No UNSAT answer occurs in
this baseline, so it provides no new UNSAT-checker throughput measurement. Mean
PAR-2 is 120.00 seconds for control, 84.31 for no-rephase and 72.54 for Kissat.
This confirms the direction of the rephasing ablation on another platform, while
showing a substantial remaining speed gap against Kissat. Planning remains open
for all three profiles at this uniform wall limit. Two repetitions on two known
instances do not establish general ranking, significance or deployment capacity.

Unmodified downloaded artifacts and a hash/origin audit are retained in
`benchmark_results/linux-baseline-20260908/`. The later factoring-only change was
disabled in this baseline. A separate local guard compares before/after ordinary
search at 1,000 conflicts on both inputs: statuses, all guarded work counters and
binary proof hashes are identical. Those UNKNOWN prefixes are trace evidence,
not UNSAT certificates or a Linux hardware-counter measurement.
