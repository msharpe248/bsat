# Target refresh and diversification experiment

The all-time longest-trail target is a plausible source of stale hints. Existing
work was reviewed first: Kissat's `src/rephase.c` resets assignment records and
uses several phase sources with stable-mode gating and a growing schedule
(https://raw.githubusercontent.com/arminbiere/kissat/master/src/rephase.c).
Our experiment isolates two small changes; it does not reproduce that schedule.

The archived patch against c81a370 adds `--rephase-refresh` (clear the target and
its record after reapplying hints) and `--rephase-diversify` (also invert saved
polarities every third rephase). Neither changes assignments, reasons or clauses.
Default behavior is preserved. Clearing target entries prevents stable decisions
from immediately overriding diversification with stale hints. The record is
bounded by rephase events, not by wall time across incremental queries.

Correctness: all release and ASan/UBSan unit suites pass; the stateful exact-oracle
test exercises both policies with a one-conflict interval. CLI validation passes
4,982 truth-table, original-model and RUP checks. Each build passes 16 targeted
policy/interval/text-or-binary/SAT-or-UNSAT cases, with UNSAT certificates converted
to LRAT and independently checked by cake_lpr. Updated core fuzzing runs 10,412
executions in 31 seconds without a finding. These are finite tests, not a proof
of implementation soundness.

Performance: 16 serial, uninstrumented macOS ARM64 trials; known CSP and planning
inputs, four profiles, two repetitions, uniform 12-second external wall deadlines.
No overlapping local builds/tests/fuzzing. Control, refresh and diversify each
solve 0/4. No-rephase solves CSP twice (6.648 and 7.780 seconds), 2/4 overall.
All planning trials time out. Two SAT models are independently checked; 14 runs
are UNKNOWN and there are no errors. Refresh's maximum RSS is 172.97 MB versus
control's 143.92 MB, with no added solves. A short development screen cannot
establish universal inferiority, but gives no reason to ship these new switches.

Decision: archive the prototype rather than expand the production option surface.
Production defaults and existing phase behavior remain unchanged. The broader
holdout already rejects globally disabling rephasing. The robust phase-selection
problem remains open; this milestone eliminates two simple proposed remedies.

Reproduction: apply `benchmark_results/phase-refresh-experimental-20260908.patch`
to c81a370 (the standalone runners in this milestone are also required), build,
run `tests/check_phase_certificates.py`, then `tests/benchmark_search_policy.py
--suite phase` using the frozen input paths and flags in the report. That suite
requires the prototype binary. `--suite propagation` uses production options.
Raw policy, binary/input hashes, all trials, and certificate identities are in
`benchmark_results/phase-refresh-*20260908*`. No prototype binary is committed.
