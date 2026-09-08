# Production workload batch

Authorized: Linux hardware/stack profiling and one compact layout experiment;
real verification circuits with retained incremental comparison; a bounded
certified-mode simplification experiment; recoverable C service; authoritative
production readiness documentation. Commit and push each validated milestone.

1. Planning profiling/layout: complete as an evaluated experiment; compact header
   rejected, Linux stacks collected, hosted PMU explicitly unavailable. See
   PLANNING_LAYOUT_EXPERIMENT.md. Hardware cache/branch attribution still needs a
   PMU-enabled Linux host.
2. Real incremental verification workloads: complete. Four pinned upstream
   circuits, 192 release/128 sanitizer queries, retained CaDiCaL, fresh Kissat,
   independent simulation and checked certificates. See INDUSTRIAL_CIRCUIT_HISTORIES.md.
3. Certified-mode simplification: in progress.
4. Service recovery: pending.
5. Readiness consolidation: pending.

Timed runs are serial, without concurrent local builds/tests. Preserve negative
results and unavailable counters; never substitute software events for PMU data.
No heuristic defaults are promoted without validation and transferable evidence.
Previous final commit 1e90d92 passes correctness, fuzzing and independent CI.
