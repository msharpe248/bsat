# Opt-in bounded clause factoring

`--factor` finds complete clause rectangles and replaces each profitable
rectangle `{a OR C | a in A, C in B}` by `{x OR a}` and `{NOT x OR C}`.
Residual clauses C contain one or two literals.
This is an initial bounded variable addition implementation, not the full set
of factoring techniques in Kissat. It currently factors original live binary and ternary
clauses in one pass; it does not factor longer residual clauses or recursively
factor the newly introduced edges.

The default work limit is 100 million operations (`--factor-budget`) and the
fresh-variable limit is 1,024 (`--factor-max-variables`). Graph construction
includes ternary clauses only when the combined arc count is at most three
million, otherwise it uses binary clauses alone. It skips graphs above four
million arcs. Temporary storage is linear in variables, residuals and arcs. All search/index loops
consume work and poll cancellation. Duplicate input clauses are deduplicated
for finding rectangles, so multiplicity cannot masquerade as a complete group.

The NOT-x replacement clauses are RAT on a fresh pivot. Each subsequent x
clause has the original rectangle clauses as resolvents. Original clauses are
deleted only after all replacements exist. Every interrupted prefix preserves
satisfiability projected onto the original variables. SAT answers are still
checked against the retained original input, and the CLI model excludes auxiliary
variables. Later input growth or queries rebuild the original variable namespace.

Reconstruction state, existing learned clauses, inprocessing and local-search
modes conservatively skip factoring. It is opt-in and does not change defaults.

Release and ASan/UBSan suites pass 7,488 signed/duplicate/budget/rebuild and
exhaustive original-variable projection cases, plus a 20-auxiliary-variable
growth regression, including chronology/VMTF combinations. Each suite passes
2,773 injected allocation failures across 16 paths/formulas. An API ASan/UBSan
fuzz campaign completes 55,571 executions in 121 seconds without a finding.
All 96 signed
text/binary proof cases, including partial optimization budgets, pass the
independent DRAT-to-LRAT/CakeML chain. Reproduce those with
`tests/check_factor_certificates.py` and the usual checker environment variables.

The frozen generalized development comparison (two inputs, two repetitions,
20-second CPU/25-second wall limits, independent certificates) solves reg-n in
1.328 and 1.704 seconds; both control runs remain UNKNOWN. On argumentation,
factoring regresses from 4.091–4.162 to 9.095–9.494 seconds. All six conclusive
answers are independently verified; no run reports an error. These two known
inputs diagnose the implementation and do not establish a general speedup.
The earlier binary-only prototype results are retained as negative evidence.
See `benchmark_results/factor-general-development-20260908.json` and its frozen
policy for timings, counters, executable/input hashes and checker costs.

The later 72-trial fresh industrial campaign finds no solved-count gain:
control and factoring each verify 2/24 trials, versus Kissat's 6/24. Factoring
remains opt-in. See `INDUSTRIAL_LONG.md` for the size/family strata, timings,
independent checks, peak memory, and the single audited timing replacement.
