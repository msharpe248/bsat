# Rejected prototypes: stable search limited by preceding focused work

Both tested variants lost the school-timetabling solve and reduced verified
solves from two of four to one of four. They are archived and removed from
the live solver; the original conflict-based schedule is restored.

The rejected [mode-dependent branching prototype](MODE_BRANCHING.md) changed
heuristic roles while retaining BSAT's conflict-only mode schedule. The pinned
local Kissat snapshot's
[mode.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/mode.c)
instead uses a focused conflict limit and a stable work allowance derived from
the preceding focused phase. This experiment isolates that broad scheduling
idea while retaining BSAT's existing branching, restart and phase policies.

## Initial prototype policy and implementation

The archived `--work-modes` prototype enables alternating search. Focused search starts
with a 1,000-conflict allowance. On entering stable search, the work consumed
by that focused phase becomes the stable allowance. Returning to focused
search doubles its conflict allowance to 2,000, then 4,000, and so on. The
next focused limit is relative to the current conflict count.
An exhausted stable allowance is acted on at the next conflict-free restart
opportunity. Pending conflict handling completes first, so work can overshoot
the nominal allowance.

Work is the saturating sum of the existing watch/collection work counter,
propagated literals, and minimization inspections. Propagations ensure that
decisions on variables without watches still consume allowance. This is a
deterministic approximation of search effort; the hardware cost per unit
varies. No new per-literal counter is added.

The initial baseline is recorded after preprocessing, including SCC solver
replacement. Subsequent mode entries record a new baseline, and rebuilt API
attempts start fresh. At least one work unit is allowed. Additions saturate,
and saturated limits disable further transitions on that counter. Existing
restart-disable and non-alternating behavior is preserved. The old conflict
schedule remains the default. `Work mode switches` counts transitions under
the experimental control.

This does not reproduce Kissat's full mode policy: BSAT retains its own work
proxy, focused interval growth, restart feedback and phase handling.

## Validation

Initial prototype release and ASan/UBSan builds each passed 48 C test executables. The new test
checks exact focused/stable boundaries, a stable transition without new
conflicts, propagation and minimization costs, work/limit saturation,
preprocessing exclusion, SCC replacement, assumptions and repeated solves.
Four mutations are caught: ignoring work mode limits, omitting propagation
cost, including preprocessing work and wrapping the next conflict limit.

Each build also passed eight exact SAT/model and UNSAT/certificate checks on
thirteen-variable assignment exclusions with an independent alias, in text and
binary proof modes, with and without SCC. Every SAT and UNSAT run switches
into and out of stable search; models and UNSAT proofs are independently
verified. This covers mode transitions that the
general validator's small random formulas usually do not reach.

Both builds passed 75 short-deadline cases, with a maximum observed CPU
overrun of 0.001 seconds. Twelve default traces match the pre-change binary
in status, return code, 24 counters and proof bytes. Incomplete trace proofs
are not answer certificates.

Each build also passed 5,141 general independent solves across 97 configurations
(seed 20261319), checking truth-table answers, original models and text/binary
proofs with external drat-trim. Source/binary hashes, full logs, deadline cases
and transition certificates are in
`benchmark_results/work-modes-validation-20260907.json`. Mutation and default
trace records use the same prefix.

## Frozen development comparison

The command policy and four reused inputs are frozen before timing in
`benchmark_results/work-modes-policy-20260907.json`. Both profiles use the same
candidate binary, chronology, gate congruence, SCC with a 100-million-work
budget, alternating search and VMTF. Only the work profile adds `--work-modes`.
Seed 20261320 shuffles one serial repetition per input/profile. The budgets
are 30 seconds of solving CPU, 35 seconds external wall and 600 seconds for
independent checking outside timing. No builds, tests or profiling compete
with the timed runs. This is development evidence, not a held-out screen.

The initial variant solved one of four inputs versus two for the conflict
schedule. School timetabling changed from SAT verified in 4.082 process CPU
seconds to UNKNOWN at 30.107 seconds. Hardware UNSAT remained verified but
slowed from 1.742 to 1.878 seconds. Diagnosis and station repacking timed out
in both profiles. Mean wall PAR-2 worsened from 36.877 to 53.064 seconds, with
no benchmark errors. Full results are in
`benchmark_results/work-modes-initial-20260907.json`.

## Follow-up with original focused interval growth

The initial variant also changed the growth of focused intervals. A bounded
follow-up preserves their original lengths: 1,000 conflicts initially,
2,000 on the first return to focused search, then 8,000, 32,000 and so on.
Stable phases still receive the preceding focused phase's measured work.
This more directly tests the stable work allowance without also replacing
the focused growth sequence.

The initial implementation and tests are preserved against `498b8f5` in
`benchmark_results/work-modes-initial-prototype-20260907.patch`; use
`git apply --unidiff-zero`. An isolated apply check reconstructs all seven
source/test files exactly. The companion JSON pins the tested binaries and
source hashes. The follow-up uses the same inputs and budgets, with seed
20261321; its commands are frozen in `work-modes-matched-policy-20260907.json`.

The follow-up also passed 48 C tests, eight transition certificate/model cases
and 75 deadline cases in each build, plus four mutations and twelve default
trace comparisons. Its release passed 5,141 independent solves. General debug
fuzzing was performed on the initial variant; the follow-up repeated the C,
sanitizer, transition-certificate and deadline checks instead. Exact scope,
source hashes and logs are in `work-modes-matched-validation-20260907.json`.

The matched-growth result is still negative:

| Input | Original conflict schedule | Work limit with original focused growth |
| --- | --- | --- |
| Diagnosis | UNKNOWN | UNKNOWN |
| Station repacking | UNKNOWN | UNKNOWN |
| School timetabling | SAT verified, 4.141 s CPU | UNKNOWN, 30.088 s CPU |
| Hardware model checking, 7bdf3e54 | UNSAT verified, 1.829 s CPU | UNSAT verified, 1.818 s CPU |

Mean wall PAR-2 worsens from 36.902 to 52.979 seconds. All completed answers
are independently verified; neither comparison reports benchmark errors.
The hardware timing difference is too small to establish a gain from one
repetition, and it does not offset a lost solve. Full results are in
`benchmark_results/work-modes-matched-20260907.json`.

## Rejection and restoration

Neither tested policy warrants retention. This does not establish that every
work-based mode policy fails; it rules out these two tested combinations as
improvements on this development screen. No further interval tuning is
promoted from these results.

The follow-up source and tests are preserved in
`benchmark_results/work-modes-matched-prototype-20260907.patch`, independently
applicable to `498b8f5` with `git apply --unidiff-zero`. Its isolated apply
check reconstructs all seven source/test files exactly. Companion JSON
records pin both variants' source, binary and validation hashes. Both
prototype CLI options and test additions are removed from the live tree.

After restoration, all tracked C sources, headers and tests match `498b8f5`
exactly. Release and ASan/UBSan builds each passed 47 C test executables.
The rebuilt release binary is byte-identical to the preserved baseline; no
debug binary identity is claimed. Full restoration hashes and test logs are
in `benchmark_results/work-modes-restored-20260907.json`.
