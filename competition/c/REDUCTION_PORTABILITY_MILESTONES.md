# Isolating reduction portability — 2026-09-08

Freeze before measuring: use the pinned exact cal3 query, compare the original
search immediately before/after the first reduction (1,999, 2,000, 2,001 conflicts),
then 4,001 and 10,000 conflicts. Two repetitions on macOS and Linux, with the
normal reduction schedule and a billion-conflict interval disabling reduction.
Archive prefix hashes, counters and diagnostic pre/post sort records. UNKNOWN
prefixes are not accepted UNSAT certificates; this is causal diagnosis.

If reduction ordering is implicated, test a total-order tie-break using clause
references after LBD and activity. Preserve an unchanged baseline; test semantic
ranking/protection/relocation behavior, release/sanitizer regressions and
independent original models/proofs. Confirm cross-host traces and measure the
candidate with 60 CPU / 90 wall seconds, two serial repetitions. Reject a claimed
speed improvement unless checked solving supports it. If determinism requires a
search-policy tradeoff, document it and do not silently promote a regression.

Status: diagnostic implementation pending.
