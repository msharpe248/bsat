# cal3 structural ablations — 2026-09-08

BSAT's existing structural options do not resolve the exact depth-16 positive
query within five process-CPU seconds. CaDiCaL resolves it even with its internal
preprocessing disabled. The evidence therefore does not support treating a
missing structural pass as the complete explanation of the search gap.

Sixteen serial trials compare five BSAT configurations, CaDiCaL default/plain,
and Kissat, with two repetitions each. Each process has five CPU seconds and a
ten-second wall fuse, including parsing and binary proof output. The input is
the exact previously saved CNF plus positive output unit. All six conclusive
answers are UNSAT and independently checked by DRAT-to-LRAT and CakeML. The
other ten results are UNKNOWN due to the external CPU limit, not wrong answers.

| Configuration | Outcomes | Median process CPU |
|---|---|---:|
| BSAT default | 2 UNKNOWN | 5.008 s |
| BSAT equivalence, 100M work allowance | 2 UNKNOWN | 5.009 s |
| BSAT congruence, 100M work allowance | 2 UNKNOWN | 5.008 s |
| BSAT both structural passes | 2 UNKNOWN | 5.008 s |
| BSAT elimination | 2 UNKNOWN | 5.008 s |
| CaDiCaL default | 2 checked UNSAT | 0.618 s |
| CaDiCaL `--plain` | 2 checked UNSAT | 2.132 s |
| Kissat | 2 checked UNSAT | 0.883 s |

The pinned CaDiCaL source marks preprocessing options in the `P` column of
`src/options.hpp`; `Options::disable_preprocessing()` in `src/options.cpp`
disables those options for the plain configuration. This is a controlled
preprocessing ablation within CaDiCaL, not an implementation of BSAT's search.
Different clause management, minimization, branching and restart behavior remain.

Default CaDiCaL reports 5,147 eliminated and 1,869 substituted variables in the
first recorded run. Kissat reports 4,313 eliminated and 2,264 substituted
variables. These counts show substantial reference simplification, but do not
prove which transformation is causally necessary. The plain result demonstrates
that this query can also be solved quickly without those preprocessing options.
Externally stopped BSAT runs do not emit final transformation statistics; no
transformation counts are inferred from their option names.

## Incremental boundary and decision

The snapshot turns the temporary output assumption into a permanent unit.
BSAT's ordinary equivalence/congruence/elimination paths are gated by
`!n_assumps` in `solve_internal_account_impl`. A one-shot transform may exploit
that unit; its consequences cannot automatically be retained when the next query
flips the assumption. Equivalence and elimination also carry reconstruction
state. No such mode is exposed as certified retained solving by this experiment.

Keep production defaults unchanged. The completed prior search-switch ablations
and this structural screen establish a remaining search-quality problem, not an
identified soundness defect or a justified new algorithm. Further work should
compare the consequences learned and retained on this exact query, with proof
validation and independent holdouts, rather than add a structural flag based on
reference simplification counts alone.

Reproduce with `tests/benchmark_cal3_structure.py` and the saved depth-16
snapshot. `benchmark_results/cal3-structural-comparison-20260908.json` and its
`.policy.json` record commands, source/binary/input hashes, seed 2026090864,
budgets and verification outcomes. CaDiCaL remains pinned to
`c60730422e758ef1cebe7aeddf2dda31c996bf04` and Kissat to
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`.
