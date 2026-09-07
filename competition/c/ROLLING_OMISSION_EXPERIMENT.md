# Rolling omissions in vivification trials

**Rejected.** The candidate preserves measured search behavior but shows no
convincing end-to-end speed benefit and slows the larger irreducible kernels.
Production sources are restored to `677aca0`. Partial-progress cutoff tests and
the irreducible benchmark mode are retained.

The baseline at `677aca0` restores the entire candidate suffix after each failed
literal-deletion trial, then shifts the same suffix again to omit the next
literal. This experiment replaces those two moves with one store while leaving
the RUP helper, assumption order, propagation, backtracking and deletion order
unchanged. Inprocessing remains opt-in.

## Representation and interruption

During a trial, the candidate buffer contains the current full clause without
literal `i`; that omitted literal is stored separately. On failure, storing it
at position `i` retains it and omits the next literal. On success, the trial
buffer already holds the strengthened clause, which is logged before preparing
the next omission. At normal completion there is no pending omission. At a
budget/error stop, the saved literal must be inserted back before installing
any earlier successful deletions. The existing 64-literal cap bounds every move.
The candidate allocation and probing helper are unchanged.

This preserves the logical inputs to every RUP call; CPU-time cutoffs can still
occur at different points because the cost changes. It avoids the deliberate
propagation-order changes that regressed the two previous experiments.

## Correctness and preservation checks

The retained suite is extended with **2,064 signed partial-progress cutoff cases**:
one removable literal followed by irreducible literals, stopped at budgets
0 through 128. These join 4,704 existing small cases, including 512 independent
full-deletion comparisons, and eight long-scan rollback/retry cases. Every small
case checks truth-table entailment and RUP against the original input, then
solves the resulting database and verifies SAT models. The baseline and candidate
both pass all **6,776 cases**.

Two mutation builds, leaving a pending omission at an early stop and overwriting
a retained literal when rotating the omission, both fail the independent RUP
assertion. The mutation record pins the test and candidate source hashes.

The [untimed trace comparison](benchmark_results/rolling-vivify-traces-20260907.json)
runs five reused competition inputs with `--inprocess --inprocess-interval 1
--conflicts 1000`. All 21 selected search counters and proof bytes match between
builds. This is a determinism check, not independent validation of incomplete
proof prefixes or a competition performance result.

Both builds pass all 39 C test executables. Each build passes 3,139
certificate-checked solves over 73 configurations, seed 20261120. Both builds pass 42 deadline cases, with maximum observed
CPU overrun 0.000 seconds. These finite checks do not prove universal soundness.

## Benchmark fixtures

The kernel generator adds an
`--irreducible` mode, and the driver adds an `unchanged` mode that checks exact
literal-set preservation. The existing core mode still checks the generated
entailed two-literal core. Both versions satisfy the expected outcomes at lengths
8, 32 and 64 in functional checks: no deletions for irreducible clauses and two
remaining literals for the reducible clauses. The measurements report both work and residual clause size, followed by an
equal-conflict competition-input comparison.

## Kernel measurements

Both serial kernel comparisons use 100 freshly initialized databases per process,
three repetitions and a thirty-second external wall limit. The
[reducible comparison](benchmark_results/rolling-vivify-core-20260907.json) uses seed
20261121; the [irreducible comparison](benchmark_results/rolling-vivify-irreducible-20260907.json)
uses seed 20261122. Each profile verifies 9/9 original SAT models on each corpus,
with zero errors. Propagation work and remaining literals match in every case.
Median accumulated simplification CPU seconds:

| Workload / length | Existing | Rolling omission |
| --- | ---: | ---: |
| Reducible / 8 | 0.006757 | 0.005210 |
| Reducible / 32 | 0.028964 | 0.028549 |
| Reducible / 64 | 0.084182 | 0.077966 |
| Irreducible / 8 | 0.004841 | 0.004828 |
| Irreducible / 32 | 0.047182 | 0.050541 |
| Irreducible / 64 | 0.180735 | 0.185824 |

The candidate improves some reducible cases but slows the larger irreducible
cases by about 7.1% and 2.8%. Fewer suffix moves do not establish lower CPU cost.
These generated fixtures isolate trial-setup behavior, not competition difficulty.

## Competition-input result and decision

The [fixed-conflict comparison](benchmark_results/rolling-vivify-work-20260907.json)
uses `--inprocess --conflicts 50000`, three repetitions per solver, a twenty-second
external wall limit, and seed 20261123. Both profiles verify **9/15 runs**, with
zero errors. Battleship, belpyramid and Hamiltonian solve in every repetition;
hardware verification and `ktf` stop UNKNOWN at the conflict cap. All **21
compared search counters** match across all six runs on each of the five inputs.

| Input | Existing median process CPU | Rolling median process CPU |
| --- | ---: | ---: |
| battleship | 0.360933 | 0.355150 |
| belpyramid | 5.572670 | 5.534782 |
| Hamiltonian | 0.126629 | 0.151402 |
| hardware verification | 2.749054 | 2.783738 |
| ktf | 7.100997 | 8.680888 |

The geometric mean of the five candidate/baseline median CPU ratios is
**1.076619**, about 7.7% slower. Timing variation is substantial: baseline `ktf`
ranges from 7.054 to 8.655 CPU seconds, and candidate `ktf` from 7.974 to 8.725.
Hamiltonian finishes before the inprocessing interval, so its difference cannot
be attributed to the changed trial setup. The experiment does not isolate
compiler-layout or system effects, and the aggregate is not a universal slowdown
claim. It does fail to establish the intended speed improvement.

Mean wall PAR-2 is 17.3903 versus 17.3218 seconds, slightly favoring the candidate;
maximum observed RSS is 59,785,216 versus 59,736,064 bytes. Neither establishes
additional solving power. The mixed kernel results and lack of a convincing CPU
benefit do not justify the extra representation complexity, so reject the runtime
change. These are reused development inputs, not a held-out competition evaluation.

## Retained state and reproduction

After restoration, all **39 C test executables** pass in both release and
ASan/UBSan debug builds, including the 6,776 vivification cases. The restored
release executable is byte-identical to the measured baseline.

The [archived runtime patch](benchmark_results/rolling-vivify-rejected-20260907.patch)
applies to the `677aca0` solver source with `git apply --unidiff-zero`. Expanded
tests pass with either implementation. The
[mutation record](benchmark_results/rolling-vivify-mutations-20260907.json) pins
the exact test and candidate-source hashes. Patch applicability, trace hashes
and all benchmark executable/input/checker hashes were verified.

Generate the original core corpus with `tests/generate_vivify_corpus.py DIR` and
the new irreducible corpus with `tests/generate_vivify_corpus.py DIR --irreducible`.
Compile `tests/vivify_driver.c` with release objects excluding `main.o`, using the
baseline or patched `solver.c` object. Compiler flags are `-std=c11
-D_POSIX_C_SOURCE=200809L -O3 -march=native -flto`, the C include directory and
`-lm`. Core driver arguments are `{input} 100`; irreducible arguments are
`{input} 100 unchanged`. The latter mode checks equality with each original
literal set, including rejection of duplicates, outside the timed simplification
region. Both modes emit a model checked against the original CNF by the benchmark.

Raw JSON records commands, exact input hashes, limits, repetitions and seeds.
Full validation uses `tests/validate.py --cases 30 --seed 20261120` with each build
and external drat-trim; deadline checks use `tests/check_deadlines.py`. Trace
comparison uses the recorded common options and compares proof-file hashes and
its 21 selected counters. Builds, validation and untimed trace checks completed
before serial performance timing began.
