# Fresh large-instance evaluation of growing reduction intervals

Fixed BSAT, growing BSAT and Kissat each have **one independently verified
solve out of six**. Growing intervals add no solves, slow the one completed
BSAT case, and increase peak RSS on every input. The earlier hardware
development gain does not generalize in this screen. Kissat additionally
reports UNSAT on diagnosis, but its proof check exceeds the frozen allowance;
that result is not counted as verified.

This screen evaluates the settings committed in `54e9346` without tuning on
the new inputs. Both BSAT profiles include the corrected LBD state and logical
chronological backtracking. The comparison changes only the reduction interval
increment and includes pinned Kissat as a reference.

## Frozen inputs and execution

The deterministic selector excludes CNF filenames and SHA256 hashes appearing
in 354 preceding JSON records. It chooses the smallest eligible input from
each of six distinct families, with a minimum file size of 12 MB. These inputs
are unused according to the recorded history; the small, size-biased selection
is not a representative competition evaluation.

| Family | File bytes | Variables | Clauses |
| --- | ---: | ---: | ---: |
| Station repacking | 12,344,165 | 31,263 | 799,432 |
| Diagnosis | 12,429,871 | 151,952 | 697,321 |
| Scheduling | 12,463,703 | 207,919 | 691,067 |
| ordering-principle-xor | 13,289,014 | 2,812 | 408,044 |
| School timetabling | 13,457,873 | 213,083 | 668,213 |
| Independent-set reconfiguration | 13,567,854 | 120,036 | 704,717 |

Both BSAT profiles enable `--chrono --congruence --equiv --equiv-budget
100000000 --alternating --vmtf`. The growing profile additionally sets
`--reduce-increment 1000`; fixed retains the 2,000-conflict interval.
Kissat is pinned at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.

Seed 20261314 shuffles one repetition per profile and input. All jobs run
serially, with no concurrent builds, validation or profiling. Every process
has a 35-second external wall limit. BSAT additionally has a 30-second solving
CPU allowance, excluding parsing; Kissat can use the full external allowance.
Process CPU and wall time include parsing and proof output. Independent
checking runs outside timing, with its own 120-second limit.

Only independently checked original SAT models or verified UNSAT proofs count
as solved. UNKNOWN is unfinished and supplies no answer certificate. Wall
PAR-2 assigns 70 seconds to an unsolved run. One repetition cannot quantify
timing variability or establish competition-level reliability.

## Results

Times below are whole-process CPU seconds, including parsing and proof output.

| Family | Fixed BSAT | Growing BSAT | Kissat |
| --- | --- | --- | --- |
| Station repacking | UNKNOWN | UNKNOWN | UNKNOWN |
| Diagnosis | UNKNOWN | UNKNOWN | UNSAT reported, 16.794 s; proof check timed out |
| Scheduling | UNKNOWN | UNKNOWN | UNKNOWN |
| ordering-principle-xor | UNKNOWN | UNKNOWN | UNKNOWN |
| School timetabling | SAT verified, 3.457 s | SAT verified, 4.682 s | SAT verified, 7.586 s |
| Independent-set reconfiguration | UNKNOWN | UNKNOWN | UNKNOWN |

There are no benchmark execution or invalid-answer errors. This does not
certify the unverified diagnosis result: the external proof checker exceeded
120 seconds, and the benchmark's temporary proof was then discarded. A separate
certificate run with retained artifacts and a larger checking allowance is
needed to resolve that verification gap. The recorded timed result remains
unverified under the frozen policy; neither timeout nor an UNSAT status line
establishes soundness.

Mean wall PAR-2 is 59.167 seconds for fixed BSAT, 59.437 for growing BSAT and
59.630 for Kissat. These numbers assign the full unsolved penalty to Kissat's
unverified diagnosis answer and must not be presented as a general BSAT win.
On the sole verified input, fixed BSAT is faster than Kissat, but five of six
BSAT problems remain unfinished.

| Family | Fixed peak RSS MB | Growing peak RSS MB |
| --- | ---: | ---: |
| Station repacking | 188.1 | 260.6 |
| Diagnosis | 246.9 | 266.0 |
| Scheduling | 318.2 | 384.8 |
| ordering-principle-xor | 122.4 | 151.2 |
| School timetabling | 312.4 | 312.5 |
| Independent-set reconfiguration | 227.7 | 444.0 |

Fixed BSAT's highest peak is 318.2 MB, growing BSAT's is 444.0 MB, and Kissat's
is 705.5 MB on ordering-principle-xor. Peak measurements depend on how long
each run executes. On school timetabling, growing intervals reduce the number
of reductions from 44 to 12 but increase conflicts from 88,309 to 90,664 and
process CPU by about 35%. Fewer reductions do not imply faster solving.

## Decision

Keep growing intervals disabled by default and exclude them from the general
experimental competition profile. The opt-in control retains a documented
hardware development use case, but this fresh screen does not support a broad
performance improvement. Subsequent policy tuning on any of these inputs must
label them development data. Competition performance and accuracy remain
unproven; broader evaluation and the outstanding certificate check remain work
to do.

## Evidence

The manifest and policy were written before execution:

- `benchmark_results/reduce-growth-fresh-corpus-20260907.json`: input metadata,
  hashes and exclusion-history hashes.
- `benchmark_results/reduce-growth-fresh-policy-20260907.json`: exact commands,
  frozen manifest hash, settings and binary/checker hashes.
- `benchmark_results/reduce-growth-fresh-20260907.json`: raw run results,
  verification, process CPU, wall time, memory and solver counters.
