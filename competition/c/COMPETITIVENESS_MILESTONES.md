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
