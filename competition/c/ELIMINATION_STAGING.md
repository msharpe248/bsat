# Bounded reconstruction staging and ownership transfer

BVE stages zero-delimited copies of removed clauses for reverse model
reconstruction. Previously, staging used an uncharged whole-clause `memcpy`, then
`elim_save` allocated another equally sized buffer and copied the record again.
Pure-literal elimination has no resolution pairs, so it could bypass the new
resolution-loop work/deadline checks entirely.

## Reproduced gap and change

The regression builds one positive 5,000-literal clause, warms the cached deadline
state, then presents an expired CPU deadline. Linked against the `5ebdf18` core,
it fails the assertion that elimination must stop: the old routine reports a
successful elimination. The final routine stops after at most 1,024 copied words,
leaving the original clause, values and reconstruction stack unchanged.

Staging now copies in chunks ending at global 1,024-work boundaries. Each copied
word and clause delimiter consumes work. A chunk is capped by remaining work,
and deadline checks at boundaries read the clock without another cached throttle.
Cancellation, a work cutoff or an expired deadline frees the temporary record
before any reconstruction entry or parent deletion is committed. This bounds
staging-copy work; it is not a bound on every allocation, proof write or deletion.

A private ownership-taking helper moves the completed staging allocation into
the reconstruction stack. It removes one record-sized allocation and copy, and
the caller clears its pointer so cleanup cannot free stack-owned memory. The
public `elim_save` API retains its copy contract for BCE and equivalence callers.
Failure leaves ownership with the caller, and the normal solver destructor frees
stored records. Stack-capacity growth is checked before doubling.

## Validation

The new test covers the reproduced pure-clause deadline gap, all 68 work-budget
boundaries around a 65-literal parent, cancellation, the public copy contract,
empty records, and growth through 48 owned reconstruction records. It validates
original-input models after reverse reconstruction and checks that aborts leave
the formula untouched. Existing projected truth-table tests cover mixed-polarity
resolution and all cost/staging cutoffs.

Both release and ASan/UBSan builds pass all 28 C executable suites and 2,709
independent truth-table, original-model, text/binary RUP and external DRAT checks
(seed 20261012; 30 random plus 13 fixed formulas under 63 configurations).
Both also pass 42 short-deadline cases. Maximum observed CPU overrun rounds to
0.000 seconds in release and 0.001 seconds in debug; these are observed checks,
not universal time bounds. Six sanitizer-driver outputs from the pure and
overlapping-clause corpora independently verify against their original inputs.
These are local checks, not evidence of remote CI.

## Measurements

The [pure-clause generator](tests/generate_elim_staging_corpus.py) creates one
positive clause of each listed length. The existing `elim_driver` eliminates
pivot 1 ten times per process, with its internal CPU sum excluding parsing and
occurrence construction. Three shuffled repetitions per profile/input use
baseline `5ebdf18` and the final implementation. All nine final models per
profile independently verify. [Raw pure-clause results](benchmark_results/elim-staging-kernel-20260907.json).

| Variables | Old internal CPU sum | Owned-buffer CPU sum | Old process CPU | Owned-buffer process CPU |
| --- | ---: | ---: | ---: | ---: |
| 10,000 | 0.000032 s | 0.000021 s | 0.008778 s | 0.009044 s |
| 50,000 | 0.000091 s | 0.000058 s | 0.035241 s | 0.035698 s |
| 100,000 | 0.000169 s | 0.000099 s | 0.076583 s | 0.077730 s |

The observed internal sums are about 1.5–1.7x lower, but very small. Parsing,
solver setup and output dominate process CPU, which shows no improvement.
Mean wall PAR-2 for these generated driver runs worsens from 0.0730 to 0.0881
seconds. This experiment does not establish an end-to-end speedup. Peak observed
RSS is effectively unchanged; the guaranteed storage saving is one temporary
record-sized buffer during each elimination, not a universal process-RSS drop.

A [mixed-polarity overlap check](benchmark_results/elim-staging-overlap-20260907.json)
reuses the previous 1,000/5,000/10,000-variable corpus with three repetitions per
profile. All nine models per profile verify. Median internal CPU sums are
0.000143/0.000654/0.001283 seconds before and 0.000140/0.000657/0.001277 after.
Process CPU and wall timing are also essentially flat on this sample.

The [full solver comparison](benchmark_results/elim-staging-targeted-20260907.json)
uses the same four competition development targets twice per profile, with
`--elim --no-probing`, ten solving CPU seconds and fifteen wall seconds. Both
profiles verify the same 6/8 runs with no errors: battleship and Hamiltonian SAT,
belpyramid UNSAT, and multiplier UNKNOWN in both repetitions.

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 6/8 | 11.0088 s | 47,710,208 bytes | 0 |
| Bounded owned staging | 6/8 | 10.9555 s | 40,976,384 bytes | 0 |

Battleship and Hamiltonian keep 250,822 and 13,351 conflicts. Belpyramid changes
from 33,655 to 34,293 conflicts while median CPU falls from 5.754316 to 5.300809
seconds. Charging staging words changes which eliminations fit the existing
budget, so search can change despite unchanged resolution logic. These short,
reused development inputs do not establish competition-level performance or a
general solved-count gain.

All performance benchmark runs are serial, separate from builds and validation. Original models
and UNSAT proofs are checked outside solver timing. Raw records retain commands,
input and executable hashes; all measured executable hashes were verified after
final checks. Process CPU includes startup, parsing and proof output; the internal
solving limit excludes parsing. An unsolved run receives twice the wall limit in
PAR-2. The generated driver has only an external wall limit.

## Retention and reproduction

**Retain the change** for the reproduced staging deadline/work-limit fix and the
removal of a redundant allocation/copy. No new option is introduced; BVE remains
off by default and available through `--elim`. Competition solved counts in this
screen are unchanged, so the wider competition-performance goal remains open.

```sh
python3 competition/c/tests/generate_elim_staging_corpus.py /tmp/bsat-elim-staging-corpus
make -C competition/c all test elim-driver
make -C competition/c all test elim-driver MODE=debug
competition/c/bin/elim_driver /tmp/bsat-elim-staging-corpus/pure-100000.cnf 10
```

The baseline already contains the same driver. For the old deadline failure,
compile this milestone's `test_elim_staging.c` against the `5ebdf18` release core
objects, excluding `main.o`; it aborts at the first `pure_deadline` assertion.
The final test passes. The raw benchmark records provide the exact paired
commands and input hashes.
