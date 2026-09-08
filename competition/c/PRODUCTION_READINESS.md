# C solver production readiness

This is the authoritative capability and release-gate summary for the C
implementation, updated 2026-09-08. The package is `1.0.0-dev`: it has substantial
correctness and integration evidence, but has not met workload-specific production
acceptance criteria. Historical milestone reports preserve measurements at their
recorded revisions; they do not override this summary or the public API contract.

SAT means a satisfying assignment exists; UNSAT means none exists. UNKNOWN means
the solver has not supplied an answer, commonly because a budget or cancellation
stopped it. There is no useful “accuracy percentage” that turns timeouts into
wrong answers. For production acceptance, independently validate each SAT model
against the original input and each UNSAT certificate against its exact query.
Passing finite tests is not a formal proof of the complete C implementation.

## Capability matrix

| Capability | Current support | Evidence and boundary |
|---|---|---|
| One-shot DIMACS solving | CLI, SAT/UNSAT/UNKNOWN, text or binary DRAT | [Hardening](HARDENING.md), parser/oracle/proof regressions; independently check original models and certificates |
| Public C/C++ embedding | Opaque ABI v1; Linux and macOS C11/POSIX | [Contract](API_CONTRACT.md), [installation](INSTALLING.md); internal `Solver` layouts are not stable ABI |
| Incremental additions and assumptions | Copied permanent clauses, temporary assumptions, model and sufficient failed core | [Contract](API_CONTRACT.md); public assumptions reference existing variables; additions introduce variables |
| Retained learning | Opt-in `BSAT_REUSE_LEARNTS`, including compatible conditional UNSAT and budget slices | [Reuse](INCREMENTAL_REUSE.md); cancellation and incompatible state rebuild from permanent input |
| Certified incremental queries | `BSAT_CERTIFICATES`, alone or with reuse; exact CNF and binary proof export | [Retained certificates](RETAINED_CERTIFICATES.md); conservative options, growing journal, synchronous export |
| Proof-producing simplification in certified mode | Opt-in `BSAT_CERTIFIED_PROBING`, requiring certification | [gen23 investigation](GEN23_CERTIFIED_DIAGNOSIS.md); bounded RUP units preserve variables/assumptions, other transformations remain disabled |
| IPASIR 1.0 | Installed adapter, assumptions, termination and learned-clause callbacks | [IPASIR](IPASIR.md); adapter may introduce assumption variables, unlike the direct public API |
| Independent solver instances | Different handles may run concurrently | [Embedding](EMBEDDING.md), thread sanitizer and isolated CPU-budget tests; serialize calls on each handle and never reenter it from callbacks |
| Query controls/statistics | Mutable CPU/conflict/decision budgets and stable latest-query snapshots | [IPASIR](IPASIR.md); CPU uses solving-thread time, zero limit means unlimited |
| Service wall/capacity controls | Cooperative per-query/checkpoint limits | [Resources](SERVICE_RESOURCES.md); capacity estimate excludes transient memory, facade, allocator and RSS; polling can overshoot |
| Journal quota and checkpoint | Exact accepted journal-byte ceiling; proactive rebuild discards learning | [Checkpoint policy](CHECKPOINT_POLICY_EXAMPLE.md); exhausted journal poisons a handle, checkpoint invalidates prior answers |
| Failed-worker recovery | Application examples retain input, recreate/replay, reject partial recovery and stale answers | [Recovery](SERVICE_RECOVERY.md); tested ENOMEM and SIGKILL worker loss, not a durable application transaction service |
| Hard external containment | Isolated process group, CPU, Linux address-space and per-file limits in harness | [Resource controls](SERVICE_RESOURCES.md); deployment must provide its own aggregate RSS/storage isolation |
| Independent UNSAT acceptance | DRAT-to-LRAT conversion followed by pinned CakeML `cake_lpr` | [Verified checking](VERIFIED_CHECKING.md); checker verifies the exact CNF, not the application's upstream encoding intent |
| Stateful industrial validation | Four published verification circuits, retained CaDiCaL and fresh Kissat | [Circuit histories](INDUSTRIAL_CIRCUIT_HISTORIES.md); bounded preprocessed circuits, not unbounded safety or customer trace coverage |
| Profiling and optimization evidence | Optional diagnostics, Linux software stacks, frozen serial benchmark reports | [Planning/layout](PLANNING_LAYOUT_EXPERIMENT.md); tested VM lacks hardware PMU, compact-header candidate rejected |
| Fuzzing and failures | Coverage-guided API/parser targets, sanitizer and allocation-failure campaigns | [Fuzzing](FUZZING.md), [coverage inventory](tests/FEATURE_COVERAGE.md); bounded exercised paths |

