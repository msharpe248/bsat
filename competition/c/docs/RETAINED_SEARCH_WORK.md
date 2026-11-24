# Retained reference search-work diagnosis — 2026-09-09

Baseline runtime c6b4ac8, certified retained flags 3. Serial Mac development
histories use the same ordered growing clauses and temporary assumptions for BSAT
and pinned CaDiCaL c607304. Independent Kissat/certificate acceptance remains
outside query timing. These diagnostic timings are not a new competitiveness
baseline; source work matches the previous production replay.

| Hard positive query | BSAT / CaDiCaL conflicts | BSAT / CaDiCaL propagations | Diagnostic query CPU, BSAT / CaDiCaL |
|---|---:|---:|---:|
| cal3 depth 16 | 482,935 / 30,060 | 307,192,953 / 4,159,789 | 23.173 / 0.839 s |
| cal100 depth 4 | 112,437 / 23,439 | 108,578,870 / 5,534,759 | 6.325 / 1.076 s |

CaDiCaL's public propagation counter counts search propagation, while its total
query CPU includes simplification; these ratios are not cycle-per-operation
comparisons. Counter snapshots are outside query timing and cumulative counters
are differenced across each query. The isolated extension uses the upstream
Wrapper's public `get_statistic_value` API, preserves IPASIR defaults (including
its disabled factor option), and passes 128 result/model/core parity histories.
Library/source hashes are retained; production BSAT exposes no new ABI surface.

CaDiCaL eliminates 2,759 additional variables during the hard cal3 query and
70,100 during cal100; its active irredundant clause count falls 12,915 → 6,935 and
187,401 → 15,539. This supports investigating certified simplification as a major
remaining capability, but does not isolate elimination as the cause: the prior
controlled plain-mode reference experiments still beat BSAT without preprocessing.

BSAT learns 62,662,476 literals on cal3 and 21,654,286 on cal100 after its existing
minimization, with 308,694,275 / 19,167,222 reason inspections. CaDiCaL's learning
counters elsewhere are pre-minimization and must not be compared as equivalent.
Previous recursive binary-reason and iterative minimization candidates already
failed performance gates. We do not repeat them. The bounded new candidate caches
failed recursive subtrees within one source candidate, retaining literals on a
cached failure and clearing all marks before the next candidate. Its frozen gate
and status are in COMPETITIVENESS_MILESTONES.md.

Evidence: benchmark_results/search-gap-{cal3,cal100}-20260909.json and
reference-statistics-parity-20260909.json. All 18 diagnostic query answers check.


## Failure-cache result

Rejected and removed from runtime source. Both target repetitions preserve every
conclusive proof hash, conflict count, learned-literal count and minimization
inspection count: cal3 remains 482,935 conflicts / 308,694,275 inspections;
cal100 remains 112,437 / 19,167,222. All 36 candidate queries check. The added cache
does not eliminate measured repeated work on these histories, so small timing
fluctuations cannot justify promotion or broader tuning. The validated patch and
66-test release/sanitizer plus 53-certificate-per-build evidence are archived in
`failure-cache-decision-20260909.json` and the adjacent prototype/target reports.
The remaining gap warrants a separately designed certified simplification or
search-quality experiment; this campaign introduces no new solver policy.
