# Compact variable metadata

The default solver previously stored a 64-bit LRB participation timestamp in
every variable record, although VSIDS never reads or updates it. An additional
32-bit `last_polarity` field had no readers or writers beyond zero-initialization.
Removing that unused field and moving LRB timestamps into an optional dense
array reduces `VarInfo` from 40 to 32 bytes on the measured arm64 ABI.

The default VSIDS and ordinary VMTF configurations save eight bytes per allocated
variable slot. LRB allocates eight timestamp bytes per slot, leaving its total
payload unchanged while making the main record smaller. Scores remain in
`VarInfo.activity`; this differs from the earlier rejected
[dense activity layout](DENSE_ACTIVITY.md), which did not reduce default storage.
Variable capacity growth, timestamp initialization, destruction and API rebuilds
handle the optional array. Scoring arithmetic, 64-bit recency, heap comparisons,
rescaling, learned clauses and proof order are unchanged.

This changes the internal C structure layout. BSAT does not promise a stable
binary ABI for these structures. The C API's options are copied at construction;
use `solver_new_with_opts` to select LRB so its storage is allocated with the
other variable arrays.

## Verification and measurements

The new regression grows both default and LRB solvers to 20,000 variables,
checks preservation of timestamps beyond 32 bits, solves, and adds another
variable to exercise rebuild ownership and initialization. Eight conflict-graph
cases compare independently calculated VSIDS/LRB scores and learned clauses,
including floating-point rescaling and conflict counts beyond 32 bits. The
validation matrix now explicitly includes six LRB combinations.

Both release and ASan/UBSan debug builds pass all 31 C test executables. Each
also passes 2,967 independent solves across 69 configurations (30 random and
13 fixed formulas, seed 20261016), checking truth-table answers, original SAT
models and text/binary RUP proofs with external drat-trim. Both pass all 42
deadline cases with maximum observed CPU overrun 0.000 seconds.

The baseline is `99869b8`. The [fixed-work screen](benchmark_results/compact-vars-work-20260907.json)
uses the existing 28-input development manifest, three repetitions per profile,
10,000 conflicts, three solving CPU seconds and a five-second external wall
limit. All eleven selected search counters match across all six runs for every
input. Both versions verify the same 9/84 runs, with zero errors.

On the 26 inputs whose baseline median process CPU exceeds 0.05 seconds, the
geometric mean compact/baseline CPU ratio is **0.997746** (0.23% lower). Individual
results go both ways; this is effectively flat and does not establish a general
speedup. Mean wall PAR-2 is 8.93548 versus 8.93350 seconds. Maximum observed RSS
is 44,253,184 versus 44,351,488 bytes, so this search sample does not show a
whole-process peak reduction. [Per-input medians and counters](benchmark_results/compact-vars-summary-20260907.json).

The [LRB check](benchmark_results/compact-vars-lrb-20260907.json) uses four targets,
two repetitions and the same limits. Every profile reaches the 10,000-conflict
cap (0/8 certified answers each), and all eleven selected search counters match
across all four runs for each input. This is an LRB execution-equivalence screen,
not certificate evidence for those UNKNOWN results. Median process CPU changes
from 0.239812/2.652167/0.186514/2.092937 seconds to
0.223246/2.517972/0.186901/1.972221 seconds for battleship, belpyramid, Hamiltonian
and multiplier respectively. Two repetitions on reused targets do not prove a
general LRB speedup. LRB payload is unchanged and observed peak RSS increases
slightly (22,249,472 to 22,364,160 bytes).

## Storage measurement

The [generated storage comparison](benchmark_results/compact-vars-storage-20260907.json)
uses declared variables and one empty clause, producing immediate UNSAT after
parsing/allocation. Three repetitions per size/profile have an external 20-second
wall limit. All nine proofs per profile verify independently. This isolates
large variable storage from difficult search; it is not a solver-performance
challenge. Median process measurements:

| Variables | Baseline peak RSS | Compact peak RSS | Baseline CPU | Compact CPU |
| --- | ---: | ---: | ---: | ---: |
| 100,000 | 18,415,616 bytes | 17,055,744 bytes | 0.004009 s | 0.003951 s |
| 500,000 | 74,907,648 bytes | 69,795,840 bytes | 0.011484 s | 0.011258 s |
| 1,000,000 | 133,595,136 bytes | 123,240,448 bytes | 0.020361 s | 0.018896 s |

At one million declared variables, observed median peak RSS falls by 10,354,688
bytes (7.75%). The guaranteed payload saving is eight bytes per allocated
variable slot; capacity growth, allocator behavior and other solver structures
make this distinct from bytes per declared variable or a universal RSS saving.

## Longer competition screen

The [ten-second comparison](benchmark_results/compact-vars-targeted-20260907.json)
runs four development targets twice per profile, with a fifteen-second external
wall limit. Kissat is pinned at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 6/8 | 9.6993 s | 32,800,768 bytes | 0 |
| Compact | 6/8 | 9.8001 s | 35,209,216 bytes | 0 |
| Kissat | 8/8 | 0.6191 s | 33,308,672 bytes | 0 |

Both BSAT versions solve battleship, belpyramid and Hamiltonian in both repeats;
multiplier remains UNKNOWN. The completed inputs retain exactly 107,461, 30,460
and 9,247 conflicts respectively. Median process CPU changes from 2.366878 to
2.178738 seconds on battleship, 4.631945 to 5.409819 on belpyramid, and 0.140365
to 0.158070 on Hamiltonian. Multiplier reaches more conflicts within its deadline
with compact records, but adds no solve. Peak observed RSS is higher in this
search sample despite the lower variable-record payload. Kissat remains clearly
ahead overall on these selected inputs.

The [seven-repeat confirmation](benchmark_results/compact-vars-confirm-20260907.json)
rechecks belpyramid and Hamiltonian after their apparent slowdown. Both profiles
verify all 14 runs with zero errors, and all eleven selected search counters
match across all fourteen runs per input. Belpyramid's median CPU is 5.723741
seconds before versus 5.424912 after (5.22% lower); ranges are 5.646217–5.772222
and 5.199906–5.513738 seconds. Hamiltonian medians are 0.156419 versus 0.156204
seconds, effectively unchanged. Mean wall PAR-2 across these solved runs is
3.05830 versus 2.94257 seconds.

The initial slowdown is therefore not reproduced in this confirmation. Both
observations are retained; machine/time variation and the small reused sample
prevent a universal speed claim. The broad fixed-work comparison remains the
more conservative aggregate result: essentially flat CPU.

## Retention and reproduction

Retain the layout for its measured default-memory reduction, unchanged checked
search behavior and lack of a reproduced slowdown in the follow-up. This does
not establish a competition solved-count gain or parity with Kissat. LRB's
timestamp arithmetic remains 64-bit; only its storage location changes.

All performance benchmark jobs are serial and separate from builds/validation.
Certificate/model checking is outside solver timing. Raw files record commands,
input hashes and executable hashes; every measured executable hash was verified
after the final comparison. Process CPU includes startup, parsing and proof
output; the internal solver limit excludes parsing. Unsolved PAR-2 penalties are
twice the external wall limit. These are development corpora, not held-out
competition families. Deadline checks ran alongside independent validation,
separately from performance benchmarks.

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 30 --seed 20261016
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat
python3 competition/c/tests/generate_var_storage_corpus.py /tmp/bsat-var-storage-corpus
```

Repeat validation/deadline commands with `bin/bsat_debug` for sanitizer coverage.
Build `99869b8` separately for the baseline; raw benchmark records supply the
paired commands, limits, repetitions and input manifests.
