# Removing redundant staged resolvents

The replacement is **rejected**. It improves one large-budget target but
regresses with the ordinary preprocessing allowance and adds no verified solves.
The original elimination implementation is restored. The candidate and its
policy-specific oracle are preserved in an
[experimental patch](benchmark_results/minimal-resolvents-rejected-20260907.patch).

## Extension of exact deduplication

The [exact-only prototype](UNIQUE_RESOLVENTS.md) added no solves on the seven-input
screen. The stronger candidate also discards a resolvent when another staged
clause is a subset of it. A newly generated smaller clause removes any larger
staged clauses it subsumes. Each retained clause is still a direct resolvent of
the original parents. The resulting conjunction is equivalent to retaining all
resolvents, because each discarded clause is implied by one that remains.
This produces a set with no clause containing another; it is not a globally
minimum-size equivalent CNF.

Equal-size fingerprints still only filter exact comparisons. Different sizes
use exact signed membership; no hash match can establish subsumption. Removing
a staged clause swaps the last entry into its slot, so addition order can change.
Staging completes before any proof emission or parent deletion, and its entire
partial result can be discarded safely on interruption or allocation failure.
The existing reconstruction and proof commit path remains in use.

The number of staged clauses can shrink. Therefore the growth check runs after
all parent pairs, rather than rejecting the first oversized intermediate set.
Temporary capacity is bounded by the parent-pair count (and the existing integer
and allocation limits), which can exceed the final growth allowance. With the
default occurrence cap, at most 100 parent pairs are considered per pivot.
Literal comparisons, fingerprints and cleanup charge work; allocated temporary
clauses are freed on all exits. Both temporary memory use and work-limited
elimination can differ from the original pass.

## Correctness checks

The archived `test_unique_resolvents.c` computes all signed-bit-set resolvents
independently, removes supersets in the reference, predicts the growth decision,
and checks projected truth tables and reconstructed original models. It covers
2,048 cases: 1,920 accepted, 128 rejected, and 386 where raw pair counting exceeds
the growth allowance but the final reduced set fits. These totals describe this
sample, not a general success rate.

Repeated late-unit fixtures first grow past eight staged clauses and then shrink
to six, catching an incorrect early growth check. The deliberate equal-size hash
collision remains covered. Eighty cutoffs around the collision fixture check
atomic failure, scratch cleanup, and successful retry after clearing the limit,
including interruption after staged clauses have been removed.

Both release and ASan/UBSan debug builds pass **43 C test executables** and
**3,139 independent formula/model/proof checks**, 73 configurations and seed
20261209. Both pass 42 short-deadline cases; maximum CPU overrun is 0.000 seconds
release and 0.001 seconds debug.
[Validation record](benchmark_results/minimal-resolvents-validation-20260907.json).
Three mutations—trusting equal-size hashes, rejecting an oversized intermediate
set early, and leaving marks uncleared—are caught by the oracle.
[Mutation record](benchmark_results/minimal-resolvents-mutations-20260907.json).

With BVE disabled, twelve text/binary proof comparisons on six reused competition
inputs preserve proof bytes and all 24 selected counters at 1,000 conflicts.
[Default trace guard](benchmark_results/minimal-resolvents-default-traces-20260907.json).
Incomplete prefixes are not certificates for UNKNOWN answers, and these finite
checks do not establish universal soundness.

## Performance and decision

The [three-way screen](benchmark_results/minimal-resolvents-targeted-20260907.json)
compares original numeric BVE, exact deduplication and subset-minimal staging.
All use `--elim --no-probing --preprocess-budget 100000000 --time 15`, text proofs,
a 20-second external wall limit and two repetitions on seven reused development
inputs, seed 20261210. Each verifies **6/14 runs**, with zero errors. All solve
battleship, belpyramid and hamiltonian twice; all time out on the other four.

| Family | Original variables eliminated | Exact-only | Subset-minimal |
| --- | ---: | ---: | ---: |
| belpyramid | 20,328 | 20,401 | 21,302 |
| hardware-verification | 5,422 | 5,422 | 5,540 |
| ktf | 14,622 | 14,622 | 14,622 |
| multiplier-circuits | 2,448 | 2,448 | 2,448 |
| battleship / hamiltonian / hamiltonian-cycle | 0 | 0 | 0 |

