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

Status: complete; prototype rejected for lack of benefit and archived.
Validation covers 67 release/sanitizer tests, 352 exhaustive signed
pairs, 77 independently checked query certificates per build, 202 focused SSR
allocation cutoffs per build and 3,485 total allocation failures pass. Four checker
and five acceptance-summary regressions pass. Explicit arena compaction after
every exhaustive clause-pair
replacement also passes release and ASan/UBSan. The local ABBA screen checks all
36 queries per build, finds zero strengthenings on both targets and a 1.32% lower
combined candidate CPU cost, below the required benefit threshold. Linux validation
and isolated memory measurements also completed successfully, as detailed below.
This is a capability experiment, not production deployment certification.


The runtime has been restored to its pre-experiment implementation. The two-file
[prototype patch](benchmark_results/certified-ssr-prototype-20260909.patch) preserves
the implementation; the manual `c-certified-ssr.yml` workflow applies it before
building the experimental configurations. Conditional diagnostic and validation
hooks remain for reproduction, and the future-query public API regression runs
in normal builds. `BSAT_CERTIFIED_SSR` is not a supported production build option.
The local raw reports and summary are in
[ssr-targets-mac-20260909](benchmark_results/ssr-targets-mac-20260909/summary.json).
Zero strengthening on these prepared circuits is evidence against this bounded
watch-indexed implementation on these targets, not against SSR in general.

The first local candidate history records 12,262 eligible target visits and
1,172,364 charged SSR work units for cal3; cal100 records 10,948 target visits
and 3,000,000 work units (the full optional allowance at each of three depths).
Neither history strengthens a clause. Every exported query proof hash matches
the corresponding first baseline history. The restored production `src/` and
`include/` match c6b4ac8 exactly and all 67 C tests pass again in release and
ASan/UBSan builds after restoration.


## Linux confirmation and final decision

[Run 34422377127](https://github.com/msharpe248/bsat/actions/runs/34422377127)
succeeded at prototype revision 253075c. Both release and ASan/UBSan builds pass
all 67 C executables, 77 independent certificates and 3,485 allocation cutoffs
(including 202 SSR replacement cutoffs). The later explicit compaction assertion
was validated on macOS in both modes; it was not yet in this Linux revision.

| Host | Checked queries per build, across two repeats | Baseline solve/export/check CPU | Candidate CPU | Change | Strengthenings |
|---|---:|---:|---:|---:|---:|
| macOS ARM64 | 36/36 | 117.34 s | 115.79 s | -1.32% | 0 |
| Linux x86-64 | 36/36 | 191.43 s | 189.95 s | -0.77% | 0 |

Both comparisons pass the nonregression gate but fail the >5% benefit threshold.
Identical local proof hashes and zero useful transformations do not support a
speedup claim. Reports include fresh Kissat and retained CaDiCaL run locally on
the respective measured host, pinned source revisions, input and tool hashes.

The additional Linux candidate screen covers 112 queries: original 48 checked,
expanded 30 checked plus two UNKNOWN, and fresh 32 checked. Every conclusive
answer passes independent validation; no wrong answers were observed. The two
UNKNOWNs are cal100 positive depth 4 and 8 at the expanded suite's 10-second
query budget. The paired depth-4 target uses 60 seconds and completes. All three
broader suites also record zero SSR strengthenings. This one-pass broader screen
is not a paired performance confirmation or a proof of general soundness.

| Isolated embedding peak RSS | Baseline | Candidate |
|---|---:|---:|
| cal3, through depth 16 | 37.48 MiB | 37.78 MiB |
| cal100, through depth 4 | 134.20 MiB | 134.72 MiB |

Each resource replay uses a fresh process with the Python encoding/hash/model
validation harness and one native solver, excluding proof checkers and other
solvers. It replays exact independently checked baseline contexts. These are
single-run embedding peaks, not native-only or complete transaction peaks; small
differences do not establish a memory improvement. No new full worker/checker
containment acceptance was required because the candidate was rejected before
promotion. The existing production acceptance evidence remains unchanged.

All raw Linux reports, host metadata and a compact validation summary are in
[ssr-linux-20260909](benchmark_results/ssr-linux-20260909/summary.json).
The outcome is an evaluated and reproducible rejected feature, stronger regression
coverage and more complete checker CPU accounting. Production runtime behavior
remains at c6b4ac8. Do not expand SSR into another tuning grid without new workload
evidence that useful strengthening opportunities exist.
