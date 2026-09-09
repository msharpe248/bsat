# Focused cal3 implementation work — 2026-09-08

Freeze the investigation before candidate edits. Compare unchanged BSAT, pinned
CaDiCaL and Kissat on the exact same query on Linux and macOS: 60 process CPU
seconds, 90 wall seconds, two serial repetitions, checked conclusive answers.
This measures a host/OS/compiler combination, not a CPU-only causal experiment.

First compare CaDiCaL plain against no on-the-fly self-subsumption and no reason
side-literal bumping. Inspect conflict traces/counters and choose one concrete
implementation experiment. Prior quality/glue reason rewards are different from
side-literal bumping; their rejected results remain relevant warnings.

Keep a byte-pinned unchanged baseline. A candidate must pass focused semantic
regressions, release/sanitizer tests and independent model/proof validation before
performance evaluation. Require repeated improvement on cal3, then no solved-case
loss and no >5% aggregate CPU PAR-2 regression on a frozen confirmation corpus.
Archive and remove an unsuccessful prototype; do not retain another unsupported
runtime flag. Document the measured outcome and commit/push each milestone.


## Completed outcomes

- [Matched-host comparison](CAL3_MATCHED_HOSTS.md): two Linux campaigns and a
  fresh local comparison. Reference solvers finish quickly on both; BSAT remains
  UNKNOWN on Linux at 60 CPU seconds. At 10,000 conflicts BSAT's paths differ
  across hosts while reference proof prefixes match. Hardware alone is not an
  adequate explanation; the first divergent event is not yet identified.
- [Implementation experiment](CAL3_REASON_SIDE_EXPERIMENT.md): bounded reason-side
  VSIDS reward, 66 release/sanitizer C executables per build, 6,532 independent
  validation solves, baseline-preserving diagnostic control and candidate traces.
  Both timed candidate runs regress to UNKNOWN versus checked baseline proofs.
  Rejected, archived and removed; no production speed improvement claimed.
- Source/binary restoration verified. Current docs link these outcomes. Normal
  production test inventory remains 65 C executables. Deployment certification
  and the broader cal3 search gap remain unresolved.
