# Larger-circuit checker resource fix — 2026-09-08

Expanded validation found a real harness limit: the fixed CakeML heap/stack
settings were insufficient for a larger valid UNSAT certificate. The exact
730,151-variable / 1,960,297-clause DSP query has a 104 MB Kissat DRAT proof.
DRAT-to-LRAT conversion succeeds, producing a roughly 125 MiB LRAT file.

| CakeML heap / stack | Result on the same CNF and LRAT |
|---|---|
| 512 / 128 MB | Heap space exhausted |
| 2,048 / 128 MB | Stack space exhausted |
| 2,048 / 512 MB | Verified UNSAT |

The successful checker run uses about 2.28 GB peak RSS and four CPU seconds.
Conversion takes about 39 seconds separately. These costs are outside solver
query timing and must be included in a deployment's acceptance resource budget.
BSAT and retained CaDiCaL timed out on this query; the checked fresh reference
proof does not convert either timeout into a solver success.

`tests/verified_check.py` now accepts `--checker-heap-mb` and
`--checker-stack-mb`, with optional `BSAT_CAKE_HEAP_MB` / `BSAT_CAKE_STACK_MB`
CLI defaults. Its Python `verify` function accepts explicit `heap_mb` and
`stack_mb`. Existing defaults remain 512/128; limits must be positive integers.
Reports record both settings. No automatic unbounded retry or relaxed acceptance
rule is introduced.

`industrial_histories.py` exposes the same settings, records complete checker
stage diagnostics before rejecting an answer, and supports `--retain-unverified`
for failed CNF/proof/LRAT/log artifacts. Previously its temporary directory was
deleted after an assertion, obscuring the reason for failure. A simulated
checker failure confirms rejection, persisted diagnostics and retained files.
The checker regression suite verifies resource-argument forwarding, rejects
invalid limits, and still rejects corrupt or incorrectly bound certificates.

The initial campaign stopped after 54 validated queries and is archived as
`expanded-certified-probing-failed-512mb-20260908.json`, explicitly incomplete.
`expanded-checker-resource-diagnosis-20260908.json` pins the successful conversion
and all three checker outcomes on identical input/LRAT hashes. The full timing
campaign is rerun from its start with explicitly fixed 2,048/512 MB checker
resources; query budgets, inputs, depths and mode order are unchanged.
