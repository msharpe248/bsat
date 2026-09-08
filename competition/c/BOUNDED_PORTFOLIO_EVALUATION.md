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
A confirmatory run with the corrected protocol is pending. The shorter number
of repetitions is explicit and the results will not be pooled across protocols.
