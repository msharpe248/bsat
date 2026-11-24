# C solver production readiness

This is the authoritative capability and release-gate summary for the C
implementation, updated 2026-09-10. The package is `1.0.0-dev`: it has substantial
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
| One-shot DIMACS solving | CLI, SAT/UNSAT/UNKNOWN, text or binary DRAT | [Hardening](docs/HARDENING.md), parser/oracle/proof regressions; independently check original models and certificates |
| Public C/C++ embedding | Opaque ABI v1; Linux and macOS C11/POSIX | [Contract](API_CONTRACT.md), [installation](INSTALLING.md); internal `Solver` layouts are not stable ABI |
| Incremental additions and assumptions | Copied permanent clauses, temporary assumptions, model and sufficient failed core | [Contract](API_CONTRACT.md); public assumptions reference existing variables; additions introduce variables |
| Retained learning | Opt-in `BSAT_REUSE_LEARNTS`, including compatible conditional UNSAT and budget slices | [Reuse](docs/INCREMENTAL_REUSE.md); cancellation and incompatible state rebuild from permanent input |
| Certified incremental queries | `BSAT_CERTIFICATES`, alone or with reuse; exact CNF and binary proof export | [Retained certificates](docs/RETAINED_CERTIFICATES.md); conservative options, growing journal, synchronous export |
| Proof-producing simplification in certified mode | Opt-in `BSAT_CERTIFIED_PROBING`, requiring certification | [gen23 investigation](docs/GEN23_CERTIFIED_DIAGNOSIS.md); bounded RUP units preserve variables/assumptions, other transformations remain disabled |
| IPASIR 1.0 | Installed adapter, assumptions, termination and learned-clause callbacks | [IPASIR](docs/IPASIR.md); adapter may introduce assumption variables, unlike the direct public API |
| Independent solver instances | Different handles may run concurrently | [Embedding](docs/EMBEDDING.md), thread sanitizer and isolated CPU-budget tests; serialize calls on each handle and never reenter it from callbacks |
| Query controls/statistics | Mutable CPU/conflict/decision budgets and stable latest-query snapshots | [IPASIR](docs/IPASIR.md); CPU uses solving-thread time, zero limit means unlimited |
| Service wall/capacity controls | Cooperative per-query/checkpoint limits | [Resources](docs/SERVICE_RESOURCES.md); capacity estimate excludes transient memory, facade, allocator and RSS; polling can overshoot |
| Journal quota and checkpoint | Exact accepted journal-byte ceiling; proactive rebuild discards learning | [Checkpoint policy](docs/CHECKPOINT_POLICY_EXAMPLE.md); exhausted journal poisons a handle, checkpoint invalidates prior answers |
| Failed-worker recovery | Application examples retain input, recreate/replay, reject partial recovery and stale answers | [Recovery](docs/SERVICE_RECOVERY.md); tested ENOMEM and SIGKILL worker loss, not a durable application transaction service |
| Hard external containment | Linux acceptance harness: cgroup memory/CPU supervision and private tmpfs covering worker/checkers | [Complete transactions](docs/LINUX_TRANSACTION_ACCEPTANCE.md); hosted provisional limits pass, deployment-specific policy still required |
| Independent UNSAT acceptance | DRAT-to-LRAT conversion followed by pinned CakeML `cake_lpr` | [Verified checking](docs/VERIFIED_CHECKING.md); checker verifies the exact CNF, not the application's upstream encoding intent |
| Stateful industrial validation | Original, expanded and fresh published verification circuit sets, retained CaDiCaL and fresh Kissat | [Circuit histories](docs/INDUSTRIAL_CIRCUIT_HISTORIES.md); bounded preprocessed circuits, not unbounded safety or customer trace coverage |
| Profiling and optimization evidence | Optional diagnostics, Linux software stacks, frozen serial benchmark reports | [Planning/layout](docs/PLANNING_LAYOUT_EXPERIMENT.md); tested VM lacks hardware PMU, compact-header candidate rejected |
| Fuzzing and failures | Coverage-guided API/parser targets, sanitizer and allocation-failure campaigns | [Fuzzing](docs/FUZZING.md), [coverage inventory](tests/FEATURE_COVERAGE.md); bounded exercised paths |

