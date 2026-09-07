# Constant-time active-reason checks

Clause reduction, vivification and blocked-clause elimination previously scanned
all literals to decide whether a clause was an active reason. On-the-fly
subsumption had a second copy of this scan. Both now use one constant-time helper
that checks the first literal's variable. Empty clauses return false safely.

This is the same structural invariant used by
[MiniSat's locked helper](https://github.com/niklasso/minisat/blob/master/minisat/core/Solver.h#L308).
In BSAT, long-clause propagation puts the implied literal at index zero; input
unit enqueueing, learned-clause enqueueing, vivified units and tagged binary
reasons do likewise. While assigned, that literal is true. Watch processing can
swap a false first literal or replace the second watch, but cannot displace this
true first literal. Backtracking clears the assignment/reason, and collection
copies literal order while remapping reason references. Vivification skips locked
clauses. The optimization changes neither ranking nor deletion policy.

Debug builds retain the original complete scan and assert equality at every
helper call. This checks the invariant during actual search and preprocessing,
in addition to dedicated fixtures.

## Validation

Release and ASan/UBSan builds pass all 18 C test executables. The new fixture tests
each possible implied variable in clauses of lengths 3, 32 and 257 (292 cases),
with actual propagation, false-watch replay, reduction protection, forced garbage
collection and deletion after backtracking. Existing suites cover learned binary
reasons, input units, assumptions, solver rebuilds and vivification.

Both builds pass 18 short-deadline cases. The truth-table/model/proof matrix uses
47 configurations and 63 formulas (13 fixed, 50 random; seed 20260928), or 2961
checked solves per build. Debug builds also run the full-scan invariant oracle.

## Measurement

The reduction microbenchmark keeps 10000 equal-ranked unlocked clauses through
100 reductions, with clause lengths 8, 128 and 1024. Setup is excluded. It includes
sorting and retained-clause processing, and isolates the cost of scanning tails
in an eligible database. Five runs per version and length are shuffled with seed
20260928. Reproduce a candidate run with `make clause-locks-benchmark` followed by
`bin/clause_locks_benchmark_release 128`. This synthetic case does not represent
the mix of protected clauses and clause lengths in full search.

The full-solver comparison uses the 28 reused development inputs, two repetitions
per version, VMTF, a 10000-conflict cap, ten solving CPU seconds and fifteen wall
seconds. Equal-conflict measurements permit search-counter comparison. Runs are
serial after all local builds and tests finish. SAT models and UNSAT proofs are
checked independently outside solver timing. Process CPU includes startup,
parsing and proof output. These short reused inputs are not a held-out competition
score. Baseline source is 794a1438c83bcf8635aafcd857a4bc31c3fa714e, binary SHA256
c5d8b9ab52cc50e9322028aee8112b25cb489afb3f6ab633969e6142e8387592.

The microbenchmark medians, in process CPU seconds, are:

| Clause length | Full scan | First-literal check | Reduction |
| ---: | ---: | ---: | ---: |
| 8 | 0.008361 | 0.006117 | 26.8% |
| 128 | 0.056198 | 0.007404 | 86.8% |
| 1024 | 0.357469 | 0.009376 | 97.4% |

The broad equal-conflict comparison verified the same 6/56 runs in both versions,
with zero errors. Every recorded counter except PID, reported time/rates and
deadline-clock reads matched for all 56 paired runs. This includes decisions,
propagations, conflicts, learned/deleted clauses, minimization, work, reductions,
garbage collections and memory statistics. Among the 26 inputs with baseline
median process CPU above 0.05 seconds, the geometric mean candidate/baseline
ratio is 0.99910. This is effectively unchanged, not evidence of a full-solver
speedup. Peak process RSS was 42.81 versus 42.84 MB.

- [Microbenchmark measurements](benchmark_results/clause-locks-micro-20260907.json)
- [Equal-conflict full-solver measurements](benchmark_results/clause-locks-broad-20260907.json)
- [Counter comparison and timing summary](benchmark_results/clause-locks-summary-20260907.json)

A further four-input comparison removes the conflict cap and repeats each profile
twice at the same ten-second solving CPU limit. Baseline verifies 4/8 and the
candidate 6/8, with zero errors. Mean wall PAR-2 is 17.5255 versus 13.0255.
The difference is maximum-constraint-partition: baseline reaches the CPU limit
at 455020–456000 conflicts, while the candidate solves at 476621 conflicts in
9.204–9.943 process CPU seconds. This is close to the limit and should not be
extrapolated to a general competition gain. Both versions solve belpyramid and
multiplier in both repetitions with identical search counters; timings vary in
both directions. Hgen stays UNKNOWN. Peak process RSS is 91.46 versus 91.83 MB.
[Complete longer-run measurements](benchmark_results/clause-locks-targeted-20260907.json).

Retain the change for its demonstrated reduction-cost improvement, constant-time
complexity and preserved search behavior. The longer sample is encouraging, but
the broad equal-conflict result remains flat and competition-level performance
is not established. No new heuristic option or default search policy is added.
