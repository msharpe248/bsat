# Competitiveness and complete-cost campaign — 2026-09-09

Baseline runtime: c6b4ac84c94e231983886828d3100495a49545c6. Its correctness,
independent integration and fuzz CI all pass. Commit and push each milestone.

1. Freeze four previously unmeasured circuit files in fresh_competitiveness_aig.json
   before tuning: protocol, cal162, arithmetic, picorv32. These share an upstream
   collection with development data; related families are not independent distributions.
   Run two serial repetitions, flags 3, depths 0,2,4,8, equal 10-second query CPU
   limits for retained BSAT/CaDiCaL, 90-second reference wall fuses, 600 seconds per
   independent checker stage with 2048/512 MB heap/stack. Fresh Kissat validates
   separately. Record input/tool hashes, checked solves, CPU PAR2, query and full
   validation cost. Linux hosted baseline is provisional, not a target deployment.
2. Diagnose cal3 depth 16 and cal100 depth 4 on existing development histories.
   Read CaDiCaL's public statistics outside timing, compare exact query deltas and
   BSAT work; distinguish search count from per-operation cost. Preserve reference
   defaults and validate the diagnostic binding against the original binding.
   Freeze one bounded candidate only after this evidence, rejecting checked losses
   or >5% aggregate CPU PAR2 regression on original and fresh confirmation sets.
3. Measure certificate size, conversion/check CPU and wall time. Test one bounded
   proof-cost hypothesis, preserving independently checked exact-query acceptance.
   Include preprocessing/compaction cost in any claimed end-to-end gain. Never
   silently weaken checking or apply permanent query units to a retained history.
4. Run updated Linux worker/checker cgroup and private-storage acceptance, including
   the hard cal3 transaction, all five existing faults and checked replay. Freeze
   provisional envelope before execution: 4 GiB memory, 1 GiB temporary storage,
   180 wall / 120 aggregate CPU seconds, 60 query CPU seconds, 256 MiB journal,
   60 seconds per checker stage with 2048/512 MB heap/stack. Keep a separate tiny
   query-budget UNKNOWN/recovery case. Deployment traces and SLOs are requested;
   absent those, report public-workload acceptance and leave deployment certification open.

Status: scope and untouched input selection frozen in 3060825. Reference extension
passes 128 result/model/core parity queries; summary passes six acceptance tests.
Fresh hosted Linux run 34388664761 is running. Acceptance harness now takes explicit
resource settings and requires two checked hard transactions in addition to normal,
UNKNOWN and failure/replay cases. Full Linux validation is the next milestone.

Proof-cost hypothesis (frozen before timing): DRAT-trim's core/literal trimming
without LRAT output can shrink clauses as well as discard unused lemmas. Test
`-l compact.drat -C`, then the ordinary independent LRAT/CakeML chain, against
direct LRAT conversion in direct/trim/trim/direct order on cal3 and cal100 exports.
Count the extra trimming pass. Promote only an optional artifact path if checked
proof bytes fall and complete verification CPU improves on both targets; do not
change solve policy or claim that smaller artifacts alone reduce acceptance time.

Search hypothesis (frozen before building): cache a failed recursive minimization
subtree only within the current source-literal candidate. Current cal3 performs
308,694,275 reason inspections and cal100 19,167,222; both default recursion paths
restore failed nodes to unseen and may revisit them through the same dependency
DAG. A cached failure only keeps a literal; depth/context-dependent failures may
conservatively miss a later removal. Clear all marks before the next source
literal and query. Test in certified policy only, with an experimental build macro.
Require full release/sanitizer tests and independently checked target histories.
Screen cal3 and cal100 twice at existing depths/budgets before broader confirmation;
reject a target checked loss or >5% combined target solve CPU regression. A failed
screen is archived and the prototype removed; no alternate cache tuning grid.


First hard acceptance run 34388896901 checks both hard repetitions (~95 seconds,
~2.67 GiB peak) but fails in the tiny-budget UNKNOWN checkpoint. Checkpoint rebuilds
honor the current CPU budget; the harness incorrectly left the one-microsecond
injection in place. Restore the separately configured ordinary budget before
checkpoint/replay. The focused real-library regression passes UNKNOWN → restored
budget → zero-journal checkpoint → independently checked SAT. Archive the failed
run; retry the same frozen outer envelope, with no raised resource limits.


Fresh Linux baseline complete: 64/64 BSAT versus 62/64 CaDiCaL within equal query
CPU budgets; strict summary and independent checking pass. See FRESH_COMPETITIVENESS.md.
Retained cal3/cal100 work diagnosis complete; see RETAINED_SEARCH_WORK.md. The
failure-cache prototype passes 66 release/sanitizer executables, focused entailment
and cleanup cases, and 53 independent certificates per build; target timing pending.


Hard Linux acceptance complete in 34389473487 at the unchanged frozen envelope;
see HARD_TRANSACTION_ACCEPTANCE.md. All five failures and checked replay pass.
The failure-cache experiment is rejected: 36 checked candidate queries preserve
all conflicts, proof streams and minimization inspections. Runtime source restored.
Separate per-solver embedding-process RSS follow-up runs in 34390131562; it excludes
checker processes and identifies Python/encoding overhead explicitly.


Proof-cost experiment complete and rejected for automatic acceptance. All eight
paths independently verify; saved artifacts shrink 87.7% / 59.1%, but total child
CPU changes +0.62% / +31.18%. See PROOF_COMPACTION_EXPERIMENT.md. Direct verification
remains unchanged. The new diagnostics and acceptance fix are the implemented
improvements in this batch; neither rejected experiment changes solver defaults.


Isolated memory follow-up complete (34390131562): one solver per embedding process,
all BSAT queries agree with pinned checked contexts; PicoRV32 peak RSS is 535.55
MiB for BSAT and 676.24 MiB for CaDiCaL. These include Python/encoding/model checks,
exclude external certificate checkers, and are not native-only memory figures.
