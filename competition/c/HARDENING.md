# C solver hardening — September 2026

This work addresses the correctness and performance review of the C implementation.
It establishes a validated baseline, not a claim of industrial production readiness.

## Changes corresponding to the review

| Priority | Implementation |
|---|---|
| Correct answers and independent checks | Strict token-stream DIMACS; empty clauses preserved; duplicate/tautological clauses normalized; original formula retained; every SAT model checked; truth-table and independent proof validation; minimized failures; sanitizer/API tests and CI. |
| Learned-clause reduction | Reduction walks `learnts`, honors retention/LBD options, retains binary/glue/locked clauses, removes watches before deletion, and compacts the learned list. |
| Default minimization | Per-clause reason-inspection budget, internal deadline checks, and depth-aware subtree caching prevent repeated dependency traversal from ignoring resource limits. See [measured regression and fix](MINIMIZATION_LIMITS.md). |
| Memory reclamation | Solver-level collection relocates original/learned references, watches and reasons together. Occurrences are rebuilt and obsolete resolvent references cleared. The unsafe arena-only GC API was removed. |
| LBD and restarts | LBD measured before backtracking with independent level marks; asserting watch ordered by backjump level; EMA initialized on its first sample; sliding-window totals maintained in constant time; Glucose threshold corrected; Luby measures conflicts since the last restart and resets state. |
| Backtracking and assumptions | Conventional asserting-clause backjumping with corrected trail boundaries; assumptions reapplied after backtracking; repeated calls restore the original formula. Failed assumptions do not make the base formula permanently UNSAT. |
| Bounded preprocessing | Probing, BVE, BCE and vivification receive explicit work budgets. BCE uses complete occurrence lists and records model-extension information. BVE stages resolvents before allocations and records all removed clauses. Subsumption uses a bounded rotating candidate set and limits candidate size. |
| Propagation | Binary fast path retained; all input clauses also have consistent arena ownership. Eager watch removal permits blocker checks before arena access. Circular replacement-watch search avoids repeatedly scanning a false prefix and can be disabled for ablations. |
| Proofs | Text and binary DRAT; logging for inferred probing units, BVE resolvents, learned/minimized clauses, vivification and deletions; terminal empty clause on UNSAT; write/open failures cannot become successful results. The proof-enabled solver runs the same minimization as ordinary search. |
| Benchmarks | Serial seeded harness with executable/input hashes, exact commands, wall/CPU time, per-process peak RSS, counters, independently verified solved counts and PAR-2. Deterministic development/heldout generator includes random 3-SAT, pigeonhole and circuit-equivalence cases. |
| Search experiments | Opt-in alternating stable/Luby and focused/LBD modes, with three-valued partial target assignments in stable mode. Vivification tests sequential removals by RUP and allocates replacements instead of corrupting arena record sizes. |

Additional fixes: numeric option validation, protection against overwriting the
input with a proof, deterministic per-instance random generators, allocation
failure propagation from watch insertion, cancellation checks inside propagation,
mode-specific build artifacts, multiline model parsing in cross-validation, and
self-contained tests replacing silent dataset skips. The CLI reports explicit
errors for internal/model/certificate failures rather than printing SAT.

## Validation

Run from the repository root:

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat \
  --cases 200 --checker /path/to/drat-trim
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug \
  --cases 100 --checker /path/to/drat-trim
