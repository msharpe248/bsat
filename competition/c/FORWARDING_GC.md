# Garbage collection with old-header forwarding

The previous collector allocated one `CRef` per old arena word for its relocation
map. That temporary allocation was as large as the entire used old clause arena,
in addition to the old and fresh arenas. This change eliminates the map by
storing forwarding references in old clause headers after copying succeeds.

[MiniSat's relocation representation](https://github.com/niklasso/minisat/blob/master/minisat/core/SolverTypes.h#L181)
stores forwarding references in relocated clause storage. BSAT uses its old
`search` header field and a two-pass copying/forwarding procedure, rather than
MiniSat's tagged relocated-clause representation.

First, copy live clauses and their complete headers into the fresh arena, in
existing order. Allocation/copy failure still returns with the original arena
and headers intact. Only after every copy succeeds does a second pass replace
old search cursors with new clause references, or `INVALID_CLAUSE` for deleted
clauses. Fresh headers already retain the original search cursors, LBD, activity
and flags. Watch, reason, original-clause and learned-clause relocation reads
these old-header references; the old arena is freed after switching ownership.

The second pass allocates nothing and preserves clause sizes, so the next old
header remains locatable. Empty and unit clauses have headers and need no special
forwarding storage. Existing watch tags/order, clause-copy order, occurrence
rebuilding, proof behavior and collection threshold remain unchanged. The
collector remains a synchronous operation; this change does not add interruptible
collection. No permanent header field or metadata allocation is added.

## Validation

The new regression enumerates 512 live/deleted combinations over empty, unit,
implicit binary, tagged binary and long records. It checks exact header/literal
copies, including invalid and valid saved search cursors; array and watch order;
remapping of live and cleared reasons; stale arena-watch filtering; and the
all-deleted arena. A separate BVE case verifies rebuilt occurrence references,
clearing obsolete resolvent references, and subsequent original-model validity.
Existing tests cover active reason locks, tagged binaries, reductions and API
rebuilds. Three generated driver inputs also pass sanitizer execution and
independent original-model checks.

Release and ASan/UBSan debug builds pass all 32 C test executables. Each also
passes 2,967 independent solves across 69 configurations (30 random and 13 fixed
formulas, seed 20261017), checking truth-table answers, original SAT models and
text/binary RUP proofs with external drat-trim. Both pass all 42 deadline cases;
maximum observed CPU overrun is 0.000 seconds in release and 0.001 in debug. Deadline checks
ran alongside independent validation, separately from performance timing.

## Isolated collection measurements

Baseline is `fd2e7a1`. Both drivers parse the same positive 32-literal clause
databases, append deleted arena-only padding, and measure only collection CPU
inside the loop. Padding is half the live arena size, so collection always
triggers. The all-positive original models are independently verified; the
padding is never part of the original formula. Three repetitions per size and
profile use a 20-second external wall limit.

[Ten-collection results](benchmark_results/forward-gc-kernel-20260907.json), medians:

| Clauses | Baseline collection CPU sum | Forwarding CPU sum | Baseline process CPU | Forwarding process CPU |
| --- | ---: | ---: | ---: | ---: |
| 10,000 | 0.001413 s | 0.001295 s | 0.010646 s | 0.009723 s |
| 50,000 | 0.019730 s | 0.008185 s | 0.057796 s | 0.038799 s |
| 100,000 | 0.042612 s | 0.026679 s | 0.113443 s | 0.100330 s |

All nine models per profile verify with no errors. Median peak RSS changes from
13,664,256/55,984,128/103,202,816 bytes to
11,501,568/48,791,552/110,444,544 bytes. The largest repeated-collection case uses
more observed peak RSS despite removal of the map; allocator retention and
allocation patterns affect process peaks. Mean wall PAR-2 also worsens from
0.09270 to 0.10023 seconds. These measurements do not prove universal memory or
end-to-end speed gains.

A [single-collection check](benchmark_results/forward-gc-single-20260907.json)
examines that memory result without ten rounds of allocation reuse. All nine
models per profile verify, with no errors. Medians:

| Clauses | Baseline peak RSS | Forwarding peak RSS | Baseline collection CPU | Forwarding collection CPU |
| --- | ---: | ---: | ---: | ---: |
| 10,000 | 13,647,872 bytes | 11,501,568 bytes | 0.000200 s | 0.000159 s |
| 50,000 | 56,000,512 bytes | 45,187,072 bytes | 0.001717 s | 0.002309 s |
| 100,000 | 103,202,816 bytes | 81,625,088 bytes | 0.003840 s | 0.005001 s |

Here peak RSS falls at every size, by 21,577,728 bytes (20.9%) at 100,000 clauses.
Single-collection CPU rises at the two larger sizes. The guaranteed change is
removal of `old_arena_size * sizeof(CRef)` temporary allocation, not a universal
RSS or CPU reduction. These generated formulas measure storage/collection work,
not difficult SAT search.

## Competition development measurements

The [fixed-work comparison](benchmark_results/forward-gc-work-20260907.json)
uses the existing 28-input development manifest, three repeats per profile,
10,000 conflicts, three solving CPU seconds and a five-second wall limit.
Eleven search counters match across all six runs on each input. Both profiles
verify the same 9/84 runs, with zero errors. Among 25 inputs with baseline median
process CPU above 0.05 seconds, the geometric mean candidate/baseline ratio is
**0.999696**, effectively unchanged. Mean wall PAR-2 is 8.93574 versus 8.93420
seconds; maximum observed RSS is 44,302,336 versus 42,582,016 bytes.
[Per-input medians and counter list](benchmark_results/forward-gc-summary-20260907.json).

The [longer comparison](benchmark_results/forward-gc-targeted-20260907.json)
runs four competition development targets twice, with ten solving CPU seconds
and a fifteen-second wall limit. Kissat is pinned at
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`.

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 6/8 | 9.6752 s | 35,127,296 bytes | 0 |
| Forwarding | 6/8 | 9.6851 s | 29,704,192 bytes | 0 |
| Kissat | 8/8 | 0.5962 s | 26,296,320 bytes | 0 |

Both BSAT profiles solve battleship, belpyramid and Hamiltonian twice, with
unchanged conflict/collection counts of 107,461/53, 30,460/2 and 9,247/4.
Median process CPU is 2.014390 versus 2.097299 seconds for battleship,
4.893091 versus 4.867754 for belpyramid and 0.130265 versus 0.131512 for
Hamiltonian. Multiplier remains UNKNOWN; forwarding completes fewer conflicts
within this particular time-limited sample. There is no solved-count gain, and
Kissat remains clearly ahead on this selected screen.

All performance jobs are serial and separate from builds/validation. Model and
proof checks run outside solver timing. Process CPU includes parsing, startup
and output; the internal solver limit excludes parsing. Unsolved PAR-2 penalties
are twice the external wall limit. Raw records retain exact commands and
input/executable hashes; every measured executable hash is checked after timing.
These are reused development inputs, not held-out competition families.

## Retention and reproduction

Retain the collector for removing the arena-sized temporary allocation while
preserving the checked search behavior. Measurements show useful savings on
single-collection memory and repeated collection CPU, with mixed results on
other metrics. This is not a general speedup or competition-parity claim. The
broader competition-performance goal remains open.

```sh
make -C competition/c all test gc-driver
make -C competition/c all test gc-driver MODE=debug
python3 competition/c/tests/generate_gc_corpus.py /tmp/bsat-gc-corpus
competition/c/bin/gc_driver /tmp/bsat-gc-corpus/clauses-100000.cnf 10
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 30 --seed 20261017
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat
```

Repeat validation/deadline commands with `bin/bsat_debug`. Build `fd2e7a1` with
this milestone's same driver to reproduce the baseline collection measurement.
Paired solver commands and manifests are recorded in the raw benchmark files.
