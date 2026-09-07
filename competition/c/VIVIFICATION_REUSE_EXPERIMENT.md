# Reusing assumptions during full deletion vivification

**Rejected.** The candidate improves the generated kernel but loses both
battleship solves in the repeated competition screen. Production sources are
restored to `cc77803`; the stronger oracles and rollback tests are retained.

The prefix-only experiment reduced kernel cost but produced weaker clauses and
regressed battleship. This follow-up keeps the existing single-literal deletion
checks and reuses the propagated assumptions that consecutive checks share.
The baseline is `cc77803`; inprocessing remains opt-in.

## Algorithm and existing work

[Kissat's pinned vivifier](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/vivify.c#L1029-L1096)
reuses compatible assumptions. BSAT's candidate uses a simpler two-level scheme
within one clause: the negated retained prefix and its closure stay at level one;
a deletion trial negates the suffix at level two. Backtracking to level one
removes only that trial. A failed deletion extends the retained prefix, and a
successful deletion emits the same kind of RUP addition as before. If the prefix
itself becomes contradictory, all later trials already have a RUP conflict.
The entire temporary trail is removed before the replacement clause is installed.

Every trial still omits exactly the literal under test. With complete propagation,
its assumption set is the same as the old fresh trial, so the candidate retains
full deletion testing rather than accepting a possibly unnecessary prefix.
Budget exhaustion can change which trials finish. Propagation order, watches,
phases and later clause order can also change, so this is not a claim that the
whole search is preserved. The probing RUP helper, replacement/proof ordering,
root replay and inprocessing scheduling are unchanged.

## Correctness and resource checks

The existing vivification suite is extended to **4,704 small cases** and **eight
long-scan cutoff/retry cases**. The small cases retain independent truth-table
and RUP checks, and 512 now compare the final literal set against an independent
full deletion oracle with a generous budget. Sixteen added signed cases require
removing an unnecessary first literal—the weakness of prefix-only reasoning.
The long-scan cases interrupt propagation in a 4,098-literal clause, require
that only root assignments survive, retry with a larger budget and verify a
subsequent SAT model against the original formula. Both the baseline and the
candidate pass the expanded suite.

Two mutation builds, including the omitted literal among assumptions and leaking
a trial's suffix into the next trial, both fail the independent RUP assertion.
The mutation record pins the exact test and candidate source hashes.

Both builds pass all **39 C test executables**. Each passes **3,139 certificate-
checked solves**, 73 configurations, seed 20261117. Both builds pass 42 deadline cases with maximum observed
CPU overrun 0.000 seconds. Finite testing is regression evidence, not a universal
soundness proof.

## Measurement plan and decision

The [serial kernel comparison](benchmark_results/reuse-vivify-kernel-20260907.json)
uses the existing three generated inputs, 100 freshly initialized databases per
process, three repetitions, a thirty-second external wall limit and seed
20261118. Both verify **9/9 original models**, with zero errors. Both leave
20,000 literals in total, removing 60,000 / 300,000 / 620,000 for lengths
8 / 32 / 64 respectively. Median accumulated simplification CPU seconds:

| Clause length | Fresh trials | Reused prefix | Old / reused propagation work |
| --- | ---: | ---: | ---: |
| 8 | 0.006667 | 0.004995 | 1,353,000 / 1,290,800 |
| 32 | 0.028790 | 0.010289 | 1,845,000 / 1,533,200 |
| 64 | 0.086290 | 0.018130 | 2,501,000 / 1,856,400 |

The largest kernel is about **4.76 times faster**, with the same final clause
sizes. This deliberately favorable fixture establishes a local cost improvement,
not a competition speedup or a general guarantee of identical clauses under
resource limits. The cost counter excludes some assumption setup and candidate
array operations, so it is not a complete CPU proxy.

The [five-input competition comparison](benchmark_results/reuse-vivify-targeted-20260907.json)
uses `--inprocess`, two repetitions, fifteen solving CPU seconds, twenty external
wall seconds and seed 20261119. Builds and validation finish before either serial
comparison starts. The existing implementation verifies **6/10 runs**, while
reuse verifies **4/10**, with zero errors in both profiles.

| Input | Existing median process CPU | Reuse median process CPU |
| --- | ---: | ---: |
| battleship | 0.364524 (SAT) | 15.002839 (UNKNOWN) |
| belpyramid | 5.616605 | 5.156959 |
| Hamiltonian | 0.155195 | 0.140250 |
| hardware verification (UNKNOWN) | 15.042787 | 15.045052 |
| ktf (UNKNOWN) | 15.034801 | 15.038861 |

Battleship solves after 17,491 conflicts in both baseline repetitions, whereas
reuse reaches 667,030 and 657,810 conflicts without an answer. Belpyramid's median
CPU improves by about 8.2%, with conflicts changing from 34,493 to 34,261. This
modest gain does not offset two lost verified solves. Hamiltonian has identical
search counters and finishes before the inprocessing interval, so its small
timing difference cannot establish a vivification-policy benefit. Both larger
inputs remain UNKNOWN in all runs, with final counters present.

Mean wall PAR-2 worsens from 17.3955 to 25.2407 seconds. Maximum observed process
RSS is 60,604,416 versus 60,407,808 bytes. Reject the runtime change. Exact local
deletion results without tight budgets do not guarantee preserved global search:
propagation order, phase effects, watches and budget boundaries still differ.
Future optimization should first preserve the existing propagation sequence;
this experiment does not identify which changed side effect caused the loss.
These reused development inputs do not establish held-out competition parity.

## Retained state and reproduction

All 39 C test executables pass in release and ASan/UBSan debug builds after
restoration, including 4,704 small cases and eight long-scan cases. The restored
release executable is byte-identical to the measured baseline. The retained
changes add sixteen signed first-literal-removal cases, 512 exact deletion-oracle
comparisons and eight long-scan rollback/retry cases to the previous suite.

The [archived runtime patch](benchmark_results/reuse-vivify-rejected-20260907.patch)
applies to the `cc77803` solver source with `git apply --unidiff-zero`. The expanded
test suite passes with either implementation. The
[mutation record](benchmark_results/reuse-vivify-mutations-20260907.json) pins the
test and candidate-source hashes and records both independent RUP failures.
Patch applicability and all recorded executable, input and checker hashes were
verified against the preserved artifacts.

Reuse `tests/generate_vivify_corpus.py` and `tests/vivify_driver.c` from the prior
milestone. Compile the driver with release objects excluding `main.o`; use the
baseline or patched `solver.c` object as appropriate. Compiler options are
`-std=c11 -D_POSIX_C_SOURCE=200809L -O3 -march=native -flto`, the C include
directory, and `-lm`. The kernel driver command is `{input} 100`; raw JSON records
the executable hashes, exact inputs, limits and seeds. Reuse the targeted record's
five competition inputs and commands for that comparison. Full validation uses
`tests/validate.py --cases 30 --seed 20261117` with each build and external
drat-trim; deadline checks use `tests/check_deadlines.py`.