```

The validator independently checks the answer by truth table for small formulas,
checks original-input SAT models, checks all RUP additions, and invokes the external
DRAT checker on every UNSAT result. It now covers thirty-seven configurations, including
binary proofs and aggressive deletion/vivification. Thirteen fixed formulas preserve
parser, BVE and equivalence regressions. SAT proof prefixes are also checked. CLI checks cover invalid numeric options, proof
I/O failures, input-file protection and resource-limit UNKNOWN results.

The C regression suite includes 800 assumption/API solve calls checked by exhaustive
assignments, duplicate assumptions and subsequent mutation, an over-1-MB DIMACS
line, exact backjump boundaries, locked/glue preservation, live-reason relocation,
and actual garbage collection. The existing feature suite now generates its own
pigeonhole instances to require learning and deletion. CI repeats release and
sanitizer checks on Linux and macOS; adding the workflow does not mean the remote
CI jobs have already run.

Initial hardening validation passed 2,472 release solves and 1,272 sanitizer solves with
independent certificates, as well as both unit suites. A held-out stress run on
19 generated formulas exercised 238 garbage collections and more than 32,000
reductions; all completed answers and certificates were valid. Final benchmark
measurements and executable hashes are recorded separately in
[the benchmark evidence](benchmark_results/hardening-20260906.json).

The subsequent [equivalence milestone](EQUIVALENCE.md) adds opt-in signed SCC
substitution with bounded traversal, proof logging and model reconstruction.
Its benchmark showed no solved-count gain, so defaults remain unchanged. Final
validation passed 3,842 release and 3,842 ASan/UBSan solves (seed 20260910),
with original-model and SAT/UNSAT proof-addition checks across 34 configurations.
Both unit suites passed all nine test executables, including the SCC regressions.

## Final repeated benchmark

The recorded executable identifies the hardening baseline before the subsequent
[minimization change](MINIMIZATION.md). On 19 held-out synthetic
instances, three repetitions of each of seven configurations produced **399/399
independently verified answers**, with zero errors. Each configuration solved 57/57
runs. The raw JSON records the exact binary and input hashes and all measurements.

| BSAT configuration | Total literal inspections (57 runs) | Garbage collections |
|---|---:|---:|
| bsat | 2,614,854 | 3 |
| linear | 3,213,252 | 3 |
| alternating | 2,598,906 | 3 |
| bve | 3,256,959 | 0 |
| stress | 4,749,180 | 714 |

Circular scanning reduced aggregate literal inspections by 18.6% against
the linear variant in this sample; it also changes search order, so this is not a
pure per-propagation microbenchmark. Process runtimes are on the millisecond scale
and do not establish a general speed advantage. Alternating mode remains opt-in.

## What the measurements establish

The medium suite contains 53 small generated instances. BSAT, Kissat and CaDiCaL
all produced independently verified answers in smoke testing. The existing eight
competition-subset instances are substantially harder: a five-second smoke run
solved two with BSAT, three with Kissat, and two with CaDiCaL, with no incorrect
completed answers. These smoke runs establish interoperability and exercise
resource limits; they are not sufficient for a speedup claim. Some smoke runs
overlapped other development work, so their timing is not used for comparisons.

Use longer limits, multiple repetitions, an idle machine, production-specific
families and disjoint held-out data before changing defaults based on performance.
Optional preprocessing/search modes remain optional. Do not infer improved
asymptotic complexity or production readiness from a small solved fraction.

## Remaining boundaries

- Repeated API solves rebuild from the retained input. This is correct under
  destructive preprocessing but deliberately forgoes incremental learned-clause
  reuse. Input retention and explicit binary ownership also cost memory.
- Proofs under assumptions are not exposed as conditional certificates. Such
  calls return UNKNOWN when proof logging is configured. Add assumption units to
  a separate augmented input to obtain a certificate for that formula.
- The API can return a sound, nonminimal conflict clause made from negated failed
  assumptions; it does not minimize assumption cores.
- The existing `--lrb` option is a recency-weighted activity variant, not the full
  MapleSAT LRB algorithm. Alternating mode is an experiment, not a reproduction
  of all Kissat policies or its separate branching queues.
- Diagnostic flags and the SIGUSR1 progress handler remain process-wide. A
  concurrent embedding API is not yet supported.
- `dimacs_write_proof` now returns a success flag and exports an existing completed
  logged proof; it cannot synthesize a certificate when logging was disabled.
- Truth values now use a [compact assignment array](VALUES.md). Further variable
  metadata splits, separate binary watch vectors, sophisticated
  occurrence-indexed learned subsumption and incremental clause reuse remain
  possible follow-up optimizations. They require profiles and workload evidence;
  they were not enabled speculatively.

## Existing work used

- [Kissat](https://github.com/arminbiere/kissat), reference for the C implementation,
  compact propagation and scheduling. Comparison revision:
  `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.
