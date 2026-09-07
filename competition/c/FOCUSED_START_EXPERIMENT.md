# Initial focused interval experiment

The larger fresh-input screen found two Kissat-only successes: `ktf` and
hardware verification. A subsequent unchanged-binary diagnostic compared default
and alternating BSAT with a 15-second solving CPU limit and a 20-second wall
limit, one repetition per case/profile, seed 20261021.

The [diagnostic record](benchmark_results/focused-start-probe-20260907.json)
shows default BSAT reaching 96,143 conflicts on `ktf` and 300,984 on hardware
verification without solving either. Hardware verification learns 59,815,910
literals (about 199 per learned clause) and deletes 297,161 clauses. These counters
identify substantial search effort; they do not prove a clause-retention defect.
Alternating mode solves `ktf` with a checked original model in 6.364482 process
CPU seconds and 36,918 conflicts, but leaves hardware verification UNKNOWN after
421,021 conflicts. This makes the alternating schedule a concrete experiment
candidate rather than assuming parsing or correctness explains the timeouts.

## Candidate

The existing alternating schedule starts with a zero mode limit, immediately
switching to stable mode at zero conflicts. Initialize the first limit to 1,000
instead, starting with focused search. Existing doubling boundaries remain
1,000, 2,000, 4,000 and so on, so this reverses the mode at subsequent intervals
as well. Ordinary non-alternating search and explicitly disabled restarts retain
their decisions about whether to switch.

The pinned Kissat source at revision
`8af8e56f174b778aef3aa45af9f739b2a5f492c2` provides the motivation:
`src/options.h` defines `modeinit=1000`, and `kissat_init_mode_limit` in
`src/mode.c` sets an initial focused conflict limit. This one-line BSAT candidate
is not a port of Kissat's later tick-based scheduling, branching or phase policy.
The old BSAT immediate stable entry is a documented heuristic choice, not a
soundness bug.

## Validation

The candidate's new direct test exercises 32 mode boundaries across heap/VMTF
and EMA/window-average configurations, including the conflict just before each
boundary, the transition itself and a repeated poll at the same count. It checks
initial focused decisions versus stable target phases and disabled policies.
Both release and ASan/UBSan builds pass all 34 C executables.

Each candidate build also passes 2,967 independent validation solves (seed
20261021), including truth-table answers, original SAT models and text/binary
UNSAT proofs checked with external drat-trim. These finite checks are regression
evidence, not a universal soundness proof. No speedup is established by tests.

## Performance and decision

The [paired target comparison](benchmark_results/focused-start-targeted-20260907.json)
uses two repetitions per input/profile, 15 solving CPU seconds, a 20-second wall
limit and seed 20261022. Both profiles verify **4/6**, with zero errors. Median
process CPU times and deterministic counters:

| Input | Immediate stable CPU | Initial focused CPU | Immediate / focused conflicts | Result |
| --- | ---: | ---: | --- | --- |
| ktf | 6.353870 s | 1.067153 s | 36,918 / 5,725 | SAT, both repetitions verified |
| hardware-verification | 15.042519 s | 15.042543 s | time-limited, variable | UNKNOWN, both repetitions |
| multiplier-verification | 0.081554 s | 0.084225 s | 41 / 40 | UNSAT, both repetitions verified |

The `ktf` improvement is about **83.2% less process CPU** and repeats with the
same conflict count. Hardware verification remains unsolved and reaches fewer
conflicts under the candidate; this gives no evidence of improvement there.
Target mean wall PAR-2 improves from 15.7739 to 13.8475 seconds, with unchanged
solved counts. Maximum process RSS is 79,036,416 versus 62,373,888 bytes.

The [guard manifest](benchmark_results/focused-start-guard-corpus-20260907.json)
adds the five remaining fresh larger inputs and four established targets.
The [guard screen](benchmark_results/focused-start-guard-20260907.json) uses one
repetition, ten solving CPU seconds, a 15-second wall limit and seed 20261023.
Both profiles verify the same **2/9** inputs (belpyramid and Hamiltonian), with
zero errors. Both leave the other seven UNKNOWN. Mean wall PAR-2 is 24.0436
versus 23.9805 seconds; maximum process RSS is 278,183,936 versus 129,466,368
bytes. This single repetition does not establish a general memory or CPU gain.

Because the two solved guard cases moved in opposite directions, a separate
[three-repeat confirmation](benchmark_results/focused-start-confirm-20260907.json)
uses the same limits and seed 20261024. Both profiles verify all **6/6** runs,
with zero errors:

| Input | Immediate stable median CPU | Initial focused median CPU | Immediate / focused conflicts |
| --- | ---: | ---: | --- |
| belpyramid-puzzle | 5.847077 s | 4.913502 s | 39,526 / 37,939 |
| Hamiltonian | 0.220231 s | 0.316069 s | 11,952 / 16,712 |

Belpyramid improves about 16.0%; Hamiltonian worsens about 43.5% (0.096 seconds).
These are real search-order tradeoffs, not a uniform speedup. No new input is
solved within the measured limits compared with the old alternating profile.

Retain the initial focused interval **within experimental `--alternating`**:
it repeats a large improvement on the newly identified `ktf` target, improves
belpyramid, and loses no solves in the measured screens. Accept the disclosed
small absolute Hamiltonian cost for this experimental profile. The ordinary
default remains non-alternating; these results do not justify enabling
alternating mode globally or claiming competition parity. Hardware verification,
longer runs and held-out performance remain open.

All timing jobs run serially, after builds and validation finish. SAT models
are checked against original CNFs and UNSAT proofs with external drat-trim,
outside solver timing, with a separate 120-second checker limit. Unverified
answers receive twice the external wall limit as PAR-2. Process CPU includes
startup, parsing and proof output; internal solving limits exclude parsing.
Wall times also reflect scheduling delays, so CPU and deterministic conflict
counts are both reported. Executable/checker hashes match after the measured
jobs; the diagnostic's original executable hash matches the preserved baseline.
The corpus is reused development material, not held-out evaluation.

## Reproduction

Build revision `30e84ee` and preserve its release executable as
`/tmp/bsat-focused-start-baseline`. Then build this candidate:

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat --checker /tmp/bsat-drat-trim --cases 30 --seed 20261021
python3 competition/c/tests/validate.py --solver competition/c/bin/bsat_debug --checker /tmp/bsat-drat-trim --cases 30 --seed 20261021
python3 competition/c/tests/benchmark.py --checker /tmp/bsat-drat-trim \
  --solver 'immediate=/tmp/bsat-focused-start-baseline --alternating --time 10 --proof {proof} {input}' \
  --solver 'focused=competition/c/bin/bsat --alternating --time 10 --proof {proof} {input}' \
  --timeout 15 --check-timeout 120 --repeats 1 --seed 20261023 \
  --manifest competition/c/benchmark_results/focused-start-guard-corpus-20260907.json \
  --output /tmp/focused-start-guard-repeat.json
```

For target and confirmation runs, use the inputs, commands, repetition counts
and seeds recorded above and in the raw JSON records. The direct mode test is
`tests/test_mode_start.c`.
