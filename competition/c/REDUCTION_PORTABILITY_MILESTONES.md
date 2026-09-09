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

Diagnosis completed: prefixes match through 1,999 conflicts; at 2,000 the
pre-sort records match exactly and tied post-sort records differ. Disabling
reduction preserves matching prefixes through 10,000. An ascending-reference
tie-break would adopt the Linux selection on the observed creation-ordered
candidate vector, changing the faster measured Mac path. Before runtime timing,
refine the candidate to a portable typed sorting routine preserving the observed
Mac partition/tie policy. Require exact local prefix parity and cross-host parity;
retain the quality comparator and all protection rules. This is a documented
change of implementation approach based on the diagnostic evidence, not tuning
after candidate performance results.
