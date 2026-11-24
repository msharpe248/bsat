# Fresh larger holdout baseline — 2026-09-08

Unchanged BSAT completes 4/18 runs and pinned Kissat 2/18, with zero errors,
under the frozen 10-process-CPU / 15-wall-second limits. All six completed runs
are independently checked SAT. The other 30 are UNKNOWN. This short screen
provides no additional completed UNSAT evidence and does not establish a general
solver ranking.

The nine fresh CNFs include eight distinct families with files of 1–30 MB and
the only previously unrecorded local planning instance, 204.8 MB. Selection
excludes names and content hashes in 584 historical reports. The size exception
for planning was fixed before solving. Two serial repetitions use seed
2026090863, with no concurrent local builds or tests. Both solvers write binary
proof streams; their commands, binaries and input hashes are recorded.

| Solver | Checked runs | Mean CPU PAR-2 | Mean wall PAR-2 | Maximum solver RSS |
|---|---:|---:|---:|---:|
| BSAT | 4/18 | 16.275 s | 24.060 s | 1.121 GB |
| Kissat | 2/18 | 17.873 s | 26.765 s | 1.335 GB |

CPU PAR-2 charges unfinished or over-budget runs 20 seconds; wall PAR-2 charges
them 30. CPU uses measured child-process CPU, including parse and output. These
different timeout penalties must not be mixed when comparing prior reports.

Both solvers solve the selected hardware-model-checking instance. BSAT additionally solves
the larger planning case: **611,755 variables and 10,974,540 clauses**, in 6.118
and 6.103 CPU seconds; Kissat reaches its external CPU ceiling in both trials.
BSAT's original-model checks take another 9.50 and 9.60 wall seconds outside
solve timing. Solver RSS excludes that separate Python validation stage. This
distinction matters to any deployment's complete answer-acceptance latency and
memory budget. Larger input size alone does not imply greater search difficulty.

The seven other inputs remain UNKNOWN for both solvers. Once inspected, this
corpus is development evidence and cannot be reused as a fresh holdout after
further tuning.

Reproduce with `tests/benchmark.py` using the exact command templates and
selection manifest recorded in
`benchmark_results/search-quality-holdout-baseline-20260908.json` and
`search-quality-holdout-selection-20260908.json`. The four separately frozen
larger circuits are covered by the certified-probing milestone.
