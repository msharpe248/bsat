# Deep-trail adaptive restart blocking

This experiment tests whether preserving unusually deep searches improves BSAT's
competition performance. Baseline is `8098f07`; the candidate adds opt-in
`--block-restarts` and leaves the default policy unchanged.

[Glucose's restart blocking](https://github.com/audemard/glucose/blob/master/core/Solver.cc#L1394)
compares the conflict trail with a rolling trail average and clears the recent
LBD queue when the trail is unusually large. Its source defaults use a
5,000-conflict trail queue and a 1.4 multiplier. BSAT implements that idea
independently with a fixed 20,000-byte optional rolling buffer, sampled before
conflict analysis/backtracking. After 10,000 conflicts, a full trail buffer and
a full recent-LBD window permit blocking when the current trail is strictly
greater than 1.4 times the updated trail average. Integer cross-multiplication
avoids floating-point threshold ambiguity and fits in 64 bits.

A block clears the recent LBD window without backtracking or resetting the
conflicts-since-restart counter. Both EMA and average adaptive restart modes
must refill that window before restarting. The EMA values themselves remain
unchanged: this is an adaptation of the window-refill safeguard to BSAT's EMA
policy, not a complete Glucose replication. With the default minimum of 100
conflicts and window of 50, the initial refill requirement adds no delay before
the first eligible restart. A smaller configured minimum can be constrained by
the refill window even before blocking is warm.

Luby, geometric and stable-mode restart policies do not use blocking. Alternating
mode transitions retain precedence; focused sampling resumes after a stable
phase. No-restarts overrides sampling and restart decisions. The optional buffer
is allocated during solver construction and freed through normal/error cleanup;
each eligible conflict updates it in constant time. The `Restart blocks` counter
reports window clears, not necessarily a restart that would have fired at that
exact conflict. Assumption handling and proof generation are unchanged.

## Validation

The direct regression checks strict threshold equality and adjacent values,
EMA/average window refill, preservation of the conflict counter, the warmup
boundary, policy exclusions and independent rolling sums through three complete
buffer cycles, with 45 counted checks. Release and ASan/UBSan builds each pass
all 31 C test executables. The independent validation matrix includes the option
with EMA, sliding-window, VMTF/reuse, preprocessing, alternating and Luby modes.
The small formula matrix does not generally reach blocking's warmup threshold;
the direct tests cover that threshold and the competition runs exercise actual
window clears with independent model/proof checks.

Each candidate build passes 2,277 independent solves across 69 configurations
(20 random and 13 fixed inputs, seed 20261015), checking truth-table answers,
original SAT models and text/binary RUP proofs, including external drat-trim.

## Competition development screen

Each profile runs the same four targets twice with ten solving CPU seconds and
a fifteen-second external wall limit. All performance jobs run serially, separate
from builds and validation. SAT models and UNSAT proofs are checked outside
solver timing. Process CPU includes parsing/startup/proof output; the internal
limit excludes parsing. Unsolved runs receive a 30-second PAR-2 penalty.

[EMA and pinned-Kissat comparison](benchmark_results/restart-blocking-targeted-20260907.json):

| Profile | Verified runs | Mean wall PAR-2 | Peak observed RSS | Errors |
| --- | ---: | ---: | ---: | ---: |
| Default EMA baseline | 6/8 | 9.9372 s | 32,915,456 bytes | 0 |
| EMA with blocking | 6/8 | 11.7453 s | 34,979,840 bytes | 0 |
| Kissat | 8/8 | 0.6564 s | 30,474,240 bytes | 0 |

Kissat is pinned at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.
Both BSAT profiles solve battleship, belpyramid and Hamiltonian twice, while
multiplier remains UNKNOWN. Median process CPU:

| Input | Default EMA | Blocking EMA | Search evidence |
| --- | ---: | ---: | --- |
| Battleship | 2.407252 s | 9.223569 s | Conflicts 107,461 to 409,355; 101 blocks |
| Belpyramid | 5.732679 s | 4.925305 s | Conflicts 30,460 to 31,271; 162 blocks |
| Hamiltonian | 0.145494 s | 0.144608 s | Same 9,247 conflicts; zero blocks |
| Multiplier | 10.010987 s | 10.011346 s | Both UNKNOWN; 396 candidate blocks |

Battleship's repeated conflict increase is a material search regression.
Belpyramid's CPU improvement despite more conflicts shows that conflict counts
alone do not measure cost. Multiplier's conflict throughput falls without a solve.

The follow-up [sliding-window comparison](benchmark_results/restart-blocking-average-20260907.json)
checks a policy closer to the original window-based approach:

| Profile | Verified runs | Mean wall PAR-2 | Errors |
| --- | ---: | ---: | ---: |
| Average baseline | 4/8 | 16.8665 s | 0 |
| Average with blocking | 4/8 | 16.5674 s | 0 |

Both solve only belpyramid and Hamiltonian in both repeats. Belpyramid improves
from 36,956 to 33,573 conflicts and median CPU 7.097112 to 5.670438 seconds.
Hamiltonian keeps 13,420 conflicts and essentially unchanged CPU (0.233624 versus
0.234412 seconds), despite 13 window clears. Battleship and multiplier remain
UNKNOWN. The average variants solve fewer runs than the existing EMA default.

These are reused development targets, not held-out families or a competition
score. All measured executable hashes were verified before restoring production
source. Raw files retain exact commands, input hashes and per-run statistics.

## Decision and reproduction

Reject the prototype: it adds no solve, worsens the default's targeted PAR-2,
and repeatedly makes battleship nearly four times slower. The sliding-window
follow-up does not provide a stronger replacement for the current default.
Preserve the implementation and tests in the
[rejected patch](benchmark_results/restart-blocking-rejected-20260907.patch).
Production source, public options and the validation matrix are restored to
`8098f07`; no blocking option is retained. The competition-performance goal
remains open.

To reproduce, build and save the baseline executable at `8098f07`, apply the
rejected patch to that revision, and run:

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 20 --seed 20261015
```

Repeat validation using `bin/bsat_debug` for sanitizer coverage. Raw measurement
files supply paired solver commands and input paths; the EMA candidate adds
`--block-restarts`, and the window comparison adds `--glucose-restart-avg` to
both profiles. After restoration, both builds pass all 30 production C test executables, and
the release executable hash exactly matches the measured baseline.
