# Shared blocker check in propagation

The runtime change is **rejected**. Six-input fixed-work measurements show only
0.19% less CPU in aggregate, mixed individual changes, and no additional solves.
The retained change is the mixed-watch truth/order regression suite. The tested
candidate is archived as a [patch](benchmark_results/blocker-dispatch-rejected-20260907.patch).

## Motivation and candidate

After chunked text proof encoding, propagation remains a major cost on larger
inputs. Diagnostic sampling of revision `c2ee11f` found propagation at the top
of 216/453 stacks on hardware-verification and 272/453 on hamiltonian-cycle.
The [profile metadata](benchmark_results/blocker-dispatch-profiles-20260907.json)
links raw reports and pins the optimized symbolized executable and inputs.
Sampling is evidence for where to investigate, not precise CPU attribution.

The candidate checks whether a watch's blocker is true before dispatching to
implicit binary, tagged arena binary, or general clause handling. A true blocker
satisfies any of those representations. The binary path can then treat an
assigned non-true blocker as a conflict without a second polarity test. The
candidate preserves the existing long-clause skipped counter and does not
refresh blockers or change watch order, clause contents, or search policy.

The pinned Kissat source also checks blocker truth before handling binary
propagation: [`src/proplit.h`, revision 8af8e56f](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/proplit.h#L70).
This source was inspected in the local pinned checkout. Its watch and value
representations differ from BSAT's, so a similar structure need not be faster
here. Unlike the earlier [blocker refresh experiment](PROPAGATION_EXPERIMENTS.md),
this candidate does not alter satisfied-clause reordering behavior.

## Tests

`test_blocker_dispatch.c` checks **10,368** cases against a literal-truth oracle:
six orderings of implicit binary, tagged binary and long watches; all sixteen
watched/blocking sign combinations; both tail signs; both phase-saving settings;
and all 27 blocker truth patterns. It checks the first conflict, subsequent
unvisited watches, exact retained watch order, work and skip counts, implication
order, values, levels, trail positions, binary/arena reasons and saved phases.
Both the baseline and candidate pass this test.

The [trace comparison](benchmark_results/blocker-dispatch-traces-20260907.json)
checks six reused competition inputs at 1,000 conflicts with text and binary
proofs. Matching proof prefixes and search counters establish deterministic
agreement on those runs; incomplete prefixes do not certify UNKNOWN answers.

Release and ASan/UBSan debug builds each pass **41 C test executables** and
**3,139 independent formula/model/proof checks**, using 73 configurations,
43 formulas and seed 20261129. Both pass 42 short-deadline cases; maximum CPU
overrun is 0.000 seconds release and 0.001 seconds debug. The
[validation record](benchmark_results/blocker-dispatch-validation-20260907.json)
pins candidate source, test and executable hashes and exact validator commands.
These finite checks do not establish universal soundness.

## Performance and decision

The [fixed-work comparison](benchmark_results/blocker-dispatch-work-20260907.json)
uses default search with text proofs, a 120,000-conflict cap, three repetitions
per solver, a 30-second external wall limit and seed 20261130. Six reused
development inputs include the previous five text-proof workloads plus the
larger hamiltonian-cycle input. All 21 selected counters match across all six
runs on each input. Both versions verify **9/18**, with no errors; the other
nine runs per version reach the conflict cap.

| Input family | Baseline median CPU (s) | Shared check median CPU (s) | Change |
| --- | ---: | ---: | ---: |
| battleship | 0.937017 | 0.913068 | -2.56% |
| belpyramid-puzzle | 5.205165 | 5.312706 | +2.07% |
| hamiltonian | 0.128177 | 0.133823 | +4.40% |
| hamiltonian-cycle | 2.513488 | 2.508272 | -0.21% |
| hardware-verification | 3.461326 | 3.269286 | -5.55% |
| ktf | 17.277339 | 17.456610 | +1.04% |

The geometric mean of the six candidate/baseline median CPU ratios is
**0.99814**, about **0.19% less CPU**. This is too small and inconsistent to
justify the change. Hardware-verification samples themselves vary substantially:
baseline 2.809–3.500 seconds and candidate 2.674–3.422 seconds. Wall-clock
variability is greater, and the promising early wall timings did not establish
a broad CPU improvement. Mean wall PAR2 is 31.158 versus 31.230 seconds.
Maximum RSS is 60,948,480 versus 60,833,792 bytes.

Jobs ran serially with no concurrent compilation, validation or sampling.
The diagnostic profile executable has debug symbols; performance compares
ordinary release binaries. CPU timing includes parsing and proof output;
independent certificate checking is outside solver timing. UNKNOWN incurs twice
the wall limit in PAR2. This is a development screen on one macOS arm64 machine,
not a held-out competition result. No new 30-second or longer large-corpus screen
was run for this rejected candidate: the fixed-work result does not support
expecting an additional larger-problem solve.

## Restored implementation and reproduction

The solver source is restored exactly to `c2ee11f`. The candidate validation
above applies to the archived candidate; it must not be confused with validation
of a retained propagation change. Restored release and ASan/UBSan suites are
checked separately in the validation record. The prior milestone's 3,139 solves
per build apply to the unchanged restored solver source; they are not new runs
from this experiment.

Apply the candidate patch to `c2ee11f` with
`git apply --unidiff-zero competition/c/benchmark_results/blocker-dispatch-rejected-20260907.patch`.
Run `make -C competition/c all test`, repeating with `MODE=debug`. Validator
commands and hashes are recorded in the validation JSON. The benchmark JSON
records exact solver commands, input and executable hashes, limits and seed;
use those inputs with `tests/benchmark.py`, placing `--conflicts 120000`
inside each `--solver` command template as recorded. Baseline and candidate binaries were preserved at
`/tmp/bsat-blocker-dispatch-baseline` and `/tmp/bsat-blocker-dispatch-candidate`;
the archived commands show the paths actually used during measurement.
