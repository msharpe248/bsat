# Application-shaped incremental histories

Two deterministic generated workloads exercise the public opaque API with
sustained variable/clause growth and changing assumptions. They model application
patterns; they are not captured customer traces or general industrial coverage.

* Bounded model checking: a 16-bit counter may increment by zero or one each
  step. The history grows to 128 steps / 4,112 variables, querying zero, middle,
  maximum reachable and first unreachable values at each eighth step. The exact
  reachable set is the interval from zero through the depth. Every SAT model is
  checked by independently simulating its chosen enables and state transitions.
  The 66 queries include an explicit cancellation and retry.
* Configuration: 512 stages each choose exactly one of eight variants, with
  adjacent compatibility constraints and permanent exclusions added over time.
  It grows to 4,096 variables and executes 512 changing-assumption queries. An
  independent dynamic program over eight states decides each exact query;
  SAT models are checked for uniqueness, compatibility and exclusions.

Each query has a 2,000-conflict / 0.2-second CPU budget. UNKNOWN is allowed and
recorded, never counted as a correct SAT/UNSAT answer. The benchmark freezes
executable/library hashes and runs serially with no concurrent local validation
loads. CI runs the histories in release and sanitizer builds.

The first comparison exposed a maintenance bug: reduction used the conflict
count that resets at every reused query. Hundreds of individually short queries
could retain thousands of learned clauses without ever triggering cleanup.
The fix carries cumulative reduction progress with the retained database, for
both fixed and growing schedules. Fresh/rebuilt databases retain their original
schedule. Limits and public statistics still reset per query. Focused tests use
one-conflict slices to force repeated cleanup, then finish the known-UNSAT input.

The original measurements are retained in `application-histories-20260908.json`:
BMC reuse takes 0.114–0.116 seconds with just the deliberate cancellation UNKNOWN;
rebuild takes 0.537–0.544 seconds with 24 UNKNOWN queries. Configuration reuse
uses 19,754 conflicts versus 42,432 for rebuild, but takes 1.818–1.966 seconds
versus 1.115–1.121 and retains about 6.47 MB versus 3.54 MB of owned capacity.
Fewer conflicts alone therefore did not imply lower total cost.

Actual large-query certificate exports use the same growing histories with
certification enabled and retained learning. They replay the real session
journal without re-solving. Independent SAT checks and the DRAT-to-LRAT/CakeML
chain validate the sampled queries in both builds, including a 4,112-variable
UNSAT counter query and 4,096-variable conditional configuration failures.
See `application-certificates-*-20260908.json` and
`tests/check_application_certificates.py`.

After preserving reduction progress, configuration reuse takes 1.607–1.654
seconds and retains 5.41 MB, down from 6.47 MB. It now uses 44,235 conflicts:
cleanup changes search, but lowers total cost despite the higher conflict count.
Rebuild remains faster on this workload at 1.110–1.113 seconds / 3.54 MB, so reuse
stays opt-in. BMC reuse takes 0.105–0.106 seconds and completes every ordinary
query; rebuild takes 0.531–0.533 seconds with 24 UNKNOWN queries. The only reuse
UNKNOWN is the intentional cancellation. Records are in
`application-maintenance-20260908.json`; they do not establish a universal speedup.

Both release and ASan/UBSan pass the C suites, 3,105 allocation-failure cases,
shared ABI concurrency/soak checks, 51 smaller retained certificate checks and
12 actual large-history exports (five UNSAT and seven SAT) per build. A public
API sanitizer fuzzer completes 208,284 executions in 121 seconds without a
finding after the schedule change. The new public-facade fuzzer joins both push
and nightly CI alongside the core and parser fuzzers.

Eight fixed-work ordinary single-query comparisons preserve result, selected
search counters and binary-proof prefix bytes exactly against the pre-batch
executable; see `maintenance-single-query-trace-20260908.json`. UNKNOWN prefixes
serve only as trace guards, never as UNSAT evidence.
