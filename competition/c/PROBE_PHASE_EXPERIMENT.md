# Rejected experiment: suppress speculative phase saving

## Hypothesis

BSAT's `rup_candidate` temporarily assumes the negation of a candidate clause,
propagates, and backtracks. It is shared by failed-literal probing and learned
clause vivification. Those temporary assignments currently update saved phases.
Kissat's pinned [assignment path](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/inlineassign.h)
excludes probing from phase saving. We tested the same separation in BSAT,
rather than assuming it would improve search.

The candidate saved `opts.phase_saving`, disabled it for the entire RUP check,
backtracked, and restored it on the shared exit path. A proven root assignment
is added afterward and still uses the caller's original phase-saving setting.
No extra buffer or hot propagation branch was introduced. This also changed
startup phase bias: suppressing probe updates leaves BSAT's initial negative
saved phases in place.

## Tests and measurements

The baseline is `092a94ec448d7ba6e3de9c7b6711550ba32dff09`. The candidate passed
all 16 C test executables in release and ASan/UBSan builds. New cases checked
phase preservation through non-unit probes, restoration after a one-unit work
budget, actual root-unit phase updates, and disabled phase saving, with heap
and VMTF branching. Linking the new regression against baseline core code
failed at its phase-preservation assertion, confirming that it detects the
policy difference. The regression is retained in the experimental patch,
not in the unchanged production test suite.

Three reused development inputs (belpyramid, hgen, and multiplier-circuits) were
tested with VMTF, two repetitions, ten solving CPU seconds, and fifteen wall
seconds per run. SAT models and UNSAT proofs were independently checked.

| Policy | Verified runs / 6 | Mean PAR-2, wall seconds |
| --- | ---: | ---: |
| Baseline | 4 | 12.7933 |
| Suppress probe saving, negative initialization | 4 | 14.4816 |

Both versions miss hgen. On multiplier-circuits, median process CPU grows from
4.951 to 9.904 seconds; belpyramid also becomes slightly slower. No extra solve
or correctness failure is observed. This is evidence against changing the
default to this policy.

## Initial-polarity control

Because phase suppression changes startup bias, a second temporary executable
also initializes saved phases positively. It passed 611 fixed-formula solves
across all 47 validator configurations, with truth-table/model/proof checks.
This positive-initialization variant was not validated with the unchanged C
unit suite, whose phase tests explicitly assume a negative default.

The same three inputs, limits, and two repetitions were then compared against
a new baseline run. Both policies again verify 4 of 6 runs with zero errors.
Mean PAR-2 is 12.7376 for baseline and 12.7585 for positive initialization plus
suppression. Multiplier-circuits has 102842 conflicts in both versions and takes
about 4.9 process CPU seconds in both. Belpyramid increases from 36652 to 39949
conflicts, and hgen remains UNKNOWN.

Positive initialization removes the multiplier regression but demonstrates no
competitive gain. Neither candidate is retained, and no new option is added.
The source and active tests are restored to the baseline. This small experiment
does not rule out preserving warm phases specifically during vivification:
inprocessing was not enabled in these timing runs.

After restoration, all 16 C test executables pass in both build modes, and the
release executable's SHA-256 exactly matches the saved baseline executable.
Both archived patches pass `git apply --check --unidiff-zero` against the restored source.

## Reproduction and limits

- [Negative-initialization candidate and regression](benchmark_results/probe-phase-suppression-rejected-20260906.patch)
- [Positive-initialization candidate, core source only](benchmark_results/probe-phase-positive-rejected-20260906.patch)
- [First timing comparison](benchmark_results/probe-phase-targeted-20260906.json)
- [Positive-initialization control](benchmark_results/probe-phase-positive-20260906.json)

Apply each zero-context patch independently to the baseline with
`git apply --unidiff-zero <patch>`. Build with `make -j4 all`;
for the negative-initialization candidate also run `make -j4 test` and
`make -j4 MODE=debug all test`. The positive candidate's fixed-formula check is
`tests/validate.py --solver <binary> --checker <drat-trim> --cases 0` (seed
20260906). Use `tests/benchmark.py` with the report's input paths and solver
templates to repeat timing. Reports pin executable/input hashes.

Runs were serial on macOS arm64 without concurrent local builds or tests.
Process CPU includes parsing/startup and proof output; the solving CPU limit
excludes parsing. Independent certificate checking is outside solver timing.
UNKNOWN and unverified answers receive twice the wall timeout as PAR-2. These
are targeted development inputs, not a broad or held-out competition evaluation.
