# Growing learned-clause reduction intervals

The preceding [policy investigation](EQUIV_LBD_POLICY_INVESTIGATION.md) found
that fixed, less frequent deletion did not recover the random-circuit solve
after repairing LBD state. This experiment tests a different policy: let the
gap between reductions grow as the search continues, retaining more learned
information during longer searches while still deleting ranked candidates.

## Implementation

Experimental `--reduce-increment N` adds N conflicts to the interval after each
scheduled reduction. With `--reduce-interval 2000 --reduce-increment 1000`,
the reductions occur at conflicts 2,000, 5,000, 9,000, 14,000, 20,000, and 27,000.
The first reduction uses the original interval. Zero increment preserves the
existing fixed modulo schedule and is the default.

The schedule is initialized after preprocessing, including any SCC solver
replacement, and starts fresh on each API solve. Two 64-bit fields store the
interval and next conflict limit. Addition saturates rather than wrapping;
a saturated limit disables further scheduled reductions. Clause ranking,
locked/binary/glue protection, deletion, proof logging and garbage collection
are unchanged. Growing the interval is not a bound on solver memory.

## Correctness checks

The C regression checks the exact fixed and growing schedules and overflow
saturation. It also excludes all eight-variable assignments to force actual
search and deletions, with an independent alias that exercises SCC replacement.
Eight combinations of SCC, chronology and heap/VMTF each run three repeated
queries, including an assumption query. The test checks UNSAT and the expected
number of reductions from the number of learned clauses, as well as actual
deletions and expected substitution.

Release and ASan/UBSan builds each passed 47 C executables. Three mutations
were caught: always using the fixed schedule, omitting interval growth, and
allowing deadline arithmetic to wrap. Twelve three-way comparisons checked
the prior binary, candidate defaults and explicit zero increment, including
24 counters and proof bytes. Those traces stop at 1,000 conflicts, below the
default reduction interval; the C schedule test checks the higher boundaries.
Incomplete trace proofs are not answer certificates.

Both builds passed 69 short-deadline cases. The maximum observed solving CPU
overrun was 0.002 seconds for release and 0.001 seconds for debug.

Each build also passed 4,876 independent solves: 53 formulas across 92
configurations, seed 20261311. The validator checks truth-table answers,
original models and text/binary RUP proofs, and uses external drat-trim on
UNSAT answers. Commands, full logs and source/binary hashes are recorded in
`benchmark_results/reduce-growth-validation-20260907.json`. The mutation and
default-trace records use the same filename prefix.

## Frozen development comparison

`benchmark_results/reduce-growth-policy-20260907.json` freezes four reused
development inputs and the commands before timing. The two profiles use the
same candidate binary and enable chronology, congruence, SCC with a
100-million-work budget, alternating search and VMTF. Only the candidate
profile adds `--reduce-increment 1000`; the other keeps the fixed interval.
Seed 20261312 shuffles one serial repetition per profile and input. Each
process has 30 seconds of solving CPU and a 35-second external wall limit.
Checking runs outside timing. This is not a fresh held-out evaluation.

| Input | Fixed process CPU | Growing process CPU | Peak RSS fixed → growing |
| --- | --- | --- | --- |
| Hardware model checking, 546f8e06 | SAT verified, 24.810 s | SAT verified, 22.907 s | 181.2 → 246.3 MB |
| Random circuits, 84995056 | UNKNOWN, 30.047 s | UNKNOWN, 30.058 s | 73.8 → 168.0 MB |
| Hardware model checking, 7bdf3e54 | UNSAT verified, 1.871 s | UNSAT verified, 1.847 s | 191.9 → 193.4 MB |
| Belpyramid puzzle, 5831f356 | UNSAT verified, 1.448 s | UNSAT verified, 1.376 s | 47.4 → 56.6 MB |

Both profiles solved three of four, with no benchmark errors. Mean wall PAR-2
was 25.961 seconds for fixed and 25.095 for growing. The random-circuit
regression is not recovered, and its memory more than doubles. On the larger
hardware SAT case, reductions fall from 402 to 33, with a roughly 7.7% CPU
improvement but roughly 36% greater peak RSS. This is a development tradeoff,
not a solve-count improvement or evidence of competition readiness.
Full results are in `benchmark_results/reduce-growth-initial-20260907.json`.

## Confirmation and decision

Two more serial repetitions per profile on hardware SAT and belpyramid use
the same settings and budgets, with seed 20261313. All eight answers were
independently verified, with no benchmark errors.

Hardware SAT took 21.836–22.779 process CPU seconds with fixed reductions and
19.481–20.094 seconds with growing intervals, an approximately 11.3% reduction
in mean CPU. Peak RSS was 176.8–177.2 MB versus 244.3–246.3 MB. This confirms
the initial speed gain on that development input, with increased memory.
Belpyramid timings overlap: fixed 1.183–1.188 seconds and growing 1.098–1.226
seconds. There is no consistent belpyramid speed gain in the repeat runs.

Retain the implemented control as opt-in. The repeatable hardware benefit
justifies further evaluation, but the unchanged solve count, random-circuit
timeout and higher memory do not justify a default change. A fresh corpus
evaluation is recorded in [REDUCTION_GROWTH_HELDOUT.md](REDUCTION_GROWTH_HELDOUT.md):
it found no added solves, a slower completed case and higher memory on all six
inputs. Growing intervals remain excluded from the general experimental
competition profile. Confirmation commands, hashes, timings and
verification results are in
`benchmark_results/reduce-growth-confirm-20260907.json`.
