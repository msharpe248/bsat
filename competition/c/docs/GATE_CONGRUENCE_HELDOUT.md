# Gate congruence: larger held-out screen

Congruence increased verified solves from **1/6 to 2/6**; pinned Kissat solved
**3/6**. All completed answers were independently checked UNSAT results. No
wrong answers or execution errors were reported. This reproduces the hardware
benefit on a previously unused instance, but does not establish competition
parity or a general benefit across families.

This screen evaluates the opt-in configuration committed in `65ac319` on six
SAT Competition 2025 inputs not previously named or hashed in the repository's
recorded JSON benchmark history. The input manifest and execution policy were
written before the first run. No implementation or configuration tuning occurred
during the screen.

## Method

The existing deterministic selector chose the smallest eligible input of at
least 8 MB from each of six distinct families. Inputs range from 8.19 to 9.56 MB,
with 11,086–167,755 variables and 389,661–489,362 clauses. Selection excluded
filenames and SHA-256 values from 320 prior JSON records. This is a size-biased
sample, not a representative competition suite, and each profile runs only once
per input.

Both BSAT profiles use the same committed binary, `--equiv --equiv-budget
100000000 --alternating --vmtf --time 30`. The candidate additionally enables
`--congruence`, using its default 100,000,000 work allowance. Kissat is pinned
to `8af8e56f174b778aef3aa45af9f739b2a5f492c2` (4.0.4). All solvers emit proofs.

Seed 20261229 shuffles the serial execution order. Each job has a 35-second
external wall limit; BSAT also has a 30-second solving CPU limit, excluding
initial parsing. Reported process CPU includes parsing and proof output. Thus
Kissat can use the full external limit, while BSAT can stop earlier. No builds,
validation suites or profiling jobs ran concurrently with solver timing.

SAT models are checked against the original formula. UNSAT proofs must pass
the pinned external `drat-trim` checker, outside solver timing, with a separate
120-second check timeout. Only verified answers count as solved. UNKNOWN is
not a certificate. Unsolved runs receive a 70-second wall PAR-2 penalty.

## Results

Process CPU seconds for verified UNSAT answers; UNKNOWN means no answer within
the configured limits:

| Family | BSAT without congruence | BSAT with congruence | Kissat |
| --- | ---: | ---: | ---: |
| Oddball weighing | UNKNOWN | UNKNOWN | 1.715 |
| Hardware model checking | UNKNOWN | 11.973 | 1.240 |
| Belpyramid puzzle | UNKNOWN | UNKNOWN | UNKNOWN |
| Tseitin formulas | UNKNOWN | UNKNOWN | UNKNOWN |
| Multiplier verification | 0.358 | 0.373 | 5.037 |
| Knights problem | UNKNOWN | UNKNOWN | UNKNOWN |

Mean wall PAR-2 was 58.400 seconds without congruence, 48.745 with congruence,
and 36.381 for Kissat. The improvement comes from the additional hardware solve.
Both BSAT profiles solved the multiplier input faster than Kissat in this run;
that result does not show a benefit from congruence.

Memory remains a concern. On belpyramid, whole-run peak RSS increased from
121.9 MB to 245.1 MB; on knights, from 289.7 MB to 389.6 MB. Neither solved.
These measurements include time-dependent search allocation and do not isolate
temporary gate storage. On the solved hardware input, congruence peaked at
193.3 MB versus Kissat's 103.4 MB.

The policy remains opt-in. The next work should investigate the remaining
search gap and selective preprocessing costs, rather than promote this
configuration to the default from six short-budget observations. These inputs
become development data if used to choose or tune subsequent changes; future
generalization claims need another frozen held-out set.

## Reproduction records

- `benchmark_results/congruence-fresh-corpus-20260907.json`: input hashes,
  family/size metadata and the exclusion-history hashes.
- `benchmark_results/congruence-fresh-policy-20260907.json`: settings and
  manifest hash frozen before execution.
- `benchmark_results/congruence-fresh-20260907.json`: commands, binary/checker
  hashes, per-run status, verification, timing, memory and solver counters.

The earlier hardware development result and its repeated matched comparison
are documented separately in [GATE_CONGRUENCE.md](GATE_CONGRUENCE.md).
