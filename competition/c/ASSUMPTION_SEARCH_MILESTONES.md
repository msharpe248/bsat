# Assumption search and generalization — 2026-09-09

1. Freeze the Linux certified comparison against pre-prefix commit d6601b9.
   Original four circuits: depths 0,1,2,4,8,16, 60 CPU seconds per query.
   Expanded four pinned circuits: depths 0,2,4,8, 10 CPU seconds per query.
   Each suite runs baseline/candidate/candidate/baseline serially on one runner.
   Retained CaDiCaL receives the same query CPU budget and 90 wall seconds;
   fresh Kissat and independent checking are outside query timing. Checker
   limits: 600 seconds per stage, 2048 MB heap / 512 MB stack. All conclusive
   answers must check; UNKNOWN stays unfinished. Record per-circuit solve losses
   and CPU PAR2, not only aggregates. These reused datasets are not pristine
   holdouts. Do not promote a new global policy that loses checked solves or
   regresses aggregate CPU PAR2 by more than 5% on its confirmation corpus.
2. Compare fresh certified temporary-assumption and permanent-unit queries for
   cal3 and gen23 at depth 16, positive output, same clause order and public flags
   3, two alternating repetitions, 60 query CPU seconds. Independently check all
   conclusive outputs against the exact same augmented input. Record conflicts,
   propagation, learning, reductions, journal and checker cost. Use separate
   100,000-conflict instrumented runs for phase accounting; not speed scores.
3. Choose one bounded implementation hypothesis from this evidence. Validate
   release/ASan/UBSan, adversarial future-query semantics and independent proofs;
   confirm the original and expanded circuits before promotion. Archive failures
   instead of changing defaults without checked gains. Update docs, commit and
   push each milestone. Prior runtime CI (365b574) is fully green.

Status: [Linux prefix generalization](CERTIFIED_LINUX_GENERALIZATION.md) is
complete: no checked losses, but both versions still time out on cal3 at 60 CPU
seconds on this runner.
[Fresh query diagnosis](QUERY_CONTEXT_DIAGNOSIS.md) is complete; the bounded
assumption-level LBD scoring prototype passes its target and retained screens.
Gated build 90091dd is undergoing paired original/expanded confirmation locally
and in Linux run 34372161034; it is not a production default.