- [CaDiCaL 2.0](https://kfazekas.github.io/papers/BiereFallerFazekasFleuryFroleyksPollitt-CAV24.pdf),
  reference for model reconstruction and API/option testing. Comparison source:
  `c60730422e758ef1cebe7aeddf2dda31c996bf04`.
- [Glucose source](https://github.com/audemard/glucose/blob/master/core/Solver.cc),
  particularly `recent_average * K > global_average` and LBD before backtracking.
- [Chasing Target Phases](https://fmv.jku.at/papers/BiereFleury-POS20.pdf),
  the basis for the opt-in stable/focused and partial-target experiment.
- [Life span of SAT techniques](https://arxiv.org/abs/2402.01202), motivating feature
  ablations and representative benchmarks rather than assuming more features help.
- [DRAT-trim](https://github.com/marijnheule/drat-trim), independent proof checker;
  CI pins `2e3b2dc0ecf938addbd779d42877b6ed69d9a985`.

## Subsequent propagation experiments

[Blocker refresh and contiguous circular scans](PROPAGATION_EXPERIMENTS.md) were
implemented, tested and benchmarked. Blocker refresh lost a solved instance;
contiguous scans preserved search counters but showed negligible aggregate timing
change. Both were reverted. The retained regression adds 256 scan-order cases;
these experiments do not establish a competition-performance improvement.

## Dynamic learned-clause quality

[Dynamic LBD](DYNAMIC_LBD.md) is an opt-in retention experiment. The fixed graph
regression checks conflict/reason updates, unchanged original clauses, independent
scratch marks and glue retention after backtracking. The validator adds three
configurations combining dynamic LBD with aggressive reduction and preprocessing.

Final dynamic-LBD validation passed 4,181 release and 4,181 ASan/UBSan solves
(seed 20260911), including independent truth-table, original-model and text/binary
proof checks. Both unit suites passed all nine test executables.
[Validation record](benchmark_results/dynamic-lbd-validation-20260906.json).

The earlier pushed milestones also passed GitHub CI on Linux and macOS:
[258cb3e](https://github.com/msharpe248/bsat/actions/runs/34054687079) and
[49aa90a](https://github.com/msharpe248/bsat/actions/runs/34054149907). These results
precede dynamic LBD and do not certify its subsequent CI run.

## Clause-activity follow-up

The [recency-activity experiment](CLAUSE_ACTIVITY.md) was tested and discarded:
new-instance measurements did not support retaining it. The production change
rejects nonfinite or out-of-range C API clause-decay values. A new options test
covers boundaries and repeated solves. Historical unit-executable counts above
were corrected from eleven to nine; eleven was the last watch test's case count.
The retained suite now has ten executables.

The pushed dynamic-LBD baseline also passed
[Linux/macOS CI](https://github.com/msharpe248/bsat/actions/runs/34060624967).
Final retained-code checks passed 4181 release formula solves and all ten C
test executables in both release and ASan/UBSan modes. See the
[final validation record](benchmark_results/clause-activity-final-validation-20260906.json).

## Profile-guided activity-layout experiment

[Dense activity storage](DENSE_ACTIVITY.md) was implemented, tested and rejected
after longer runs exposed substantial family-specific regressions. The existing
layout remains unchanged. The retained heap tests cover score initialization,
growth, ordering, backtracking, large-score rescaling and API rebuilds. Profiles
also identify CPU-time checking as a separate performance candidate.
