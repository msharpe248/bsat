# Exact-deduplication prototype for BVE

## Candidate

The numeric-order BVE pass previously counted every non-tautological parent pair
against the clause-growth limit, then generated accepted pivots' resolvents in
a second traversal. Different parent pairs can produce the same signed-literal
set. Counting each copy can reject an elimination even when its distinct
resolvents fit the growth bound, and emitting each copy can add redundant work.

The candidate stages resolvents once, retains the first occurrence of each
signed-literal set, and counts distinct clauses for the growth check. A sum of
squared encoded literals filters comparisons by size and fingerprint; exact
signed membership decides equality. Collisions cannot authorize dropping a
clause. Literal order of the retained first occurrence is preserved.

Staging finishes before reconstruction records, proof emission, or parent
removal. Allocation failures, resource limits and comparison cleanup retain the
existing fail-closed behavior. A failed staging attempt frees its partial set and
leaves the formula unchanged. The public `elim_cost` stages and discards a set;
normal elimination stages only once and transfers the accepted clauses to the
existing commit path. Pure elimination keeps its two-word reconstruction cost.

The numeric schedule, occurrence cap, growth allowance, preprocessing budgets,
phase order and default-disabled `--elim` option are unchanged. Work accounting
changes: staging avoids the former second resolution traversal, while hashing,
equality comparisons and scratch cleanup add work. Thus budget-limited search
can change even where no duplicate is removed.

## Verification

The archived `test_unique_resolvents.c` uses an independent bit-set oracle on 2,048 signed
parent sets. It predicts distinct non-tautological resolvents and the growth
bound, compares projected truth tables and reconstructed original models, and
checks rejection atomicity. The sample has 1,919 accepted cases, 129 rejected
cases and 257 cases where distinct clauses fit but raw pair counting exceeds
the growth allowance. Parent permutations exercise order-independent equality.

An explicit collision uses `{2,9}` and `{6,7}`, whose positive encoded literals
both have squared sum 340. Both clauses must survive despite matching hashes.
All 82 work cutoffs around that fixture check marker cleanup, atomic staging,
and successful retry/model reconstruction after clearing the limit.

Both release and ASan/UBSan debug builds pass **43 C test executables** and
**3,139 independently checked solves**, 73 configurations and seed 20261207.
Both pass 42 short-deadline cases with maximum observed CPU overrun 0.000 seconds.
[Validation record](benchmark_results/unique-resolvents-validation-20260907.json).
Three deliberate mutations—trusting a colliding fingerprint, retaining duplicate
resolvents, and leaving scratch marks uncleared—are caught by the oracle.
[Mutation record](benchmark_results/unique-resolvents-mutations-20260907.json).

With BVE disabled, twelve text/binary proof comparisons on six reused competition
inputs preserve proof bytes and all 24 selected counters at 1,000 conflicts.
[Default trace guard](benchmark_results/unique-resolvents-default-traces-20260907.json).
Incomplete prefixes are not certificates for UNKNOWN answers. These finite checks
do not establish universal soundness.

## Performance and decision

The [seven-input comparison](benchmark_results/unique-resolvents-targeted-20260907.json)
uses `--elim --no-probing --preprocess-budget 100000000 --time 15` in both
versions, text proofs, a 20-second external wall limit and two repetitions per
input, seed 20261208. Both verify **6/14 runs** with zero errors, solving
battleship, belpyramid and hamiltonian twice. Both time out on hardware, ktf,
hamiltonian-cycle and multiplier-circuits.

Elimination counts change only on belpyramid, from 20,328 to 20,401 variables.
Its conflict count increases from 27,936 to 28,672. Median process CPU is 1.399
versus 1.367 seconds, but candidate samples range from 1.214 to 1.519 seconds,
so this does not establish a useful speed gain. Other families retain their
elimination counts. Mean wall PAR2 is 23.809 versus 23.641 seconds; maximum RSS
is 71,352,320 versus 70,303,744 bytes. Wall-clock variation is substantial.

The prototype is not retained on this evidence. It is preserved as
an [exact-only patch](benchmark_results/unique-resolvents-exact-only-20260907.patch)
and as an ablation for the stronger [staged-subsumption experiment](MINIMAL_RESOLVENTS.md).
The patch contains the exact-only implementation and its matching test, relative
to `2392333`; apply with `git apply --unidiff-zero`. The validation record pins
preserved exact-only source, test and binary paths. The current test may belong
to the stronger experiment; do not apply its hash to the exact-only record.

All performance jobs ran serially, separately from builds, validation and
sampling. CPU includes parsing and proof output; independent checking is outside
solver timing. UNKNOWN incurs twice the wall limit in PAR2. These are reused
development inputs on one macOS arm64 machine, not held-out competition results.
