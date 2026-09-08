# Next production implementation batch

Authorized order: diagnose reg-n, reduce large-input memory, confirm the faster
profile on additional inputs, exercise hard incremental histories, extend safe
reuse, and finish release packaging/ABI/race checks. Commit and push each milestone.

Each experiment pins its input, binary and policy before timing. No local builds,
tests or fuzzers run alongside timed comparisons. Existing unrelated `.zvec-grep/`
workspace state is preserved. Linux PMU profiling still needs a target host.

1. reg-n diagnosis complete: 14 reference ablations, eight verified UNSAT answers,
   six UNKNOWN, no errors. Factoring is the isolated promising missing technique;
   see `REGN_DIAGNOSIS.md`. This is diagnosis, not a BSAT factoring implementation.
2. Memory lifetimes and watch capacity: measured roughly 11% less peak RSS on
   the largest input at fixed work; see `MEMORY_LIFETIMES.md` for checks and limits.
3. Hard incremental histories complete before changing conditional reuse; see
   `HARD_INCREMENTAL.md`. This establishes a checked baseline for that change.
4. Installation/pkg-config and frozen ABI-v1 C/C++ consumers pass locally in
   release and ASan/UBSan builds. Linux ThreadSanitizer passed job 102000866067
   in run 34207651049 at `092b1b5`; compiler/platform CI continues separately.
5. Fresh profile confirmation complete: 64 audited factorial runs on eight new
   inputs; no-VMTF wins on two inputs from two families. See `PROFILE_CONFIRMATION.md`.
6. Conditional-UNSAT and compatible-preprocessing reuse is implemented and
   independently checked; destructive transformations retain fallback. Repeated
   identical hard queries use roughly one tenth the CPU in the bounded protocol;
   growing histories have mixed work results. See `INCREMENTAL_REUSE.md`.

Final implementation revision `caf04a7` passes all seven jobs in
[CI run 34209432051](https://github.com/msharpe248/bsat/actions/runs/34209432051):
GCC/Clang release and ASan/UBSan on Linux, Clang release and ASan/UBSan on macOS,
and Linux ThreadSanitizer. The following documentation-only synchronization
updates the API contract and README to match that tested implementation.
