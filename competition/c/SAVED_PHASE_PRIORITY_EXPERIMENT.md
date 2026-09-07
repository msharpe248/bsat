# Rejected saved-phase priority experiment

The diagnostic target is the additional battleship input
`ed6d842f96d10f3400bce251f9e95bfb.cnf`, where default heap BSAT took roughly 5.7
CPU seconds versus pinned Kissat's 0.15 seconds in the preceding milestone.
A one-run diagnostic tested existing BSAT controls and Kissat without factoring.
Elimination and equivalence substitution preserved the default 258382 conflicts
and approximately 5.8 seconds. Inprocessing slowed the solve to 9.902 seconds;
iterative minimization and alternating modes timed out. Disabling random phases
reduced the solve to 107461 conflicts and 2.443 CPU seconds. Kissat without
factoring solved in 2903 conflicts and 0.028 seconds, faster than its default
10615-conflict, 0.194-second run. Factoring is therefore not the immediate missing
technique indicated by this diagnostic.

## Prototype

`--prefer-saved-phase` protects known target and saved phases from per-decision
random overrides. Unknown phases still use the existing random probability.
The ordinary policy remains available when the option is absent. Saved-phase
priority is motivated by the target/saved/initial fallback in pinned
[Kissat decide.c](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/decide.c#L155),
not a full port of its focused-mode phase schedules or random-walk behavior.
The pinned source was inspected locally.

A new validity bit distinguishes the initialized negative polarity from an
actually saved negative phase. Decisions, assumptions, all propagation paths,
rephasing and local-search model installation update it when they save a phase.
SCC replacement copies it with polarity; ordinary API rebuilds reset it with
search state. Probe assignments count as saved phases, consistent with BSAT's
existing behavior of saving speculative assignments. With phase saving disabled,
there is no saved phase to protect; a known stable-mode target can still win.
Protected decisions consume no RNG draws. Counters expose protected decisions
and random-phase selections. The flag remains disabled by default during testing.

## Validation

Both release and ASan/UBSan builds pass all 20 C test executables. New tests
cover unknown phases, saved positive and negative values, RNG consumption,
heap/VMTF choice, disabled phase saving, target precedence, unit/binary/tagged
binary/long-clause propagation, SCC replacement and repeated API/assumption use.
The existing default-policy phase regressions continue to pass.

The matrix uses 61 configurations, 13 fixed formulas and 30 random formulas
(seed 20260930): 2623 truth-table/model/proof checks per build. New profiles cover
VMTF, disabled probing, binary proofs, SCC/BVE/BCE, alternating/inprocessing,
disabled phase saving with probability one, and local search. Both builds pass
24 short-deadline cases with at most 0.000 seconds observed CPU overrun at the
printed precision. Diagnostic counters are added to final statistics after the
matrix; the algorithm is unchanged by those output lines.

## Performance method

Compare retained default heap, the prototype with saved-phase priority, and the
retained solver with random phases disabled. Target inputs are battleship and
Hamiltonian from the additional-instance corpus plus the reused belpyramid and
multiplier circuit. Each profile runs twice with ten solving CPU seconds and
fifteen wall seconds. Runs are serial after local builds/tests finish, with
independent SAT model and UNSAT certificate checking outside solver timing.
Process CPU includes startup, parsing and proof output; the solving limit
excludes parsing. PAR-2 penalizes unfinished runs at twice the wall timeout.
This targeted development comparison is not a held-out competition score.

Baseline source is the retained C implementation at 972579f (runtime last changed
in 47d38cb), binary SHA256
8237cde01317a46a16053817485fb4f77aed08d2f96e3dbaf11883a50fc49a7b.

## Result and decision

| Profile | Verified runs | Mean wall PAR-2 | Peak RSS (MB) |
| --- | ---: | ---: | ---: |
| Retained heap | 6/8 | 11.4118 | 33.33 |
| Prefer saved phases | 6/8 | 10.3785 | 37.27 |
| Existing no-random-phase | 6/8 | 10.0039 | 35.27 |

All completed answers verify; there are zero errors. The prototype improves
battleship from 5.760–5.806 to 2.361–2.434 process CPU seconds. However, it makes
zero random choices on this input, exactly reproducing the existing no-random
policy's 107461 conflicts. That existing policy takes 2.424–2.436 seconds.
Hamiltonian likewise has zero prototype random choices and exactly matches the
no-random policy's 9247 conflicts, versus 19416 for retained heap. Both controls
and the prototype miss multiplier-circuits at the ten-second limit; the
prototype again makes zero random choices there.

Belpyramid distinguishes the policies: the prototype makes 19 random choices
and takes 36558 conflicts and 6.929–6.969 CPU seconds. The existing no-random
policy takes 30460 conflicts and 5.965–5.966 seconds, while retained heap takes
33836 conflicts and 6.731–6.759 seconds. Protecting known phases therefore fails
to improve on the simpler existing option on these targets.

Reject the additional option and restore the retained runtime. The useful next
question is whether the existing no-random policy generalizes across broader
inputs, rather than introducing phase-validity state and another control.
The diagnostic and targeted results alone do not justify changing the default.

- [Initial diagnostic](benchmark_results/battleship-diagnostic-20260907.json)
- [Repeated three-profile comparison](benchmark_results/saved-phase-targeted-20260907.json)
- [Tested prototype patch](benchmark_results/saved-phase-priority-rejected-20260907.patch)

The zero-context patch is applicable with `git apply --unidiff-zero` and includes
its unit tests, proof-validation configurations, deadline configuration, and SCC
state preservation. Existing defaults and public options are restored.

After restoration, release and ASan/UBSan builds each pass all 19 retained C test
executables. The rebuilt release binary exactly matches the baseline SHA256
above, and runtime source matches 47d38cb. The archived patch passes
`git apply --check --unidiff-zero` against the restored tree.
