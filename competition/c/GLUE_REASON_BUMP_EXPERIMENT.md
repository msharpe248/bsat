# Glue-reason VSIDS reward experiment

The prototype adds `--glue-bump`: when conflict analysis first visits a
current-decision-level variable with an explicit learned reason whose stored
LBD is at most `--glue-lbd`, give it one additional VSIDS increment. Decisions,
implicit input-binary reasons, original reasons, higher-LBD reasons and variables
at earlier levels do not qualify. The existing VMTF and LRB branches do not
consume this reward. It is disabled by default and needs no additional
per-variable storage or reason-collection vector.

This is inspired by the extra variable bump in
[Glucose 4.2.1 conflict analysis](https://github.com/audemard/glucose/blob/4.2.1/core/Solver.cc#L796).
Glucose compares a candidate's learned-reason LBD with the final learned-clause
LBD. This experiment deliberately tests a different, simpler policy: a fixed
glue threshold evaluated when the variable is visited. It therefore does not
claim to reproduce Glucose's heuristic. Dynamic LBD, when enabled, can affect
which reasons qualify in subsequent analysis.

The increment is applied twice using the existing rescaling procedure, rather
than doubling a potentially huge floating-point increment before rescaling.
`Glue reason bumps` counts the extra rewards. Clause literals, reasons, proof
steps, minimization and backjump rules are not directly changed; decision
ordering and subsequent search can change.

## Validation

A real propagation graph supplies a current-level learned reason and an
earlier-level glue reason. The direct test covers 512 combinations of the
option gate, original/learned current-level reasons, LBD values, glue thresholds,
VMTF/LRB modes and large increments that trigger rescaling. It checks exact
normalized activity scores, finite values, extra-bump counts, scratch cleanup,
and the unchanged asserting clause and backjump. The earlier-level reason must
never get an extra reward.

Both release and ASan/UBSan builds pass all 27 C executable suites, including
the final strengthened reward regression. The independent validator adds three
profiles for the new option, including dynamic LBD, binary proofs, aggressive
reduction, iterative/binary minimization and trail reuse.

Both builds also pass 2,178 independent truth-table, original-model, text/binary
RUP and external DRAT checks with seed 20261009 (20 random plus 13 fixed formulas
under 66 configurations). These are local checks, not evidence of remote CI.

## Targeted measurements

[Raw results](benchmark_results/glue-bump-targeted-20260907.json) use the same
prototype binary with the flag off/on, default heap search, ten solving CPU
seconds and fifteen wall seconds. Four existing competition development targets
run twice per profile in shuffled order. Runs are serial, separate from builds
and validation. Original SAT models and UNSAT proofs are checked outside timing.
The record captures exact commands and input/executable hashes; executable hashes
were rechecked before restoring production sources.

| Profile | Verified runs | Mean wall PAR-2 | Errors |
| --- | ---: | ---: | ---: |
| Baseline | 6/8 | 9.7228 s | 0 |
| Glue reward | 6/8 | 10.1085 s | 0 |

Both profiles solve the same three inputs in both repetitions and time out on
the multiplier. The development sample is small and not held out.

| Target | Baseline median process CPU | Reward median process CPU | Conflicts, baseline to reward |
| --- | ---: | ---: | --- |
| Hamiltonian `a497d784` | 0.140601 s | 0.092501 s | 9,247 to 5,642 |
| Belpyramid `5831f356` | 5.095963 s | 5.328621 s | 30,460 to 33,111 |
| Battleship `ed6d842f` | 2.191592 s | 2.410026 s | 107,461 to 107,461 |

Hamiltonian uses 1,108 extra bumps and belpyramid 98,713, exactly repeated in each
run. Battleship uses zero extra bumps, so its timing difference cannot be credited
to the heuristic; the unchanged search illustrates host timing variability.
Process CPU includes startup, parsing and proof output, while the internal limit
excludes parsing. PAR-2 penalizes an unsolved run by twice the wall limit.

The multiplier reaches 112,873–138,245 conflicts with the reward versus
57,804–71,659 without it at ten seconds, but neither reports a solution. More
conflicts per second alone do not establish better search.

## Longer multiplier check

A [paired follow-up](benchmark_results/glue-bump-circuit-20260907.json) runs the
same input and profiles twice with thirty solving CPU seconds and thirty-five
wall seconds. All four runs finish UNKNOWN with no errors. Baseline reaches
173,149–180,210 conflicts; the reward reaches 334,184–335,960 and performs roughly
5.7 million extra bumps. Peak observed RSS rises from 43,499,520 to 69,992,448 bytes.
Thus the higher conflict throughput still adds no solve at this longer limit,
and changes in search create a memory cost despite requiring no new scratch
array. This is not a worst-case memory measurement.

## Decision and reproduction

**Reject the fixed-glue reward.** The repeated Hamiltonian improvement is real
in search counts, but the targeted aggregate adds no solve, the belpyramid
search worsens, and the longer multiplier test does not convert increased
throughput into a solution. These results do not justify retaining another
experimental production flag. They do not rule out Glucose's different rule
that compares reason quality with the final learned clause; that would require
its own implementation and measurements.

The [tested patch](benchmark_results/glue-bump-rejected-20260907.patch) applies
to `a360c82` and includes the implementation, option, counter, 512-case regression
and expanded independent validator. Production sources and validation profiles
are restored. The restored release and ASan/UBSan builds each pass all 26 C
executable suites. The competition-performance goal remains unmet.
