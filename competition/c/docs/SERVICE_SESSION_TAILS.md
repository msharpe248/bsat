# Long-running service sessions and checkpoint policy

`tests/benchmark_service_sessions.py` measures serialized public API calls with
`perf_counter`, including ctypes-call overhead. Six macOS ARM64 release-library
sessions each execute 8,192 queries: 49,152 total. Every result matches an independent
domain oracle. Each session samples 64 exported queries (384 total), compares the
exported formula exactly with the caller's original clauses and assumptions, and
checks SAT models or converts UNSAT proofs to LRAT for cake_lpr validation. All
checks pass. Binary/checker/input/proof hashes are recorded. Validation and Python
context checks are outside the measured call intervals. No local builds/tests or
fuzzers overlap the final timed run.

Workloads:
- Fixed: 16,384-variable equality chain, 32,766 binary clauses.
- Growing: equality chain grows from 1,024 to 16,384 variables, then stays fixed.
- Learning: a 4,096-variable equality chain plus guarded PHP(7,6), 4,139 variables
  and 8,323 clauses total. Alternating selector assumptions give SAT and UNSAT;
  checkpoints discard useful learned clauses and require renewed search.

Each session includes 32 query cancellation/retry pairs, nine cancelled checkpoint
retries and 17 wall-deadline exhaustion/retry pairs. There are no unexpected UNKNOWNs
or persistent errors. Across all sessions this is 192 query cancellations, 54
checkpoint cancellations, and 102 deadline retries, in addition to the conclusive
queries. Explicit checkpoint counts below include successful cancellation retries.

| Workload | Journal watermark | Query p95 / p99 ms | Checkpoints | Checkpoint p99 ms | Export p99 ms |
|---|---:|---:|---:|---:|---:|
| fixed | 256 B | 0.317 / 0.391 | 97 | 4.053 | 5.760 |
| fixed | 2,048 B | 0.323 / 0.410 | 17 | 3.789 | 5.646 |
| growing | 256 B | 0.313 / 0.343 | 94 | 3.784 | 5.885 |
| growing | 2,048 B | 0.302 / 0.320 | 17 | 3.761 | 5.719 |
| learning | 8,192 B | 2.533 / 2.718 | 4105 | 1.116 | 1.871 |
| learning | 65,536 B | 0.069 / 0.819 | 17 | 1.027 | 1.454 |

The learning workload makes the policy cost concrete. An 8 KiB watermark causes
4,105 checkpoints and 2,691,016 conflicts. At 64 KiB these fall to 17 checkpoints
and 41,992 conflicts. Total measured solve plus checkpoint time falls from
13.666 to 0.512 seconds for the same 8,192-query history, about 27x in this workload.
The solver algorithm and answers are unchanged. This supports selecting a
watermark above the normal query proof burst when memory/disk allowance permits,
rather than checkpointing after every moderately expensive query. It does not
justify a universal 64 KiB default or a general solver-speedup claim.

Hard journal limits are 128 KiB for chain sessions and 1 MiB for learning sessions.
Observed journal maxima are 258/261 bytes at the 256-byte watermark, 2,052 at 2 KiB,
and 16,204/81,732 for the learning policies. A watermark is checked between queries,
so it can be exceeded by one query's accepted proof records; the hard quota is
separate and never exceeded. Reserve sufficient headroom for the query workload.
These runs do not promise a bound on arbitrary single-query proof size.

Owned core capacity stays effectively flat for fixed input (about 4.012 MB). The
growing chain rises from 0.273 to 4.012 MB while capacity per permanent input literal
falls from 66.64 to 61.22 bytes. Learning peaks near 1.615 MB for either watermark.
These are requested core capacities, not RSS, transient checkpoint overlap, Python
memory, filesystem allocation or a total process-memory bound. No unexplained
core-capacity growth appears in these finite histories.

Checkpoint and export calls are materially longer than ordinary chain queries.
The existing API provides the controls needed for this workload: proactive
`bsat_get_journal_bytes`, a chosen watermark, `bsat_checkpoint`, and a separate
hard `bsat_set_journal_limit`. Expensive learning argues for less frequent
checkpoints where quota permits. The measurements do not yet justify replacing
the proof-journal implementation with learning-preserving compaction.

Each profile has one final measured session. Quantiles use nearest rank; export
samples number 64, and some checkpoint samples only 17, so p99 is often the observed
maximum. These synthetic workloads and host observations are not a production tail
latency guarantee or a representative application suite. Cancellation can also
cause retained state to be rebuilt, and remains part of the measured histories.

Reproduce with a release shared library and `BSAT_DRAT_TRIM`/`BSAT_CAKE_LPR` set:
`python3 tests/benchmark_service_sessions.py --library bin/libbsat_release.dylib
--output service-sessions.json.gz` (use `.so` on Linux). A `.gz` output preserves
all raw query observations compactly. The committed summary contains the compressed
raw report's SHA256; both live under `benchmark_results/service-sessions-20260908*`.
The Linux integration workflow runs a shorter 128-query-per-profile lifecycle
check; its hosted times are not treated as a performance baseline.
