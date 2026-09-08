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
