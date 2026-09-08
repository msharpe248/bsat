# Longer fresh evaluation

The completed 24-run evaluation verifies **4/12 BSAT runs versus 10/12 Kissat
runs**, with zero errors for either solver. Each solver's status repeats on every
input: BSAT solves two of six inputs twice, Kissat five of six twice. BSAT wins
polynomial multiplication but still trails the reference across this small sample.

## Frozen scope

Six distinct families were selected before timing by seeded family/file shuffling
(seed 20261334), excluding CNF filenames and content hashes in 399 existing JSON
benchmark records. The selected inputs were also checked against the two then
existing top-level JSON records, whose hashes are retained in the manifest.
Eligible file sizes were 1–64 MB; selected sizes are 3.7–56.1 MB. The largest input
has 622,818 variables and 2,552,775 clauses. These inputs become development data
after this evaluation; they must be excluded from future fresh-score samples.

BSAT uses the retained buffered-proof production binary, with `--chrono
--congruence --equiv --equiv-budget 100000000 --alternating --vmtf --binary-proof`.
Inprocessing, including the rejected ordering experiment, is not enabled. The
reference is the pinned Kissat 4.0.4 build at source revision
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`. Both write binary proofs.

Each solver runs twice per input, serially in shuffled order (seed 20261336),
with an equal 120-second external wall limit. This doubles the prior broad
60-second limit. There are no overlapping local builds, fuzz campaigns or other
benchmarks during solver timing. The host is macOS 26.6.1 ARM64, Apple Clang
21.0.0, production `-O3 -march=native -flto`. The binary hashes, commands and
policy were frozen before launch. The retained production binary is byte-identical
to the baseline used for the rejected selection experiment.

Every SAT model is checked against its original CNF. Every UNSAT proof is converted
to LRAT and checked by pinned cake_lpr against that same original CNF. Conversion
and checking are outside solver timing: each stage has a 600-second limit, with a
1,250-second enclosing checker limit. These scores do not measure end-to-end
certificate-generation-and-checking latency. Both UNSAT answers here are from
Kissat on reg-n; BSAT returns four checked SAT models and eight UNKNOWN results.
Large BSAT UNSAT evidence is separately recorded in `VERIFIED_CHECKING.md`.

## Results

Wall seconds, repetition 0 / repetition 1. UNKNOWN means the 120-second budget
expired and is never counted as a verified answer.

| Family | Variables | Clauses | BSAT | Kissat |
| --- | ---: | ---: | --- | --- |
| argumentation | 20,925 | 960,801 | SAT 30.578 / 30.654 | SAT 20.638 / 20.374 |
| at-least-two-sol | 622,818 | 2,552,775 | UNKNOWN / UNKNOWN | SAT 61.333 / 61.620 |
| clique-coloring | 6,050 | 205,556 | UNKNOWN / UNKNOWN | UNKNOWN / UNKNOWN |
| polynomial-multiplication | 57,935 | 229,320 | SAT 0.186 / 0.186 | SAT 5.706 / 6.107 |
| reg-n | 4,608 | 825,728 | UNKNOWN / UNKNOWN | UNSAT 1.708 / 1.663 |
| stedman-triples | 24,624 | 212,761 | UNKNOWN / UNKNOWN | SAT 80.393 / 80.575 |

Mean wall PAR-2, charging every unverified run 240 seconds, is **165.1336 seconds
for BSAT versus 68.3431 for Kissat**. Maximum solver-process RSS is
**1,373,978,624 bytes versus 875,675,648 bytes**. These RSS measurements exclude
subsequent model/certificate checking. BSAT reaches its peak on the largest input
and times out there in both repetitions; Kissat solves that input twice.

The polynomial-multiplication advantage is repeatable on this input. It is not a
general speedup claim. Argumentation, reg-n, Stedman-triples and the largest input
show substantial remaining search-performance gaps. Clause representation and
propagation cost remain relevant to the measured memory gap. The fresh sample is
small and differs from the prior twelve-family cohort, so solved fractions and
PAR-2 must not be interpreted as a before/after gain between those cohorts.

No wrong completed answer was observed. That is finite regression evidence, not
universal solver soundness. This is also not a target-server measurement: no Linux
host was supplied. `LINUX_PROFILING.md` documents the ready-to-run real PMU tooling
and the outstanding hardware evidence.

## Artifacts and completed checks

`benchmark_results/longer-fresh-corpus-20260907.json` and
`longer-fresh-policy-20260907.json` preserve selection and run policy.
`longer-fresh-comparison-20260907.json` retains all 24 results, work statistics
where available, CPU time, wall time and RSS. External timeouts may prevent final
solver statistics from being printed. `longer-fresh-audit-20260907.json` records
verification of all input/binary/checker/manifest hashes and the complete repeated
matrix. No unverified completed answer required artifact recovery.

All six hosted compiler/build configurations passed on retained production
milestone `5dd699a`: Linux GCC and Clang, plus macOS Clang, each in release and
ASan/UBSan modes. `compiler-matrix-20260907.json` records
[run 34185746931](https://github.com/msharpe248/bsat/actions/runs/34185746931).
The independent positive/negative verified-checker tests and counter-parser
regressions are included. The separate coverage-guided fuzzing workflow also
passes; local short and extended campaigns completed 4,800,716 executions without
an oracle or sanitizer failure. See `FUZZING.md` for their bounded scope.
