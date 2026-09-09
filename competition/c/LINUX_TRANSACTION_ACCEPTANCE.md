# Complete Linux transaction acceptance — 2026-09-08

Historical envelope. See [hard-query acceptance](HARD_TRANSACTION_ACCEPTANCE.md)
for the current runtime, explicit larger envelope and successful hard cal3 transactions.

The provisional Ubuntu acceptance passes, including independently checked answers
and recovery from worker death, memory, CPU, wall and temporary-storage exhaustion.
This validates the supplied harness on a hosted runner; it is not certification
for an unspecified deployment or a durable transaction service.

The [recorded run](https://github.com/msharpe248/bsat/actions/runs/34303009677)
uses revision `62de694`, Ubuntu 24.04, Linux 6.17 Azure x86-64, portable `-O3`,
and pinned DRAT-to-LRAT/CakeML tools. All 65 release C test executables pass before
acceptance. [Raw evidence](benchmark_results/linux-acceptance-20260908/results.json)
records hashes, cgroup counters, stages, exit states and checks; adjacent files
preserve host details and worker output. The earlier bootstrap run also passed.

## Scope and limits

The worker and all checker descendants share a cgroup with 4 GiB `memory.max`,
zero swap and 64 tasks. A private mount namespace puts a 512 MiB tmpfs at `/tmp`,
covering C `tmpfile()` journals as well as exports and checker artifacts. Each
transaction has a 60-second wall deadline and a 30-second aggregate CPU allowance.
The lightweight supervisor remains outside this cgroup. Repository inputs and
supervisor result logs are also outside the temporary-storage quota. No result
is published until the complete worker transaction has passed all checks.

The supervisor polls CPU usage and storage every 20 ms. CPU/wall termination can
overshoot: the one-second injection stops at 1.0257 CPU seconds; the wall injection
ends at 1.0153 wall seconds. Neither is a zero-overshoot guarantee. Storage samples
are lower bounds on peak occupancy; tmpfs capacity is enforced independently.
Cgroup memory semantics, including possible transient overshoot, follow the
[Linux kernel documentation](https://docs.kernel.org/admin-guide/cgroup-v2.html).
Successful transactions are separately required to fall within the provisional
CPU, wall and memory gates.

## Complete costs and answers

Pinned gen23 depth 8 contains 83,701 variables and 206,920 permanent clauses.
Each normal transaction loads that base, solves both output polarities, exports
the exact query, checks UNSAT through LRAT/CakeML or SAT against the original
clauses, and checkpoints after each answer. Both repetitions produce checked
UNSAT then checked SAT. Checkpoints clear journal bytes and invalidate old values.

| Measurement | First run | Second run |
|---|---:|---:|
| Complete wall seconds | 2.521 | 2.493 |
| Worker/checker aggregate CPU seconds | 2.492 | 2.469 |
| Sum of two solve calls, wall seconds | 0.232 | 0.228 |
| Sum of independent validation stages, wall seconds | 1.398 | 1.385 |
| Cgroup peak memory, MiB | 660.19 | 659.88 |
| Sampled peak private temporary storage, MiB | 9.613 | 9.613 |

The complete time includes startup, decompression, parsing, loading, export,
checking and checkpointing. Per-stage process CPU excludes checker descendants;
use the cgroup CPU total for aggregate cost. Solver-only time substantially
understates this workload's complete cost. The cal3 depth-16 transaction takes
5.251 wall seconds: its positive query remains unaccepted UNKNOWN after its
five-second query allowance; the subsequent negative query is checked SAT.

## Failure and recovery

| Injection | Observed containment | Total failure plus checked replay, wall seconds |
|---|---|---:|
| Worker SIGKILL | Exit -9, no complete result | 3.284 |
| 64 MiB memory ceiling | Cgroup OOM kill, exit -9 | 2.863 |
| One-second aggregate CPU allowance | Whole cgroup killed | 3.557 |
| One-second wall deadline | Whole cgroup killed | 3.536 |
| Fill private tmpfs to 512 MiB | ENOSPC; solver UNKNOWN/error, no exposed value | 3.288 |

Each killed worker is replaced and replayed from the accepted permanent input;
each replay returns checked UNSAT and SAT. Storage recovery removes the injected
filler and recreates the handle inside the worker before replay. All recovery
scenarios stay below 60 wall seconds and 30 aggregate CPU seconds. The 64 MiB
injection can kill during loading, before the explicit allocation loop; it tests
aggregate OOM containment rather than a specific solver allocation site.

These are two normal repetitions and selected failure points, not latency-tail
statistics or exhaustive fault injection. Parent death, power loss, durable input
logging, customer encoding semantics and deployment-specific SLOs remain open.
The CPU and wall fault injections are not accepted successful transactions.

## Reproduction

Dispatch `.github/workflows/c-linux-acceptance.yml` on a disposable Ubuntu runner.
It builds pinned dependencies and runs `tests/linux_acceptance.py` as root with
cgroup v2 and private-mount support. Missing containment support fails the run;
it is never replaced by a simulated pass. The gzipped fixture manifest pins both
compressed bytes and decompressed CNFs. Local macOS smoke testing of transaction
logic is useful but cannot substitute for this Linux acceptance.
