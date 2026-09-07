# Small-clause normalization storage

Clause insertion now uses a fixed 16-literal (64-byte) automatic buffer for short
clauses. Longer clauses retain the previous heap allocation. The original input
is still recorded before normalization, and the caller's array is copied before
sorting. Duplicate removal, tautology detection, arena storage, unit handling and
watch insertion follow the same paths. The arena copies literals; no pointer into
the temporary buffer escapes insertion. Every exit frees only a heap-backed buffer.

The cutoff keeps the additional automatic storage small while covering ordinary
short CNF clauses. It is an implementation choice, not a new search option or an
experimentally optimal threshold. The change removes a malloc/free pair for each nonempty clause of at most
sixteen literals that reaches normalization; other solver allocations remain. Clauses recorded after an already known contradiction retain their early
return and do not need normalization storage.

## Tests

The regression exercises lengths 0, 1, 2, 15, 16, 17, 32 and 65 with distinct
literals, duplicates and complementary literals. It checks caller-buffer
immutability, exact original-input preservation, normalized arena contents,
tautology removal, empty-clause UNSAT, SAT models, and rebuilds after solving.
This covers both sides of the storage cutoff and its early-return paths. The
existing release and sanitizer suites additionally cover propagation, proofs,
preprocessing, assumptions, reduction and repeated API use.

## Measurements

The baseline is commit `2e24a05083c29dd3901d47eb6ea39e8404b70e8e`, including buffered
input. Nine previously recorded files (3–461 MB) were parsed twice per variant.
A second label invokes the identical baseline executable as a timing control.
All original-input fingerprints and counts match.

The geometric mean candidate/baseline parsing CPU ratio is 0.9236 (7.6% lower),
while the identical-binary control ratio is 1.0054. Every input's candidate median
is lower, with ratios from 0.8719 to 0.9595. The three large files improve by
roughly 7–8%; for example, CRIL miscellaneous falls from 2.1875 to 2.0151 CPU
seconds. These gains are incremental to the preceding buffered-parser milestone.

The twelve-input search comparison uses two repetitions, 10000 conflicts,
20 solving CPU seconds and 25 wall seconds. All fourteen compared search,
deadline and target counters match. Both versions verify two of 24 runs, with
zero errors; the remaining runs are UNKNOWN. The eleven inputs whose baseline
median CPU exceeds 0.05 seconds have a geometric mean ratio of 0.9961, essentially
unchanged. No competition solved-count gain is demonstrated.

Runs were serial on macOS arm64 without concurrent local builds or tests.
Parser-driver timing includes loading and clause insertion, with fingerprints
computed afterward. Input hashing warms file data before timing. CLI timing also
includes startup, search and proof output; independent certificate checking is
outside timing. These are reused development inputs, not held-out families.

## Evidence

- [Parsing runs](benchmark_results/small-clause-parse-20260906.json)
- [Parsing summary](benchmark_results/small-clause-parse-summary-20260906.json)
- [Search runs](benchmark_results/small-clause-search-20260906.json)
- [Search summary and counter comparison](benchmark_results/small-clause-search-summary-20260906.json)
- [Release deadlines](benchmark_results/small-clause-limits-release-20260906.json)
- [Sanitizer deadlines](benchmark_results/small-clause-limits-asan-20260906.json)

Build each revision with `make -j4 all parse-benchmark`, then use
`tests/benchmark_parse.py` with the driver paths and the raw report's input list.
The reports pin executable/input hashes, settings and per-run observations.

Final validation passed 4859 release and 4859 ASan/UBSan formula solves across
43 configurations (seed 20260918), all fourteen C test executables in both modes,
and twelve short-deadline runs per mode. Final executable hashes match the measured
binaries. See the [validation record](benchmark_results/small-clause-validation-20260906.json).
