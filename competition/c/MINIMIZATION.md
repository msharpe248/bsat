# Learned-clause minimization experiment — September 2026

Historical measurements of the initial experiment follow. The subsequent
[default-minimizer fix](MINIMIZATION_LIMITS.md) adds a work budget and caching
to the conservative path too; `--minimize-budget` now applies to both modes.

The new binary-aware iterative minimizer is **opt-in**. It reduces learned
literal volume and modestly improves fixed-conflict throughput on this sample,
but causes a substantial search regression on one solved instance. The existing
minimizer remains the default; these results do not establish competition parity.

## Implementation

`src/minimize.c` follows both implicit binary reasons and arena clauses without
C recursion or per-conflict allocations. Successful dependency closures are
cached within a learned clause. Strict trail ordering rejects stale/cyclic
reasons, failed traversals cannot populate the success cache, and all scratch
marks are cleared. The asserting literal is preserved. Budget exhaustion keeps
any literal whose redundancy has not been established.

Enable with `--iterative-minimize`. Its `--minimize-budget N` counts inspected
reason literals per learned clause, including explicit propagated literals;
default 10000, zero disables this experimental minimizer. `--no-minimize`
disables either implementation. The budget does not affect the legacy default.
Counters expose inspections, binary steps, cache hits and budget exhaustion.
Both ordinary builds and the explicit PGO build lists include the new source.

The baseline executable was preserved from the hardened working tree before
this experiment, rather than from the older Git HEAD. Its SHA-256 is
`d0b69502422089cbda8ff187ff7de4c9af67de26105c228014a3b79188f0d504`.

## Measurements

Runs were serial with seeded ordering, proof output enabled, independent model
and DRAT checks for completed answers, and no concurrent test/build workload.
The machine was an Apple arm64 macOS development machine, not a controlled
competition host. Two repetitions are preliminary evidence.

The [eight-instance screen](benchmark_results/minimization-screen-20260906.json)
used a ten-second external limit and one repetition. Both BSAT versions solved
2/8, while Kissat solved 3/8. There were no incorrect completed answers. Timing
at this solved fraction and repetition count does not establish a speedup.

For the [fixed-conflict diagnostic](benchmark_results/minimization-work-20260906.json),
we selected the smallest available file in each family, then the first sixteen
families by file size, before measuring performance. The
[manifest](benchmark_results/minimization-corpus-20260906.json) records selection
and hashes. Each version ran twice per input, with 30000 conflicts, a two-second
internal CPU limit and a five-second external wall limit. Four of 32 runs per
version completed and were independently verified. The other runs returned
UNKNOWN, not incorrect answers. Diagnostic PAR-2 values must not be interpreted
as competition scores because conflict limits intentionally interrupt search.

Fourteen families reached exactly 30000 conflicts in both versions:

| Measurement on those fourteen families | Baseline | Iterative |
|---|---:|---:|
| Sum of mean process CPU seconds per instance | 8.524 | 8.098 |
| Learned literals across both repetitions | 39,329,776 | 29,456,250 |
| Removed literals across both repetitions | 1,485,488 | 5,339,756 |
| Propagation literal inspections | 1,242,792,156 | 1,183,091,260 |

The aggregate CPU reduction is 5.0%; the geometric mean candidate/baseline CPU
ratio is 0.965. Learned literal volume fell 25.1%. Search trajectories differ,
so these are neither isolated kernel timings nor evidence of faster solving.
The iterative traversal itself inspected another 145,321,012 reason literals;
the baseline did not instrument corresponding work. Do not treat the propagation
counter decrease as a decrease in total solver work.

The Hamiltonian case explains the default decision: baseline solves in 1039
conflicts and about 0.015 process CPU seconds; iterative solves in 24370 conflicts
and about 0.473 seconds. Both models validate, and both repeats reproduce the
conflict counts. The scheduling case remains at 22 conflicts in both versions.

These raw measurements precede the final option gate: the measured candidate
enabled the iterative algorithm by default. Reproduce its algorithm on the final
binary with `--iterative-minimize`; raw executable hashes are intentionally kept
as measured. Final default/option validation is recorded below.

The [final mode check](benchmark_results/minimization-final-modes-20260906.json)
confirms that default and `--iterative-minimize` exactly reproduce their
respective measured conflict, decision, propagation, learned-literal,
minimized-literal and inspection counters on Hamiltonian, scheduling, miter and
hgen inputs. This check overlapped correctness validation; its timing is not used.

## Correctness coverage

`tests/test_minimize.c` covers implicit and explicit binary reasons, shared
dependencies, a 2000-edge mixed reason chain, failed-branch rollback, cyclic
reasons, exhaustion and disabled modes. Small minimized clauses are independently
checked by exhaustive assignments. Scratch cleanup is asserted after each call.

The Python validator exercises the original twelve configurations with both
minimizers, plus iterative budgets zero and one: twenty-six configurations in
total. It checks truth-table answers, original-input models, text/binary RUP
steps and external DRAT certificates. Sanitizer and release test commands are
documented in [README.md](README.md). Both final unit suites pass (11/11 each).
The final release validator passed 5356 solves and the ASan/UBSan validator
passed 2756 solves across all twenty-six configurations, with independent
answers and certificates. Negative and
overflowing minimization budgets were rejected in both builds.

## Existing work

The standard implication-closure approach and successful-check caching were
informed by [MiniSat's conflict analysis](https://github.com/niklasso/minisat/blob/master/minisat/core/Solver.cc)
and [Kissat's minimizer](https://github.com/arminbiere/kissat/blob/master/src/minimize.c).
This implementation uses its own bounded iterative traversal and BSAT's two
reason representations. Additional tuning requires larger, disjoint workload
families and longer time-to-solution measurements; smaller learned clauses alone
are insufficient grounds to change the default.
