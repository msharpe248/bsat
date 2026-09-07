# Temporary retention of used learned clauses

`--protect-used` gives a learned clause with LBD at most six a reprieve at the
next database reduction after it participates as the conflict clause or an
expanded reason during conflict analysis. It is opt-in and disabled by default.
Newly learned clauses do not receive protection merely for being created.

The existing unused `CLAUSE_FROZEN` flag stores the reprieve without enlarging the
16-byte clause header. Each reduction consumes the flag on every live learned
clause, including locked and glue clauses. An otherwise eligible protected clause
skips that reduction; without another qualifying use, it competes normally next
time. This temporary protection takes precedence over `--max-lbd` for that one
reduction. Binary clauses, permanent glue protection, locking, scoring and the
fraction of eligible clauses retained otherwise follow the existing policy.

When combined with `--dynamic-lbd`, analysis updates LBD before checking the
protection threshold. Original clauses are not marked. Garbage collection copies
the flag with the clause header; API rebuilds discard learned clauses and retain
the configured option. Protection changes retention, not logical inference or
proof generation.

[Kissat's reducer](https://raw.githubusercontent.com/arminbiere/kissat/master/src/reduce.c)
uses clause-use counters and glue tiers when collecting reduction candidates.
That motivates testing use-sensitive retention. BSAT's single reprieve and fixed
threshold are a smaller, distinct policy; they do not reproduce Kissat's dynamic
tiers, multi-bit counters, reduction schedule or ranking.

## Regression coverage

A propagated conflict fixture checks both conflict and reason marking, unmodified
original clauses, the disabled mode, LBD values four through seven, dynamic-LBD
interaction, and expiry at a second reduction without reuse. Lifecycle checks
exercise garbage collection, the threshold boundary, consuming protection while
locked, backtracking and API rebuilds. Formula validation includes protection
alone, aggressive reduction with binary proofs and a low maximum LBD, and combined
dynamic LBD/preprocessing/inprocessing.

## Evaluation method

The baseline is commit `866069dce2dca52d7eb52355570293bcdb639d53` (its production
release binary is identical to the preceding target-saving milestone). The first
screen compares baseline, protection alone, and protection plus dynamic LBD on the
existing 28 development inputs, at two CPU seconds and five wall seconds. The
confirmation compares baseline and protection twice at three CPU seconds and six
wall seconds. A fresh batch uses sixteen previously unrecorded inputs from the
same competition collection at three CPU seconds, once per solver. Selection is
seeded and recorded before timing; these are fresh inputs, not held-out families.

All runs are serial on macOS arm64 without concurrent local tests or builds.
Models and proofs are independently checked outside timing. Whole-process times
include parsing/startup/proof output, while internal CPU limits apply to solving.
The samples and short limits are development evidence, not competition parity.

## Search results

| Sample | Baseline verified | Protection verified | Combined verified |
| --- | ---: | ---: | ---: |
| Existing 28 inputs, 2 seconds, one repetition | 3/28 | 4/28 | 3/28 |
| Same inputs, 3 seconds, two repetitions | 6/56 | 8/56 | Not run |
| Fresh 16 inputs, 3 seconds, one repetition | 1/16 | 1/16 | Not run |

Every batch reports zero errors. The additional ensemble-computation result is
SAT with an independently verified model: protection solves it in about 0.7 CPU
seconds in all three runs, while the baseline reaches its two/three-second limit.
The combined mode fails to reproduce this gain in the initial screen. Protection
alone is therefore the supported experiment; the results do not establish that
combining independently motivated heuristics improves them.

Confirmation mean PAR-2 falls from 10.7179 to 10.3170 under the six-second external
wall limit. The fresh batch is neutral (11.2512 versus 11.2513), and includes large
inputs whose parsing/solving exceeds the external limit. Maximum RSS there is
about 2.52 GB baseline and 2.46 GB candidate. UNKNOWN and timeout results do not
count as solved. These short-budget results justify further evaluation of an
opt-in policy, not changing the default or claiming competition parity.

## Disabled-mode controls and final implementation

The initial helper incurred an apparent 3.9% aggregate increase at 10000 conflicts
on twelve larger inputs over two repetitions. Protection marking was consolidated
into the existing LBD-analysis helper, and reduction avoids flag work when the
option is disabled. A three-repetition control then measured a 6.4% increase.

However, an identical-binary A/A comparison at the same work budget measured a
6.7% aggregate difference, with per-instance ratios from 0.9716 to 1.2007. The
longer final/default comparison at 30000 conflicts and three repetitions measured
a ratio of 0.9831, with a range of 0.8380 to 1.1896. All fourteen compared counters
match in these controls. This substantial variability prevents attributing the
short-run differences to the code or claiming a default-mode speed improvement.
No stable aggregate regression was established either; default overhead remains
a subject for measurement on a controlled host.

The first and final enabled implementations match all fourteen counters on the
twelve larger inputs at 10000 conflicts. A final two-repetition ensemble check
again verifies SAT with protection and returns UNKNOWN for the baseline at three
CPU seconds. Its external limit is 25 seconds (the harness control setting), so
its PAR-2 values should not be compared directly with the six-second confirmation.

The [initial-helper patch](benchmark_results/used-clauses-initial-helper-20260906.patch)
reconstructs the earlier marking/reduction arrangement from the final source.
Raw reports distinguish both executable hashes. These two arrangements implement
the same enabled retention policy; full correctness validation targets the final
implementation.

## Evidence

- [Initial screen](benchmark_results/used-clauses-screen-20260906.json)
- [Repeated confirmation](benchmark_results/used-clauses-confirm-20260906.json)
- [Fresh inputs](benchmark_results/used-clauses-fresh-corpus-20260906.json) and [runs](benchmark_results/used-clauses-fresh-20260906.json)
- [Per-instance outcomes](benchmark_results/used-clauses-summary-20260906.json)
- [Initial disabled control](benchmark_results/used-clauses-default-20260906.json)
- [Final disabled control](benchmark_results/used-clauses-final-default-20260906.json)
- [Identical-binary control](benchmark_results/used-clauses-aa-20260906.json)
- [Longer disabled control](benchmark_results/used-clauses-long-default-20260906.json)
- [Enabled counter comparison](benchmark_results/used-clauses-equivalence-20260906.json)
- [Final ensemble confirmation](benchmark_results/used-clauses-final-ensemble-20260906.json)
- [Release deadlines](benchmark_results/used-clauses-limits-release-20260906.json) and [sanitizer deadlines](benchmark_results/used-clauses-limits-asan-20260906.json)

The raw reports record commands, input/executable hashes, checker identity, search
counters and per-run outcomes. Derived control summaries are stored alongside them.

Applying the archived helper patch in a fresh temporary source tree and rebuilding
produced a byte-identical executable to the initial measured candidate. See the
[reproduction record](benchmark_results/used-clauses-reproduction-20260906.json).

Final validation passed 4859 release and 4859 ASan/UBSan formula solves across
43 configurations (seed 20260916), all thirteen C test executables in both modes,
and twelve short-deadline runs per mode. The final release executable matches
the final benchmark hashes. See the
[validation record](benchmark_results/used-clauses-validation-20260906.json).
