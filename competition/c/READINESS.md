# Production readiness work

This is the current work tracker, not a claim of production or competition readiness.
Completed answers must have an independently checked original-input model or
UNSAT certificate. UNKNOWN is unfinished, not an incorrect answer.

## Milestones

1. CI certificate regressions: complete. The Linux/macOS release/debug workflow
   now runs `test_benchmark_artifacts.py` with its pinned external checker.
   Local validation: all seven tests pass with `/tmp/bsat-drat-trim`.
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
5. Profiling and broader frozen evaluation: profiling complete; evaluation running.
   `READINESS_PROFILING.md` records six native profiles. The new seeded selector
   gives families equal entry opportunities instead of preferring the smallest
   inputs (six selector tests and three manifest tests pass). The frozen sample
   contains twelve unused families, two repetitions per solver, equal 60-second
   wall limits and a pinned Kissat reference. The manifest/policy are committed
   before final results are assessed; no policy changes during execution.
6. Coverage documentation: complete. `tests/FEATURE_COVERAGE.md` now maps
   executable checks to behavior and limits, replacing the unsupported 100%
   claim. `tests/TEST_SUMMARY.md` points to that maintained inventory.

Each milestone is committed and pushed separately. Benchmark manifests and
policies must be frozen before execution, and reused inputs are development data.
