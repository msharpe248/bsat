# Recovering failed solver workers

The public-ABI recovery example retains an application-owned permanent-input log,
destroys failed solver handles, and creates a fresh worker by replaying exactly
that accepted input. No internal error flag is cleared. The POSIX supervisor
example also replaces a child killed during replay while the parent retains input.

## Handle recovery

[recoverable_service.h](examples/recoverable_service.h) is a small integration
pattern, not part of the installed solver ABI. It provides a fixed-capacity log,
a maximum journal quota, explicit replay and solve operations, and cancellation.
The example bounds variables and clause length to 1,000,000, below core limits.

`recovery_add` validates and commits to the owner log before forwarding a clause.
Return 1 means recorded even if forwarding fails; the worker is then discarded.
Return 0 means nothing was accepted. This distinction lets callers retain every
accepted clause without depending on a partly failed worker's input state.
Input accepted while offline is replayed when the caller recreates the worker.

`recovery_rebuild` frees the old worker first, respecting peak-memory pressure,
then creates and populates a replacement. A cancelled or allocation-failed replay
discards the partial replacement. Only a complete replay installs a new generation.
Old results are invalidated on additions, solves and every recovery attempt.
Temporary assumptions never enter the log. Callers resubmit them for each query.

An errored solve returns UNKNOWN and discards its worker. The caller chooses when
to retry, clear cancellation or raise a journal quota within the configured cap.
There is no implicit unlimited retry loop or silent quota escalation. Allocation
failure can recur if memory is still unavailable; success requires resources for
the replay. Each solve has a five-thread-CPU-second example budget. Replay polls
cancellation between clauses; this is not a hard time or RSS guarantee.

## Process isolation

[recovery_supervisor.c](examples/recovery_supervisor.c) forks from a single-threaded
owner with retained input. Its first child is deliberately killed during replay.
The parent checks the signal, accepts another permanent clause, then forks a new
child. The replacement exercises interrupted replay and journal exhaustion before
exporting a certified UNSAT query and a SAT model. The parent publishes completion
only after the replacement exits cleanly; attempt-specific artifact names avoid
confusing a failed attempt's output with the accepted result.

The log survives worker death because the parent owns it. It is not a durable
transaction log: parent death or power loss still requires storage outside this
example. A production supervisor should use durable accepted-input sequence IDs,
OS process/memory limits, bounded recovery attempts and independently validated
answers. Do not fork a live multithreaded process containing solver handles.
The test uses SIGKILL to model abrupt worker loss; it does not force the host into
an actual kernel OOM condition. ENOMEM is tested deterministically inside the core.

## Validation and reproduction

Release and ASan/UBSan builds pass:

- 131 injected allocation-failure cutoffs spanning construction, replay,
  forwarded additions and solving, followed by exact-model recovery and
  contradictory-assumption/retry checks.
- Journal-quota exhaustion, five cancellation boundaries during replay, accepted
  offline additions, atomic log-capacity rejection, quota-cap refusal, and no
  stale model after failure or cancellation.
- Killed-child replacement and independently checked exact SAT/UNSAT snapshots
  containing every retained permanent clause and only the current assumptions.

```sh
make -C competition/c recovery-fault-test recovery-example test
make -C competition/c MODE=debug recovery-fault-test recovery-example test
BSAT_DRAT_TRIM=/path/to/drat-trim BSAT_CAKE_LPR=/path/to/cake_lpr \
  python3 competition/c/tests/check_recovery_supervisor.py \
  --binary competition/c/bin/recovery_supervisor_release --output /tmp/recovery.json
```

The correctness CI matrix runs these checks on Linux GCC/Clang and macOS Clang,
release and sanitizer builds. Independent certificate evidence:
[release](benchmark_results/recovery-supervisor-20260908.json) and
[sanitizers](benchmark_results/recovery-supervisor-debug-20260908.json).
The existing [checkpoint policy](CHECKPOINT_POLICY_EXAMPLE.md) remains the proactive
way to avoid journal exhaustion; recovery handles failure after prevention fails.
