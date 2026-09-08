# Certified gen23 diagnosis and optional probing — 2026-09-08

The ordinary one-million-work-unit failed-literal probing budget reduces median
query CPU by about half on the pinned gen23 depth-0-through-16 history. It cuts
search work and proof growth. It is now available as `BSAT_CERTIFIED_PROBING`,
requiring `BSAT_CERTIFICATES`, with optional `BSAT_REUSE_LEARNTS`.
Existing flags keep their defaults: confirmation includes slower individual
queries, so this is not a blanket speed improvement.

```c
bsat *s = bsat_create(BSAT_ABI_VERSION,
    BSAT_CERTIFICATES | BSAT_REUSE_LEARNTS | BSAT_CERTIFIED_PROBING);
```

## Diagnosis: search work versus certificate overhead

The initial three journal-on/off pairs use the same test-only library and
identical certified search options. Every query has identical status, original
CNF, assumptions and recorded search counters. Median aggregate query CPU falls
from 1.312 to 1.038 s without reuse, and 1.025 to 0.822 s with reuse when the
journal path is disabled. Individual timings vary substantially.

That intervention disables journal/facade state and changes query-buffer lifetime
and subsequent export activity; it does not isolate disk writing. We therefore
do not attribute the full approximately 20% difference to proof serialization.
The journal-off control cannot export certificates and is never a product mode;
its answers are checked through independent models and fresh reference proofs.

Separate intrusive accounting over all four original public modes finds that
direct proof emission occupies only 0.0045/2.257 and 0.0179/1.405 seconds in the
two certified histories (about 0.2% and 1.3%). Propagation occupies about 68%/67%.
These inclusive, instrumented intervals are not calibrated production timing
fractions, but do not support prioritizing a proof-writer micro-optimization.

At depth 16, the certified retained positive query performs 29.07 million
propagations versus 11.88 million with the ordinary noncertified options. The
certified rebuilding negative query performs 12.36 million versus 2.06 million.
The relevant default option difference is failed-literal probing, which the
certified facade had disabled. The earlier rejected experiment tried only
100,000 work units; the ordinary preprocessing budget is 1,000,000.

## Controlled candidate experiment

The test-only candidate enables probing and leaves the ordinary budget unchanged.
Three serial pairs alternate the order of control and candidate. Each history has
the same 12 queries at depths 0,1,2,4,8,16, under five thread-CPU seconds per query.
Every SAT model is checked against original input and independently simulated;
every certified UNSAT export is checked via DRAT-to-LRAT and cake_lpr.

| Certified mode | Control median query CPU | Probing median query CPU | Conflicts, control → probing | Peak journal, control → probing |
|---|---:|---:|---:|---:|
| Rebuild (flags 2 → 6) | 1.300 s | 0.634 s | 4,315 → 2,848 | 1.71 → 1.24 MB |
| Retain learning (flags 3 → 7) | 1.002 s | 0.486 s | 6,707 → 3,350 | 5.97 → 1.31 MB |

Every pair improves more than the predeclared 10% target threshold; median gains
are 51.2% and 51.5%. Retained-mode aggregate query CPU per conflict changes only
from about 149 to 145 microseconds. Most of that mode's gain comes from needing
roughly half as many conflicts. This ratio includes query preparation and is
not a standalone measurement of the inner conflict-analysis routine.

## Confirmation and limits

Before testing the candidate, confirmation was fixed to all four pinned circuits,
depths 0,1,2,4,8,12,20, and retained certified mode. Both configurations resolve
54/56 queries. Both time out on the two deeper positive cal3 queries; no solved
case is lost. Total CPU PAR-2 changes from 22.414 to 22.833 seconds, a 1.9%
regression inside the declared 5% aggregate limit. This is one confirmation
history per circuit, not evidence of universal superiority.

Individual regressions matter: the image-FIFO positive depth-20 query rises from
about 0.075 to 0.159 s, and gen23 positive depth 20 rises from 1.452 to 1.929 s.
The candidate wins strongly on the original target and gen23 depth 12, while
remaining workload-dependent. This motivates an explicit option with unchanged
defaults. It does not solve the cal3 gap.

## Soundness and integration boundary

Probing runs at root before temporary assumptions and emits only RUP-implied
units from permanent input. Those consequences remain valid after later clause
additions. Variables are preserved. With reuse, probing repeats on permanent
input changes; rebuilding handles repeat preprocessing on the fresh core.
Other certified transformations remain disabled.

The budget controls optional probing work with polling granularity, not a hard
wall limit; mandatory root propagation can continue beyond it. Journal quota,
write errors and cancellation keep their existing fail-closed behavior. The flag
is rejected without certification. It adds no exported function or ABI layout.

Validation covers 65 release/ASan/UBSan C executables, including 4,096 independent
oracle queries per build for certified controls/probing, cancellation, checkpoints,
failed cores and quota failure. Allocation sweeps cover 3,283 cutoffs across 22
profiles/formulas. Public flags 6/7 pass 128 growing differential queries with
checked certificates, 96 release real-circuit queries and 64 sanitizer real-circuit
queries. All 24 gen23 release queries match the experimental facade in input,
answer, search counters, capacity, journal size and proof hashes. The updated public
API fuzzer completes 39,321 runs in 31 seconds without a finding. Installed-header
C++ and frozen ABI-v1 C consumers remain part of the package checks.

## Reproduction and artifacts

`industrial_histories.py` now accepts circuit, flag, CPU-budget and diagnostic
profile selectors. Test facades are built with `make query-diagnostics`; they are
separate from installed libraries. `analyze_query_pairs.py` requires exact search
trace parity for journal attribution. `analyze_probe_confirmation.py` evaluates
the target and confirmation gates and records source-report hashes.

Reports under `benchmark_results/` include:

- `gen23-journal-attribution-20260908.json` and its six source histories;
- `gen23-phase-attribution-20260908.json`;
- `gen23-probe-confirmation-policy-20260908.json`, frozen before candidate runs;
- `gen23-probe-acceptance-20260908.json`, six target runs and two confirmation histories;
- `certified-probing-public-stateful-20260908.json`, `certified-probing-public-release-20260908.json`,
  `certified-probing-public-debug-20260908.json` and `certified-probing-public-parity-20260908.json`.

The timing comparisons use the test-only facade before public exposure of the
same option. Production-library verification and parity evidence are recorded
separately; diagnostic profiles are not installed API.
