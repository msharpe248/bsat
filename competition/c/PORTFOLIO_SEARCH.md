# Focused-first sequential portfolio

The larger-input development screen showed a complementary search result:
focused search solved battleship, while experimental alternating search solved
ktf quickly. Neither profile reliably covered both. This experiment gives the
existing focused search a CPU slice, then starts alternating search from the
original formula if that slice expires.

## Implementation and contract

`--portfolio 2` invokes the C API `solver_solve_portfolio(s, 2.0)`. The first
attempt uses focused search for at most two solving CPU seconds. A completed,
validated answer returns immediately. If only that private time slice expires,
a fresh solver is reconstructed from the immutable input, and its search uses
`--alternating`, including the existing 1,000-conflict initial focused interval.
Other search and preprocessing settings are preserved. The portfolio selects
both modes itself, regardless of the incoming `alternating` option.

`--time` bounds both attempts together, including rebuilding, model checking and
proof flushing. The CLI's initial DIMACS parsing remains outside this solving
CPU interval. Conflict and decision limits are also shared: the second attempt
receives only the unused count. A total limit, allocation failure, certificate
error or other non-time-slice UNKNOWN does not authorize another attempt.
An unlimited total time permits an unlimited second attempt.

Rebuilding polls the total deadline while restoring variables and clauses.
Allocation, a single clause insertion and cleanup are not preemptible; this is
cooperative deadline enforcement, not a hard real-time guarantee. The old solver
remains available if reconstruction fails. The second attempt discards the first
attempt's learned clauses, activities, saved phases and proof. Its certificate
starts from the original CNF. Closing or reopening a proof unsuccessfully returns
UNKNOWN. A manually attached proof stream without `opts.proof_path` is refused,
since there is no path with which to reset its certificate. There is no portfolio
assumption API. Options are restored on return; ordinary repeated solves remain
supported.

Ordinary search counters describe the final attempt. The additional portfolio
fields expose the number of attempts, first-attempt conflicts and decisions,
and their totals. CPU time covers the whole portfolio; do not interpret a
final-attempt counter divided by that time as combined search throughput.
A two-second time slice is intentionally machine-dependent. It is not a fixed
conflict schedule and does not promise identical search work across runs.

The ordinary default remains unchanged. This is an experiment in combining
existing strategies, not a new logical inference rule.

## Correctness evidence

The retained C test checks 512 portfolio answers against independent truth
tables for 256 signed formulas, including repeated calls, equivalence
substitution, BCE and BVE, then ordinary solving again. It also exercises global
CPU, conflict and decision limits after actual work in both attempts; text and
binary proof reset; certificate failures; and refusal of an unresettable stream.
Sub-clock slices encourage fallback on small formulas; an immediate root answer
may legitimately complete within one clock tick without falling back.

Both release and ASan/UBSan builds pass all 44 C test executables. The independent
validator checks 3,311 solves per build (77 configurations on 43 formulas,
seed 20261213), including four portfolio configurations. Original SAT models,
text/binary RUP proofs and external drat-trim checks all pass. Both builds pass
51 short-deadline cases; maximum observed CPU overruns are 0.000 seconds in
release and 0.001 seconds in debug. These finite checks do not establish
universal soundness.

Four deliberate mutations are detected: refreshing the conflict limit,
refreshing the decision limit, refreshing the CPU deadline, and hiding a
first-attempt certificate error. The disabled-portfolio trace guard compares
12 text/binary pairs across six existing inputs at 1,000 conflicts. All 24
selected counters and proof bytes match the pre-change executable; incomplete
traces are not treated as UNSAT certificates.

Records: [validation](benchmark_results/portfolio-validation-20260907.json),
[mutations](benchmark_results/portfolio-mutations-20260907.json),
[default traces](benchmark_results/portfolio-default-traces-20260907.json).

## Performance protocol

All timing jobs run serially after compilation, validation, mutation tests and
trace checks have finished. SAT models are checked against the original CNF;
UNSAT proofs are checked with external drat-trim outside solver timing, with a
120-second checker limit. Only verified answers count as solves. Wall PAR2
charges every UNKNOWN, failed check or error twice the external wall limit.
Raw records pin solver, checker and input hashes and preserve per-run counters,
process CPU, wall time and peak RSS.

The development comparison uses focused, alternating and portfolio search with
15 solving CPU seconds and a 20-second external wall limit, twice per input,
seed 20261214. The four inputs are battleship, belpyramid, ktf and hardware
verification. The two-second slice was fixed before this comparison and before
timing the fresh corpus.

