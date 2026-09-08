# Algorithm and service batch

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
2–3. Longer industrial evaluation and full-search memory tradeoffs remain
   in progress.
