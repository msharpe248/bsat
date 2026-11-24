# Independent incremental integration

`tests/differential_histories.py` drives only the public opaque ABI using typed
ctypes bindings. Four flag combinations (rebuild/retain, with/without certificates)
replay the same 256-query history, seed 2026090834. A planted 128-variable 3-CNF
receives permanent additions and changing signed assumptions. Each exact query
snapshot is solved independently by pinned Kissat 8af8e56f174b778aef3aa45af9f739b2a5f492c2.
Kissat runs in a fresh process per query; this checks BSAT history semantics against an independent
solver, not the internal state of a second incremental implementation.

The completed local campaign has 1,024 matching conclusive answers: 936 SAT and
88 UNSAT. Both SAT models are checked against every original clause and assumption.
Every reference UNSAT proof passes DRAT-to-LRAT conversion and cake_lpr validation.
All 88 returned BSAT failed-assumption cores are independently solved and their
UNSAT certificates checked. The 44 BSAT UNSAT answers from certified handles also
export independently checked proofs against their exact original query contexts.
Exported SAT contexts and models are checked too. No mismatches, rejected proofs,
or unexpected UNKNOWNs occur.

The histories include 64 cancellation/retry pairs, 36 checkpoints, and 31 actual
one-conflict UNKNOWN slices. Conclusive slice answers must agree with the final
independently checked result. Checkpoints invalidate models. Failure reports retain
the seed, flag combination, query, original clauses, assumptions, and error for
reproduction. Reports include binary, converter, checker, input and proof hashes.

A separate runner, `tests/check_upstream_ipasir.py`, compiles the unmodified
`genipaessentials` C++ application from upstream IPASIR revision
461a8f4611c41980037723d0e26856fb224ab188 directly against libbsat. It checks 64 small
inputs by exhaustive enumeration and one 4,096-variable input (8,192 dual-rail
solver variables). All 65 cases pass. The large SAT case exercises 4,096 successive
assumption queries after the initial solve, through a real external API consumer.
The application's "essential" means necessary to assign in a satisfying partial
assignment; the oracle removes both polarities of a variable, rather than testing
whether its value is fixed in all total models. This distinction matters for
independent validation. Source/header hashes and build command are recorded.

Upstream source: https://github.com/biotomas/ipasir/tree/461a8f4611c41980037723d0e26856fb224ab188/app/genipaessentials

The new `c-integration.yml` workflow builds pinned references and runs both checks
on Linux for pushes and PRs. These are correctness checks, not timing benchmarks.
Local results use the release library; the separate C unit/embedding suites also
pass under ASan/UBSan. This does not claim every third-party application or every
possible incremental history is covered. The upstream application exits without
releasing its solver; it is tested as an isolated process, unmodified.

Run with `BSAT_DRAT_TRIM` and `BSAT_CAKE_LPR` pointing at the independent tools,
`--library` pointing at a built public library, and `--reference` at Kissat. The
upstream runner requires a checkout at the pinned revision and a C++ compiler.
Raw reports: `benchmark_results/differential-histories-20260908.json` and
`benchmark_results/upstream-ipasir-20260908.json`.
