# Planning profile and original-header experiment — 2026-09-08

Decision: retain the production layout. The compact prototype passes correctness
checks but provides no transferable speed improvement. Hardware PMU measurements
remain unavailable on the tested GitHub-hosted machine.

## Linux evidence

[Workflow 34282849365](https://github.com/msharpe248/bsat/actions/runs/34282849365)
succeeded on Ubuntu 24.04, pinned to CPU 0, using the 50,277-variable planning
development fixture and 100,000 conflicts. `perf stat` explicitly reports
`<not supported>` for hardware events. Running as root does not expose the VM's
PMU. These are missing measurements, not zero cache or branch misses.

Separate `cpu-clock:u` sampling collected 1,720 samples with no lost samples:
propagation 70.06%, analysis 6.40%, backtracking 5.00%, recursive minimization
3.02%, watch insertion 2.50%, garbage collection 2.03%. Both runs returned UNKNOWN.
This establishes where sampled CPU time goes, not cache causality or soundness.
Raw counters, stacks, commands, hashes and host information are in
[the report directory](benchmark_results/planning-linux-profile-20260908).
The full `perf.data` is a workflow artifact, identified by SHA-256 in results.json.

`tests/profile_linux.py` remains the strict hardware-counter comparison driver
for a Linux host with PMU access. Do not replace unavailable events with software
counts. The profiling workflow also validates status markers and checks any
conclusive answer independently.

## Upstream review and prototype

Pinned CaDiCaL [watch.hpp](https://github.com/arminbiere/cadical/blob/c60730422e758ef1cebe7aeddf2dda31c996bf04/src/watch.hpp)
stores a clause pointer, blocking literal and clause size (16 bytes on the reviewed
64-bit target). BSAT already uses two 32-bit fields (8 bytes), including its binary
reference tag. Copying CaDiCaL's watch representation would enlarge BSAT's watches.
CaDiCaL's [clause.hpp](https://github.com/arminbiere/cadical/blob/c60730422e758ef1cebe7aeddf2dda31c996bf04/src/clause.hpp)
also keeps literals adjacent to metadata and retains a circular search position.

The archived prototype instead shrinks original clause headers from 16 to 8 bytes;
learned headers retain LBD/activity and diagnostic headers retain attribution.
Literal addressing, allocation, deletion and GC account for both record sizes.
This adds a learned/original layout test when accessing literals and changes GC
triggering. It is not an identical execution trace experiment.

Release and ASan/UBSan C suites passed. An independent truth-table/model/text and
binary RUP campaign passed 5,796 solves, seed 2026090848. The prototype is archived
as [an applicable patch](benchmark_results/original-header-experimental-20260908.patch);
production sources were restored before the next milestone.

## Serial timing evidence

Three repeats at 100,000 conflicts, without concurrent local test/build activity:

| Development input | Baseline median CPU | Compact median CPU |
|---|---:|---:|
| planning 16c999 | 4.5032 s | 4.5808 s |
| planning 21b173 | 1.4369 s | 1.4492 s |
| random CSP 9b998 | 0.5244 s | 0.5562 s |

All returned UNKNOWN. This screen did not emit proofs; it measures fixed-conflict
throughput only. The largest planning arena peak shrank from 11.63 to 9.54 MB,
but measured process peak RSS increased from 117.3 to 124.6 MB. Arena size alone
does not establish service memory savings.

The fresh holdout selected eight previously unrecorded families by seed 2026090850,
then ran both binaries with proof output, one repeat and an eight-second external
deadline. Both solved the same one of eight inputs (UNSAT, independently checked),
with no errors. This small screen provides no promotion evidence. Raw reports:
[development](benchmark_results/original-header-development-20260908.json),
[selection](benchmark_results/original-header-holdout-selection-20260908.json),
[holdout](benchmark_results/original-header-holdout-20260908.json).
