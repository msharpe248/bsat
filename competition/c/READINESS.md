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
4. Production API contract: pending. Specify ownership, error/result lifetime,
   repeated solves, assumptions, concurrency and interruption behavior; exercise
   the supported contract. Learned-clause reuse and concurrent embedding are not
   promised by the existing API.
5. Profiling and broader frozen evaluation: pending. Profile development cases
   separately from timing, then evaluate a family-balanced unseen sample with
   longer budgets and repetitions against a pinned reference solver.
6. Coverage documentation: pending. Replace the outdated feature-count claims
   with executable coverage, remaining limitations and reproducible commands.

Each milestone is committed and pushed separately. Benchmark manifests and
policies must be frozen before execution, and reused inputs are development data.
