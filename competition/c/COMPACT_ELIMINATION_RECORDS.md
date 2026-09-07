# Compact BVE reconstruction records

BVE previously retained every removed parent clause for model reconstruction.
It now stores a default pivot value followed by only one polarity's parents,
choosing the polarity with the smaller total serialized word count. The record
uses the existing zero-delimited representation and ownership transfer.

[MiniSat's elimination records](https://github.com/niklasso/minisat/blob/master/minisat/simp/SimpSolver.cc#L474)
use one polarity plus a default literal. BSAT chooses by stored words, including
clause delimiters, rather than by parent count. BSAT scans each record forward,
so its default unit comes first; MiniSat's reversed flat stack places it last.
The public copying API for BCE and equivalence witnesses is unchanged.

## Why one polarity is sufficient

Suppose positive parents are `(x | A_i)` and negative parents are `(~x | B_j)`,
and the positive parents are saved. The default `~x` satisfies every omitted
negative parent. If all `A_i` are true, that assignment also satisfies the saved
parents. If some `A_i` is false, reconstruction sets `x` true. Every resolvent
`A_i | B_j` is satisfied by the remaining model, so every `B_j` must then be true:
the omitted parents remain satisfied after the flip. Discarded tautological
resolvents are true for every assignment and satisfy the same argument. The
negative-side case is symmetric. Reverse elimination order supplies the required
values for other eliminated variables before each record is interpreted.

The record occupies `2 + min(positive_words, negative_words)` literal words,
including a default literal and delimiter. Pure variables need only two words.
A valid nonempty parent group needs at least two words, so this representation
never exceeds the previous full-parent record. Equal costs choose the positive
parents deterministically. Resolvent generation, clause growth policy, proof
ordering and deletion of all parents are unchanged.

The existing bounded staging loop copies only the selected parents and charges
the default's two words. A fresh deadline check before commit preserves the pure
staging regression even when copying shrinks below a polling interval. Less
staging work can let more variables fit the same preprocessing budget; therefore
budget-limited search can change. Final SAT models can also differ while still
satisfying the original formula.

## Validation

The new regression checks 4,096 four-parent formulas under all four tail
assignments, for 16,384 independent existential-projection comparisons. Every
satisfying transformed assignment is reconstructed and checked against the
original input. Tests also check exact record sizes/default polarity, reordered
watched literals, unit/conflicting resolvents, choosing two short parents over
one longer parent, and a negative pure-variable default. Existing tests retain
5,408 two-parent projection checks and exercise record ownership, stack growth,
cancellation and work cutoffs. Pure records now charge two words, so the existing
remaining-budget cases use that actual storage cost; the expired-deadline
assertion is preserved.

Release and ASan/UBSan debug builds each pass all 29 C test executables.
Each also passes 3,969 independent solves across 63 configurations (50 random
and 13 fixed inputs, seed 20261013), checking truth-table answers, original SAT
models and text/binary RUP proofs, including external drat-trim checks. Each
build passes 42 deadline cases; maximum observed CPU overrun is 0.003 seconds
in release and 0.000 in debug. Six generated driver inputs also pass sanitizer
execution and independent original-model checks.

## Measurements

Baseline is `5738afc`. Both measured drivers include the reconstruction-word
counter outside the internal timer. Each generated input runs three times per
profile, with ten eliminations per run. All nine models per profile verify in
each generated corpus.

[Pure-clause results](benchmark_results/compact-record-pure-20260907.json):

| Variables | Old record words | Compact words | Old internal CPU sum | Compact internal CPU sum |
| --- | ---: | ---: | ---: | ---: |
| 10,000 | 10,001 | 2 | 0.000020 s | 0.000010 s |
| 50,000 | 50,001 | 2 | 0.000062 s | 0.000021 s |
| 100,000 | 100,001 | 2 | 0.000099 s | 0.000028 s |

These are medians. The largest record payload falls from 400,004 to 8 bytes.
Internal timings are tiny; whole-process CPU falls only modestly (0.075169 to
0.073268 seconds at 100,000 variables). Mean wall PAR-2 worsens from 0.06974 to
0.07477 seconds, with essentially unchanged peak RSS. This does not establish
a general end-to-end speedup.

[Mixed-polarity overlap results](benchmark_results/compact-record-overlap-20260907.json)
reduce records from 2,002/10,002/20,002 words to 1,003/5,003/10,003 words.
Median internal CPU sums change from 0.000130/0.000643/0.001270 seconds to
0.000141/0.000644/0.001246 seconds. Timing is mixed and effectively flat;
mean wall PAR-2 is 0.01177 versus 0.01173 seconds.

The [targeted comparison](benchmark_results/compact-record-targeted-20260907.json)
runs four competition development inputs twice per profile. BSAT uses
`--elim --no-probing --time 10`; the external wall limit is 15 seconds.
Kissat is pinned at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 6/8 | 10.9762 s | 40,976,384 bytes | 0 |
| Compact | 6/8 | 10.6178 s | 45,727,744 bytes | 0 |
| Kissat | 8/8 | 0.6179 s | 26,591,232 bytes | 0 |

Both BSAT profiles solve battleship, belpyramid and Hamiltonian in both repeats;
multiplier remains UNKNOWN. Belpyramid conflicts fall from 34,293 to 31,767,
and median process CPU from 5.305010 to 4.506862 seconds. Battleship and
Hamiltonian conflict counts are unchanged; their timing differences do not
establish a search improvement. Targeted peak RSS increases despite the smaller
record payload. Kissat still wins clearly on this selected sample.

The [broader 16-input development screen](benchmark_results/compact-record-broad-20260907.json)
uses the same BSAT limits, one run per profile and input. Both verify the same
3/16 inputs, with zero errors and no solved gains or losses. Mean wall PAR-2 is
24.8814 versus 24.8829 seconds. This corpus overlaps the targeted screen and
is not held out; it does not establish competition-level performance.

All performance benchmark runs are serial and separate from builds and
validation. Original models and UNSAT proofs are checked outside solver timing.
Raw records preserve exact commands and input/executable hashes; every measured
executable hash was verified after final checks. Process CPU includes parsing,
startup and proof output; the internal solving limit excludes parsing. Unsolved
PAR-2 penalties are twice the external wall limit. Generated drivers use only
an external 20-second wall limit.

## Retention and reproduction

Retain the standard one-polarity reconstruction strategy for its guaranteed
record-payload reduction and reduced staging work. BVE remains opt-in through
`--elim`; measured competition solved counts are unchanged. The broader
competition-performance goal remains open.

```sh
make -C competition/c all test elim-driver
make -C competition/c all test elim-driver MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 50 --seed 20261013
python3 competition/c/tests/check_deadlines.py --solver competition/c/bin/bsat
python3 competition/c/tests/generate_elim_staging_corpus.py /tmp/bsat-elim-staging-corpus
python3 competition/c/tests/generate_elim_resolution_corpus.py /tmp/bsat-linear-elim-corpus
competition/c/bin/elim_driver /tmp/bsat-elim-staging-corpus/pure-100000.cnf 10
```

Repeat validation/deadline commands with `bin/bsat_debug` for the sanitizer
build. To reproduce baseline storage measurements, build `5738afc` with the
same driver counter added in this milestone; the counter does not alter the
solver or timed elimination loop. Raw benchmark records supply paired commands,
input manifests and hashes.
