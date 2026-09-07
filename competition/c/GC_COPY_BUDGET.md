# Interruptible garbage-collection copying

The collector previously ignored work limits and cached expired CPU deadlines
while allocating/copying the fresh arena. Its entire copy completed even when
the caller's work budget was already exhausted. This change makes the copy
phase cancellable before old-header forwarding commits relocation.

A fresh budget/deadline check runs before allocation and immediately before
forwarding. Each old header visit charges one work unit, including deleted
records. Live records copy their complete header and literals into the reserved
fresh arena in chunks of at most 1,024 words, ending at global work boundaries
and capped by remaining work allowance. The normal budget helper polls after
each chunk, enforcing work limits immediately and advancing the shared CPU-clock
throttle with copy work. These polls use the normal cached-clock policy; entry
and pre-commit checks explicitly force fresh clock reads. The old
arena and all external references remain untouched until copying succeeds.
Cancellation discards the fresh arena; a later call can retry from the original.

Fresh storage already reserves the complete live payload. Copying raw records
avoids initializing headers only to overwrite them, and preserves clause order,
search cursors, flags, LBD and activity. The existing forwarding/remapping phase
still runs synchronously once committed. Allocation/free costs, reduction
sorting and committed relocation are not made interruptible by this change;
this is not a strict latency bound for the entire collector or solver.

Work accounting now includes copied GC words and visited headers. Consequently,
`Literal inspections` (the existing label for this broader work counter) changes,
and budget-limited preprocessing may stop earlier. Ordinary unlimited-work
search order is intended to remain unchanged; deadline-limited runs can stop at
different points.

## Regression coverage

The new test checks all 73 remaining-work allowances from zero through 72 on a
65-literal live record, plus nine cutoffs around chunk boundaries and completion
on a 4,097-literal record. Aborted cases compare the entire old arena byte for
byte and retain its identity/reference; short cases also retry and validate the
original model. The final case warms the deadline cache with an already-expired
deadline and requires an immediate fresh check without copy work or collection.

Compiled against baseline `5552068`, the new test aborts at the first exhausted
work-limit case: the old collector incorrectly commits collection. The final
implementation passes. Existing forwarding tests cover 512 deletion patterns,
complete headers, cursors, watch encodings/order, reasons and BVE occurrences.

Both release and ASan/UBSan debug builds pass all 33 C test executables.
Three generated collection-driver cases also pass sanitizer execution and
independent original-model checks. Each build passes 2,967 independent validation
solves (seed 20261018), checking truth-table answers, original SAT models and
text/binary UNSAT proofs with external drat-trim. Each also passes 42 short-deadline
runs, with maximum observed CPU overrun of 0.000 seconds. These finite checks
provide regression evidence, not a universal soundness proof.

## Performance cost

The [collection comparison](benchmark_results/gc-budget-kernel-20260907.json)
uses the previous generated positive 32-literal databases. Both drivers use ten
collections per run, three repetitions per size/profile and a 20-second external
wall limit. All nine original models per profile verify, with zero errors.
Median measurements:

| Clauses | Baseline collection CPU sum | Bounded collection CPU sum | Baseline process CPU | Bounded process CPU |
| --- | ---: | ---: | ---: | ---: |
| 10,000 | 0.001308 s | 0.001741 s | 0.009580 s | 0.010366 s |
| 50,000 | 0.010510 s | 0.011162 s | 0.044330 s | 0.043256 s |
| 100,000 | 0.027585 s | 0.031469 s | 0.092879 s | 0.094537 s |

The added checks cost collection CPU at each size. At 100,000 clauses, the
increase is about 3.9 milliseconds over ten collections (14.1%). Mean wall PAR-2
worsens from 0.08800 to 0.10466 seconds; maximum observed RSS is effectively
unchanged, 110,444,544 versus 110,395,392 bytes. This experiment does not claim
faster collection.

The [28-input competition development comparison](benchmark_results/gc-budget-work-20260907.json)
uses three repeats per profile, 10,000 conflicts, three solving CPU seconds and
a five-second external wall limit. Both profiles verify the same 9/84 runs,
with zero errors. Ten selected search counters match across all six runs on
every input: decisions, propagations, conflicts, restarts, learned clauses and
literals, deleted clauses, minimization inspections, garbage collections and
reductions. `Literal inspections` is excluded because it intentionally gains
GC work; this exclusion is recorded explicitly in the
[derived summary](benchmark_results/gc-budget-summary-20260907.json).

For the 26 inputs whose baseline median process CPU exceeds 0.05 seconds, the
geometric mean bounded/baseline CPU ratio is **1.008767** (0.88% slower). Mean
wall PAR-2 is 8.93507 versus 8.93749 seconds. Maximum observed RSS is 42,631,168
versus 42,582,016 bytes. This is a small measured execution cost with unchanged
solved counts in the screen, not a performance improvement or competition-parity
result. The reused development corpus and conflict cap are not held-out
competition evaluation.

All performance jobs are serial and separate from builds/validation. Original
models and proofs are checked outside solver timing. Raw records retain exact
commands and executable/input hashes, and all measured executable hashes are
verified after checks. Process CPU includes startup, parsing and output; the
internal solve limit excludes parsing. Unsolved PAR-2 penalties are twice the
external wall limit.

## Retention and reproduction

Retain the change to fix the reproduced ignored-work-budget behavior and permit
safe copy cancellation on CPU deadlines. The tests verify rollback and retry;
the added runtime cost is disclosed above. The subsequent committed forwarding
phase remains synchronous, so complete collector latency control is still open.
The broader competition-performance goal also remains open.

```sh
make -C competition/c all test gc-driver
make -C competition/c all test gc-driver MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 30 --seed 20261018
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat
python3 competition/c/tests/generate_gc_corpus.py /tmp/bsat-gc-corpus
competition/c/bin/gc_driver /tmp/bsat-gc-corpus/clauses-100000.cnf 10
```

Repeat validation/deadline commands with `bin/bsat_debug`. Build `5552068` for
the baseline and link this milestone's `test_gc_budget.c` against its core
objects, excluding `main.o`, to reproduce the failing budget regression. The
driver is unchanged from that baseline; raw records supply paired commands.