## Current evidence and unresolved gaps

**November 2026 CDCL, inprocessing, GF(2) Gauss, SBVA, symmetry & parallel
portfolio overhaul**: Default one-shot solving enables alternating Focused
(VMTF) / Stable (4-ary VSIDS) modes, chronological backtracking, trail reuse,
3-state binary-aware minimization, block-level all-UIP shrinking, OTFS, dynamic
3-tier clause management, reason-side bumping, literal-indexed `lit_values[]`
BCP fast path with 1-ahead watch prefetching, in-search dynamic GF(2) Gaussian
elimination (`src/gauss.c`, 2..7-var XOR extraction with subsumed-XOR recovery,
Tseitin XOR-chain stitching, and Bird/Han-Jiang two-watched-column matrices
with zero-undo backtracking and 1-UIP temporary reason clauses), multi-core
clause-sharing portfolio solving (`-j <N>` / `--threads <N>`, `src/portfolio.c`,
TSan-clean ring buffer sharing logically-entailed clauses of `size <= 4`),
frozen-variable protection (`solver_freeze`/`solver_melt`/`solver_is_frozen`)
across BVE/SCC/BCE/symmetry and rebuilds, and a tick-budgeted inprocessing
engine (probing/HBR + RUP-certified double-lookahead probing intersection,
preprocessing & periodic inprocessing gate congruence closure plus 3..8-input
N-ary AND congruence, in-place Tarjan SCC, global 64-bit gate-DAG simulation +
dual-cone embedded Mini-Kitten CDCL SAT sweeping with RUP lemma projection,
model-guided global backbone extraction, forward subsumption/SSR, gate-aware
BVE with model reconstruction, post-BVE binary transitive reduction,
prefix-sharing vivification, and Kissat-style ProbSAT `(1+break)^(-2.3)` phase
seeding). Non-RUP passes (root & in-search GF(2) XOR Gaussian elimination,
exact syntactic BreakID-style symmetry breaking (`--sym`, `src/symmetry.c`),
AMO binary-clique Sinz sequential-counter re-encoding on `>=80%` binary
formulas, and Structured Bounded Variable Addition (`--sbva`, `src/sbva.c`))
are automatically skipped whenever `--proof` or `BSAT_CERTIFICATES` is active,
and destructive inprocessing is gated off for assumption queries, certified
proof journals, and `BSAT_REUSE_LEARNTS` sessions so the public ABI and
certified incremental invariants are preserved. Historical experiment and
milestone documents are archived under [`docs/`](docs/).

Measured on the frozen Apple Silicon industrial fixtures and targeted
XOR/AMO/symmetry/random-SAT benchmarks versus the pre-overhaul baseline and
recorded Kissat timings:
- `cal3-query.cnf` (UNSAT): `22.7 s → 0.447 s` non-PGO (`20,294` conflicts, vs. Kissat `1.045 s` — **2.34× faster than Kissat**), including verified DRAT proof emission.
- `16c999d0` (planning, 50k vars / 284k clauses, UNSAT): `>60 s → 54.14 s` (`-j 1`, `26.1M` props/s) and **`25.68 s` wall time (`-j 4`, 1.84× faster than Kissat `47.33 s`)**.
- `09cd83e2` (belpyramid, UNSAT): `2.193 s → 0.342 s` (`18,181` conflicts, `8.81M` props/s).
- `42f1311c` (Tseitin XOR, UNSAT): `>15 s → 0.004 s` (0 conflicts via GF(2) RREF in non-proof mode).
- Mixed underdetermined XOR+3SAT (`N=140`, 126 4-XORs + 3-SAT): SAT `12,703 → 27` conflicts (`0.005 s`), UNSAT `11,510 → 34` conflicts (`0.007 s`) via dynamic in-search GF(2) Gauss.
- Pigeonhole symmetry & AMO (`PHP(12,11)`, `PHP(20,19)`, `PHP(30,29)`, UNSAT): `>5 s` timeout → `0.062 s` (`0` conflicts), `0.006 s` (`0` conflicts), and `0.055 s` (`80` conflicts) via exact BreakID row-interchange lex-leader symmetry breaking.
- Random 3-SAT (`N=300, M=1260`, SAT): `0.138 s` (`7,976` conflicts) → `0.009 s` (`0` conflicts) via ProbSAT `(1+break)^(-2.3)` warmup phase seeding.
- Verification: 67/67 C unit tests pass across `release`, `diagnostic`, and ASan/UBSan `debug` builds (including `test_gauss_and_amo` covering dynamic GF(2) Gauss, XOR chain stitching, subsumed XOR recovery, N-ary congruence, AMO cliques, SBVA, 4-thread portfolio, BreakID symmetry breaking, and frozen-variable inprocessing), plus ThreadSanitizer (`MODE=tsan` zero data races), `embedding-test`, `soak-test`, `package-test`, `fault-test` (3,134+ injected OOM failures), `recovery-fault-test`, `application_histories`, `check_query_diagnostics`, `check_deadlines`, `check_process_failures`, `fuzz_public` OOM reproducer, and `validate.py` (`11,316` release solves + `5,796` ASan/UBSan solves with independent truth-table, model, and text/binary RUP checking). Hard crypto S-box, multiplier-equivalence, and large argumentation/scheduling instances remain open performance targets.

