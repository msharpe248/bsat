# Probe permanent input revisions once

Public handles using `BSAT_REUSE_LEARNTS` now attempt optional failed-literal
probing once per permanent input revision. Changing assumptions alone does not
repeat the pass. Adding input, or taking the conservative rebuild path, enables
it again. The core option is `probe_on_change`; ordinary CLI defaults are
unchanged. Certificate handles and IPASIR already disable probing.

This is a heuristic work policy, not a claim that learned clauses cannot make
later probing useful. Required root propagation and CDCL still run every query.
The input log's monotonically growing size identifies additions, including empty
clauses. Cancelled queries rebuild before reuse. A bounded probing attempt also
counts as an attempt; it does not restart its optional allowance on each query.
New `Probing calls` and `Probing work` core statistics expose the actual work.

Two fresh serial release repetitions before/after, with exact application
oracles, show:

| Reused workload | Before wall seconds | After wall seconds | Maximum owned capacity before / after |
|---|---:|---:|---:|
| Configuration, 512 queries | 1.663–1.768 | 0.302–0.445 | 5.414 / 4.379 MB |
| BMC, 66 queries | 0.107–0.108 | 0.081–0.088 | 2.521 / 2.443 MB |

Configuration has zero UNKNOWNs; BMC has only its intentional cancellation.
Rebuilding configuration remains 1.13–1.31 seconds in the candidate campaign.
Configuration conflicts fall from 44,235 to 10,170; BMC conflicts rise from 7,676
to 8,749 despite lower wall time. These are generated histories on the local
desktop, not representative customer trace or general throughput claims.

Reports: `benchmark_results/probe-revision-{before,after}-20260908*`. A direct
regression checks the initial probe, assumption-only skip, and re-probe after a
permanent addition. Existing exhaustive incremental histories now exercise the
policy alongside chronology, variable queues and equivalence/congruence modes.

Validation: all release and ASan/UBSan unit suites and embedding clients pass;
51 retained/rebuilt debug certificates pass independent conversion/checking and
wrong-context rejection. Public API fuzzing completes 50,417 cases in 32 seconds
without a finding. Certificate mode's disabled-probing contract is unchanged.
