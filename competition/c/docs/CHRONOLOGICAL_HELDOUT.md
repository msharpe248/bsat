# Chronological backtracking: larger held-out screen

Both BSAT profiles solved **0/6**; Kissat solved **3/6**, with independently
verified answers. BSAT returned UNKNOWN on every case. The earlier development
improvements therefore do not generalize to a solve-count gain on this sample.

This screen freezes the configuration committed in `d231d65`. Six previously
unused SAT Competition 2025 inputs were selected before execution, and no runtime
or configuration tuning occurred during the comparison.

The deterministic selector chose the smallest eligible files of at least 9.5 MB,
one per family, excluding filenames and hashes from 337 existing JSON records.
The resulting inputs span 9.70–11.16 MB, 2,520–166,266 variables and
345,528–532,892 clauses. This small, size-biased sample is not a representative
competition evaluation.

## Method

Both BSAT profiles use the same committed binary with `--congruence --equiv
--equiv-budget 100000000 --alternating --vmtf --time 30`. The candidate adds
`--chrono`, using the default threshold of 100. The reference is Kissat 4.0.4
at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.

Seed 20261306 shuffles one repetition per profile and input. Jobs run serially
with a 35-second external wall limit. BSAT additionally has a 30-second solving
CPU allowance, which excludes initial parsing; Kissat can use the full external
limit. Process CPU measurements include parsing and proof output. No builds,
validation suites, diagnostics or profiling ran concurrently with timing.

SAT models are checked against original inputs. UNSAT answers require the pinned
external `drat-trim` checker, outside solver timing, with its own 120-second
timeout. Only independently verified answers count as solved. UNKNOWN is not an
answer certificate. Unsolved runs receive a 70-second wall PAR-2 penalty.

## Results

Verified results and process CPU seconds:

| Family | BSAT baseline | BSAT chronology | Kissat |
| --- | --- | --- | --- |
| Hardware model checking | UNKNOWN | UNKNOWN | SAT, 5.203 s |
| Oddball weighing | UNKNOWN | UNKNOWN | UNSAT, 2.057 s |
| grs-fp-comm | UNKNOWN | UNKNOWN | UNKNOWN |
| reg-n | UNKNOWN | UNKNOWN | UNSAT, 0.727 s |
| Rooks | UNKNOWN | UNKNOWN | UNKNOWN |
| xor_op | UNKNOWN | UNKNOWN | UNKNOWN |

Mean wall PAR-2 was 70.000 seconds for each BSAT profile and 36.444 for Kissat.
Peak RSS was 1,070.7 MB for baseline and 1,113.9 MB for chronology, both on
`grs-fp-comm`. Kissat's largest observed peak was 620.8 MB on `xor_op`; on
`grs-fp-comm` it used 196.9 MB. These are whole-run, time-dependent measurements.
There were no execution errors; no BSAT answer was certified on this set.

The chronological option remains experimental. This screen does not justify
default promotion or a competition-readiness claim. Its short budgets and small,
size-biased sample also do not quantify performance on the full competition suite.

During read-only investigation, the equivalence-rebuild path was found to omit
the preallocated `level_seen` array and `levels_capacity` from the replacement
solver. A small diagnostic, run only after timing ended, reproduced a missing
scratch array and a learned binary with LBD zero. Zero LBD protects clauses as
glue and prevents the usual positive-LBD moving-average restart signal. That
state-transfer defect is the next fix; it was not changed during this screen.
The diagnostic does not establish how much of the measured gap it explains.

## Reproduction records

- `benchmark_results/chrono-fresh-corpus-20260907.json`: input metadata and hashes,
  plus the hashed exclusion history.
- `benchmark_results/chrono-fresh-policy-20260907.json`: execution policy and
  manifest hash frozen before the first run.
- `benchmark_results/chrono-fresh-20260907.json`: exact commands, binary/checker
  hashes, per-run results and counters.

The prior development improvements are documented separately in
[LOGICAL_CHRONOLOGICAL_BACKTRACKING.md](LOGICAL_CHRONOLOGICAL_BACKTRACKING.md).
These fresh inputs become development data if used to choose subsequent changes.
