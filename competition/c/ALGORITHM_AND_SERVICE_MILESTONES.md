# Algorithm and service batch

> Historical milestone report. Its claims apply to the recorded revisions. Use
> [PRODUCTION_READINESS.md](PRODUCTION_READINESS.md) for current capabilities,
> evidence limits and release gates.

Authorized scopes: proof-producing factoring; longer stratified industrial
evaluation; full-search memory/speed tradeoffs; application-shaped incremental
histories; per-query embedding budgets/statistics and IPASIR; certificates over
retained incremental learning. Commit and push each validated milestone.

Timed experiments freeze inputs, executable hashes and options before execution
and run without concurrent local builds/tests/fuzzers. Preserve unrelated
`.zvec-grep/` state. Keep experimental transformations opt-in and validate SAT
against original input and UNSAT through independent certificates.

1. Factoring: implemented binary/ternary residual rectangles with RAT proofs,
   bounded work, auxiliary namespace isolation, exhaustive projection tests,
   allocation injection and sanitizer fuzzing. reg-n development gap closed;
   argumentation regression keeps the option experimental. See FACTORING.md.
5. Embedding controls/IPASIR: implemented and validated in release and
   ASan/UBSan, including frozen-header and installed C++ consumers. See IPASIR.md.
6. Retained-state certificates: implemented and independently validated over
   reused/rebuilt histories in both builds. See RETAINED_CERTIFICATES.md.
4. Application histories: added exact domain oracles, >4,000-variable growth,
   actual retained certificates and a fix for cleanup starvation across short
   queries. Measured reuse/rebuild tradeoffs; see APPLICATION_HISTORIES.md.
3. Full-search memory: four 120-second largest-input runs and twelve longer
   fixed-work format comparisons complete. Retained savings persist; RSS is
   mixed and no general speedup is claimed. See FULL_SEARCH_MEMORY.md.
2. Longer industrial evaluation: 72 audited trials across 12 fresh inputs / 11
   families, 60-second solve limits and two repetitions complete. Control and
   factoring each verify 2/24 trials; Kissat verifies 6/24. Ten conclusive answers
   pass independent checks, 62 remain UNKNOWN, and no errors occur. The planned
   replacement of one potentially contaminated timing is recorded explicitly.
   See INDUSTRIAL_LONG.md. All six authorized milestones are complete for the
   documented scope; experimental options remain opt-in.

Implementation validation: all seven Linux/macOS correctness jobs, including
ThreadSanitizer, pass for 9496c4f (run 34246109456). Expanded core/parser/public
fuzzing and ABI soak CI also pass. Subsequent memory/industrial commits contain
measurement records and documentation; they do not change solver code.
