# Larger stateful and certificate assurance

`make soak-test` links an external client against the opaque shared API. Each
instance has 256 variables and a chain of parity constraints with exactly four
models. The client constructs those four models independently, filters them for
permanent added clauses and per-call assumptions, and checks every solver result
and every returned SAT assignment. It alternates reuse/rebuild modes, adds input
between queries and repeatedly constructs/releases instances.

The local release and ASan/UBSan runs each pass 128 instances / 16,384 queries:
9,062 SAT and 7,322 UNSAT. This broadens stateful histories beyond the small
truth-table fuzzer while retaining an independent exact oracle. It does not
represent hard industrial incremental search: the four-model structure is chosen
for independently predictable results. Existing long-search tests exercise deeper
conflict/reduction/GC behavior separately.

`BSAT_SOAK_CYCLES` scales the run up to 100,000 instances. Linux CI runs the soak
under ASan/UBSan with leak detection; scheduled/manual runs use 2,048 instances
(262,144 queries). Local macOS ASan success is not a claim of Linux leak-test
completion. The existing coverage fuzzing workflow remains in place.

Conditional proof support and wrong-context rejection are documented in
`CONDITIONAL_CERTIFICATES.md`. The benchmark and verified-checker reports now
separate solver and verification costs. Process-group timeout regressions verify
that a descendant heartbeat stops after timeout. Independent checker acceptance
remains mandatory; UNKNOWN and unchecked UNSAT do not count as verified answers.

The frozen industrial coverage run selects the five smallest local hardware
inputs by clause count, with a 60-second solver limit and one run each. It is a
size-selected development check, not a representative or held-out benchmark.

| Input prefix | Variables | Clauses | Result | Solver wall | Validation wall |
| --- | ---: | ---: | --- | ---: | ---: |
| 51482a5b | 51,108 | 152,338 | UNKNOWN | 60.007 s | — |
| 70da0b78 | 68,100 | 191,642 | verified UNSAT | 0.620 s | 1.522 s |
| 7bdf3e54 | 160,903 | 399,948 | verified UNSAT | 1.441 s | 4.356 s |
| 96dea345 | 161,065 | 400,494 | verified UNSAT | 1.389 s | 3.757 s |
| 546f8e06 | 147,004 | 467,279 | verified SAT | 19.058 s | 0.311 s |

All three UNSAT certificates pass DRAT-to-LRAT conversion and cake_lpr against
snapshotted original CNFs. The SAT model passes the independent original-input
checker. No errors occur. This adds two UNSAT input instances beyond the previously
recorded hardware certificate, within the same family. Peak solver RSS is 199 MB.
Checker timing can exceed solver timing; callers need an end-to-end budget.
Policies, run records and compact certificate receipts are retained under
`benchmark_results/assurance-*-20260908.json`; full certificates remain at the
local artifact paths named by the receipts.
