# Long-clause watch prefetch experiment

The candidate reads four watch entries ahead during propagation and prefetches
an upcoming long clause's arena header into the cache. It skips implicit and
tagged arena binaries and bounds the lookahead by the remaining watch count.
GCC/Clang use `__builtin_prefetch`; other compilers omit the hint. The baseline
is `01d31fa`. Watch order, literal order, blocker updates and work accounting
remain unchanged. The intended benefit is hiding arena-access latency; possible
costs include extra branches, bandwidth and cache pollution for satisfied clauses.

The local pinned Kissat source (`8af8e56f174b778aef3aa45af9f739b2a5f492c2`,
`src/inlineassign.h`) prefetches a newly assigned literal's watch vector. This
experiment instead prefetches future clause headers, so it is not a replication
of Kissat's policy. Earlier BSAT blocker-refresh and contiguous-scan experiments
are documented in [Propagation experiments](PROPAGATION_EXPERIMENTS.md).

A new regression enumerates 459 mixed-watch cases: list sizes zero through 16,
three rotations of implicit binary/tagged binary/long clauses, and every possible
conflict position plus a no-conflict case. It checks propagation values, exact
reason/conflict references and preservation of the watch list. The suite covers
lookahead boundaries and ensures binary tags cannot become arena pointers.

## Validation and measurement

The candidate passes all 30 C test executables in release and ASan/UBSan debug
builds, including the 459 new cases. Each build also passes 2,709 independent
solves across 63 configurations (30 random and 13 fixed formulas, seed 20261014),
checking truth-table answers, original SAT models and text/binary RUP proofs,
with external drat-trim verification. This establishes regression evidence,
not universal correctness or competition parity.

The serial comparison uses the existing 28-input development manifest, three
repetitions per executable/input, a 10,000-conflict cap, a three-second solving
CPU limit and a five-second external wall limit. Builds and validation finished
before performance timing began. Original-model and proof checks are outside
solver timing. Process CPU includes parsing, startup and proof output. An
unsolved run receives a ten-second PAR-2 penalty.

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 9/84 | 8.9390 s | 45,613,056 bytes | 0 |
| Prefetch | 9/84 | 8.9344 s | 44,466,176 bytes | 0 |

All eleven compared search counters match across all six runs on each of the
28 inputs: decisions, propagations, conflicts, restarts, learned clauses/literals,
deleted clauses, minimization inspections, literal inspections, garbage
collections and reductions. The same three inputs are solved in every repeat.

For the 26 inputs with baseline median process CPU above 0.05 seconds, the
geometric mean candidate/baseline CPU ratio is **1.013618** (1.36% slower).
Individual changes go both ways. The tiny wall PAR-2 improvement does not
establish a useful throughput gain. These reused development inputs and conflict
caps are a diagnostic, not a competition score or held-out evaluation.

- [Raw measurements](benchmark_results/watch-prefetch-work-20260907.json)
- [Per-input medians and counter comparisons](benchmark_results/watch-prefetch-summary-20260907.json)
- [Rejected implementation patch](benchmark_results/watch-prefetch-rejected-20260907.patch)

Both measured executable hashes were verified before restoring the production
source. The raw file also records exact commands and input hashes.

## Decision and reproduction

Reject the prefetch hint: the measured aggregate CPU result does not justify
adding work to the propagation loop. Production solver source is restored to
`01d31fa`; retain the mixed-watch regression and evidence. This result applies
to four-watch clause-header lookahead on the measured macOS arm64 host, not to
all prefetch policies or architectures. The competition-performance goal remains
open.

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 30 --seed 20261014
python3 competition/c/tests/benchmark.py \
  --solver 'baseline=/tmp/bsat-prefetch-baseline --conflicts 10000 --time 3 --proof {proof} {input}' \
  --solver 'prefetch=competition/c/bin/bsat --conflicts 10000 --time 3 --proof {proof} {input}' \
  --manifest competition/c/benchmark_results/compact-trail-corpus-20260907.json \
  --checker /tmp/bsat-drat-trim --timeout 5 --repeats 3 \
  --output /tmp/watch-prefetch-results.json
```

For candidate reproduction, preserve a baseline executable from `01d31fa`,
apply the rejected patch to that source, and rebuild before running the paired
benchmark. Repeat validation with `bin/bsat_debug` for sanitizer coverage.
The retained regression also passes with the restored production implementation.
