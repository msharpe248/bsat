# Rejected inverse-square break-weighted walk

## Decision

Do not retain this prototype in the production solver. It reduces work on planted
3-SAT examples but substantially regresses the structured battleship target
across four seeds, without adding a verified solve in the measured competition
sample. The rejection concerns this implementation and configuration, not every
probabilistic local-search algorithm.

The [complete tested patch](benchmark_results/prob-walk-rejected-20260907.patch)
applies to `d0c3b76` and contains the implementation, CLI and standalone-driver
support, direct regression, and expanded validation configurations. Production
source, options and validators are restored; `--ls-probabilistic` is not a
supported production option.

## Implementation and existing work

Kissat's [local-search source](https://github.com/arminbiere/kissat/blob/master/src/walk.c)
uses break-based scores to select literals probabilistically. Inspired by that
approach, this experiment assigns each movable variable weight
`1 / (1 + breaks)^2`. Here `breaks` counts currently satisfied clauses that become
unsatisfied on flipping the variable; it is distinct from BSAT's cached
break-minus-make score. This is a simplified variant, not a reproduction of
Kissat's tuned scoring and sampling scheme.

The prototype counts breaks through the currently true polarity's occurrence
list and uses weighted reservoir sampling in one pass over candidate variables.
It allocates nothing per flip and excludes root-fixed variables. It ignores the
WalkSAT noise setting when selected explicitly, while leaving ordinary WalkSAT
as the default. Long scoring loops check deadlines. Its extra scoring and clock
checks are included in the reported performance; the comparison does not assume
equal cost per flip.

## Correctness and resource checks

The prototype passes all 26 C executable suites in release and ASan/UBSan builds.
Its direct regression uses 10,000 seeds on a clause with break counts two and
zero, independently checking the expected 1:9 weight ratio, deterministic random
draws, the resulting assignment, and SAT outcome. Another 128 seeds exercise root
exclusion when saved phases disagree with fixed values.

Independent model/proof validation passes 832 cases per build across sixteen
option configurations and 52 formulas. Both builds pass 42 short-deadline cases,
with maximum observed CPU overrun rounding to 0.000 seconds. These are tested
cases, not a worst-case bound for all local-search initialization and update work.
Correct answers and passing tests did not justify retaining a slower heuristic.

After removing the prototype, all 25 production C suites pass in both builds.
The preserved patch passes `git apply --check` against the restored source.

## Planted-formula results

All runs use the standalone driver with noise 0.5 and seeds 1, 2 and 3. Both
selectors solve every formula, and every model is independently verified.

| Corpus | Selector | Verified models | Total flips | Median flips |
| --- | --- | ---: | ---: | ---: |
| 50–200 variables, 20,000-flip limit | WalkSAT | 18/18 | 33,168 | 425 |
| 50–200 variables, 20,000-flip limit | Probabilistic | 18/18 | 8,576 | 425.5 |
| 500–2,000 variables, 100,000-flip limit | WalkSAT | 18/18 | 144,588 | 5,821 |
| 500–2,000 variables, 100,000-flip limit | Probabilistic | 18/18 | 83,711 | 3,209.5 |

The small corpus's lower total does not improve its median. The larger corpus
improves both measures. Summed internal walk CPU is 0.005116 versus 0.002346
seconds on the small corpus and 0.032355 versus 0.027365 on the larger one. These
very short measurements do not establish a reliable throughput ratio. Planted
formulas are biased development examples, not held-out competition evidence.

[Small-corpus results](benchmark_results/prob-walk-planted-20260907.json) use the
existing six committed fixtures. [Larger-corpus results](benchmark_results/prob-walk-larger-planted-20260907.json)
use two formulas each at 500, 1,000 and 2,000 variables, with 4.2 clauses per
variable. They were fixed before timing, but selected as a follow-up to the small
corpus results. The [generator](benchmark_results/prob-walk-larger-generator-20260907.py)
recreates them and their manifest under `/tmp/bsat-prob-corpus`; recorded hashes
allow exact input verification.

## Competition-target results

Both selectors use VMTF, trail reuse, local search every 5,000 conflicts and
10,000 flips per attempt. Four recent development targets are repeated twice,
with ten solving CPU seconds and fifteen wall seconds. Model/proof verification
is outside timing; no builds or validation run concurrently with measurements.

The [targeted comparison](benchmark_results/prob-walk-hybrid-20260907.json) certifies
8/8 runs for each selector, with no errors. Mean wall PAR-2 worsens from 1.6996 to
2.3584 seconds. On battleship, WalkSAT wins in its first walk after 2,834 flips,
using 0.130–0.140 process CPU seconds. The probabilistic variant needs fifteen
walks and 141,407 flips, using 1.959–2.020 CPU seconds. No other target records a
local-search win, and no new solved input is established.

The [additional-seed check](benchmark_results/prob-walk-seeds-20260907.json) confirms
the battleship regression:

| Seed | WalkSAT CPU | Probabilistic CPU | WalkSAT flips | Probabilistic flips |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 0.129812 s | 1.404900 s | 7,222 | 92,248 |
| 3 | 0.126381 s | 6.480470 s | 3,764 | 459,240 |
| 4 | 0.133864 s | 4.769501 s | 9,272 | 328,711 |

Every result is a verified SAT model. The regression persists across seeds and
is explained by far more search work, not just noisy wall timing. Keep the
existing WalkSAT heuristic. A future probabilistic design would need stronger
structured-instance evidence and a separate evaluation of its scoring and
feedback policies before becoming a production option.
