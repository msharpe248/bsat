# Buffered DIMACS input

The parser reads stream bytes in fixed 64 KiB chunks instead of calling `fgetc`
for each byte. Token boundaries, decimal conversion, comments, whitespace and
zero-terminated clauses retain their existing rules. The header's `cnf` token
uses the same reader with comment skipping disabled, so no separate stdio scanner
can bypass buffered data. Each parse uses one 64 KiB automatic input buffer; it
does not load the entire file into a new buffer.

Two malformed-input behaviors are corrected. An embedded NUL in a token is now
rejected, rather than silently terminating `strcmp` or `strtoll` before remaining
token bytes. For example, the baseline reports SAT for the bytes `1<NUL>x 0` in a
one-variable formula; the new CLI reports a format error. NUL bytes inside ignored
comments remain comment payload. Stream read failures now report a file error,
including failures while reading the header.

`test_input_reader` checks seventy offsets around a buffer boundary, splitting
header fields, signed literals, whitespace and terminators. It also covers a
150000-byte comment ending at EOF, final tokens without a newline, embedded NULs
in each header/body token position, 63/64-byte token limits, integer overflow,
and an actual stream-read error. Valid cases are solved and their SAT models are
checked. The CLI validator separately rejects embedded NULs in a literal and in
the format token.

A fresh parallel build exposed a missing Makefile dependency: object compilation
could start before its output directory existed. Object targets now have an
order-only dependency on directory creation. The clean release build passed all
fourteen C test executables and reproduced both production and parse-benchmark
executable hashes.

## Parse-only measurement

`make parse-benchmark` builds `bin/parse_benchmark_release` (or the debug suffix
with `MODE=debug`). Its CPU timer surrounds input parsing and clause insertion,
excluding solver creation, the subsequent fingerprint pass and SAT search. It
also records variable/clause counts, a fingerprint of original literal order and
clause separators, and peak resident memory. Independent SAT/model/proof
validation is performed separately.

`tests/benchmark_parse.py` runs these drivers serially in seeded shuffled order,
pins executable/input SHA256 hashes, and rejects any disagreement in original
input fingerprints or counts. An identical baseline binary appears under two
labels to expose timing variability. Input hashing warms file data before runs;
these measurements characterize CPU loading cost, not cold-storage latency.

The baseline parser comes from commit
`4ade636d27c64ad16289f2151b41c922a2184865`. Both parser drivers use the same solver
sources, compiler options and benchmark driver. To recreate the baseline in an
isolated copy of this milestone, replace only `src/dimacs.c` with that revision's
file, then run `make -j4 parse-benchmark`. Exact driver and input hashes are in
the raw results. Measurements use macOS arm64 and do not establish timing on
other platforms.

## Results

Final measurements use the Makefile-built drivers, two repetitions per parser,
and an identical-binary control. All original-input fingerprints and counts match.

| Input family | File size (MB, decimal) | Baseline parse CPU | Buffered parse CPU |
| --- | ---: | ---: | ---: |
| Independent set | 36.7 | 0.8183 s | 0.2300 s |
| Modular circuits | 3.3 | 0.0739 s | 0.0247 s |
| GRS FP communication | 20.4 | 0.4689 s | 0.1404 s |
| Equivalence checking | 31.1 | 0.7269 s | 0.2320 s |
| Bitvector | 29.7 | 0.6733 s | 0.2155 s |
| Stedman triples | 3.0 | 0.0681 s | 0.0214 s |
| CRIL miscellaneous | 333.4 | 7.4191 s | 2.1710 s |
| MD5 equivalence checking | 327.8 | 7.5084 s | 2.2950 s |
| Software verification | 460.6 | 9.8013 s | 2.5800 s |

The geometric mean buffered/baseline CPU ratio is 0.3108 on the six medium files
and 0.2866 on the three large files: about 69% and 71% lower, respectively. The
identical-binary control ratios are 1.0153 and 0.9995. Earlier direct-link driver
screens also showed large gains; final reports below use the repeatable Makefile
target and its pinned executable hashes.

The full CLI loading comparison uses these nine files with one decision, probing
and on-the-fly subsumption disabled, three CPU seconds for solving, and 25 wall
seconds. All fourteen compared search/deadline/target counters match. Whole-process
CPU has a geometric mean ratio of 0.3183 (about 68% lower). All runs are UNKNOWN
at the decision cap, with zero errors; this deliberately isolates loading and is
not a solved-count improvement. Peak RSS is approximately 3.15 GB for both builds.

The search comparison uses the twelve larger development inputs, two repetitions,
10000 conflicts, 20 solving CPU seconds and 25 wall seconds. All fourteen compared
counters again match. Both versions verify two of 24 runs, with zero errors. On
eleven inputs with baseline median CPU above 0.05 seconds, the whole-process CPU
ratio is 0.9638. This smaller aggregate difference should not be generalized given
previously observed host timing variability. The substantial, controlled parse-only
gain is the primary performance evidence.

Parser-driver timing includes clause insertion and input retention; CLI timing
also includes startup, search and proof output. Certificate checks are outside
CLI timing. These files are reused development inputs, not held-out competition
families, and the experiments do not establish competition parity.

## Evidence and reproduction

- [Medium input manifest](benchmark_results/buffered-input-corpus-20260906.json) and [final runs](benchmark_results/buffered-input-final-corpus-20260906.json)
- [Large input manifest](benchmark_results/buffered-input-large-corpus-20260906.json) and [final runs](benchmark_results/buffered-input-final-large-corpus-20260906.json)
- [CLI loading comparison](benchmark_results/buffered-input-loading-20260906.json)
- [CLI search comparison](benchmark_results/buffered-input-search-20260906.json)
- [Derived summaries and counter comparisons](benchmark_results/buffered-input-final-summary-20260906.json)
- [Embedded-NUL reproduction](benchmark_results/buffered-input-nul-20260906.json)
- [Compiler, builds and hashes](benchmark_results/buffered-input-build-20260906.json)
- [Driver smoke test](benchmark_results/buffered-input-smoke-20260906.json)
- [Release deadlines](benchmark_results/buffered-input-limits-release-20260906.json) and [sanitizer deadlines](benchmark_results/buffered-input-limits-asan-20260906.json)

Build each parser driver with `make parse-benchmark`, then pass the driver paths
and manifest inputs to `tests/benchmark_parse.py`, using `--parser NAME=PATH`,
`--repeats 2` and `--output results.json`. Include the baseline under a second
name for the identical-binary control. Linux/macOS CI builds the driver and runs
a small protocol/fingerprint smoke test in release and sanitizer modes.

Final validation passed 4859 release and 4859 ASan/UBSan formula solves across
43 configurations (seed 20260917), all fourteen C test executables in both modes,
and twelve short-deadline runs per mode. CLI malformed-NUL checks and parser-driver
smoke tests also passed. Final executable hashes match the measured binaries. See
the [validation record](benchmark_results/buffered-input-validation-20260906.json).
