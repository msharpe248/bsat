# Certificates from retained incremental learning

Create a public handle with `BSAT_CERTIFICATES`, optionally combined with
`BSAT_REUSE_LEARNTS`. After a conclusive solve, `bsat_export_query` writes two
new files: the exact permanent input plus that query's assumption units, and a
binary DRAT proof prefix. UNSAT exports append a query-local empty clause.
Export performs no solving and does not change the query statistics, model or
core. SAT exports still require checking the returned model against the query.
Always independently check an UNSAT export against its accompanying CNF.

The solver appends globally entailed RUP additions to a temporary-file journal.
CDCL treats assumptions as decisions, so learned clauses remain consequences of
permanent input. A conditional UNSAT result records the negated assumption
clause, not an unconditional empty clause, in that journal. Later permanent
clauses strengthen the formula, preserving validity of earlier RUP steps.
Deleted clauses stay in the proof journal: a stronger checker database preserves
RUP and allows subsequent proofs to use retained learning. Cancellation or
budget exhaustion can leave a valid proof prefix, but neither permits an export
or a conclusive answer. Cancellation rebuilds search while preserving the journal.

Certified handles use conservative CDCL options: no probing, variable factoring,
equivalence substitution, congruence, elimination, inprocessing or local search.
This keeps original variables and global implication semantics intact. Ordinary
CLI proof streams retain their original ownership/reset behavior; the internal
assumptions API still rejects an ordinary proof stream. The borrowed journal is
owned and closed by the public facade, including after core replacement.

Journal write/flush failures poison the handle and return UNKNOWN. Export uses
exclusive file creation, so existing files are preserved. Output errors return
zero and can leave incomplete new files; they do not certify anything. Journal
read/seek failures poison the handle. Export is synchronous and separate from
solve budgets; application supervision is needed for wall-time or disk quotas.
A successful close is not a power-loss durability guarantee.

The tradeoff is storage and checking cost: the append-only journal grows over
the session, and each export copies its full prefix. There is no compaction,
checkpoint protocol or constant-size per-query proof claim. The existing
`tests/certify_query.py` fresh-solve wrapper remains available for CLI workflows.

Release and ASan/UBSan validation each checks 51 exported queries (27 UNSAT,
24 SAT), with matching rebuilt/retained contexts, duplicate/contradictory
assumptions, hard guarded pigeonhole learning, a budget-limited prefix,
cancellation/retry, variable growth and permanent UNSAT. Every UNSAT export
passes DRAT-to-LRAT conversion and the independent CakeML checker; every SAT
model satisfies its exact exported input. A proof rebound to the satisfiable
base input is rejected. The largest proof in this focused suite is 20,186 bytes.
Core tests additionally exercise deletion suppression, stream ownership and
write failure. Allocation injection passes 3,105 cases across 20 paths/formulas
per build. Reproduce with `make query-certificate-driver` and
`tests/check_retained_certificates.py`, setting the usual checker variables.

The public-facade/IPASIR ASan/UBSan fuzzer completes 347,903 executions in
121 seconds without a finding, followed by 48,570 executions in 31 seconds
after final deadline handling changes. Export flush-failure tests preserve
the solver and successfully retry into new output files.
