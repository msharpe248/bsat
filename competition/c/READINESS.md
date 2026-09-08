# Production readiness work

This is the current work tracker, not a claim of production or competition readiness.
Completed answers must have an independently checked original-input model or
UNSAT certificate. UNKNOWN is unfinished, not an incorrect answer.

## Milestones

1. CI certificate regressions: complete. The Linux/macOS release/debug workflow
   now runs `test_benchmark_artifacts.py` with its pinned external checker.
   Local validation: all seven tests pass with `/tmp/bsat-drat-trim`.
2. Resource failures and interruption: in progress. Existing
   `test_proof_encoding.c` already covers 22 short-write cutoffs and two deferred
   flush failures; the earlier gap assessment understated that coverage.
   Add systematic allocator failure injection and process-interruption checks.
3. Long-search validation: pending. Add structured SAT/UNSAT cases and equivalent
   variable/clause permutations with independent certificates and event coverage.
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