Historical campaign summary (pre-overhaul September 2026 baseline): the certified LBD runtime c6b4ac8 passes correctness, independent
integration and coverage-fuzz CI. The [fresh four-circuit baseline](docs/FRESH_COMPETITIVENESS.md)
checks 64/64 BSAT queries versus 62/64 retained CaDiCaL queries within equal CPU
budgets, adding strengths on cal162 and a selected PicoRV32 case. The
[retained work diagnosis](docs/RETAINED_SEARCH_WORK.md) still shows large search-work
gaps on cal3/cal100. These results establish workload dependence, not general
competitiveness. The latest [certified SSR experiment](docs/CERTIFIED_SSR_MILESTONES.md) passed macOS/Linux
correctness checks but found zero strengthenings on its paired targets or broader
circuit screen and failed the benefit gate. Its runtime is archived. The additional
Linux screen has 110 independently checked answers and two UNKNOWNs across 112
queries, with no observed wrong answers; isolated embedding memory was measured.

The current [search usefulness campaign](docs/SEARCH_USE_MILESTONES.md) measures clause
use at reduction, including deleted clauses, and freezes six fresh direct DIMACS
inputs. The certified score-refresh candidate lost the hard cal3 solve and was
archived. Both restored release/sanitizer builds pass 69 C tests. Both builds
check 2/12 local holdout runs; the rest are UNKNOWN at 15 CPU seconds. Linux
confirms the target regressions and the same holdout checked count. The offline SSR audit finds 47 root-unassigned targets
in a nontrivial multiplier input; another input with witnesses is already root
UNSAT. Neither finding establishes a simplification speedup.

The campaign history below records earlier revisions and their then-current gaps.

The real-circuit release campaign covers 192 queries through depth 16, growing to
158,101 variables and 391,536 permanent clauses. It has 96 checked SAT, 92 checked
UNSAT and four UNKNOWN results, with no disagreements. All four UNKNOWN results
are positive `cal3` depth-16 queries; both independent solvers prove them UNSAT.
This is an explicit performance gap. The sanitizer circuit smoke adds 128 queries.

The [targeted cal3 diagnosis](docs/CAL3_SEARCH_DIAGNOSIS.md) confirms that gap with
equal retained-query CPU budgets: CaDiCaL proves the difficult query in under one
second, while certified BSAT remains UNKNOWN even at 30 seconds. The optional
certified-probing public-library campaign adds 96 release queries (48 SAT, 46
UNSAT, two UNKNOWN) and 64 sanitizer queries, with independently checked answers.

Earlier [retained-reference histories](docs/RETAINED_INCREMENTAL_REFERENCE.md) cover
growing variables, changing assumptions, cancelled checkpoint retries and budget
slices. [Long service sessions](docs/SERVICE_SESSION_TAILS.md) cover 49,152 queries and
sampled independent exports. Those generated workloads complement the real
circuits; they do not represent every deployment's latency or proof-growth tails.