The fresh corpus was selected from inputs absent by filename and SHA256 from
297 prior JSON records, with a minimum file size of 5 MB, one file per family,
smallest eligible file first. Its eight inputs span 5.83–8.32 MB, 3,248–160,903
variables and 192,064–499,890 clauses. This is a size-biased, finite selection;
it does not hold out entire families or represent a full competition suite.
See the [selection manifest](benchmark_results/portfolio-fresh-corpus-20260907.json).

## Development result

The [targeted comparison](benchmark_results/portfolio-targeted-20260907.json)
verifies **6/8 portfolio runs**, versus **4/8** for either standalone profile,
with zero errors. These counts represent two repetitions of four formulas.

| Family | Focused median CPU | Alternating median CPU | Portfolio median CPU | Portfolio outcome |
| --- | ---: | ---: | ---: | --- |
| battleship | 0.995964 s | UNKNOWN | 1.000979 s | SAT, 2/2 verified; first attempt |
| belpyramid | 5.430583 s | 4.673895 s | 6.660196 s | UNSAT, 2/2 verified; second attempt |
| ktf | UNKNOWN | 0.969306 s | 3.018452 s | SAT, 2/2 verified; second attempt |
| hardware verification | UNKNOWN | UNKNOWN | UNKNOWN | Neither attempt solves it |

The portfolio combines the complementary solves on this development set.
Belpyramid becomes **22.6% slower** than focused search, and ktf pays the
expected two-second failed attempt before its quick alternating solve. One
portfolio hardware run reaches the external wall deadline before its internal
CPU deadline, so final counters are absent; they must not be interpreted as
zero work. Mean wall PAR2 is 21.8751 seconds focused, 21.6860 alternating and
13.1824 portfolio. Peak process RSS is 60,751,872, 59,998,208 and 89,767,936 bytes,
respectively. Reconstructing the fresh solver temporarily holds both solver
states, so memory is a material portfolio cost.

## Fresh-input protocol

The fresh comparison uses a **30-second external wall limit for all four
profiles**, including pinned Kissat, with one repetition and seed 20261215.
There is no extra solver CPU limit in this comparison. This avoids giving one
solver less total wall time; external termination can leave no final statistics.
The portfolio still has its two-second first-attempt CPU slice. The selection
and slice are frozen before timing this corpus. Results belong to this screening
protocol, not to full competition time limits.

## Fresh-input result and decision

The [fresh comparison](benchmark_results/portfolio-fresh-20260907.json) verifies
**1/8 inputs for each BSAT profile, versus 3/8 for Kissat**, with zero errors.
Every completed SAT model and UNSAT proof passes independent checking. All
three BSAT profiles solve oddball-weighing; Kissat additionally solves
random-circuits (SAT, 7.155 seconds wall) and hardware-model-checking (UNSAT,
1.059 seconds wall). The five other inputs remain UNKNOWN for all four profiles.
This fresh screen adds no portfolio solve.

| Profile | Verified inputs | Mean wall PAR2 | Peak process RSS |
| --- | ---: | ---: | ---: |
| Focused | 1/8 | 53.2041 s | 344,817,664 bytes |
| Alternating | 1/8 | 52.9297 s | 371,638,272 bytes |
| Portfolio | 1/8 | 53.2988 s | 326,942,720 bytes |
| Kissat | 3/8 | 38.5833 s | 276,791,296 bytes |

All four memory maxima occur on hamiltonian-cycle. These are time-limited runs
with different search histories and progress, so the lower observed portfolio
maximum does not establish a general memory saving. On the solved oddball case,
RSS is 64,192,512 bytes focused and 86,982,656 portfolio, consistent with the
cost of holding the old and fresh solver during reconstruction.

Oddball process CPU is 3.629380 seconds focused, 2.306699 alternating and
4.302848 portfolio. Corresponding wall times are 5.633009, 3.437424 and 6.390067
seconds. The portfolio pays its two-second first attempt and is 18.6% slower
in process CPU than focused search in this one run. CPU and wall time differ
materially on this machine; one repetition does not establish a stable general
performance ratio.

**Retain the portfolio as an experimental, opt-in search strategy; do not enable
it by default.** The repeated development comparison demonstrates a useful
combination of existing strengths, but the frozen fresh-input screen shows no
additional solves and a cost on its only solved case. Neither result establishes
competition parity. The next performance investigation should address the
fresh random-circuits and hardware-model-checking gaps, including existing
preprocessing/search profiles before inventing another heuristic. Larger suites,
longer limits and more repetitions remain necessary.

Explicit callback cancellation is tracked separately from private-slice timeout
and never authorizes a second attempt, even if the callback consumed the slice.
See [CANCELLATION_BOUNDARIES.md](CANCELLATION_BOUNDARIES.md) for the reproduced
regression and correction.
