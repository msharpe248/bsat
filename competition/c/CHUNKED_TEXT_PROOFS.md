# Chunked text proof encoding

A diagnostic sample of the `62af9c4` C solver on the hardware-verification input
found 220 of 453 sampled stacks under text proof output, mostly in per-literal
`fprintf`. This includes both learned-clause additions and deletions during
reduction. Sampling is diagnostic, not a precise CPU attribution or speed result;
the raw [report](benchmark_results/text-proof-profile-20260907.txt) and
[metadata](benchmark_results/text-proof-profile-20260907.json) pin the optimized
symbolized executable, input and sampler settings.

## Implementation

The candidate encodes text DRAT into a bounded 4,096-byte stack buffer and writes
chunks through `fwrite`. Unsigned decimal conversion replaces general-purpose
formatted I/O for each literal. The format is unchanged: optional `d `, signed
decimal literals followed by spaces, then `0\n`. The buffer reserves space for
the largest representable literal before each conversion and for the final
terminator. There is no per-clause heap allocation or persistent output buffer.

The binary encoder is unchanged. The same FILE stream remains responsible for
stdio buffering, export, ownership transfers and final flush. A short chunk
write marks the solver erroneous; the existing stream-error and final-flush
checks remain active, so output failures return UNKNOWN. Search state,
assumption order, proof order and clause contents are unchanged. CPU limits may
naturally stop at different conflicts as encoding cost changes.

## Correctness and error checks

The new test executable checks **288 byte-comparison cases**, each containing
addition, deletion and empty records. It covers text and binary formats,
buffered/unbuffered streams, decimal and varint boundaries, mixed signs, chunk
boundaries, exact-fit and split terminators, and long clauses. Formatter tests reach the literal representation's
signed integer limit without allocating those variables in a solver. Reference
text uses standard `fprintf`; binary reference encoding uses base-128 division.
Disabled proof output is also checked.

Controlled output sinks test **22 immediate output cutoffs** and **two deferred
flush failures**. Every case requires UNKNOWN rather than an uncertified answer.
The controlled sinks are enabled on macOS and glibc platforms; this milestone
was tested on macOS. Other platforms still run the byte comparisons. Both the baseline and candidate pass the new executable.
Three mutation builds—missing minus signs, missing final chunks and ignoring
final flush errors—are caught by the byte or result checks. The
[mutation record](benchmark_results/text-proof-mutations-20260907.json) pins the
exact test and candidate source hashes.

The [untimed trace comparison](benchmark_results/text-proof-traces-20260907.json)
checks five competition inputs at 1,000 conflicts in both text and binary modes.
Proof bytes and all 21 selected search counters match the baseline in all ten
cases. Incomplete proof prefixes are compared for determinism, not treated as
certificates for UNKNOWN answers.

Both builds pass **40 C test executables**. Release and ASan/UBSan debug validation each pass **3,139
certificate-checked solves**, 73 configurations, seed 20261124. Both builds pass 42 deadline cases with maximum observed
CPU overrun 0.000 seconds. These finite checks do not establish universal soundness.

## Performance and decision

Retain the encoder: across five reused development inputs, the geometric mean
of candidate/baseline median process CPU ratios is **0.7064**, or **29.36% less
CPU with text proof logging**. This is not a claim about proof-disabled runs.
The [fixed-work comparison](benchmark_results/text-proof-work-20260907.json)
uses default search, a 120,000-conflict cap, three repetitions, a 30-second
external wall limit and seed 20261125. All 21 selected search counters match
across all six runs on each input. Both versions verify 9/15 runs; the others
reach the conflict cap. No errors occur.

| Input family | Baseline median CPU (s) | Chunked median CPU (s) |
| --- | ---: | ---: |
| battleship | 2.391723 | 0.943781 |
| belpyramid-puzzle | 5.397898 | 5.263559 |
| hamiltonian | 0.153259 | 0.128298 |
| hardware-verification | 5.995178 | 3.438319 |
| ktf | 18.250955 | 17.379904 |

Maximum process RSS is 62,046,208 versus 61,014,016 bytes. Mean wall PAR2 is
25.995 versus 25.402 seconds; UNKNOWN penalties dominate that metric.

The [format guard](benchmark_results/text-proof-format-guard-20260907.json)
compares binary proof and disabled proof output on hardware-verification and
ktf, at 50,000 conflicts, twice per profile, seed 20261126. All 21 counters
match across profiles. No material regression is observed: median CPU changes
range from roughly -2.3% to +0.8%. Two repetitions do not establish a precise
performance bound. All 16 runs return UNKNOWN at the cap, so these are not
additional certified answers.

## Larger problems and remaining gap

The [larger screen](benchmark_results/text-proof-large-20260907.json) compares
both BSAT versions with a pinned Kissat executable on eight reused SAT
Competition 2025 inputs of about 5–5.7 MB, one repetition each, with a 30-second
external wall limit, seed 20261127. Inputs span 7,200–90,662 variables and
239,878–372,720 clauses. **Both BSAT versions verify 1/8; Kissat verifies 3/8**.
All completed SAT models and UNSAT proofs pass independent checks. Other runs
return UNKNOWN at the deadline; there are no erroneous completed answers.

All three solve multiplier-verification. Kissat additionally solves ktf (SAT,
20.37 seconds wall) and hardware-verification (UNSAT, 17.67 seconds). All three
time out on grs-fp-comm, scheduling, oddball-weighing, unknown-cases and
hamiltonian-cycle. Mean wall PAR2 is 52.509 seconds for either BSAT version
versus 42.661 for Kissat. The text encoder adds no solve at this cutoff.

Peak RSS in this timed screen rises from **252,837,888 to 304,496,640 bytes**
for BSAT; Kissat peaks at 149,110,784. All maxima are on hamiltonian-cycle.
The encoder adds only a bounded stack buffer, but killed runs lack final search
counters, so the cause of the higher observed peak is not established.

A [memory control](benchmark_results/text-proof-memory-guard-20260907.json)
on that same hamiltonian-cycle input uses 10,000 conflicts and two repetitions
per version, seed 20261128. All 21 counters match; maximum RSS is 38,535,168
versus 38,666,240 bytes (about +0.34%). All four runs return UNKNOWN at the
conflict cap. This finds no substantial early fixed-work memory increase, but
does not explain or rule out the higher peak later in the 30-second runs.
The first invocation used a nonexistent input path and failed before any solver
ran; the archived measurement uses the path from the larger-screen manifest.

These are development measurements on one macOS arm64 machine, not held-out
competition results. They support retaining a text-proof throughput improvement,
not competition parity or universal soundness. A broader held-out corpus and
longer time limits are still needed to assess competition performance.

## Reproduction

Build and test with `make -C competition/c all test`, then repeat with
`MODE=debug`. Run `tests/validate.py` for each binary with `--cases 30 --seed
20261124` and an independent DRAT checker; run `tests/check_deadlines.py` for
both builds. Each benchmark JSON records exact solver commands, input paths,
seeds, limits and executable/input/checker SHA-256 hashes. The baseline is
revision `62af9c4`; the candidate is this commit's source. Performance jobs ran
serially, without concurrent builds, validation or sampling. Certificate checking
runs outside solver timings. UNKNOWN receives twice the wall limit in PAR2.
