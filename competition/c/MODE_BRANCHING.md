# Rejected prototype: different branching heuristics by search mode

The tested prototype is archived and removed from the live solver. It solves
one of four development inputs versus two for the existing queue profile:
a hardware speed gain does not offset losing the school-timetabling solve.

## Motivation and existing work

A frozen three-input development comparison on diagnosis, station repacking
and school timetabling found no completed diagnosis or station solve within
30 seconds of solving CPU. Default BSAT solved none of the three. Chronology,
alternating search and VMTF solved school timetabling in 7.475 process CPU
seconds; adding gate congruence and SCC reduced that to 4.108 seconds, while
increasing peak RSS from 166.4 to 312.4 MB. This does not isolate a single
cause for the remaining diagnosis gap. Commands and results are in
`benchmark_results/search-profile-policy-20260907.json` and
`search-profile-diagnosis-20260907.json`.

The pinned local Kissat snapshot uses queue bumps/decisions in focused mode
and activity scores in stable mode: see
[bump.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/bump.c)
and [decide.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/decide.c).
BSAT's existing `--vmtf` profile instead uses the queue in both modes. This
experiment tests the same broad division of heuristic roles, using BSAT's
existing structures; it is not a port of Kissat's complete search policy.

## Implementation and invariants

The prototype's `--mode-branching` enabled alternating search and used VMTF
in focused mode and the activity heap in stable mode, overriding `--vmtf`.
Its C API set `opts.mode_branching` and `opts.alternating` explicitly. When
alternating is disabled, the configured ordinary heuristic remains active,
including during a portfolio attempt that disables alternating search.

Conflict bumps and score decay update only the active heuristic. The heap
already exists during queue search and keeps its ordering/assignment
bookkeeping, so entering stable mode needs no new allocation or heap rebuild.
The queue must receive unassignment notifications even during heap search:
otherwise backtracking can leave its cursor behind a newly available higher
priority variable and skip it at the next focused decision. Chronological and
ordinary backtracking both preserve this invariant.

`Mode queue decisions` and `Mode heap decisions` count decisions while this
control is active; their sum matches total decisions in a fully alternating
attempt. Defaults remain unchanged.

## Validation

Prototype release and ASan/UBSan builds each passed 48 C test executables. The new test
checks actual decisions through mode changes, queue cursor restoration during
heap backtracking, mode-specific activity updates, and repeated exact UNSAT
queries with assumptions, SCC, chronology and clause reductions. Three
mutations were caught: using the queue in stable mode, losing cursor updates,
and updating the wrong conflict heuristic.

Each build also passed eight dedicated SAT/model and UNSAT/certificate checks
on complete eleven-variable assignment exclusions, with an independent alias.
Every UNSAT proof exercises both modes and is externally verified in text or
binary format, with and without SCC. This covers mode transitions that the
small random formulas in the general validator usually do not reach.

Both builds passed 75 short-deadline runs, with maximum observed CPU overruns
of 0.000 seconds for release and 0.001 seconds for debug. Twelve default trace
cases match the previous binary in status, return code, 24 counters and proof
bytes; incomplete trace proofs are not certificates.

Full validation also exposed a false rejection in the preceding checker rule:
the pinned drat-trim exits one on its successful trivial-UNSAT path. The
compatibility fix requires both exact success/trivial markers for that exit,
and rejects conflicting verdicts. Ten harness tests, including real trivial
UNSAT proofs, pass. See [CERTIFICATE_ARTIFACTS.md](CERTIFICATE_ARTIFACTS.md) and
`benchmark_results/checker-trivial-compatibility-20260907.json`.

Each prototype build passed 5,141 independent validator solves across 97
configurations (seed 20261317). Full logs, source/binary hashes, deadline cases
and the dedicated transition-certificate results are in
`benchmark_results/mode-branching-validation-20260907.json`. Mutation and
default-trace records use the same filename prefix.

## Matched development result and rejection

The policy and four reused inputs were frozen before timing in
`benchmark_results/mode-branching-policy-20260907.json`. Both profiles use the
same candidate binary, chronology, congruence, SCC with a 100-million-work
budget, alternating search and VMTF. Only the prototype adds mode branching.
Seed 20261318 shuffles one serial repetition per input/profile. The budgets
are 30 seconds of solving CPU, 35 seconds external wall and 600 seconds for
independent checking outside timing. No builds or validation compete with
the timed runs.

| Input | Existing queue profile | Mode-branching prototype |
| --- | --- | --- |
| Diagnosis | UNKNOWN | UNKNOWN |
| Station repacking | UNKNOWN | UNKNOWN |
| School timetabling | SAT verified, 4.107 s CPU | UNKNOWN, 30.107 s CPU |
| Hardware model checking, 7bdf3e54 | UNSAT verified, 1.842 s CPU | UNSAT verified, 1.499 s CPU |

Mean wall PAR-2 worsens from 36.888 to 52.912 seconds. There are no benchmark
errors, and all completed answers are independently verified. School
timetabling decisions grow from 2,548,079 to 21,151,538; both prototype
heuristics are active. The one-run hardware gain is not enough to promote a
policy that loses a verified solve. This is a negative result for this BSAT
policy combination, not a general conclusion about Kissat's broader policy.
Full counters, hashes and verification results are in
`benchmark_results/mode-branching-initial-20260907.json`.

The C runtime and CLI are restored. The checker compatibility correction and
its regressions remain. The prototype's code, C test and dedicated certificate
script are preserved in `benchmark_results/mode-branching-prototype-20260907.patch`
against `2a63679`; use `git apply --unidiff-zero`. An isolated apply check
reconstructed all eight source/test files byte-for-byte. The companion JSON
pins their hashes and the tested release/debug binaries. The archived patch
also includes the contemporaneous checker compatibility correction so its
validation environment can be reconstructed from that base.

After removal, all C source/header files match the pre-prototype revision and
the release binary is byte-identical to the preserved baseline. Both restored
builds pass 47 C test executables. The restored release additionally passes
4,876 independent solves (92 configurations, seed 20261317) with the corrected
checker acceptance rule. The restoration hashes and full logs are in
`benchmark_results/mode-branching-restored-20260907.json`.
