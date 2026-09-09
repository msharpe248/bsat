# Linux acceptance and search-quality follow-up — 2026-09-08

Authorized: complete the three recommended workstreams, update current docs,
commit and push each validated milestone. Freeze this policy before experiments.

1. **Complete Linux transactions.** Use an available Ubuntu 24.04 hosted runner
   unless a deployment target is supplied. Provisional transaction gates: 60 wall
   seconds, 30 aggregate CPU seconds, 4 GiB cgroup memory with swap disabled,
   512 MiB private temporary filesystem, 64 tasks. Measure loading, solving,
   export, independent verification, checkpoint and replay/recovery together.
   Use pinned real gen23 depth-8 and cal3 depth-16 bases. Check SAT and UNSAT
   answers, preserve UNKNOWN, and inject worker death, memory, CPU, wall and
   storage exhaustion. A clean checked replay must follow recoverable failures;
   no partial worker output may be accepted. Missing Linux containment support
   is a failed/unsupported acceptance prerequisite, never silently emulated.
2. **cal3 learning quality.** Audit the existing conflict/minimization/retention
   code and rejected experiments first. Compare existing BSAT minimization modes
   and retention ablations with pinned CaDiCaL default/plain, minimization,
   shrinking and reduction ablations. Same exact depth-16 query, independently
   checked answers. Use 10 CPU seconds / 15 wall seconds, two serial repetitions;
   a separate bounded-conflict run obtains final counters where needed. These
   development ablations diagnose causes, not default-promotion evidence.
3. **Longer fixed subset.** BSAT/Kissat on planning 16c999, the exact cal3 snapshot,
   and the previously frozen influence-maximization and scheduling inputs that
   both solvers left UNKNOWN at ten seconds. Two serial repetitions, 60 process
   CPU seconds / 90 wall seconds, seed 2026090868. Checking is outside solver
   timing with explicit 2,048/512 MB checker resources. These are development
   inputs; no fresh-holdout claim and no tuning after inspecting results.

Local timing runs do not overlap local builds/tests/fuzz. Independent remote
Linux execution may overlap local work on the separate macOS host. Every result
records tool/input hashes, host and limits. Finite provisional gates cannot
certify an unspecified application's semantics, durability or service SLOs.

Status: [complete Linux acceptance](LINUX_TRANSACTION_ACCEPTANCE.md) passes the
provisional gates. The [longer subset](LONGER_DEVELOPMENT_SUBSET.md) is complete:
both solvers check 4/8 runs; BSAT one-shot cal3 finishes, while planning/scheduling
remain UNKNOWN. The [learning diagnosis](CAL3_LEARNING_DIAGNOSIS.md) completes
52 runs, including source/counter auditing; no BSAT ablation adds a ten-second
solve, so defaults remain unchanged. All three scoped workstreams are complete.

The learning matrix is frozen in `tests/benchmark_cal3_learning.py`: BSAT
default, no minimization, iterative, binary, combined, retention fraction 0.9,
and reduction interval 1,000,000,000 (effectively disabled for this screen).
CaDiCaL: default, plain, plain without minimization, plain without shrinking,
plain without either, and plain without reduction. Run both the CPU-limited
screen and a separate 10,000-conflict diagnostic, two repetitions each. The
default CaDiCaL shrinking pass can still operate with `--no-minimize`; disabling
both is a distinct ablation. No new runtime option or default is proposed.
