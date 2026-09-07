# Tagged watches for learned binary clauses

Learned binary clauses previously traversed the general arena-clause propagation
path. Tagged watches now let the solver dispatch directly to
binary propagation without loading a clause header or searching for replacement
watches. Each watch remains eight bytes and retains the exact arena reference.

Arena-backed reasons are significant: the default minimizer treats implicit
binary reasons conservatively. Tagged clauses keep arena reasons, literal order,
conflict references and minimization work charges. The tag is decoded during
watch deletion, SCC graph inspection and garbage collection, and restored after
relocation. Both conflict learning and vivification attach tagged binary watches.
Original implicit binaries keep their existing representation.

The tag occupies bit 31 of the reference. Arena growth already uses a 30-bit
capacity limit; initialization, reservation and allocation now consistently
reject requests above that limit, including overflowing size arithmetic.

The dedicated regression exercises actual conflict learning with assumptions,
heap and VMTF modes, propagation reasons, first-UIP analysis, reduction, arena
relocation, exact deletion among mixed watch representations, API rebuilding,
and vivification to binary clauses. It also checks the default minimizer's
one- and two-inspection budgets, eight-byte watch size, reference tag boundaries,
and oversized arena requests without attempting huge allocations.

Production source baseline before the experiment is
`de9556318618b66de0763b6b2ed790f655cd8ee8`. The independent timing-test correction
`38c752f` changes no solver source. Timings use a saved baseline executable and
pin both executable and input hashes. Results are local macOS arm64 development
measurements, not held-out competition results.

## Validation

All 17 C test executables pass in release and ASan/UBSan builds. Each build
passes 5311 validator solves across 47 configurations, 13 fixed formulas and
100 random formulas (seed 20260924). Checks include truth-table answers,
original models, text/binary RUP proofs, and external drat-trim verification of
UNSAT certificates. Both builds pass all 18 short CPU-deadline cases, with
maximum observed overruns rounded to 0.000 seconds.

## Direct propagation measurement

`tests/benchmark_binary_watches.c` builds a chain of 49999 arena-backed learned
binary clauses. Each timed run propagates and backtracks 50000 variables 1000
times. General and tagged watches use the same executable and formula; the
only selection is watch representation. The harness checks propagation success,
trail length and the final implied value each iteration. This is a synthetic
propagation benchmark, not a full SAT search or a proof-validation campaign.

Five serial runs per mode in randomized order (seed 20260924) have median CPU
times 0.551485 seconds for general watches and 0.461032 for tagged watches,
about 16.4% lower. Every run has 50000000 propagations and 49999000 work
inspections. Initialization and destruction are outside timing; backtracking
is inside timing. The short, regular chain is favorable to this operation and
does not predict a comparable competition speedup.

## Targeted end-to-end measurement

Three reused development inputs, three repetitions, VMTF, ten solving CPU
seconds and fifteen wall seconds per run:

| Input | Baseline median process CPU | Tagged median process CPU |
| --- | ---: | ---: |
| belpyramid | 3.2727 | 3.2590 |
| multiplier-circuits | 4.9303 | 4.9063 |
| hgen | UNKNOWN at ten-second limit | UNKNOWN at ten-second limit |

Both versions verify 6/9 runs with zero errors. Mean wall PAR-2 is 12.7567 for
baseline and 12.7491 for tagged watches. Decision, conflict and minimization
inspection counts match exactly on both solved inputs. The measured end-to-end
differences are below one percent and too small to establish a general speed gain.

## Broader and default-heap checks

The 28-input VMTF development sample verifies the same five inputs in both
versions, with zero errors and no gained or lost solves. Mean wall PAR-2 is
24.9409 for baseline and 24.9411 for tagged watches: effectively unchanged.

The same three targeted inputs were also tested with default heap branching,
three repetitions, a 50000-conflict cap and ten-second solving CPU fallback.
All runs reach their conflict cap or complete before that CPU deadline.
Decision, conflict, propagation, literal-work, minimization-inspection,
learned-clause/literal, minimized-literal and restart counts match between
versions. Only belpyramid completes; UNKNOWN on the other two inputs is expected
for this fixed-work comparison. Both verify 3/9 runs with zero errors.

| Input | Baseline median process CPU | Tagged median process CPU |
| --- | ---: | ---: |
| belpyramid | 6.3198 | 6.3816 |
| hgen | 0.6081 | 0.6121 |
| multiplier-circuits | 9.5832 | 9.5819 |

These end-to-end differences are about one percent or less in either direction.
The change is retained as a measured specialization of binary propagation, with
consistent arena-capacity guards and regression coverage. It does not establish
an overall solver speedup or an increase in competition solves. Larger, more
binary-intensive real workloads and other architectures remain unmeasured.

## Evidence

- [Validation and deadline record](benchmark_results/tagged-binary-validation-20260906.json)
- [Direct propagation measurements](benchmark_results/tagged-binary-micro-20260906.json)
- [Targeted VMTF comparison](benchmark_results/tagged-binary-targeted-20260906.json)
- [Broad VMTF comparison](benchmark_results/tagged-binary-broad-20260906.json)
- [Fixed-conflict heap comparison](benchmark_results/tagged-binary-heap-20260906.json)

## Reproduction

```sh
make -C competition/c -j4 all test binary-watches-benchmark
make -C competition/c -j4 MODE=debug all test
competition/c/bin/binary_watches_benchmark_release general
competition/c/bin/binary_watches_benchmark_release tagged
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /path/to/drat-trim --cases 100 --seed 20260924
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug --checker /path/to/drat-trim --cases 100 --seed 20260924
```

Use `tests/benchmark.py` with the reports' inputs, solver templates, limits and
repetitions for end-to-end comparisons. Runs were serial without concurrent
local BSAT builds or tests. Process CPU includes parsing/startup and proof output;
the solving limit excludes parsing. Independent checking is outside solver
timing. UNKNOWN and unverified answers receive twice the wall timeout as PAR-2.
Temporary baseline executable and dataset paths must be recreated elsewhere.
