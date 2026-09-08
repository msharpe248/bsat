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

Two pinned development inputs (CSP and planning), two repetitions, seed
2026090843, serial execution pinned to one available Linux CPU. All conclusive
answers require original-model validation or DRAT-to-LRAT conversion followed by
CakeML's verified checker. Proof checking has the same separate 600-second wall
allowance for every profile. Validation time is reported separately and included
in end-to-end elapsed time; it is not charged as solver search time. Address-space
and file ceilings cover the solver process, not aggregate checker disk/RSS.

This is a targeted development comparison, not a fresh holdout or promotion test.
The macOS resource harness passes six applicable tests, including an actual CPU
limit killing a child that printed a fake early SAT answer; Linux address-space
enforcement is checked by the remote workflow. Results pending.
