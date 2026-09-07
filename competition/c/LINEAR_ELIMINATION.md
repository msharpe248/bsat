# Linear resolution pairs in bounded variable elimination

BVE previously used nested literal comparisons to test whether a resolvent was
tautological, repeated that test while staging resolvents, and used another
nested scan to remove duplicate literals. Long overlapping parents therefore
caused quadratic work in both costing and construction.

The new shared resolution-pair routine marks each non-pivot variable with its
literal's polarity. A matching mark identifies a duplicate; an opposite mark
identifies a tautology. It preserves the old first-parent/second-parent output
order for normalized input clauses, including clauses whose watch positions
have been reordered. Each pair takes linear time in the parent lengths, plus
linear cleanup of the unique marks. It borrows the idle minimizer scratch during
root preprocessing, adding no persistent array or per-pair mark allocation.

Pinned [Kissat resolution generation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/resolve.c)
also uses marks to detect repeated and complementary literals. BSAT's routine
uses its existing per-variable signed marks and shares the implementation
between its cost and staging passes; this is not a copy of Kissat's complete
elimination policy.

## Bounds and correctness

The old cost counter charged a parent-length product before each tautology test
and did not charge the staging comparisons. Both new passes charge literal visits
and mandatory mark cleanup. Work-limit and deadline checks can stop a pair;
all marks are cleared, staged memory is freed and no parent clause is deleted
before staging completes. Cleanup is mandatory and can exceed the work limit by
the number of outstanding marks. This is a bound on the resolution-pair work,
not a worst-case time guarantee for all preprocessing, allocation or copying.

The numerical preprocessing budget and growth settings are unchanged, but the
work accounting changes which candidates fit that budget. It does not uniformly
permit more elimination. Cost counting also avoids overflow in the growth test,
and reconstruction-record lengths are checked before narrowing or allocation.
Aborted staging returns failure; only a committed elimination reports success. Proof ordering and reverse model reconstruction remain
unchanged.

The new direct regression covers all 676 pairs of nonempty three-variable tails,
with watched-literal and tail permutations. Its 5,408 projected truth-table
checks compare the transformed formula with existential elimination of the
pivot; satisfying tail assignments are extended and checked against the original
input. A separate quadratic predicate checks tautology classification. Every
work cutoff through cost and staging checks cleanup and preservation of original
clauses on abort. Cancellation and a deadline reached inside a 5,000-literal
pair are also exercised.

Release and ASan/UBSan builds pass all 27 C executable suites. Both builds also
pass 2,709 independent truth-table, original-model, text/binary RUP and external
DRAT checks (seed 20261011; 30 random plus 13 fixed formulas under 63 profiles).
These are local checks, not a claim about remote CI.

## Isolated scaling measurement

The [generator](tests/generate_elim_resolution_corpus.py) makes two overlapping
clauses with opposite signs of pivot 1 and the same positive tail. The
[driver](tests/elim_driver.c) eliminates only that pivot ten times per process;
its internal CPU sum excludes parsing and occurrence construction. Each process
emits its final reconstructed model for independent checking. This isolates
resolution cost on deliberately long overlapping clauses, not typical SAT search.

[Raw kernel comparison](benchmark_results/linear-elim-kernel-20260907.json)
contains three shuffled repetitions per version/input. All nine runs per version
verify their final original-input models with no errors. Median CPU sums for ten
eliminations are:

| Variables | Old internal CPU | Linear internal CPU | Internal speedup | Old process CPU | Linear process CPU |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1,000 | 0.007829 s | 0.000141 s | 55.5x | 0.010911 s | 0.003208 s |
| 5,000 | 0.184941 s | 0.000654 s | 282.8x | 0.192059 s | 0.007525 s |
| 10,000 | 0.727030 s | 0.001311 s | 554.6x | 0.739565 s | 0.012790 s |

Process CPU includes parsing, repeated solver creation and final model output;
its speedups are smaller. Work-counter values are not directly comparable
because their accounting changed. The old driver is linked against `cc6b398`
core objects using the same driver source. The new driver links the tested core.

## Full solver measurements

Both full-solver profiles enable BVE and disable failed-literal probing to expose
elimination work (`--elim --no-probing`); other defaults are unchanged. Limits are
ten solving CPU seconds and fifteen wall seconds. Timing is serial and separate
from builds and validation. Original models and UNSAT proofs are independently
checked outside timing. Executable hashes in all four measurement records were
verified against the measured binaries after final checks.

The [four-target comparison](benchmark_results/linear-elim-targeted-20260907.json)
runs twice per profile. Both versions verify 6/8 runs, solving the same three
inputs and timing out on the multiplier, with no errors. Mean wall PAR-2 is
10.8333 seconds before and 10.7943 after; this small difference does not establish
a general speedup. Belpyramid's conflict count changes from 34,110 to 33,655;
Hamiltonian and battleship retain 13,351 and 250,822 respectively. Timings are mixed.

The [broader comparison](benchmark_results/linear-elim-broad-20260907.json) uses
the existing 16-input development corpus, one repetition per profile. It overlaps
the targeted sample on Hamiltonian and battleship and is not a held-out test.
Both profiles verify the same 3/16 inputs, with no gains, losses or errors. Mean
wall PAR-2 is 24.8812 seconds before and 24.8845 after, effectively flat.

An apparent Hamiltonian slowdown prompted a
[seven-repeat confirmation](benchmark_results/linear-elim-hamiltonian-confirm-20260907.json).
All fourteen models verify; both versions retain 13,351 conflicts. Median process
CPU is 0.242448 seconds before and 0.235852 after, so the initial slowdown does
not reproduce. This does not establish a broad speed improvement either.

## Retention and reproduction

**Retain the linear resolution routine.** The isolated long-clause scaling gain
is substantial, projection and reconstruction checks pass, and the measured
competition solved sets are unchanged. The result improves an elimination
kernel; it neither adds a competition solve in these samples nor proves parity
with a leading solver. Keep BVE off by default and available through `--elim`.
Further elimination-order and preprocessing-policy work needs separate evidence.

Final release and ASan/UBSan C suites include the internal-deadline regression.
Both builds pass 42 short-deadline cases, with maximum observed CPU overrun
rounding to 0.000 seconds in each; this is observed coverage, not a universal
bound. The sanitizer build of the isolated driver also produces three models
independently checked against the generated original inputs.

Recreate the scaling inputs and build the driver:

```sh
python3 competition/c/tests/generate_elim_resolution_corpus.py /tmp/bsat-linear-elim-corpus
make -C competition/c all test elim-driver
make -C competition/c all test elim-driver MODE=debug
competition/c/bin/elim_driver /tmp/bsat-linear-elim-corpus/overlap-10000.cnf 10
```

For an old-core comparison, use `cc6b398` in a separate checkout and copy this
milestone's driver source and Makefile driver target into that checkout. Build
with the same compiler flags, then use the exact commands recorded in the raw
benchmark files. All inputs are hash-identified; the generated corpus does not
require a persistent index or a downloaded dataset.
