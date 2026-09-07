# Rejected experiment: local work counter in clause scans

The candidate cached `s->work` in a local variable while scanning a long clause,
writing it back at the existing 1,024-work polling boundaries and after the scan.
The hypothesis was that avoiding a solver-field store for every literal could
reduce propagation cost while preserving inspection order and budget checks.

The [source patch](benchmark_results/scan-work-rejected-20260907.patch) is retained
for reproduction. Production source is restored to `12b1430`; only the new
regression and evidence are retained. No new option or runtime behavior remains.

## Measurement

The [fixed-work comparison](benchmark_results/scan-work-pilot-20260907.json)
reuses the 28-input compact-trail development manifest. Both profiles have a
10,000-conflict cap, three solving CPU seconds, five external wall seconds and
three repetitions per input, with seeded serial order (20261101). Builds and tests
complete before timing starts. SAT models and UNSAT proofs are independently
checked outside solver timing. Both verify the same **9/84** runs with zero
errors; UNKNOWN is not a certified answer.

Eleven counters match across all six runs on every input: decisions, propagations,
conflicts, restarts, learned clauses/literals, deleted clauses, minimization
inspections, garbage collections, reductions and literal inspections. This gives
strong evidence that the candidate preserved the tested search and work paths,
rather than changing search to make the benchmark finish earlier.

The [derived summary](benchmark_results/scan-work-summary-20260907.json) includes
per-input medians and the selection rule: retain inputs with matching counters
and baseline median process CPU above 0.05 seconds. All 28 match counters; 26 meet
the time threshold. Their geometric mean candidate/baseline CPU ratio is
**1.013283**, about **1.3% slower** in this measurement. Individual results vary
in both directions. Mean wall PAR-2 is 8.936604 versus 8.936751 seconds; maximum
process RSS is 42,631,168 versus 42,319,872 bytes.

This does not establish a worthwhile speedup, so reject the extra code. The result
does not prove that local counters are universally slower, but it does not
justify retaining this implementation in the default hot path. Process CPU
includes startup, parsing and proof output; the internal solve limit excludes
parsing. Unverified runs receive twice the wall limit as PAR-2. This short,
reused development screen is not held-out competition evaluation.

## Retained regression and checks

`tests/test_scan_work.c` exercises **108** combinations of long-clause circular
scan positions and initial work offsets around polling boundaries. It checks
first/late/wrapped watch replacement, unit propagation, conflicts, and interrupted
scan replay. Expected inspection counts use the circular distance directly.
After a budget stop, the pending watch and trail position must remain usable;
resuming must charge the repeated scan exactly and install the correct reason.
These tests apply to both the original loop and future rewrites.

The candidate passed all 37 release C test executables before the performance
screen. After rejection, both restored release and ASan/UBSan builds pass all
37 executables, including the new test. No full external randomized validation
was run on the rejected candidate; its timing runs checked completed answers.
The restored solver is unchanged, so the earlier validation remains applicable.
The rebuilt release executable matches the preserved baseline hash. The archived
patch applies cleanly to the restored source. Candidate timing hashes match the
preserved rejected executable; baseline hashes match the restored executable.

## Reproduction

Build `12b1430` and preserve its executable as `/tmp/bsat-scan-work-baseline`.
Apply the archived patch, build the candidate, then run:

```sh
make -C competition/c all test
python3 competition/c/tests/benchmark.py --checker /tmp/bsat-drat-trim \
  --solver 'stored=/tmp/bsat-scan-work-baseline --conflicts 10000 --time 3 --proof {proof} {input}' \
  --solver 'local=competition/c/bin/bsat --conflicts 10000 --time 3 --proof {proof} {input}' \
  --timeout 5 --repeats 3 --seed 20261101 \
  --manifest competition/c/benchmark_results/compact-trail-corpus-20260907.json \
  --output /tmp/scan-work-repeat.json
```

Remove the candidate patch to return to the retained implementation. Raw records
pin executable/input/checker hashes and exact command templates.
