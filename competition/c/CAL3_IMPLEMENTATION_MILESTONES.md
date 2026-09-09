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
