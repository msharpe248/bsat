# Combined phase/factoring holdout

Frozen before timing: eight distinct families, seed 2026090831, inputs between
100 KB and 20 MB, excluding filenames and content hashes recorded in 504 prior
JSON reports (including nested artifact directories). This fixes a discovery gap
in the old selector, which scanned only top-level reports. Six selector tests pass,
including nested history, content aliases, replay, and malformed-history refusal.

64 serial trials on macOS ARM64: four explicit profiles, eight inputs, two
repetitions, identical 10-second external wall deadlines. No concurrent local
builds/tests/fuzzers. The binary is the pre-batch 0af2d90 solver; its SHA256 and
all input/checker hashes are recorded. Harness files were prepared before timing;
later source edits were not built during this run. SAT models are independently
checked against original input. All eight conclusive results are SAT; this screen
provides no new UNSAT proof-checking evidence.

| Profile | Verified runs / 16 | Mean PAR-2 seconds | Max RSS MB |
|---|---:|---:|---:|
| control | 4 | 17.152 | 428.80 |
| no-rephase | 0 | 20.000 | 413.65 |
| factor | 2 | 18.689 | 442.40 |
| both | 2 | 18.522 | 423.67 |

`factor` uses `--factor-min-gain 32`; `both` also disables rephasing. All profiles
include chrono, congruence, equivalence with a 100M work budget, alternating search,
and binary proof output. There are 56 UNKNOWNs and zero errors. Control solves
both repetitions of the argumentation and scheduling inputs; each combined run
solves only argumentation. The fixed combined profile fails the predeclared no-lost-input
and 10% PAR-2 improvement gates. Keep existing defaults and explicit options.

This is a short rejection screen, not a representative ranking or proof that a
policy never helps. The previously observed CSP gain does not generalize to this
fresh sample. These inputs are now development evidence, not a reusable fresh
holdout for subsequent policy tuning. Longer and separately held-out validation
would be required to promote a replacement policy.

Reproduce with `tests/select_corpus.py` (selection manifest preserves the exact
historical exclusion hashes) and `tests/benchmark_combined.py`. Replay the saved
manifest rather than selecting again against subsequently expanded history.
Raw selection, frozen policy, and all trials: `benchmark_results/combined-holdout*20260908.json`.
