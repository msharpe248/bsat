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
environment that grants counter access. Measured results will be recorded after
the first workflow completes; creating the workflow alone is not evidence.