## Current evidence and unresolved gaps

The real-circuit release campaign covers 192 queries through depth 16, growing to
158,101 variables and 391,536 permanent clauses. It has 96 checked SAT, 92 checked
UNSAT and four UNKNOWN results, with no disagreements. All four UNKNOWN results
are positive `cal3` depth-16 queries; both independent solvers prove them UNSAT.
This is an explicit performance gap. The sanitizer circuit smoke adds 128 queries.

Earlier [retained-reference histories](RETAINED_INCREMENTAL_REFERENCE.md) cover
growing variables, changing assumptions, cancelled checkpoint retries and budget
slices. [Long service sessions](SERVICE_SESSION_TAILS.md) cover 49,152 queries and
sampled independent exports. Those generated workloads complement the real
circuits; they do not represent every deployment's latency or proof-growth tails.

The recovery milestone adds 131 deterministic ENOMEM cutoffs per release/sanitizer
build, interrupted replay and journal-quota tests, and a killed-child replacement
with checked SAT/UNSAT snapshots. Parent-process/power-loss durability and a deployed
supervisor's cgroup/storage policy remain application work.

Linux planning sampling attributes about 70% of samples to propagation. Hardware
cache/branch events are unsupported on the tested hosted machine; target-server
PMU attribution is still missing. Compact original headers and the 100,000-work
certified probing prototype were rejected. A later ordinary-budget probing
investigation cuts median certified gen23 query CPU by about half on the target
history and is now available explicitly via `BSAT_CERTIFIED_PROBING`. Defaults
remain unchanged because some confirmation histories regress. See
[the targeted investigation](TARGETED_PERFORMANCE_MILESTONES.md).

## Release gates

A production release needs a concrete target workload and deployment envelope.
“All milestones complete” means the scoped implementation or experiment is done;
it does not mean these gates are automatically satisfied.

| Gate | Required acceptance evidence |
|---|---|
| Exact candidate integrity | Clean tracked tree, pinned revision/compiler/flags/tool/input hashes, reproducible install and frozen ABI consumers; portable build flags for distribution |
| Correctness CI | All Linux GCC/Clang and macOS Clang release/ASan/UBSan jobs, thread sanitizer, independent integration and fuzz jobs pass on the release candidate; triage every failure |
| Answer integrity | No unchecked conclusive result in the acceptance corpus; preserve input, assumptions, model/proof and verification result; mismatches block release, UNKNOWN stays unfinished |
| Application semantics | Review/validate the actual encoding and assumption protocol; replay representative real application histories, including future additions after conditional UNSAT/cancellation |
| Performance | Freeze fresh representative holdouts and explicit throughput, tail-latency, memory and proof-growth limits before tuning; compare references serially under documented comparable budgets |
| Failure and recovery | Exercise quota/ENOMEM/worker death and interrupted replay under the intended supervisor; validate accepted-input replay, bounded retries, artifact/result identity and required durability |
| Operational limits | Validate OS-level memory/storage/CPU containment on the target host; demonstrate checker/export resource policy and cancellation behavior within the deployment's limits |

The repo supplies tests, benchmarks and examples for these gates. It does not yet
supply a deployment-specific acceptance corpus, numerical service SLOs, durable
supervisor or target-hardware PMU evidence. Keep the development designation until
those requirements and the exact-candidate checks are satisfied.

## Where to look

- [Current batch and outcomes](TARGETED_PERFORMANCE_MILESTONES.md)
- [Public ABI and lifetime contract](API_CONTRACT.md), [build/install](INSTALLING.md)
- [Executable coverage and reproduction](tests/FEATURE_COVERAGE.md)
- [Correctness CI](../../.github/workflows/c-solver.yml), [independent integration](../../.github/workflows/c-integration.yml), [fuzz CI](../../.github/workflows/c-fuzz.yml)
- [Archived readiness work](READINESS.md) and revision-specific reports under `benchmark_results/`
