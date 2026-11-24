# Competitiveness and complete-cost campaign — 2026-09-09

Status: the scoped public-workload campaign is complete. All milestones were
committed and pushed. Runtime remains c6b4ac84c94e231983886828d3100495a49545c6;
neither evaluated optimization justified a solver/default change. The concrete
changes are reproducible benchmark/diagnostic tooling, explicit acceptance
controls, and corrected budget restoration before checkpoint recovery.

1. **Fresh competitiveness and memory baseline: complete.** Four files selected
   before tuning, two serial repetitions, depths 0,2,4,8, certified retained
   flags 3, ten query CPU seconds per retained solver. BSAT checks 64/64 versus
   CaDiCaL 62/64. Linux runs 34388664761 and 34390131562 pass. Isolated per-family
   embedding peak RSS adds actual memory measurements; Python/encoding/model work
   is included and external proof checking excluded. See FRESH_COMPETITIVENESS.md.
2. **Remaining search-work diagnosis and bounded experiment: complete.** Retained
   cal3/cal100 use far more conflicts and propagations than CaDiCaL. The public
   statistics extension passes 128 result/model/core parity queries. A single
   within-candidate failed-minimization cache passes 66 release/sanitizer tests,
   focused entailment/cleanup and 53 independent certificates per build. All 36
   target queries check, but every conflict count, proof and reason-inspection
   count stays unchanged. Patch archived, source restored, no tuning grid or
   unsupported promotion. See RETAINED_SEARCH_WORK.md.
3. **Proof-cost experiment: complete, rejected.** Exact exports compared in
   direct/trim/trim/direct order on cal3 and cal100. Trimming shrinks DRAT bytes
   87.7% / 59.1%; final total CPU including orchestration changes -0.03% / +32.63%.
   All eight final paths independently verify. The initial child-only accounting
   screen is also retained. Ordinary direct LRAT/CakeML acceptance stays in place.
   See PROOF_COMPACTION_EXPERIMENT.md.
4. **Provisional Linux deployment acceptance: complete.** Run 34389473487 passes
   two normal and two checked hard transactions, tiny-query UNKNOWN/recovery,
   and five failures with checked replay. Fresh cal3 takes 93.9–94.4 wall seconds,
   ~2.67 GiB peak cgroup memory and 648 MiB sampled temporary storage. The first
   run exposed a harness checkpoint attempted under the tiny query budget;
   restore the normal budget before rebuilding. The focused regression and rerun
   pass without relaxing the frozen outer envelope. See HARD_TRANSACTION_ACCEPTANCE.md.

## Frozen protocols and interpretation

Fresh selection seed 2026090952, sorted/seeded per-family pools, 30,000–180,000-byte
AIG files, hashes in tests/fixtures/fresh_competitiveness_aig.json. The exhausted
2019 opensource pool is replaced by its 2020 pool before solving. These are new
files from a reused upstream collection; they are now development evidence, not
future untouched holdouts. Within each query BSAT precedes CaDiCaL in both repeats.

Fresh reference wall fuses are 90 seconds; independent checking is 600 seconds per
stage with 2048/512 MiB heap/stack. UNKNOWN/late answers receive no timed credit;
all conclusive answers need exact-context witnesses. Fresh Kissat is a separate
validation reference, not a retained solver timing result. The summary's six
integrity tests cover incomplete/context-changed/contradictory/late/nonfinite data.

The search prototype was restricted to certified policy. Target checked losses
or >5% combined target CPU regression reject it; absence of any work reduction
already provides no reason to promote or expand its tuning. A beneficial candidate
would additionally require original/fresh confirmation without checked losses or
>5% CPU PAR2 regression. The proof candidate required complete CPU improvement on
both targets, counting its extra pass; it failed that requirement.

Hard acceptance envelope: 4 GiB cgroup memory, zero swap, 64 tasks, private 1 GiB
storage, 180 wall / 120 aggregate CPU seconds, 60 query CPU seconds, 256 MiB journal,
60 seconds per checker stage with 2048/512 MiB heap/stack. The supervisor and durable
source/results are outside. Storage polling is a sampled lower bound and deadlines
can overshoot; rejected attempts never become accepted answers.

## Remaining boundary

The implementation has workload-specific wins and substantial correctness evidence,
but still has large retained search and certified-simplification gaps. Actual
customer traces, the deployment host, latency/memory SLOs and durable parent/power-loss
recovery are unavailable and remain unvalidated. Hosted public-workload acceptance
is not deployment certification. The runtime c6b4ac8 correctness/integration/fuzz
CI is green; later campaign harness CI is recorded separately in the final audit.


Final audit: `benchmark_results/competitiveness-final-audit-20260909.json` records
all three successful manual campaigns, local checks and unchanged runtime source.
Campaign integration/fuzz pass; the extra correctness rerun 34388655578 still has
its macOS sanitizer job running at audit time. The same runtime's prior full CI is
already green. This pending rerun is not reported as a completed pass.
