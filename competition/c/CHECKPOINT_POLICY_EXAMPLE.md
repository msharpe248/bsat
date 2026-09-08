# Public C checkpoint policy example

Build `make -C competition/c checkpoint-example`; run
`competition/c/bin/checkpoint_service_release 1024`. The example uses only
`bsat.h`, retains learning and certificates, and runs four changing guarded
pigeonhole regimes with permanent clause additions. The optional second argument
`f` compares a fixed 8 KiB checkpoint watermark. Per-query JSON and a final summary
include results, journal bytes, reserved headroom, conflicts and CPU cost.

`examples/checkpoint_policy.h` is application policy, not a new solver ABI.
It tracks the largest observed journal increase, reserves the greater of 64 KiB
and twice that burst, and keeps that peak across checkpoints and regime changes.
For small quotas the floor is reduced to one quarter of quota. The default hard
quota is 1 MiB; `BSAT_EXAMPLE_QUOTA` accepts 4 KiB through 1 MiB. At 25% occupancy it considers
a checkpoint after 64 queries, but defers a previously expensive rebuild until
its CPU cost is at most 1% of accumulated query CPU. Approaching reserved
headroom overrides this cost preference. These constants are example choices,
not established workload-independent optima; learning loss is an additional cost
that this simple policy does not predict.

No observed-burst estimate bounds the next query. An unexpected burst may still
exhaust the journal and poison the handle. The example stops on an error; a real
service must retain permanent input and recreate the instance, with more quota
or a shorter query allowance. If twice an observed burst cannot fit the quota,
the policy explicitly reports that more capacity is needed. Arithmetic saturates
rather than wrapping. A cancelled checkpoint can be retried after cancellation
is cleared; policy state is updated only after successful checkpoint completion.
Consume or export the answer before checkpointing, which invalidates it.

`check_checkpoint_example.py` compares adaptive and fixed policies in alternating
order. Every query is checked against the guarded pigeonhole domain oracle;
eight sampled queries per run are exported and checked against an independently
constructed exact original formula plus assumptions. SAT models are checked,
and UNSAT proofs pass DRAT-to-LRAT conversion and the verified CakeML checker.
Optional exports use `BSAT_EXAMPLE_EXPORT_DIR` and require new file paths.
Only query/checkpoint API CPU is included in reported cost; output, additions
and independent validation are outside those intervals.

The [release comparison](benchmark_results/checkpoint-policy-example-20260908.json)
passes 8,192 queries (two quotas, two policies, two repetitions), with 64 sampled
exact-context exports independently checked: 32 SAT models and 32 UNSAT proofs.
All queries agree with the domain oracle; no hard quota is exceeded.

| Hard quota | Adaptive checkpoints/session | Fixed checkpoints/session | Adaptive / fixed median solve+checkpoint CPU |
| --- | --- | --- | --- |
| 64 KiB | 1 | 128 | 0.0101 / 0.4330 s |
| 1 MiB | 0 | 128 | 0.0065 / 0.4326 s |

This reproduces the cost of an overly small fixed watermark. It does not show
an advantage over the previously tuned 64 KiB watermark, predict arbitrary
workload bursts, or establish a service latency guarantee. Input sizes are small;
the earlier SERVICE_SESSION_TAILS.md report provides larger-session context.
Adaptive peak journal bytes are at most 22,815; fixed peaks at 18,583. The example
spends more journal capacity to retain useful learning.

Release and ASan/UBSan policy tests cover cost deferral, headroom precedence,
changed-regime bursts and saturating arithmetic. The
[debug lifecycle smoke](benchmark_results/checkpoint-policy-debug-20260908.json)
passes another 256 queries and 32 independently checked exports. It runs alongside
other correctness work and is not a performance measurement.
