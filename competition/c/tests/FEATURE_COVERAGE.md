# Executable validation coverage

This matrix describes checked behavior, not a percentage of solver correctness.
The former “100% of directly testable features” claim and 2025 test counts were
outdated. The Makefile discovers `test_*.c` automatically (66 C executables as of
2026-09-09). Release and ASan/UBSan use separate object directories. The authoritative
capability and release-gate summary is [Production readiness](../PRODUCTION_READINESS.md).

| Area | Executable evidence | Boundary |
| --- | --- | --- |
| Parsing/input | `test_dimacs`, `test_input_reader`, parser cases in `test_regressions` | Malformed bytes, boundaries, stream errors; not arbitrary-file fuzz exhaustion |
| CDCL/backtracking | `test_solver`, `test_chrono`, `test_watch`, `test_watch_lookahead`, `test_learned_binaries` | Direct invariants and regression fixtures |
| Learning/minimization | `test_minimize`, `test_binary_minimize`, `test_features` | Learned-clause checks and bounded minimization paths |
| Preprocessing/reconstruction | `test_equiv`, `test_congruence`, `test_elim_*`, `test_model_transfer`, `test_bce_linear` | Gate/SCC/elimination interactions and original model restoration |
| Reduction/collection | `test_reduction_sort`, `test_reduction_ranking`, `test_reduce_growth`, `test_clause_locks`, `test_gc_*` | Fixed tie order, forced sort fallback, tied retention boundaries, locked reasons, relocation and budgets |
| Search policies | `test_heap`, `test_vmtf`, `test_phases`, `test_rephase`, `test_portfolio`, `test_restart_reuse` | Policy/state invariants, certified assumption-prefix restarts and query-aware LBD scoring with 300 duplicates, boundary reset and future query/addition checks; not a general performance guarantee |
| Local search | `test_local_search_*`, `test_walk_phase_feedback`, `validate_local_search.py` | Model checks, initialization, counters, root assignments and budgets |
| API | `test_api_contract`, `test_options`, `test_regressions` | Supported synchronous contract, invalid arguments, repeated calls and truth-table comparisons |
| Allocation failures | `make fault-test recovery-fault-test` | Deterministic ENOMEM sweeps, including 131 recovery cutoffs; not every possible allocation sequence |
| Proof I/O | `test_proof_encoding`, `check_process_failures.py` | Text/binary bytes, short writes, deferred flush failures, file-size limits and signals |
| Independent answers | `validate.py` with pinned drat-trim | Small truth tables, original models, RUP checks and external UNSAT certificates across option combinations |
| Long search | `check_long_search.py` | 24 assignment-exclusion/pigeonhole cases with renamed variables and reordered literals/clauses; counter gates require real restarts, reductions, GC and SCC substitution |
| Matched-host cal3 controls | `benchmark_cal3_host.py`, `c-cal3-host.yml` | Checked timed outcomes and fixed-conflict prefix comparisons; host/OS/compiler effects are not isolated CPU effects |
| cal3 learning diagnosis | `benchmark_cal3_learning.py` | Frozen 13-profile timed/fixed-conflict matrices, checked conclusive answers; development evidence only |
| Measurement integrity | `test_select_corpus.py`, `test_benchmark_manifest.py`, `test_benchmark_artifacts.py`, `test_incremental_summary.py` | History exclusion, frozen hashes, certificate retention and checker exit/status handling |
| Retained incremental reference | `differential_histories.py`, `incremental_reference.py` | Persistent CaDiCaL plus fresh Kissat; additions, assumptions, cancellation and checkpoints |
| Real verification circuits | `test_aag_history.py`, `industrial_histories.py`, `test_competitiveness_summary.py` | Pinned AIGER input, growing CNF, independent circuit simulation and checked proofs; bounded safety queries |
| Expanded circuit/checkpoint histories | `expanded_industrial_aig.json`, `industrial_histories.py --checkpoint-every`, `test_expanded_probing.py` | Larger pinned sources, checkpoint invalidation and journal reset, paired CPU/UNKNOWN accounting |
| Certified journals | `test_proof_journal`, `test_query_export`, `test_journal_limits`, `check_retained_certificates.py` | Exact query context, RUP journal, quota and I/O failures; independent checker required |
| Certified probing | `test_certified_probing`, public flags 6/7 in `industrial_histories.py` and `differential_histories.py` | 4,096 oracle queries per C build, assumption-safe RUP units, future additions, cancellation and journal quotas |
| Service recovery | `test_recovery_service`, `make recovery-fault-test`, `check_recovery_supervisor.py` | Retained input replay, ENOMEM, killed child, cancellation and quota; parent durability outside example |
| Complete Linux acceptance | `linux_acceptance.py`, manual `c-linux-acceptance.yml` | Cgroup/private-tmpfs worker and checker containment, five failures and checked replay; provisional hosted limits, supervisor outside |
| Service limits | `test_service_limits`, `test_query_controls`, `test_resource_limits.py` | Cooperative API limits and isolated OS process limits; owned capacity is not RSS |
| IPASIR/ABI/concurrency | `test_ipasir`, `make embedding-test package-test soak-test` | Installed/frozen consumers, independent instances and callbacks; same-handle calls serialized |

