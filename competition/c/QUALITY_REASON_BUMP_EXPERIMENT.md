# Relative-quality reason reward experiment

This prototype tests the relative-quality activity reward used in
[Glucose 4.2.1 conflict analysis](https://github.com/audemard/glucose/blob/4.2.1/core/Solver.cc#L796).
During first-UIP analysis, collect first-visited current-level variables whose
explicit reasons are learned clauses. After minimization, compare each reason's
current stored LBD with the final learned clause's LBD. A strictly lower reason
LBD earns one extra VSIDS increment. Equal or worse quality earns none.

This addresses the key difference in the rejected
[fixed-glue reward](GLUE_REASON_BUMP_EXPERIMENT.md): eligibility depends on the
new clause's quality, not a fixed glue threshold. BSAT applies the reward after
both of its minimizers and final LBD recomputation, before backtracking. Dynamic
LBD updates made during analysis are visible to the comparison. This does not
reproduce every part of Glucose's search policy.

`--quality-bump` enables the experiment. It is off by default and inactive for
VMTF and LRB ordering. A separate candidate vector survives minimizer scratch
reuse. It is allocated lazily only for the enabled heap policy, using four bytes
per variable at the largest analyzed variable count, and freed with the solver.
Growing the variable set grows the buffer at the next analysis. Allocation
failure sets the solver error and stops the solve without claiming an answer.

The candidate list is reset at analysis entry and consumed once. Rewarding calls
the existing VSIDS update, including rescaling and heap repair. Deadline and
cancellation checks occur before the first candidate and every 1,024 candidates;
interruption may leave partial activity hints but no partially installed learned
clause. `Quality reason bumps` counts actual additional increments.

## Validation

Both release and ASan/UBSan builds pass 27 C executable suites. A 2,304-case
regression uses a real implication graph with current- and earlier-level learned
reasons. It checks strict quality boundaries, option and ordering gates, original
reasons, minimizer scratch reuse, exact normalized activity values, rescaling,
unchanged first-UIP clause/backjump, cancellation and expired deadlines, single
consumption and candidate-buffer growth. The earlier-level glue reason never
qualifies. Synthetic final LBD values exercise the comparison independently;
full-solver validation covers actual final-LBD integration.

Both builds also pass 2,178 independent truth-table, original-model, text/binary
RUP and external DRAT checks with seed 20261010: 20 random and 13 fixed formulas
under 66 configurations. The added profiles combine the new reward with dynamic
LBD, binary proofs, aggressive reduction, iterative/binary minimization and trail
reuse. These are local checks, not a claim that remote CI ran.

## Paired reference comparison

[Raw results](benchmark_results/quality-bump-targeted-20260907.json) compare the
same candidate binary with the reward off/on and pinned Kissat at
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`. Exact commands, input and executable
hashes, outcomes and counters are recorded. Every measured executable hash was
checked again before restoring production sources.

Four existing competition development targets run twice per profile in shuffled
order, with ten solving CPU seconds and fifteen wall seconds. BSAT uses default
heap search. Timing runs serially, after builds and independent validation;
original SAT models and UNSAT proofs are checked outside solver timing. Process
CPU includes startup, parsing and proof output, while BSAT's internal limit
excludes parsing. PAR-2 assigns twice the wall limit to unsolved runs. This is a
small development screen, not a held-out competition score.

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 6/8 | 9.9064 s | 32,718,848 bytes | 0 |
| Relative-quality reward | 4/8 | 16.8552 s | 75,825,152 bytes | 0 |
| Pinned Kissat | 8/8 | 0.6614 s | 26,230,784 bytes | 0 |

| Target | Baseline median process CPU | Reward median process CPU | Kissat median process CPU |
| --- | ---: | ---: | ---: |
| Battleship `ed6d842f` | 2.418170 s | 10.003179 s (UNKNOWN) | 0.190618 s |
| Belpyramid `5831f356` | 5.646180 s | 7.036159 s | 1.516717 s |
| Hamiltonian `a497d784` | 0.158246 s | 0.164719 s | 0.058357 s |
| Multiplier `90bec6dc` | 10.011191 s (UNKNOWN) | 10.011343 s (UNKNOWN) | 0.543287 s |

The reward loses both battleship solves: baseline finishes at 107,461 conflicts,
whereas the reward times out after 446,353–450,001 conflicts and about 0.8 million
extra bumps. Unlike the rejected fixed-glue rule, the relative rule is active on
this instance and materially changes its search.

Belpyramid conflicts increase from 30,460 to 38,988, with 240,532 extra bumps in
each repetition. Hamiltonian conflicts increase from 9,247 to 9,816, with 45,284
extra bumps. On the multiplier, the reward reaches roughly 110,000 conflicts
versus roughly 58,000 for baseline, but neither solves. Higher conflict
throughput again fails to establish more effective search. Peak RSS includes
all search state; its increase cannot be attributed solely to the candidate
vector. These are observed peaks, not worst-case memory bounds.

## Rejection and reproduction

**Reject the relative-quality reward.** Both repeated battleship regressions,
the worse belpyramid search and the absence of an additional solve make a broader
retention trial unjustified for this implementation/configuration. The result
does not show that Glucose's heuristic is generally ineffective: its surrounding
policies differ from BSAT's. It does show that this direct integration fails the
current retention screen and increases the measured gap to Kissat.

The [tested patch](benchmark_results/quality-bump-rejected-20260907.patch) applies
to `aa9e3dd`. It includes candidate allocation/lifecycle, the post-minimization
reward, option and counter, the 2,304-case regression and expanded independent
validator. Production sources and validator profiles are restored; neither the
option nor the candidate array is retained. The restored release and ASan/UBSan
builds each pass all 26 C executable suites. Competition-level performance is
still unachieved.
