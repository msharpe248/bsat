# Performance and assurance follow-up

This tracker covers the authorized follow-up to `READINESS.md`. Each milestone
is implemented, checked and measured before being committed and pushed. Negative
performance experiments are recorded without retaining regressing runtime changes.

1. Buffered binary proofs: complete. 336 encoding cases, 48 C executables and
   24 long-search certificates/models pass per release/sanitizer build. Thirty-six
   fixed-work traces match exactly. Both Summle repetitions improve (52.0–53.4 s
   versus 57.5–59.4 s); hardware UNSAT remains verified. See `BINARY_PROOFS.md`.
2. Propagation and search-efficiency measurements: pending. Add reproducible Linux
   hardware-counter collection and separate search work from cost per work unit.
   The target Linux host has been requested; do not substitute hosted VM timing
   for dedicated target-hardware evidence.
3. Simplification candidate/work selection: pending. Implement a bounded candidate
   experiment, verify certificates, and retain only supported improvements.
4. Verified UNSAT checking: complete. Pinned, hash-checked cake_lpr assembly
   accepts the original CNF plus converted LRAT. Valid, corrupted, mismatched,
   long-search and large hardware certificates exercised; see `VERIFIED_CHECKING.md`.
5. Continuous stateful and coverage-guided fuzzing: pending. Include input/API
   sequences, bounded independent oracles, failure artifacts and replay/shrinking.
6. Explicit GCC/Clang coverage: pending. Run both compilers on Linux in CI without
   dropping the existing platform/build checks.
7. Longer evaluation: pending. Freeze a fresh multi-family corpus and longer run
   policy after implementation; report all checked answers and timeouts. Target
   hardware and long local runs are separate evidence scopes.

No formal verification of BSAT, exhaustive fuzz coverage or competition readiness
is implied by completing these milestones.
