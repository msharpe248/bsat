# Production readiness work

September 8 update: embedding, conservative learned reuse, conditional-certificate
bundles and broader assurance now have implementations and validation recorded in
[PRODUCTION_GAPS.md](PRODUCTION_GAPS.md). Capability limits below describe this
earlier milestone where they conflict with the current API contract.

This is the current work tracker, not a claim of production or competition readiness.
Completed answers must have an independently checked original-input model or
UNSAT certificate. UNKNOWN is unfinished, not an incorrect answer.

## Milestones

1. CI certificate regressions: complete. The Linux/macOS release/debug workflow
   now runs `test_benchmark_artifacts.py` with its pinned external checker.
   Local validation: all seven tests pass with `/tmp/bsat-drat-trim`.
   Hosted run 34177820780 passed both release jobs and Linux debug, but macOS
   debug was cancelled at the 20-minute allowance during independent validation.
   The allowance is now 60 minutes and matrix fail-fast is disabled, preserving
   all cases and independent platform results. This is an observed CI-runtime
   correction, not a solver correctness failure.
2. Resource failures and interruption: complete. Existing
   `test_proof_encoding.c` already covers 22 short-write cutoffs and two deferred
   flush failures; the earlier gap assessment understated that coverage.
   Release and sanitizer builds each pass 1,868 injected allocation failures,
   six process failure cases, 47 C test executables and 3,956 independent solves.
   The sweep exposed and fixed error clearing during repeated solves. Failed
   instances now remain poisoned; construct a new solver to retry. Evidence:
   `benchmark_results/resource-failures-20260907.json`.
3. Long-search validation: complete. Each build passed 24 assignment-exclusion
   and pigeonhole cases, including signed variable permutations and reordered
   clauses/literals. Independent models and text/binary certificates passed.
   Per build: 215,802 conflicts, 3,834 restarts, 5,632 reductions, 2,501 garbage
   collections and 16 substituted variables. CI now runs the suite; failures
   preserve input/proof files. Records: `benchmark_results/long-search-*-20260907.json`.
   These are deterministic stress cases, not exhaustive combinations or a
   substitute for large application benchmarks.
4. Production API contract: complete. `API_CONTRACT.md` defines ownership,
   error/result lifetime, repeated solves, assumptions and the supported
   single-threaded synchronous boundary. Invalid pointer/literal inputs now
   poison the instance consistently. 48 C tests and 1,868 allocation failures
   pass per build; final sanitizer long-search and process suites also pass.
   Evidence: `benchmark_results/api-contract-20260907.json`. Concurrent embedding,
   conditional proofs and learned-clause reuse remain unsupported capabilities,
   rather than implied production promises.
5. Profiling and broader frozen evaluation: complete. Six native profiles
   identify propagation and proof output as substantial costs. The frozen screen
   completed all 48 runs across twelve families, two repetitions per solver and
   equal 60-second wall limits. BSAT verified one input twice (2/24 runs); Kissat
   verified two inputs twice (4/24). No invalid-answer or execution errors occurred.
   Both text/binary proofs for a separate 160,903-variable, 399,948-clause hardware
   UNSAT case also verified after timing. See `READINESS_EVALUATION.md` and
   `READINESS_PROFILING.md` for results, limitations and all pinned records.
6. Coverage documentation: complete. `tests/FEATURE_COVERAGE.md` now maps
   executable checks to behavior and limits, replacing the unsupported 100%
   claim. `tests/TEST_SUMMARY.md` points to that maintained inventory.

Each milestone is committed and pushed separately. Benchmark manifests and
policies must be frozen before execution, and reused inputs are development data.

## Final validation status

All four hosted Linux/macOS release/sanitizer jobs passed in
[run 34179106951](https://github.com/msharpe248/bsat/actions/runs/34179106951),
after raising the macOS sanitizer allowance without removing coverage. The final
integrity audit confirms the complete benchmark grid, exact frozen tool/input
hashes, checked conclusive answers and unchanged runtime source during timing.

The identified readiness work is complete. Competition performance, formal
whole-program soundness and the unsupported embedding capabilities remain outside
these claims. On the repeated Summle case, BSAT takes about 58.3 seconds versus
Kissat's 9.48 seconds; that is a concrete remaining performance gap.
