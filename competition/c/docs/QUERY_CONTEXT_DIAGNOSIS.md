# Fresh certified query-context diagnosis — 2026-09-09

Fresh public certified handles receive identical ordered cal3 depth-16 permanent
clauses and default certified options. The positive query literal is either a
temporary assumption or the final permanent unit. Each mode gets 60 solving CPU
seconds; loading and export/check CPU are recorded separately. Two repetitions
reverse mode order. Every conclusive result is independently checked against
exactly the same augmented clause list. This is a controlled diagnostic, not a
safe way to reuse permanent query units across changing application queries.

| Circuit / mode | Repeated CPU seconds | Conflicts | Result |
|---|---|---|---|
| cal3 assumption | 60 / 60 | 1,372,341 / 1,563,995 at cutoff | UNKNOWN |
| cal3 unit | 12.179 / 12.148 | 329,859 both | checked UNSAT |
| gen23 assumption | 1.193 / 1.162 | 7,248 both | checked UNSAT |
| gen23 unit | 1.279 / 1.242 | 7,274 both | checked UNSAT |

At 100,000 conflicts, cal3 assumption/unit runs create 9.44M/7.03M learned
literals and classify 1,263/2,394 learned clauses as glue. Instrumented inclusive
propagation CPU is 2.23/2.27 seconds and conflict-analysis CPU 0.62/0.58 seconds.
These nested timings are not additive or benchmark scores. They do not show an
order-of-magnitude per-conflict throughput gap; search length is the main observed
difference. Minimization inspections differ 20.13M/9.28M, so learning cost also
warrants attention if the targeted search change fails.

Post-query inspection finds 909 of 3,417 live learned clauses contain a literal
assigned in the fixed assumption prefix. Of these, 733 have stored LBD 3, just
above the default glue threshold 2. No learned clauses contain the negated query
literal itself: fixed implications, not an explicit guard literal, carry this
relationship. This snapshot does not prove that retaining these clauses helps.

## Frozen bounded implementation hypothesis

Test treating fixed assumption levels like root levels *for LBD scoring only*,
while retaining every learned literal, reason, proof addition and ordinary
conflict backjump. Restrict the prototype to the certified restart policy.
Set/reset a transient query scoring boundary around each solve; later queries
must never inherit it. This changes restart and retention heuristics and may
protect clauses irrelevant to a later assumption set. It is not a proof rule.

Require release/sanitizer and independent retained certificates before timing.
First repeat the same fresh cal3/gen23 matrix: the candidate must add a checked
cal3 assumption solve within 60 seconds, without losing a checked target solve.
If successful, compare the original and expanded retained circuits under the
frozen budgets against the current prefix-retaining default, and reject any
checked-solve loss or >5% aggregate CPU PAR2 regression. Do not tune another grid
if this particular candidate fails; archive the result and preserve defaults.

Reports: `benchmark_results/query-context-baseline-20260909.json`,
`query-context-fixed-work-20260909.json`,
`query-context-score-diagnosis-20260909.json`. The helper records input/tool/library
hashes and certificate logs. Test-facade control parity checks 48 calls, exact
work/models/proof bytes, and absence of diagnostic production exports.
