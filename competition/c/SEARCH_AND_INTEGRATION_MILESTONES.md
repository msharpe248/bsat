# Search and integration batch

Authorized: evaluate combined phase/factoring policies on fresh inputs; test
bounded target refresh and diversification; measure planning propagation/search
bottlenecks; independent incremental differential histories and an upstream IPASIR
application; larger long-running service sessions. Commit and push each validated
milestone. Preserve unrelated `.zvec-grep/` state.

1. Combined holdout: complete; COMBINED_HOLDOUT.md. Combined defaults rejected.
2. Phase refresh: implementation prepared; validation and measurements pending.
3. Planning propagation: pending.
4. Differential histories and upstream client: harnesses prepared; execution pending.
5. Service session tails: harness prepared; execution pending.

Timed workloads run serially, without concurrent local builds, tests or fuzzing.
All conclusive benchmark answers require independent checks. A short holdout
screen can reject a policy but cannot alone justify production defaults. Report
UNKNOWN separately from errors and preserve negative or inconclusive results.