## Reproduce

From `competition/c`, set `DRAT_TRIM` to the independent checker executable:

```sh
make -j4 all test fault-test
make -j4 all test fault-test MODE=debug
export DRAT_TRIM=/path/to/drat-trim
python3 tests/test_benchmark_artifacts.py
python3 tests/test_select_corpus.py
python3 tests/test_benchmark_manifest.py
python3 tests/validate.py --solver bin/bsat --cases 100 --checker "$DRAT_TRIM"
python3 tests/validate.py --solver bin/bsat_debug --cases 100 --checker "$DRAT_TRIM"
python3 tests/check_process_failures.py --solver bin/bsat
python3 tests/check_long_search.py --solver bin/bsat --checker "$DRAT_TRIM" --output long-search.json
```

Use the debug binary for the last two commands to repeat sanitizer checks.
`.github/workflows/c-solver.yml` runs these categories on Linux/macOS release/debug,
along with local-search validation, deadlines and benchmark smoke tests. A local
pass does not establish that the latest remote CI run has passed.

## Interpretation and remaining limits

The original-input SAT model and the independently checked UNSAT proof are the
acceptance evidence. Merely matching another solver's status, printing UNSAT,
passing statistics assertions or returning UNKNOWN does not establish soundness.
No formal verification of the whole C implementation is claimed. Coverage does
not exhaust every option combination, allocation site, platform or long history.
The larger frozen application screen complements these constructed stress cases.

The supported API, concurrency, cancellation and reuse boundaries are explicit
in [API_CONTRACT.md](../API_CONTRACT.md). Conditional certificates use a separate
augmented-input workflow. [PRODUCTION_GAPS.md](../PRODUCTION_GAPS.md) tracks the
latest milestones and evidence.
Older experiment records are historical snapshots; their counts and performance
claims apply only to their pinned binaries and workloads.

## September 8 production extensions

- `test_accounting.c`: opt-in phase/capacity estimates and replacement lifetime.
- `test_congruence_memory.c`: lazy conditional-join allocation.
- `test_incremental_reuse.c`: eight-variable exact oracle, retained clauses,
  added input, conditional-UNSAT fallbacks and repeated conflict slices.
- `embedding_client.c`: opaque shared ABI, concurrent independent instances,
  application-owned signals, atomic cancellation and separate thread CPU budgets.
- `test_cancellation.c`: search, rebuild, equivalence and cached-result polling.
- `embedding_soak.c`: 256-variable parity histories with an exact four-model oracle.
- `test_certify_query.py`: augmented-query acceptance and wrong-base rejection.
- `test_process_control.py`: timeout cleanup of descendants and nested wrappers.

The fresh competitiveness campaign additionally checks an isolated CaDiCaL public
statistics extension (`check_reference_statistics.py`, 128 result/model/core queries)
and frozen summary integrity (six missing/context/contradiction/deadline tests).
`measure_retained_resources.py` isolates one solver/embedding process per circuit;
its RSS includes Python encoding/model validation and excludes proof checkers.
