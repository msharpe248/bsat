# Rejected experiment: random decision bursts

Periodic random variable choices might escape unproductive branching orders.
Two implementations were tested and rejected: neither increased verified solves
on the development sample. Production solver code and options remain unchanged.

## Existing work and candidate

Kissat's pinned [decision code](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/decide.c)
starts random sequences at root. Its [defaults](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/options.h)
enable them in focused mode, with initial interval 500 and length 10;
its [limit scaling](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/kimits.c)
uses `log10(sequence_count + 9)`.

The BSAT prototypes expose `--random-decision-interval` (zero disables), start
bursts only at root, and suppress random selection in stable mode. The first
uses fixed ten-conflict windows and a fixed interval. The second multiplies
both length and interval by the logarithmic scale above, truncating to integers.
Neither extends an active window when another root restart occurs. Overflow
guards prevent wrapped deadlines.

Sampling uses the existing per-solver RNG and rejects assigned or eliminated
variables. After 32 attempts it falls back to ordinary branching, bounding work
on sparse tails. A random heap selection removes the chosen variable at its
actual heap position; VMTF retains its usual backtrack bookkeeping. Phase
selection is unchanged, but shared RNG consumption can affect random phases.
This is not a full Kissat port: it lacks its active-variable registry and warming
conditions, and uses bounded rejection instead of unbounded sampling.

## Measurements

Baseline: `b6b023952ca6b180e2d2d5ac1338cd060df74928`, whose solver source is
unchanged from `092a94e`. All comparisons use VMTF, ten solving CPU seconds and
fifteen wall seconds per run; candidate interval is 500. SAT models and UNSAT
proofs were independently checked. Each comparison has a fresh baseline run.

| Comparison | Baseline verified | Candidate verified | Baseline mean PAR-2 | Candidate mean PAR-2 |
| --- | ---: | ---: | ---: | ---: |
| Fixed bursts, three inputs twice | 4/6 | 4/6 | 12.5078 | 12.8675 |
| Growing bursts, three inputs twice | 4/6 | 4/6 | 12.7144 | 12.9013 |
| Growing bursts, 28 inputs once | 5/28 | 5/28 | 24.9415 | 24.9856 |

PAR-2 is in wall seconds. Every comparison has zero reported errors. The three
targeted inputs are belpyramid, hgen, and multiplier-circuits; hgen remains
UNKNOWN. The broad comparison solves exactly the same five inputs in both
versions. Reduced conflict counts on some inputs did not produce an overall
speed benefit. Small timing differences are not evidence of a general regression
or improvement; the evidence does not justify retaining another solver option.

Runs were serial on macOS arm64 without concurrent local builds or tests.
Process CPU includes startup, parsing, and proof output; the solving limit
excludes parsing. Independent checking is outside solver timing. Unverified
answers and UNKNOWN receive twice the wall timeout as PAR-2. These are reused
development inputs with short limits, not a held-out competition evaluation.
Heap branching performance was not benchmarked.

## Validation and reproduction

Both prototypes passed 17 C test executables in release and debug builds.
Tests cover deterministic sampling, root scheduling, focused/stable gating,
bounded fallback, eliminated variables, empty solvers, reset behavior, overflow,
and heap/VMTF backtracking. The growing prototype additionally covers logarithmic
scaling and repeated arbitrary heap removals with heterogeneous activities,
checking heap order, unique membership, and position mappings. No full randomized
validator or CLI deadline campaign was run for these rejected prototypes.

After restoration, all 16 production C test executables pass in release and
debug builds. The rebuilt release executable has the same SHA-256 as the saved
baseline: `1f6ca7fd8078bef247a8adc063a0dcd3f79d163b521b4e84c33278e68290cc04`.
Both archived patches pass `git apply --check --unidiff-zero` against the restored
source. Experimental regressions are archived with their prototypes.

- [Fixed prototype and tests](benchmark_results/random-decisions-fixed-prototype-20260906.patch)
- [Growing prototype and tests](benchmark_results/random-decisions-growing-prototype-20260906.patch)
- [Fixed targeted report](benchmark_results/random-decisions-fixed-20260906.json)
- [Growing targeted report](benchmark_results/random-decisions-growing-targeted-20260906.json)
- [Growing broad report](benchmark_results/random-decisions-growing-broad-20260906.json)

Apply each zero-context patch independently to the baseline from the repository
root with `git apply --unidiff-zero <patch>`. Run
`make -C competition/c -j4 all test` and
`make -C competition/c -j4 MODE=debug all test`.
Use `competition/c/tests/benchmark.py` with the input paths, solver command
templates, limits, and repetitions recorded in each report to repeat timings.
Reports preserve executable and input hashes; temporary baseline binaries and
local dataset paths must be recreated on another machine.