The recovery milestone adds 131 deterministic ENOMEM cutoffs per release/sanitizer
build, interrupted replay and journal-quota tests, and a killed-child replacement
with checked SAT/UNSAT snapshots. Parent-process/power-loss durability and a deployed
supervisor's deployment policy remain application work. The new
[Linux transaction acceptance](docs/LINUX_TRANSACTION_ACCEPTANCE.md) passes provisional
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
[the targeted investigation](docs/TARGETED_PERFORMANCE_MILESTONES.md).

The [planning comparison](docs/PLANNING_SEARCH_COMPARISON.md) leaves BSAT default,
the existing planning profile and Kissat UNKNOWN on both frozen inputs at equal
15-second process-CPU / 20-second wall budgets. BSAT reaches 100,000 conflicts
faster, but that does not establish better solving: the solvers perform different
search and simplification work. No additional planning policy is promoted.

The [fresh larger holdout](docs/EXPANDED_HOLDOUT_BASELINE.md) adds nine CNFs and 36
serial BSAT/Kissat runs. BSAT completes 4/18 and Kissat 2/18; all completed answers
are checked SAT. BSAT solves the 611,755-variable, 10,974,540-clause planning
case in about 6.1 process-CPU seconds in both repetitions. Original-model checking
adds about 9.5 wall seconds outside solve timing; those costs belong in deployment
acceptance budgets. Most fresh inputs remain UNKNOWN for both solvers.

The [clause eligibility follow-up](docs/LEARNED_CLAUSE_ELIGIBILITY.md) corrects a
repeated recommendation: scan-heat selection had already failed a prior holdout.
About 71% of learned scans on the larger development planning input occur beyond
the existing strengthening size limit. The new counters are diagnostic-only.
[cal3 structural ablations](docs/CAL3_STRUCTURAL_DIAGNOSIS.md) also remain unresolved
for BSAT; CaDiCaL solves even with preprocessing disabled. These findings do not
justify another production flag or a default change.

Expanded circuit validation also exposed a [checker resource limit](docs/CHECKER_RESOURCE_SCALING.md):
a valid larger reference certificate exhausted the fixed CakeML heap, then its
stack. Both sizes are now explicit settings and failures retain diagnostics and
optional artifacts. The same certificate verifies with 2,048/512 MB settings,
using about 2.28 GB checker RSS. These are checker runtime settings, not OS-level
aggregate memory ceilings, and are separate from solver resource requirements.

The completed [expanded probing comparison](docs/EXPANDED_CERTIFIED_PROBING.md) adds
128 paired queries through depth 24, reaching 730,151 variables and 1,960,296
permanent clauses. Each mode returns 36 checked SAT, 20 checked UNSAT and eight
UNKNOWNs. Probing CPU PAR-2 increases 3.4% with no solved-case loss; it remains
opt-in. Another 32 sanitizer and 64 deeper release queries exercise checkpoint
invalidation, zeroed journals and subsequent checked answers. Neither finite
validation nor a configurable checker resource envelope establishes application
SLOs or general solver competitiveness.

The [longer fixed development subset](docs/LONGER_DEVELOPMENT_SUBSET.md) uses 60 CPU
seconds and 90 wall seconds. Each solver checks 4/8 runs. BSAT now proves the
fresh one-shot cal3 snapshot in 22.7 CPU seconds (Kissat 1.05), and solves influence
maximization in 56.9 seconds (Kissat 24.5). Both leave the selected planning and
scheduling inputs UNKNOWN. This does not resolve retained-query performance or
establish a default improvement; cal3 proof checking adds another 7.8 wall seconds.

The [learning-policy diagnosis](docs/CAL3_LEARNING_DIAGNOSIS.md) adds 52 controlled
runs. All 14 BSAT ten-second ablations remain UNKNOWN; all 12 timed CaDiCaL runs
check UNSAT, including disabling both minimization and shrinking. Iterative
BSAT produces 66% fewer learned literals at 10,000 conflicts without adding a
solve, and retaining more raises memory usage. No default is promoted. CaDiCaL
and BSAT learned-literal counters are explicitly distinguished before/after
minimization; raw totals are not matched quality measurements.

