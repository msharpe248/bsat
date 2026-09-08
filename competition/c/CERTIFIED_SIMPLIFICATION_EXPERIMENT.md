# Bounded certified probing — 2026-09-08

Decision: keep certified-mode preprocessing unchanged. A namespace-preserving
failed-literal probing prototype passed independent validation but did not improve
solved counts and slowed the retained certified mode on the circuit screen.

The preceding [real-circuit campaign](INDUSTRIAL_CIRCUIT_HISTORIES.md) found a
measurable certification cost. A small proof-producing pass was tested before
adding more aggressive transformations to the retained journal path.

## Candidate and soundness boundary

The prototype enables the existing failed-literal probing pass only in an
experimental build, with 100,000 optional work units and input-change tracking.
It runs before temporary assumptions, adds only units demonstrated by reverse
unit propagation from the permanent formula, and journals each unit before using
it. Such consequences remain entailed after future permanent additions; variable
elimination, substitution and other equisatisfiable rewrites stay disabled.
The budget bounds optional probing work, not mandatory root propagation or a
hard wall deadline. Cancellation/error handling still applies to the whole query.

The [archived patch](benchmark_results/certified-probing-experimental-20260908.patch)
is enabled with `-DBSAT_EXPERIMENT_CERT_PROBING` in compiler flags. No new public
ABI flag is shipped, and the production source was restored after evaluation.

## Validation and screen

The experimental build passed the full C suite, 96 industrial queries at depths
through 16, 64 industrial ASan/UBSan queries through depth 4, and 256 stateful
differential queries with growing input, assumption changes, retained CaDiCaL,
fresh Kissat, cancellation, checkpoints and independent certificate validation.

The serial circuit screen used identical histories and budgets to the preceding
baseline, with no concurrent local build/test work. Both certified modes still
timed out on the positive `cal3` query at depth 16 and resolved the same 94/96
queries. On the common conclusive queries:

| Certified mode | Baseline query CPU | Probing query CPU |
|---|---:|---:|
| Rebuild, flag 2 | 1.424 s | 1.118 s |
| Retain learning, flag 3 | 1.151 s | 1.469 s |

This is a single exploratory history per mode, not a statistically established
speed comparison. Probing slowed both USB and image-FIFO histories, and its effect
on `gen23` reversed between modes. `gen23` journal peaks grew from 1.71 to 5.00 MB
for flag 2, and 5.97 to 7.99 MB for flag 3. The lower `cal3` journal size under
a fixed CPU timeout is not evidence of better compression: different amounts of
search can finish before the deadline. There is no basis for default promotion.

Raw evidence: [release circuit screen](benchmark_results/certified-probing-industrial-20260908.json),
[sanitizer smoke](benchmark_results/certified-probing-debug-20260908.json),
[stateful validation](benchmark_results/certified-probing-stateful-20260908.json).
The unresolved work is a targeted improvement on `cal3` and certified `gen23`
without increasing proof growth or harming retained-query throughput.
