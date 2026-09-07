# Local-search budgets and fresh deadline checkpoints

## Deadline defect and fix

While evaluating longer local-search walks, a direct budget regression exposed
nested throttling. The walk called `solver_budget_exhausted` every 256 flips, but
that function could reuse its cached CPU reading for 128 polls. Walk flips do not
advance the CDCL work counters that otherwise force clock reads. Consequently,
a fresh CPU reading could be delayed by roughly 32,000 flips.

`solver_budget_exhausted_now` now requests a fresh CPU reading through the same
error, interruption and work-limit handling. Local search uses it at its existing
256-flip checkpoints, starting before the first flip. Other solver callers keep
the existing amortized checker. With no CPU limit, the new path does not read the
clock. This changes deadline responsiveness, not flip selection or budget sizes.

The regression warms the cached clock, moves the solver's start time two seconds
into the past with a one-second limit, and starts an unsatisfiable walk. It fails
on `94b625a` because flips occur before expiry is noticed; the fixed version stops
before the first flip with exactly one additional clock read. It also checks:

- Exact flip counts at budgets 0, 1, 255, 256, 257, 1,000, 10,000 and 100,000.
- No clock reads when the time limit is disabled.
- Cancellation before the first flip and a subsequent successful resumption.
- Exactly 391 fresh readings during a 100,000-flip walk with a distant deadline.

All 25 C suites pass in release and ASan/UBSan builds. Independent validation
passes 416 local-search model/proof checks per build. Both builds pass 36 short-
deadline cases; the maximum observed CPU overrun rounds to 0.000 seconds in each.
This is not a worst-case wall-time or CPU bound: initialization and individual
flips may still scan many literals, and they are not interrupted internally by
this change. Returning a verified result and honoring limits remain distinct
requirements.

## Preliminary flip-budget evaluation

Before changing the deadline checker, the current `94b625a` binary was evaluated
with 1,000, 10,000 and 100,000 flips per local-search attempt. All profiles used
VMTF, trail reuse, local search every 5,000 conflicts, ten solving CPU seconds and
fifteen wall seconds. Four recent development targets were repeated twice. SAT
models and UNSAT proofs were independently verified outside timing.

| Flip budget | Verified runs | Mean wall PAR-2 |
| ---: | ---: | ---: |
| 1,000 | 8/8 | 1.8929 s |
| 10,000 | 8/8 | 1.6002 s |
| 100,000 | 8/8 | 1.9297 s |

On battleship `ed6d842f`, 10,000 and 100,000 flips find a model in the first walk,
in 0.125–0.134 process CPU seconds; 1,000 flips needs seven walk attempts and
0.896–0.933 CPU seconds. On belpyramid `5831f356`, all seven walks fail and CDCL
proves UNSAT: 1,000 flips takes 2.946–3.011 CPU seconds, 10,000 takes
3.182–3.215, and 100,000 takes 4.996–5.049. Larger budgets can therefore help one
input while spending substantial unproductive time on another. See the
[complete preliminary comparison](benchmark_results/walk-budget-targeted-20260907.json).

This small selected sample does not justify changing the default flip budget or
enabling local search by default. The default remains 100,000 flips when local
search is explicitly enabled. The 10,000-flip profile merits a broader evaluation
after fixing the deadline behavior; that evaluation is not established here.

## Cost of the deadline fix

The same four targets are compared before and after the fix with the existing
100,000-flip budget, three repetitions per implementation, and the same options
and limits. Timing is serial and separate from compilation and validation.
[Raw results](benchmark_results/walk-deadline-hybrid-20260907.json) record commands,
input hashes, executable hashes, timings and verification outcomes.

| Target | Cached median process CPU | Fresh median process CPU |
| --- | ---: | ---: |
| hamiltonian `a497d784` | 0.701479 s | 0.705031 s |
| multiplier-circuits `90bec6dc` | 0.923603 s | 0.909238 s |
| belpyramid-puzzle `5831f356` | 4.033318 s | 4.179082 s |
| battleship `ed6d842f` | 0.122924 s | 0.108532 s |

Both versions certify 12/12 runs, with zero errors. All non-timing statistics
other than the intentionally changed clock-read count are identical across
paired runs. Mean wall PAR-2 is 1.9826 versus 1.8673 seconds, but this small
selected sample with noisy wall timing does not establish a general speedup.
Retain the fresh deadline check for the reproduced responsiveness defect; no
additional competition solves are claimed.
