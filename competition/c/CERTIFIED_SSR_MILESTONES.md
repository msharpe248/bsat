# Certified root self-subsumption experiment — 2026-09-09

Baseline runtime c6b4ac8, workspace dae80ca. Previous campaign CI is now fully
green, including macOS sanitizers. Commit/push each completed milestone.

Implement one bounded self-subsuming-resolution pass before temporary assumptions.
For source (~p OR B) and target (p OR B OR C), derive (B OR C), journal the RUP
addition before replacing the target. Keep original input and variable namespace
unchanged. Initial scope: live original targets of size 3–16 with no assigned
variables, witnesses of size 2–4 visible in the opposite-pivot watch list, one
literal removed per target visit, at most the existing 1,000,000 preprocessing
work allowance, rotating target cursor, only after permanent input changes.
Partial/incomplete indexing is acceptable; this is not a fixpoint simplifier.
Compile-time BSAT_CERTIFIED_SSR gate, certified handles only. No new public flag.

1. Require exhaustive signed clause-pair truth-table equivalence, immutable input,
   independent certificates, no assumption leakage, budgets/cancellation, quota
   and allocation failure, checkpoint and later additions, all C release/sanitizer
   tests. Preserve reasons/watches/arena traversal and reject on any mismatch.
2. Serial paired target histories: cal3 depths 0,1,2,4,8,16; cal100 depths 0,2,4;
   flags 3, 60 query CPU seconds, retained CaDiCaL same CPU/90 wall, fresh Kissat
   90 wall and independent checking 600 seconds/stage, 2048/512 MiB heap/stack.
   Compare baseline/candidate/candidate/baseline. Include solving, export, checker
   cost, proof growth and owned capacity. Require target benefit; reject any checked
   loss or >5% combined solve+BSAT-check CPU regression. No second parameter grid.
3. If target passes, confirm original/expanded/fresh circuit histories with their
   frozen 60/10/10 query CPU budgets and no checked loss or >5% CPU PAR2 regression
   per corpus. Use Linux confirmation and complete worker/checker memory acceptance
   before promotion. If target fails, archive patch/results and restore runtime.
   Still finish a bounded broader validation screen and report measured resources.

Status: prototype implemented; 67 release/sanitizer tests, 352 exhaustive signed
pairs, 77 independently checked query certificates per build, 202 focused SSR
allocation cutoffs per build and 3,485 total allocation failures pass. Four checker
and five acceptance-summary regressions pass. Explicit arena compaction after every exhaustive clause-pair
replacement also passes release and ASan/UBSan. The local ABBA screen checks all
36 queries per build, finds zero strengthenings on both targets and a 1.32% lower
combined candidate CPU cost, below the required benefit threshold. Linux broader
validation and isolated memory measurements are still running. This is a capability
experiment, not a promise of speedup or production deployment certification.
