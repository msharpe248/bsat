# Performance and assurance follow-up

This tracker covers the authorized follow-up to `READINESS.md`. Each milestone
is implemented, checked and measured before being committed and pushed. Negative
performance experiments are recorded without retaining regressing runtime changes.

1. Buffered binary proofs: complete. 336 encoding cases, 48 C executables and
   24 long-search certificates/models pass per release/sanitizer build. Thirty-six
   fixed-work runs match selected counters and proof bytes. Both Summle repetitions
   improve (52.0–53.4 s versus 57.5–59.4 s); hardware UNSAT remains verified.
   See `BINARY_PROOFS.md`.
2. Propagation and search-efficiency tooling: implemented. `compare_work.py`
   separates matched work counters from process cost; `profile_linux.py` records
   CPU-pinned real PMU events, work counters and independent answer checks.
   The target Linux host has been requested; do not substitute hosted VM timing
   for dedicated target-hardware evidence.
3. Simplification candidate/work selection: experiment complete; runtime change
   rejected. Bounded short/low-LBD ordering passed correctness but regressed
   Battleship and aggregate timing. See `SIMPLIFICATION_SELECTION.md`.
4. Verified UNSAT checking: complete. Pinned, hash-checked cake_lpr assembly
   accepts the original CNF plus converted LRAT. Valid, corrupted, mismatched,
   long-search and large hardware certificates exercised; see `VERIFIED_CHECKING.md`.
5. Continuous stateful and coverage-guided fuzzing: implemented and locally
   exercised. Independent bounded oracles, allocation failures, nightly/PR CI,
   retained corpora and replay/minimization instructions; see `FUZZING.md`.
6. Explicit GCC/Clang coverage: complete. All six hosted configurations passed
   in run 34185746931: Linux GCC/Clang and macOS Clang, release and sanitizer.
7. Longer local evaluation: complete. Six fresh families, 24 serial runs, twice
   the prior wall limit, up to 622,818 variables / 2,552,775 clauses. BSAT verifies
   4/12 runs versus Kissat 10/12, with zero errors; see `LONGER_EVALUATION.md`.
   Dedicated Linux target measurements remain outstanding until a host is supplied.

No formal verification of BSAT, exhaustive fuzz coverage or competition readiness
is implied by completing these milestones.