Mean wall PAR2 is 23.671, 23.640 and 23.560 seconds respectively. Peak RSS is
119,209,984, 70,352,896 and 70,500,352 bytes. The original peak is on the
hamiltonian-cycle timeout, where no BVE occurs and different runs reach different
search states; the other peaks are on ktf. This does not establish a memory gain
from preprocessing. CPU and wall timings varied substantially in this screen.

## Repeated confirmation and ordinary-budget guard

The [five-repeat confirmation](benchmark_results/minimal-resolvents-confirmation-20260907.json)
compares original and subset-minimal staging on the three completed workloads
using the same 100-million allowance and time limits, seed 20261211. Both verify
15/15 runs with zero errors. Median process CPU:

| Family | Original CPU (s) | Subset-minimal CPU (s) |
| --- | ---: | ---: |
| battleship | 2.357003 | 2.408982 |
| belpyramid | 1.375290 | 1.264247 |
| hamiltonian | 0.204882 | 0.200055 |

Belpyramid improves about **8.1% CPU**, with conflicts falling from 27,936 to
25,581 and propagations from 14,491,379 to 11,679,766. The other two inputs have
unchanged search counters and no eliminated variables. The geometric mean of
the three median CPU ratios is 0.97167, about 2.8% less CPU in this small guard.
Peak RSS rises from 35,323,904 to 37,011,456 bytes.

The [ordinary-budget guard](benchmark_results/minimal-resolvents-default-budget-20260907.json)
uses `--preprocess-budget 1000000`, still with BVE enabled and probing disabled;
this is **not the solver's default configuration**. It repeats each workload
twice, seed 20261212. Both verify 6/6 runs, with zero errors. Belpyramid now
regresses from **5.4310 to 6.3371 seconds CPU**, about **16.7% slower**. Its BVE
progress falls from 2,815 eliminated variables to 82, and removed clauses from
18,332 to 387. Conflicts rise from 31,767 to 33,917; propagations rise from
81,940,776 to 93,100,888. More expensive staging spends the available allowance
before achieving the former simplification progress.

Across those three workloads the geometric mean median CPU ratio is 1.04679,
about 4.7% worse. Mean wall PAR2 worsens from 3.194 to 3.723 seconds, while peak
RSS falls from 34,488,320 to 32,473,088 bytes. The larger allowance's modest gain
and the absence of new solves do not justify this ordinary-budget regression.
Both staged-resolvent prototypes are therefore rejected as replacements.

Performance jobs ran serially without concurrent builds, validation or sampling.
CPU includes parsing and proof output; independent checking runs outside solver
timing. UNKNOWN incurs twice the wall limit in PAR2. These reused SAT Competition
2025 inputs are development cases on one macOS arm64 machine, not held-out
competition evidence. The broader competition-performance goal remains open.

## Retained coverage and reproduction

`elim.c` is restored exactly to `2392333`. The retained
`tests/test_resolvent_projection.c` accepts conservative bounded cost estimates
and checks the existential projection and original-model reconstruction whenever
elimination succeeds. It keeps 2,048 signed-parent cases (1,534 accepted and 514
rejected under the restored implementation), distinct-set collision witnesses,
and all 84 staging cutoffs with atomic failure and retry checks. It does not
require either rejected optimization. Restored release and ASan/UBSan suites are
recorded separately in the validation JSON; each passes 43 C test executables.

The 3,139-per-build validations above apply to their respective prototypes. The
prior milestone's validation applies to the unchanged restored algorithm; those
are not new validation runs from this experiment.

Apply either prototype patch independently with `git apply --unidiff-zero` to
reproduce it and its matching `test_unique_resolvents.c`. Build/test using
`make -C competition/c all test`, repeating with `MODE=debug`. Validation records
pin source, test and executable hashes and commands. Benchmark JSON records
exact profiles, input/checker hashes, seeds and limits. The preserved binaries
are `/tmp/bsat-unique-resolvents-baseline`, `/tmp/bsat-unique-resolvents-exact`, and
`/tmp/bsat-minimal-resolvents-candidate`; recorded commands show their actual
paths during measurement, before restoring the original implementation.
