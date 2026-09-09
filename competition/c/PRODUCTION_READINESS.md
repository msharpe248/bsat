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
| Hard external containment | Linux acceptance harness: cgroup memory/CPU supervision and private tmpfs covering worker/checkers | [Complete transactions](LINUX_TRANSACTION_ACCEPTANCE.md); hosted provisional limits pass, deployment-specific policy still required |
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

The [targeted cal3 diagnosis](CAL3_SEARCH_DIAGNOSIS.md) confirms that gap with
equal retained-query CPU budgets: CaDiCaL proves the difficult query in under one
second, while certified BSAT remains UNKNOWN even at 30 seconds. The optional
certified-probing public-library campaign adds 96 release queries (48 SAT, 46
UNSAT, two UNKNOWN) and 64 sanitizer queries, with independently checked answers.

Earlier [retained-reference histories](RETAINED_INCREMENTAL_REFERENCE.md) cover
growing variables, changing assumptions, cancelled checkpoint retries and budget
slices. [Long service sessions](SERVICE_SESSION_TAILS.md) cover 49,152 queries and
sampled independent exports. Those generated workloads complement the real
circuits; they do not represent every deployment's latency or proof-growth tails.

The recovery milestone adds 131 deterministic ENOMEM cutoffs per release/sanitizer
build, interrupted replay and journal-quota tests, and a killed-child replacement
with checked SAT/UNSAT snapshots. Parent-process/power-loss durability and a deployed
supervisor's deployment policy remain application work. The new
[Linux transaction acceptance](LINUX_TRANSACTION_ACCEPTANCE.md) passes provisional
cgroup/private-tmpfs gates, including all five injected failures and checked replay.
Normal gen23 transactions take 2.49–2.52 wall seconds end to end, versus about
0.23 seconds in solving; peak aggregate memory is about 660 MiB. UNKNOWN stays
unaccepted. CPU/wall termination is polled and may overshoot; the supervisor and
durable application storage are outside this measured worker/checker envelope.

Linux planning sampling attributes about 70% of samples to propagation. Hardware
cache/branch events are unsupported on the tested hosted machine; target-server
PMU attribution is still missing. Compact original headers and the 100,000-work
certified probing prototype were rejected. A later ordinary-budget probing
investigation cuts median certified gen23 query CPU by about half on the target
history and is now available explicitly via `BSAT_CERTIFIED_PROBING`. Defaults
remain unchanged because some confirmation histories regress. See
[the targeted investigation](TARGETED_PERFORMANCE_MILESTONES.md).

The [planning comparison](PLANNING_SEARCH_COMPARISON.md) leaves BSAT default,
the existing planning profile and Kissat UNKNOWN on both frozen inputs at equal
15-second process-CPU / 20-second wall budgets. BSAT reaches 100,000 conflicts
faster, but that does not establish better solving: the solvers perform different
search and simplification work. No additional planning policy is promoted.

The [fresh larger holdout](EXPANDED_HOLDOUT_BASELINE.md) adds nine CNFs and 36
serial BSAT/Kissat runs. BSAT completes 4/18 and Kissat 2/18; all completed answers
are checked SAT. BSAT solves the 611,755-variable, 10,974,540-clause planning
case in about 6.1 process-CPU seconds in both repetitions. Original-model checking
adds about 9.5 wall seconds outside solve timing; those costs belong in deployment
acceptance budgets. Most fresh inputs remain UNKNOWN for both solvers.

The [clause eligibility follow-up](LEARNED_CLAUSE_ELIGIBILITY.md) corrects a
repeated recommendation: scan-heat selection had already failed a prior holdout.
About 71% of learned scans on the larger development planning input occur beyond
the existing strengthening size limit. The new counters are diagnostic-only.
[cal3 structural ablations](CAL3_STRUCTURAL_DIAGNOSIS.md) also remain unresolved
for BSAT; CaDiCaL solves even with preprocessing disabled. These findings do not
justify another production flag or a default change.

Expanded circuit validation also exposed a [checker resource limit](CHECKER_RESOURCE_SCALING.md):
a valid larger reference certificate exhausted the fixed CakeML heap, then its
stack. Both sizes are now explicit settings and failures retain diagnostics and
optional artifacts. The same certificate verifies with 2,048/512 MB settings,
using about 2.28 GB checker RSS. These are checker runtime settings, not OS-level
aggregate memory ceilings, and are separate from solver resource requirements.

The completed [expanded probing comparison](EXPANDED_CERTIFIED_PROBING.md) adds
128 paired queries through depth 24, reaching 730,151 variables and 1,960,296
permanent clauses. Each mode returns 36 checked SAT, 20 checked UNSAT and eight
UNKNOWNs. Probing CPU PAR-2 increases 3.4% with no solved-case loss; it remains
opt-in. Another 32 sanitizer and 64 deeper release queries exercise checkpoint
invalidation, zeroed journals and subsequent checked answers. Neither finite
validation nor a configurable checker resource envelope establishes application
SLOs or general solver competitiveness.

The [longer fixed development subset](LONGER_DEVELOPMENT_SUBSET.md) uses 60 CPU
seconds and 90 wall seconds. Each solver checks 4/8 runs. BSAT now proves the
fresh one-shot cal3 snapshot in 22.7 CPU seconds (Kissat 1.05), and solves influence
maximization in 56.9 seconds (Kissat 24.5). Both leave the selected planning and
scheduling inputs UNKNOWN. This does not resolve retained-query performance or
establish a default improvement; cal3 proof checking adds another 7.8 wall seconds.

The [learning-policy diagnosis](CAL3_LEARNING_DIAGNOSIS.md) adds 52 controlled
runs. All 14 BSAT ten-second ablations remain UNKNOWN; all 12 timed CaDiCaL runs
check UNSAT, including disabling both minimization and shrinking. Iterative
BSAT produces 66% fewer learned literals at 10,000 conflicts without adding a
solve, and retaining more raises memory usage. No default is promoted. CaDiCaL
and BSAT learned-literal counters are explicitly distinguished before/after
minimization; raw totals are not matched quality measurements.

The [focused implementation follow-up](CAL3_IMPLEMENTATION_MILESTONES.md) adds
matched Linux/macOS controls and an implemented reason-side VSIDS experiment.
The candidate passes finite validation but loses both target solves and is
removed. Linux BSAT remains UNKNOWN at sixty CPU seconds while both references
finish near one second. BSAT's 10,000-conflict trace differs across hosts; the
references' prefixes match. The cause of that divergence remains to be isolated.
These results establish neither a runtime improvement nor a hardware-only cause.

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

- [Current batch and outcomes](CAL3_IMPLEMENTATION_MILESTONES.md)
- [Public ABI and lifetime contract](API_CONTRACT.md), [build/install](INSTALLING.md)
- [Executable coverage and reproduction](tests/FEATURE_COVERAGE.md)
- [Correctness CI](../../.github/workflows/c-solver.yml), [independent integration](../../.github/workflows/c-integration.yml), [fuzz CI](../../.github/workflows/c-fuzz.yml)
- [Archived readiness work](READINESS.md) and revision-specific reports under `benchmark_results/`
