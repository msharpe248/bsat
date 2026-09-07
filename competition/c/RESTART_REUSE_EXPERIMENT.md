# Restart trail reuse experiment

## Hypothesis and reference

The preceding minimum-interval experiment found that more frequent root
restarts increased runtime even when they reduced conflicts. This prototype
tries to reduce the work of a restart by retaining leading decisions whose
current priority exceeds that of the next unassigned decision variable.

The reference is Kissat's [restart implementation at the pinned revision](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/restart.c),
specifically its score-based stable and stamp-based focused trail reuse. The
local pinned source was inspected before implementation. BSAT's existing
restart timing is unchanged; this experiment changes the backtrack level.

## Prototype

`--reuse-trail` enables priority-based prefix retention. Heap mode compares
activity scores and VMTF compares queue stamps. The scan stops at the first
decision whose priority is no greater than the next available variable's.
Ties are not retained. Heap peeking lazily removes assigned and eliminated
entries, leaving the next available variable in the heap. Both scans perform
periodic resource-limit checks without per-restart allocation.

The operation runs after propagation reaches a consistent state. Backtracking
uses the existing implementation and maintains reasons, watches, queue recovery
and heap membership. It need not reproduce the complete trail a root restart
would generate: it is a search heuristic. If every variable is assigned, the
current trail can be retained. Root simplification is still reached after
root backjumps; prefix retention can reduce how often it runs.

Assumption solves fall back to root restarts, avoiding empty assumption-level
frames. Alternating mode also falls back to root restarts, preserving its mode
switch behavior. A cumulative `Reused levels` statistic exposes actual use.
The prototype is disabled by default.

## Evaluation

The candidate passed all 20 C test executables in release and ASan/UBSan debug
builds, including new heap/queue prefix boundary and order recovery tests and
repeated-assumption API checks. The final suite also passed an added regression covering retained binary and
arena-backed implications, reason preservation, propagation position and
interruption. Testing was separate from performance timing. Each build also passed 2,709 independently
validated solves (63 configurations, 13 fixed plus 30 random formulas, seed
20261002), checking truth-table answers, original models, text/binary RUP and
external DRAT proofs. Five configurations exercise reuse with combinations of
VMTF, Luby, preprocessing, random phases and local search. Both builds passed
30 short-deadline cases, with maximum observed CPU overrun 0.000 seconds at
printed precision.

The targeted screen uses four previously selected development inputs, two
repeats per profile, ten solving CPU seconds and fifteen wall seconds. Both
profiles use the same candidate executable with reuse disabled/enabled. SAT
models and UNSAT proofs are independently checked outside timing. Local builds
and tests finish before timing begins. Commands and executable/input/checker
hashes are recorded in the benchmark JSON. These are reused development inputs,
not held-out competition evidence.

## Heap results

[Complete heap comparison](benchmark_results/restart-reuse-targeted-20260907.json).
Root restarting certified 6/8 runs (mean wall PAR-2 9.9478), while reuse certified
4/8 (16.9638), with zero errors in either profile.

| Input | Root restart CPU | Reuse CPU |
| --- | ---: | ---: |
| Battleship | 2.430–2.456 s | UNKNOWN in both repeats |
| Belpyramid | 5.864–5.946 s | 6.961–6.963 s |
| Hamiltonian | 0.160–0.162 s | 0.282–0.292 s |
| Multiplier circuits | UNKNOWN in both repeats | UNKNOWN in both repeats |

Reuse was active: both belpyramid runs retained 2,039 levels cumulatively and
both Hamiltonian runs retained 305. Yet belpyramid propagation count grew from
82,544,060 to 94,703,516 and Hamiltonian from 675,505 to 1,103,245. Battleship
reached roughly 440,000 conflicts without an answer instead of solving after
107,461. Retaining prefixes changed the search adversely enough to overwhelm
any saved restart work. This rejects reuse as a heap-default change.

## VMTF targeted results

[Complete VMTF comparison](benchmark_results/restart-reuse-vmtf-20260907.json).
Both profiles certified 6/8 runs with zero errors. Mean wall PAR-2 improved from
10.8763 to 8.7237. These use the current no-random-phase default in both profiles.

| Input | Root restart CPU | Reuse CPU |
| --- | ---: | ---: |
| Battleship | UNKNOWN in both repeats | UNKNOWN in both repeats |
| Belpyramid | 3.080–3.165 s | 3.049–3.068 s |
| Hamiltonian | 0.381 s in both repeats | 0.600–0.608 s |
| Multiplier circuits | 7.282–7.292 s | 0.820–0.827 s |

The multiplier result is a repeated approximately 8.8-fold CPU improvement:
conflicts fall from 97,543 to 18,183 and propagations from 106,656,059 to
15,505,952, with 3,435 reused levels. Hamiltonian regresses, with conflicts
increasing from 19,744 to 30,661. This motivates a broader VMTF screen rather
than a global default change.

## Broader screen and retention decision

The [fixed 44-input manifest](benchmark_results/no-random-corpus-20260907.json)
from the phase-policy evaluation is reused for one run per VMTF profile/input,
with the same ten-CPU/fifteen-wall limits and independent verification.
[Complete comparison](benchmark_results/restart-reuse-broad-20260907.json).

| VMTF policy | Certified inputs | Mean wall PAR-2 | Peak process RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Root restart | 7/44 | 25.5135 | 107,954,176 bytes | 0 |
| Prefix reuse | 7/44 | 25.3499 | 96,501,760 bytes | 0 |

No inputs were gained or lost. Mean PAR-2 decreased about 0.64%; the 37
unresolved inputs dominate this metric. Multiplier circuits again improved
(7.124 to 0.794 process CPU seconds). The two Hamiltonian inputs regressed:
`a497...` from 0.362 to 0.600 seconds, and `6e139...` from 0.075 to 0.231.
Belpyramid improved slightly (2.994 to 2.858 seconds); the remaining three
solves were very short in both profiles. The single-run broad comparison is
a development screen, not a claim of statistical or competition superiority.

Retain `--reuse-trail` as an experimental opt-in, principally for evaluating
VMTF configurations. Its repeated large circuit gain justifies retaining this
small implementation for further work. Do not enable it by default: it loses
battleship with heap ordering and regresses on Hamiltonian even with VMTF.
Default heap ordering and root restarting remain unchanged. Six completed
heap-default target runs matched the previous milestone's status and selected
search counters (conflicts, decisions, propagations, restarts, learned clauses
and literals, literal inspections and minimization inspections).

The feature summary also had obsolete restart defaults and comparison formulas;
these now match the implementation and the corrected restart guide.

## Competition reference

The [pinned Kissat comparison](benchmark_results/restart-reuse-reference-20260907.json)
uses the same four target inputs, two repeats, limits and independent checker.
It was run serially after the broader BSAT screen, without builds or tests in
parallel. Kissat certified 8/8 runs with zero errors:

| Input | Kissat process CPU |
| --- | ---: |
| Battleship | 0.150 s in both repeats |
| Belpyramid | 1.198–1.204 s |
| Hamiltonian | 0.046–0.047 s |
| Multiplier circuits | 0.427–0.428 s |

Reuse brings BSAT's VMTF circuit result much closer to this reference, but it
remains roughly twice as slow there and leaves larger gaps on other targets.
The competition-performance goal remains unproven. These targeted results do
not substitute for a representative held-out evaluation at competition limits.

The final release executable matches the measured SHA256
`c4f02ba3a315d3d29b4a9f5d9d1d260e3de971a33cdc950d31c76b9650ba9584`.
