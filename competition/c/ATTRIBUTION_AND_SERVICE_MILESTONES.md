# Attribution, bounded search and service integration

> Historical milestone report. Its claims apply to the recorded revisions. Use
> [PRODUCTION_READINESS.md](PRODUCTION_READINESS.md) for current capabilities,
> evidence limits and release gates.

Authorized batch; commit and push each validated milestone. Preserve unrelated
`.zvec-grep/`. Timed workloads run serially without concurrent local builds/tests.

1. Clause attribution: complete; CLAUSE_ATTRIBUTION.md, diagnostic origin/age/reuse data.
2. Selective strengthening: complete; HOT_CLAUSE_STRENGTHENING.md, tested prototype archived after holdout regression.
3. Bounded portfolio: complete; BOUNDED_PORTFOLIO_EVALUATION.md, shared limits and launch accounting verified; no additional solves.
4. Checkpoint policy example: complete; CHECKPOINT_POLICY_EXAMPLE.md, public C client and validated changing workloads.
5. Independent incremental comparator: complete; RETAINED_INCREMENTAL_REFERENCE.md, 1,024 release and 256 sanitizer queries pass.

Heuristic experiments may be rejected and archived when validation or performance
does not support shipping them. UNKNOWN is distinct from an incorrect answer.

All five scopes are complete. Production additions are diagnostic attribution,
a public C service-policy example, independent retained-reference coverage and
more accurate benchmark resource accounting. The strengthening prototype is
archived after its holdout regression; existing search defaults remain unchanged.
The planning search gap remains unresolved rather than being hidden by tuning.

Attribution commit b1de132 passes the full Linux/macOS correctness matrix,
ThreadSanitizer, coverage fuzzing and independent integration. The new retained
CaDiCaL and checkpoint-example integration passes Linux at 845ebb0. Its GCC
release matrix exposed a CPU-test wall-fuse assertion, corrected and locally
revalidated in the final report; fresh CI for that test-only follow-up is pending.
The corrected Linux portfolio workflow 34277647416 passes. Superseded correctness
runs 34275751452, 34276462425 and 34276782585 were cancelled to free capacity.

The production executable was rebuilt after removing the hot-clause prototype
and its SHA256 matches the pre-experiment binary exactly. All commits were pushed
individually; unrelated `.zvec-grep/` state remains untouched.