The [focused implementation follow-up](docs/CAL3_IMPLEMENTATION_MILESTONES.md) rejected
a reason-side VSIDS experiment and exposed cross-host search divergence. The
[portable reduction fix](docs/PORTABLE_REDUCTION_ORDER.md) now isolates and fixes its
first cause: platform-dependent ordering of tied learned-clause scores. Identical
pre-sort states diverged at the first reduction, conflict 2,000. The default now
uses a fixed typed sort preserving the observed Mac policy. Linux changes from
two sixty-second UNKNOWNs to two checked UNSATs at 32.8/33.6 CPU seconds on the
first runner. The exact-default comparison on a newer runner checks all answers
and improves median CPU from 50.5 to 20.8 seconds (2.43× on this query), with
complete proofs matching Mac byte for byte. The five-input local confirmation
checks 10/10 runs in both versions, with identical proof streams and less than
1% aggregate CPU change. This improves the measured Linux case; it does not close
the remaining reference-solver gap or establish workload-specific readiness.

The [certified assumption-prefix follow-up](docs/ASSUMPTION_RESTART_PREFIX.md) adds a
checked retained cal3 solve within 60 CPU seconds: the control remains UNKNOWN,
while the certified candidate completes in repeated 40.7–52.9-second runs. Both
four-circuit candidate passes check 48/48 queries. A global version regresses
uncertified cal3, so the final internal policy is enabled only by the certified
facade. Certified gen23 becomes slower in the measured pass; the change is not a
universal speedup. Full release/sanitizer and independent certificate checks pass;
the direct production library checks 96/96 queries and proves certified cal3 in
46.7 CPU seconds. Runtime CI remains separately tracked in that report. The broader eight-family
Linux reduction screen also passes its frozen regression gate, with only 4/16
checked runs per version; its many UNKNOWNs limit generalization.

The [assumption-aware LBD follow-up](docs/ASSUMPTION_LBD_EXPERIMENT.md) passes frozen
original/expanded confirmation on Mac and Linux and enables scoring that excludes
query-fixed levels for certified solves. Linux improves 94/96 → 96/96 and
60/64 → 62/64 checked queries, with no checked losses; CPU PAR2 improves 72.83%
and 19.13%. cal100 depth 8 remains UNKNOWN, and Linux gen23 and some journal sizes
regress. Full query checking remains outside solve timing. The scoring change
preserves all learned literals and query context; it does not establish a general
soundness proof or deployment readiness.

The [hard transaction follow-up](docs/HARD_TRANSACTION_ACCEPTANCE.md) now passes two
fresh checked cal3 transactions at about 94 seconds end to end, with about 2.67 GiB
cgroup peak memory and 648 MiB sampled temporary storage. All five fault/replay
scenarios pass within the frozen provisional envelope. The deliberately interrupted
checkpoint sequence was corrected to restore its ordinary CPU budget. This remains
hosted public-workload acceptance; customer traces and deployment SLOs are missing.

The [complete proof-cost experiment](docs/PROOF_COMPACTION_EXPERIMENT.md) rejects an
automatic DRAT pre-trimming pass: artifact size falls substantially, but total
verification CPU is unchanged on cal3 and 32.6% worse on cal100. The bounded
failed-minimization cache also preserves all target work and is removed. These
negative results narrow future work without introducing unmeasured defaults.

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

- [Search usefulness, fresh DIMACS and score-refresh results](docs/SEARCH_USE_MILESTONES.md)
- [Certified SSR experiment and outcomes](docs/CERTIFIED_SSR_MILESTONES.md)
- [Public ABI and lifetime contract](API_CONTRACT.md), [build/install](INSTALLING.md)
- [Executable coverage and reproduction](tests/FEATURE_COVERAGE.md)
- [Correctness CI](../../.github/workflows/c-solver.yml), [independent integration](../../.github/workflows/c-integration.yml), [fuzz CI](../../.github/workflows/c-fuzz.yml)
- [Archived readiness work](docs/READINESS.md) and revision-specific reports under `benchmark_results/`
