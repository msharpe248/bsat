# Default minimizer: bounded work and subtree caching

The default minimizer now enforces the existing `--minimize-budget` option,
checks solver deadlines during reason traversal, and caches completely proved
subtrees within one literal's redundancy check. This fixes the observed failure
to honor internal time limits while retaining the conservative default's binary
reason and recursion-depth rules. It does not establish competition parity.

## Failure and fix

The old recursive minimizer had a depth cutoff of 128, but no work bound or
internal deadline check. A graph where each implication depends on the previous
two implications can cause exponentially repeated traversal despite having only
64 nodes. Linking the new regression against the preserved old core exceeds a
two-second external timeout. The final minimizer proves the same redundancy with
**186 reason-literal inspections**.

Every explicit reason literal inspected by the default minimizer now consumes
one unit of the per-learned-clause budget (default 10000). Zero disables either
minimizer. Exhaustion retains unproved literals and unwinds scratch marks. A
deadline check occurs every 1024 accumulated reason inspections, in addition to
existing solve-loop checks. Cancellation also retains the unproved remainder.

Successful subtree proofs are cached in the existing `seen` array. Each cache
entry stores its required depth, so a proof found on a shallow path cannot
bypass the original depth limit when reused on a deeper path. Exploring nodes
never count as proved. The existing scratch vector records unique cached nodes;
all entries are cleared before the next candidate literal because its source
set may have changed. There is no per-conflict allocation or new variable array.
The separate binary-aware iterative minimizer remains opt-in.

## Reproducible evidence

The pre-change baseline is the working tree after compact assignment storage,
with executable SHA-256
`2d163da182852249a1d3bf397d5708296b39ba7561acc88c60ebf46a491271c8`.
This is not the older Git HEAD. The
[source diff](benchmark_results/minimization-limits-source-20260906.patch)
captures the final core/header change. Raw records retain executable and input
hashes, exact commands and independently checked completed answers.

Timing runs were serial, with no simultaneous builds or correctness-validation
workloads, on the Apple arm64 macOS development machine. Each benchmark has
three repetitions per input/version. This is preliminary development evidence,
not a controlled competition evaluation.

### Deadline behavior

On multiplier-circuits input `90bec6dcf2399d7c7f73acba44a8641e.cnf`, a half-second
internal CPU limit makes all three old-solver runs require the two-second
external timeout. An intermediate budget-only implementation returns in about
0.53 wall seconds. The final cached implementation also returns UNKNOWN in all
three runs: median 0.532 wall seconds, maximum 0.677, with reported solver CPU
time 0.500 seconds. Sources:
[before/budget-only](benchmark_results/bounded-minimization-deadline-20260906.json),
[final build](benchmark_results/minimization-limits-deadline-20260906.json).

The earlier 0.1-second probe is timing-sensitive: in this session both old and
new binaries returned before entering the problematic traversal. That negative
result is retained in the [short probe](benchmark_results/bounded-minimization-deadline-short-20260906.json).
The half-second test and small implication-graph regression establish the defect
without relying on the shorter probe.

### Search work and runtime

The [final repeated comparison](benchmark_results/minimization-limits-work-20260906.json)
uses the prior sixteen-family corpus plus the affected multiplier instance,
30000 conflicts, a three-second internal CPU limit and a five-second external
wall limit. Both versions complete and independently validate 6/51 runs; other
runs are UNKNOWN. Conflict-limited PAR-2 fields are diagnostic, not competition
scores.

On all sixteen original instances, the final implementation exactly matches
baseline decisions, conflicts, propagations, learned literals, minimized
literals and propagation-inspection counters across all repetitions. Fourteen
instances reach 30000 conflicts; their geometric mean candidate/baseline ratio
of median process CPU time is **1.0024**, approximately 0.2% higher. This is too
small to establish a meaningful timing difference in this sample. The two solved
cases remain at 1039 and 22 conflicts, preserving the Hamiltonian behavior that
regressed in the earlier binary-aware minimization experiment.
The [derived summary](benchmark_results/minimization-limits-summary-20260906.json)
records per-instance medians, exclusions and the verified final executable hash.

For comparison, the [intermediate budget-only implementation](benchmark_results/bounded-minimization-work-20260906.json)
has effectively unchanged equal-work CPU time on thirteen families, but one
budget hit changes the relativized-pigeon-hole trajectory. Final caching avoids
that hit and preserves the original trajectory there. Both bounded versions
make progress and honor the CPU limit on the multiplier instance; the old
version is externally killed without final counters.

### Remaining competition gap

With an eight-second internal limit and ten-second external limit on the
multiplier instance, the final implementation makes roughly 42000 conflicts of
progress but remains UNKNOWN in all three repetitions. The budget-only version
also remains UNKNOWN, at roughly 49000 conflicts. Their trajectories differ;
conflict counts alone do not measure solution quality. The old solver is
externally killed each time. Kissat solves SAT with independently verified
models in all three runs, at a median 0.567 wall seconds. Sources:
[baseline, budget-only and Kissat](benchmark_results/bounded-minimization-circuit-20260906.json),
[final cached solver](benchmark_results/minimization-limits-circuit-20260906.json).

This change fixes unbounded minimization work and demonstrates conservative
search compatibility on the measured corpus. It does **not** improve solved
count in these experiments or close the gap to Kissat. The remaining frequent
budget hits on the circuit case and the large time-to-solution gap require
further search/preprocessing work.

## Tests

`tests/test_minimize.c` now covers the shared implication graph, exact work
exhaustion, zero budget, preexisting cancellation, an expired deadline reached
after descending into several reasons, partial successful minimization before
exhaustion, complete scratch cleanup, and the depth-128/129 boundary with cached
subtrees. Small resulting clauses are checked by exhaustive assignments.
The final release and ASan/UBSan unit suites pass.

The independent validator now uses 28 configurations: the previous 26 plus
default-mode budgets zero and one. Both minimizers are exercised with
preprocessing, aggressive reduction, local search, alternate search modes and
text/binary proofs.

Final validation passed **2968 release and 2968 ASan/UBSan solves** on seed
`20260909`: 100 random plus six fixed formulas under all 28 configurations.
Truth-table answers, original-input models, text/binary RUP steps and external
DRAT certificates all passed. These local checks do not imply remote CI ran.

Validation commands:

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat \
  --cases 100 --seed 20260909 --checker /path/to/drat-trim
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug \
  --cases 100 --seed 20260909 --checker /path/to/drat-trim
```
