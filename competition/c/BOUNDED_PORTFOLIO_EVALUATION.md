# Bounded portfolio evaluation

The existing C portfolio uses one process and two sequential attempts. This
comparison adds an OS process CPU ceiling to the benchmark harness, covering
initial parsing, both attempts, reconstruction and proof flushing. It does not
refresh at fallback. All four profiles have 60 process CPU seconds, a 60-second
external wall deadline, 4 GiB virtual address space and 1 GiB per file. Both
live solver states during reconstruction count against the same address space.
The CPU soft limit terminates with SIGXCPU; the hard limit is one second later.
Kernel accounting has scheduling granularity. SIGXCPU under a configured limit
is recorded as UNKNOWN, even if a child printed an early answer. Other signals
remain errors rather than being guessed to be resource exhaustion.

The frozen workflow (`c-performance.yml`, suite `portfolio`) compares alternating,
alternating without rephasing, focused-first/alternating portfolio, and that
portfolio with rephasing disabled throughout. The private focused slice remains
two solving CPU seconds; every whole run shares the same external budgets.
This uses existing opt-in policies, not another search implementation.

Two pinned development inputs (CSP and planning), one confirmatory repetition,
seed 2026090846, serial execution pinned to one available Linux CPU. All conclusive
answers require original-model validation or DRAT-to-LRAT conversion followed by
CakeML's verified checker. Proof checking has the same separate 600-second wall
allowance for every profile. Validation time is reported separately and included
in end-to-end elapsed time; it is not charged as solver search time. Address-space
and file ceilings cover the solver process, not aggregate checker disk/RSS.

This is a targeted development comparison, not a fresh holdout or promotion test.
The macOS resource harness passes seven applicable tests, including an actual CPU
limit killing a child that printed a fake early SAT answer; Linux address-space
enforcement is checked by the remote workflow. A launch-delay regression test
requires an early SAT answer to be rejected when launch plus solve exceeds the
shared wall allowance.

The [initial two-repetition run](benchmark_results/bounded-portfolio-launch-gap-20260908/portfolio.json)
(seed 2026090843) exposed a harness gap: `Popen.wait(timeout)` started a fresh
wall allowance after process launch. Reported portfolio elapsed times reached
61.2–61.9 seconds. CPU was limited across the process, but total wall was not
fully charged. No portfolio solve was added: no-rephase and portfolio-no-rephase
each verified the CSP twice, while control and portfolio solved neither input.
The solver/reference binaries and raw platform records are retained, but these
runs are not presented as satisfying the intended total-wall protocol.

The harness now deducts measured launch time from the remaining wait and rejects
any conclusive result observed after the total wall deadline. It records launch
time explicitly. Cleanup and scheduler latency can still make observed elapsed
time exceed a requested deadline; this never earns an accepted late solve.
The [corrected confirmatory run](benchmark_results/bounded-portfolio-20260908/portfolio.json)
passes on Linux (workflow 34277647416), pinned to CPU 0 of an AMD EPYC 7763
host. Its shorter number of repetitions is explicit; results are not pooled
with the initial Intel Xeon 8573C host or the earlier clock protocol.

| Profile | Verified inputs | Mean wall PAR2 | Peak RSS |
| --- | --- | --- | --- |
| Alternating control | 0/2 | 120.000 s | 74.2 MB |
| Alternating, no rephase | 1/2 | 84.379 s | 60.9 MB |
| Focused-first portfolio | 0/2 | 120.000 s | 83.1 MB |
| Portfolio, no rephase | 1/2 | 84.734 s | 83.1 MB |

Both conclusive answers are independently checked SAT models for the CSP input.
No-rephase takes 48.757 seconds wall / 48.691 CPU, and portfolio-no-rephase takes
49.469 wall / 49.382 CPU, using both attempts. One repetition cannot establish
a stable speed ratio. All planning runs and both rephase-enabled CSP runs are
UNKNOWN; there are zero errors and no completed UNSAT proofs in this screen.
Model validation takes about 0.049 seconds per answer, outside search time.

Recorded launch times are 1.7–2.5 ms. Thus launch accounting is a real harness
boundary fix, but it does not explain the much larger initial overrun by itself.
The new UNKNOWN runs still show up to 0.424 seconds of deadline observation and
kill/reap overhead. A configured external deadline is not a hard real-time
completion guarantee; no late conclusive answer is accepted. Both attempts share
one process CPU/address-space ceiling, and no final-attempt-only budget is used.

Decision: retain the existing portfolio as experimental opt-in, with no default
promotion. It adds no solve here and pays fallback cost on the solved CSP. This
bounded development comparison does not resolve the planning performance gap.

The [local two-second smoke](benchmark_results/portfolio-deadline-smoke-20260908.json)
exercises ordinary and portfolio search twice each. All four stop with UNKNOWN,
with observed elapsed 2.004–2.006 seconds including cleanup and measured launch
time below 2.1 ms. This is a deadline check, not a search-performance comparison.

An [independent CI test-harness failure](benchmark_results/cpu-limit-ci-fuse-20260908.json)
exposed a five-second wall fuse in the one-CPU-second resource test. The assertion
did not print the result row, so wall-fuse exhaustion is an interpretation of its
elapsed time rather than a recorded signal diagnosis. The test now permits 60
wall seconds while still requiring actual CPU-limit termination and UNKNOWN;
future failures include the full row. This changes no benchmark budget. CPU
signal telemetry also uses the actual child exit status, so a late reap cannot
hide SIGXCPU behind a simultaneous wall-limit indication.
