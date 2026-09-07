# Isolate the random-circuit regression after restoring LBD state

The corrected solver's random-circuit timeout is sensitive to both clause
retention and focused restarts. A four-way development experiment changes those
two controls separately while keeping the LBD ownership fix intact. This does
not justify undoing the fix or changing production defaults.

## Matched diagnostic

All four profiles use the same fixed release binary, chronology, congruence,
equivalence substitution with a 100-million-work budget, and alternating VMTF.
The input is random-circuits/849950561ddce887c78fef773dccfa80.cnf, already used
for development. Jobs ran serially with one repetition, seed 20261309, a
30-second solving CPU budget, a 35-second external wall limit, and independent
answer verification outside timing.

The retention control sets `--glue-lbd 4294967295`, protecting every learned
clause from reduction. The restart control sets `--glucose-k 0.000000001`,
suppressing the focused moving-average trigger on this input. Mode switches
and stable Luby restarts remain active. These extreme settings isolate the
effects of the previous zero-LBD state; they are not recommended defaults.

| Profile | Result | Process CPU seconds | Conflicts | Restarts | Peak RSS MB |
| --- | --- | ---: | ---: | ---: | ---: |
| Corrected defaults within the experimental profile | UNKNOWN | 30.053 | 1,413,672 | 4,090 | 75.4 |
| Retain all learned clauses | UNKNOWN | 30.055 | 350,919 | 898 | 104.2 |
| Suppress focused restarts | UNKNOWN | 30.054 | 1,517,242 | 263 | 75.3 |
| Both controls | SAT verified | 24.681 | 282,298 | 54 | 100.6 |

Neither control alone recovered the solve. Together they reproduce the prior
buggy run's decisions (615,476), propagations (21,222,754), conflicts (282,298),
restarts (54), chronological backtracks (183), and retained assignments (1,966).
Maximum LBD is correctly 60 instead of zero. These matching counters support
the heuristic explanation; they do not prove complete internal trace identity.

The experiment establishes an interaction on one development instance, not a
general policy winner. Unlimited retention also removes the memory benefit
that motivated this investigation. Raw commands, binary and input hashes,
verification and counters are in
`benchmark_results/equiv-lbd-factorial-20260907.json`. The prior comparison is
documented in [EQUIV_LBD_STATE.md](EQUIV_LBD_STATE.md).

## Smaller retention changes

A follow-up keeps ranked deletion and tests reducing every 10,000 conflicts
instead of 2,000, with and without focused restarts. A third profile keeps the
2,000-conflict interval but removes the hard LBD-30 deletion cutoff using
`--max-lbd 4294967295`. It still deletes the worse half of eligible clauses.
The same input, binary, common settings and budgets apply, with seed 20261310
and one serial repetition per profile.

| Profile | Result | Process CPU seconds | Conflicts | Peak RSS MB |
| --- | --- | ---: | ---: | ---: |
| Reduction interval 10,000 | UNKNOWN | 30.055 | 783,988 | 75.9 |
| Interval 10,000 and suppressed focused restarts | UNKNOWN | 30.056 | 1,155,725 | 77.6 |
| Ranked deletion without the hard LBD cutoff | UNKNOWN | 30.053 | 1,454,251 | 72.1 |

All three remained within a similar memory range but failed to recover the
solve. The completed benchmark records contain no errors; UNKNOWN supplies no
answer certificate. There is no demonstrated solve improvement to promote,
and one repetition cannot establish timing stability. No runtime policy or
default is changed by this investigation. It rules out these specific simple
adjustments as a demonstrated recovery at this budget, not all retention or
restart policies. Raw evidence is in
`benchmark_results/equiv-lbd-retention-20260907.json`.
