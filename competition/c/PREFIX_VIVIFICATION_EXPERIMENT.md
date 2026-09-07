# Incremental prefix vivification experiment

**Rejected.** The replacement adds no verified solves and repeatedly slows
battleship by about nine times in process CPU. Production sources are restored
to `d5bd74a`. The independent correctness tests, diagnostic tools and measurements
are retained; `--inprocess` continues to use deletion testing.

The `--inprocess` vivifier at `d5bd74a` tests each single-literal deletion by
rebuilding the negation of the remaining clause and propagating from scratch.
This experiment replaces those repeated tests with one incremental assumption
pass. After each propagation, a bounded scan of the remaining clause detects
a satisfied literal immediately, before introducing unrelated assumptions.
Inprocessing stays opt-in; probing retains its existing single-candidate
RUP helper. The candidate changes learned clauses, propagation order and saved
phases, so it is not a search-preserving optimization.

## Existing work and implementation

The pinned [Kissat vivification implementation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/vivify.c#L1029-L1096)
uses successive assumptions and propagation, skipping implied-false literals and
stopping on an implied-true literal or conflict. BSAT's candidate uses those
basic implications, but does not implement Kissat's reason-based deduction,
candidate sorting, cross-clause assumption reuse or candidate-excluding probing.

For each clause literal, the candidate preserves the closure of earlier negated
assumptions. An already false literal can be omitted: adding the negations of
the remaining literals would falsify the original clause. An already true
literal proves the retained prefix including that literal. A propagation
conflict proves the negation of the assumed prefix impossible. Both latter
outcomes allow truncating the unvisited suffix. If a resource check stops the
pass, the unvisited suffix is retained; only established implications can remove
literals. Temporary assignments are backtracked before the single final RUP
addition is emitted. The original clause remains available until the replacement
is logged and allocated, preserving the existing proof/deletion and watch-update
order. Root propagation is replayed by the existing replacement path.

The [discovery probe](benchmark_results/inprocess-discovery-20260907.json) compares
default search, `--inprocess` and `--subsume-budget 0` on hardware verification and
`ktf`, once each with ten solving CPU seconds and fifteen external wall seconds,
seed 20261111. All six runs are UNKNOWN, with zero errors. Enabling vivification
changes clause statistics but adds no certified answer in this probe. This is
motivation for investigation, not evidence that the feature should be enabled
by default.

## Correctness checks

`test_vivify_prefix.c` checks **4,688 cases** against independent truth-table and
RUP oracles over the original input formula. These cover 512 generated formulas
at nine work budgets and 80 signed cases for early prefix contradiction,
implied-false removal, a satisfied later literal and true/false root assignments.
Every case also solves the resulting database and checks SAT models against the original formula.
The tests pass against both the conservative baseline and the candidate.
Two temporary mutation builds, discarding an unvisited suffix and skipping an
unassigned literal, both fail the independent RUP assertion.

Both candidate builds pass **39 C test executables**. Each passes
**3,139 solves** over 73 profiles, seed 20261114, with truth-table answers,
original SAT models and text/binary UNSAT proofs checked independently. Both
builds pass **42 deadline cases**; maximum observed CPU overrun is 0.000 seconds
for release and ASan/UBSan debug. These finite checks do not establish universal
soundness.

## Measurements and decision

The [competition-input comparison](benchmark_results/prefix-vivify-targeted-20260907.json)
compares the existing and candidate vivifiers with `--inprocess`, fifteen solving
CPU seconds, twenty external wall seconds and two repetitions on five inputs,
seed 20261116. Both verify **6/10 runs**, with zero errors. They solve the same
three inputs; hardware verification and `ktf` remain UNKNOWN.

| Input | Existing median process CPU | Prefix median process CPU |
| --- | ---: | ---: |
| battleship | 0.324646 | 2.929471 |
| belpyramid | 5.482411 | 5.702290 |
| Hamiltonian | 0.123788 | 0.140067 |
| hardware verification (UNKNOWN) | 15.040939 | 15.038904 |
| ktf (UNKNOWN) | 15.039067 | 15.035686 |

Battleship repeats the same conflict regression: **17,491 to 134,516**, with
learned literals increasing from 2,320,157 to 17,724,418. Median wall time rises
from 0.372568 to 4.896314 seconds; the CPU ratio is about 9.02, versus a wall ratio
of 13.14, so wall time alone would overstate the algorithmic CPU regression.
Belpyramid produces fewer learned literals and conflicts, but its median process
CPU does not improve. Hamiltonian follows identical search counters and finishes
before the 10,000-conflict inprocessing interval; its small timing difference
cannot establish a vivification-policy effect.

Mean wall PAR-2 worsens from 17.2690 to 18.1946 seconds. Maximum observed RSS is
60,030,976 versus 60,899,328 bytes. Both large inputs produce final statistics in
all runs; additional conflicts on UNKNOWN inputs are not additional verified
solves. These reused development inputs are sufficient to reject the replacement,
not to judge held-out competition performance.

Reject the runtime change. The short-clause and search regressions prevent
promoting the favorable long-clause kernel result. Future work should investigate
removing unnecessary assumptions through implication dependencies or combining
prefix detection with stronger deletion checks; neither is implemented here.

The kernel generator creates 100 redundant clauses of lengths 8, 32 and 64,
each containing the same two-literal RUP core. `vivify_driver.c` duplicates these
input clauses into the learned database and times only simplification; it checks
that every duplicate retains the entailed two-literal core, and reports residual
size separately. An all-positive model of each original input is independently checked by the benchmark. This is an intentionally
favorable repeated-propagation diagnostic, not a search-difficulty benchmark.

The [initial kernel attempt](benchmark_results/prefix-vivify-kernel-initial-20260907.json)
recorded nine candidate errors because the driver's strict size-two postcondition
failed. Watch movement changes candidate order: the first implementation could
make unnecessary assumptions before reaching an already true later literal. The
candidate was revised to inspect the suffix after propagation, with signed tests
for that case. However, the prefix algorithm can still retain an earlier
unnecessary assumption, because it does not analyze the implication graph to
remove it. Untimed functional checks leave 298 literals across 100 clauses,
versus 200 for deletion testing, on each generated size.

These were driver postcondition failures, not reported wrong SAT/UNSAT answers.
The final diagnostic checks entailment of each result on this generated family
and measures both remaining literals and CPU work. It deliberately makes no
equal-strengthening assumption: less work can produce a weaker result.

## Kernel results

The [final kernel comparison](benchmark_results/prefix-vivify-kernel-20260907.json)
uses 100 freshly initialized databases per process, three repetitions, seed
20261115 and a thirty-second external wall limit. Both profiles verify 9/9 models
with zero errors. Median accumulated simplification CPU seconds:

| Clause size | Deletion testing | Prefix pass | Remaining literals, old / prefix |
| --- | ---: | ---: | ---: |
| 8 | 0.006940 | 0.008725 | 20,000 / 29,800 |
| 32 | 0.028946 | 0.009072 | 20,000 / 29,800 |
| 64 | 0.086288 | 0.009365 | 20,000 / 29,800 |

The largest kernel is about 9.21 times faster, but the shortest is slower and
the prefix pass leaves 49% more literals in this family. Reported propagation
work is also higher: 3,545,600 for prefix versus 1,353,000 / 1,845,000 / 2,501,000
for deletion testing. This counter excludes rebuilding assumptions and moving
the candidate array, so it is not a complete CPU-cost proxy. These results do
not establish equal-quality strengthening or a competition speedup.

## Retained state and reproduction

After restoration, all **39 C test executables** pass in release and ASan/UBSan
debug builds, including the new 4,688-case oracle test. The restored release
executable is byte-identical to the measured baseline. The
[rejected runtime patch](benchmark_results/prefix-vivify-rejected-20260907.patch)
applies to the `d5bd74a` solver source with `git apply --unidiff-zero`; the retained
tests work with either implementation. The
[mutation record](benchmark_results/prefix-vivify-mutations-20260907.json) pins the
test and candidate-source hashes and records both independent RUP failures.

Raw records pin solver/driver commands, executable hashes, input hashes, checker
hashes, limits and seeds. The initial strict-postcondition driver binaries are
preserved separately from the final driver binaries when checking these hashes.
Kernel CPU is measured only around `solver_simplify`; process CPU additionally
includes parsing, setup and model output. Builds and validation do not run
concurrently with either serial performance comparison.

Generate the kernel inputs with:

```sh
python3 competition/c/tests/generate_vivify_corpus.py /tmp/bsat-prefix-vivify-corpus
```

Build `tests/vivify_driver.c` with the release solver objects excluding `main.o`.
For the baseline, use `solver.c` from `d5bd74a`; for the candidate, apply the
archived patch before building that object. Compile with `-std=c11
-D_POSIX_C_SOURCE=200809L -O3 -march=native -flto`, the C include directory,
and link with `-lm`. Run each driver with `{input} 100` through `tests/benchmark.py`, using
the generated manifest, three repetitions, seed 20261115 and a thirty-second
wall limit. The raw targeted record supplies the five exact competition inputs
and solver commands. Reuse those inputs and its two repetitions/seed/limits.
Full validation uses `tests/validate.py --cases 30 --seed 20261114` with each build
and external drat-trim; deadline checks use `tests/check_deadlines.py`.
