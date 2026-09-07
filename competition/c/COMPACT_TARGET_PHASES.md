# Compact target-phase storage

`Solver.rephase.best_phase` now stores one `uint8_t` per allocated variable slot,
rather than an `lbool` enum. Values remain `UNDEF=0`, `FALSE=1`, and `TRUE=2`.
The allocation and full-clear lengths follow the pointed-to element type. New
entries remain unknown, and disabling rephasing still avoids the allocation.

On this macOS arm64 build the enum occupies four bytes, so the target payload
falls by 75%, saving three bytes per allocated slot. This is an internal array
representation change; it does not change the truth-value enum or the public
solve result. Code directly accessing this internal pointer must use its updated
type. The C solver does not promise a stable internal structure ABI.

The historical target selection, prefix reuse, rephase frequency, saved boolean
polarities and stable-mode decisions are unchanged. Logical copy/clear counters
still count values, not bytes; they should remain equal between implementations.

## Validation

The target regression retains its independent `lbool` reference array and now
compares values individually instead of assuming identical byte layouts. It
already covers changed branches, restarts, incremental target capture, capacity
growth, all-known targets and API rebuilds. Additional coverage checks all three
codes through repeated growth to 20,000 variables, the one-byte element size,
and absence of the allocation when rephasing is disabled. The newly retained
partial-target rephase regression also runs against the compact representation.

Both builds pass all **38 C test executables**. Each passes **3,139 independent
validation solves** (73 configurations, seed 20261105), checking truth-table
answers, original SAT models and text/binary proofs with external drat-trim. Both builds pass 42 short-deadline cases, with
maximum observed CPU overrun 0.003 seconds in release and 0.000 seconds in debug.
These finite checks are regression evidence, not a universal soundness proof.

The core-types section of `docs/DATA_STRUCTURES.md` is corrected to match the
actual unsigned literal encoding, 32-bit decision levels, truth codes and inline
literal helpers. No core truth-value encoding changes in this milestone.

## Performance and retention

The [allocation comparison](benchmark_results/compact-target-storage-20260907.json)
uses 100,000, 500,000 and 1,000,000 declared variables with an original empty
clause. There are three repetitions per size/profile, a ten-second external wall
limit and seed 20261106. Both profiles verify **9/9** UNSAT proofs, with zero
errors. Median peak process RSS:

| Declared variables | Wide target | Compact target |
| --- | ---: | ---: |
| 100,000 | 17,022,976 bytes | 16,826,368 bytes |
| 500,000 | 69,812,224 bytes | 66,600,960 bytes |
| 1,000,000 | 123,273,216 bytes | 117,948,416 bytes |

The largest case saves 5,324,800 bytes of observed median peak RSS, about **4.3%**.
This process-level result includes allocator growth/retention effects and should
not be interpreted as the exact target payload difference. The exact payload
saving remains three bytes per allocated slot. Median process CPU at the largest
size is 0.019711 versus 0.018868 seconds, too short to claim a general speedup.
These are storage cases, not difficult search cases.

The [fixed-work comparison](benchmark_results/compact-target-work-20260907.json)
uses the existing 28-input development manifest, 10,000 conflicts, three solving
CPU seconds, a five-second wall limit and three repetitions (seed 20261107).
Both verify the same **9/84** runs, with zero errors. Thirteen selected counters
match across all six runs on every input: decisions, propagations, conflicts,
restarts, learned clauses/literals, deleted clauses, minimization inspections,
garbage collections, reductions, literal inspections, target literals copied and
target values cleared.

For the 26 inputs with baseline median process CPU above 0.05 seconds, the
geometric mean compact/wide CPU ratio is **1.007067** (about **0.7% slower**).
The [summary](benchmark_results/compact-target-summary-20260907.json) records
per-input medians, counter agreement and the selection rule. Mean wall PAR-2 is
8.934590 versus 8.935486 seconds; maximum process RSS is 42,647,552 versus
42,483,712 bytes. There is no demonstrated solved-count or broad CPU gain.

Retain the compact representation for its exact per-slot storage reduction and
measured large-allocation memory benefit, with the small observed CPU cost
disclosed. Search policy and measured work remain unchanged. The competition
performance goal remains open; this is not a parity result.

All performance jobs run serially after terminal completion of validation,
builds and deadline checks. Original SAT models and UNSAT proofs are checked
outside solver timing. Process CPU includes parsing/startup/output; the internal
solve limit excludes parsing. Unverified runs receive twice the wall limit as
PAR-2. Executable and checker hashes match after measurement. A release-flag
layout probe confirms `sizeof(lbool)==4` and a one-byte target element. The
corpora are reused development material, not held-out competition evaluation.

## Reproduction

Preserve the release executable from `9ad8aa1` as the baseline, then build this
change and run:

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 30 --seed 20261105
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug --checker /tmp/bsat-drat-trim --cases 30 --seed 20261105
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat_debug
python3 competition/c/tests/generate_var_storage_corpus.py /tmp/bsat-compact-target-storage
```

Raw performance records pin inputs, executables, checker and exact commands.
The declared-variable corpus tests allocation with an immediate empty-clause
UNSAT certificate; its variable count does not imply difficult search.

For the allocation screen, use the generated manifest with `tests/benchmark.py`,
`--timeout 10 --repeats 3 --seed 20261106` and `--proof {proof} {input}` in both
solver templates. For fixed work, use the compact-trail manifest, the caps above
and seed 20261107. Exact baseline/candidate commands are in the raw records.
