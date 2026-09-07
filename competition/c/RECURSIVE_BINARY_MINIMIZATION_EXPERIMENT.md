# Recursive minimization with implicit binary reasons

The candidate was rejected: it added no verified solves and repeatedly slowed
battleship and Hamiltonian. Production solver sources and executable are restored
to `db31543`; generic signed-clause regression tests are retained.

The default recursive minimizer treats every `INVALID_CLAUSE` reason
as a decision, including implicit binary propagation. Those propagations retain
the remaining reason-clause literal in `binary_reasons`. The rejected candidate follows
that literal while retaining the default depth-128 bound, abstract-level filter,
per-candidate cache lifetime and per-clause work allowance.

A missing binary reason still ends the proof unsuccessfully. A binary reason
costs one inspected antecedent, and increments the existing binary-step counter.
Self references and non-root antecedents that do not precede their consequent
on the trail are rejected conservatively. Higher-level antecedents remain
rejected by the existing level check. Failure restores exploring marks; successful
subtree marks are cleared before another source literal is considered.

This differs from the opt-in iterative minimizer, which has a different traversal
and shares successful dependencies across source literals. It also differs from
the separate `--binary-minimize` resolution pass. The latter two implementations
are unchanged. The candidate changes default learned clauses and search order;
it is not a search-preserving implementation optimization.

The [larger-input discovery probe](benchmark_results/legacy-binary-discovery-20260907.json)
ran iterative, binary-resolution and combined minimization once each on `ktf`
and hardware verification, with ten solving CPU seconds, fifteen wall seconds
and seed 20261108. All six runs returned UNKNOWN with zero reported errors.
The substantial minimization activity motivated isolating binary support in the
recursive policy rather than simply enabling the whole iterative algorithm.

## Correctness and resource checks

The archived candidate-only recursive-mode tests cover **84 cases**: sixteen polarity combinations
at each of five budgets; chain depths at the 128-edge boundary; and self/future
reason rejection. Truth tables independently check every small strengthened
clause against the original formula. The shared two-edge path must be reproved
for the second candidate, so budgets two/three remove one literal while four
remove two. All cases require scratch cleanup, including incomplete proofs.
Existing tests cover arena reasons, shared dependencies, disabled minimization,
interruption, cache heights and the iterative implementation.

Both candidate builds pass all **38 C test executables**. Each passes
**3,139 independent validation solves** (73 profiles, seed 20261109), with
truth-table answers, original SAT models and text/binary proofs checked using
external drat-trim. Both builds pass 42 deadline
checks, with maximum observed CPU overrun 0.000 seconds. These finite checks are
regression evidence, not a universal soundness proof.

## Performance and decision

The [serial comparison](benchmark_results/legacy-binary-targeted-20260907.json)
uses five inputs, two repetitions per solver, fifteen solving CPU seconds and a
separate twenty-second external wall limit, seed 20261110. Both configurations
verify **6/10 runs**, with zero errors. They solve the same three inputs; hardware
verification and `ktf` remain UNKNOWN. Median process CPU seconds:

| Input | Conservative default | Binary candidate |
| --- | ---: | ---: |
| battleship | 1.984597 | 11.822768 |
| belpyramid | 4.839231 | 4.926999 |
| Hamiltonian | 0.137005 | 0.359231 |
| hardware verification (UNKNOWN) | 14.766494 | 15.033697 |
| ktf (UNKNOWN) | 15.038052 | 15.031465 |

Battleship slows approximately 5.96 times, with conflicts increasing from 107,461
to 512,757 in both repetitions. Hamiltonian slows 2.62 times, with conflicts
increasing from 9,247 to 22,929. Belpyramid produces fewer learned literals but
has no observed speed benefit. Mean wall PAR-2 worsens from 17.6247 to 19.9176
seconds. Maximum observed RSS decreases from 61,784,064 to 60,375,040 bytes;
this does not offset the repeated search regressions.

The second conservative hardware run reaches the external wall limit before
finishing its internal CPU allowance, so its final counters are missing, not
zero. More conflicts or shorter clauses on UNKNOWN inputs do not establish a
solving improvement. This reused development screen is sufficient to reject
the default change; it is not a held-out competition evaluation.

## Retained tests and replay

The final tree adds **160 generic cases** across both existing minimizers,
sixteen polarity combinations and five work budgets. They check independent
truth-table entailment against the original formula, stable subsequence order,
preservation of the asserting and decision-cover literals, removal accounting
and scratch cleanup. They do not require the rejected candidate's removal counts.
All **38 C test executables** pass in both release and ASan/UBSan debug builds
after restoration. The restored release executable is byte-identical to the
measured conservative baseline; no production source change remains.

The [archived candidate patch](benchmark_results/legacy-binary-rejected-20260907.patch)
contains the runtime change and its 84 specific test cases. It applies to
**`db31543`**, before the retained generic tests; replay it in a separate checkout
of that revision using `git apply --unidiff-zero`. Patch applicability was checked
against those sources. The archive is not intended to apply directly over this
milestone's modified test file. Raw JSON records exact commands, input hashes,
limits, executable hashes and checker hashes. Preserved candidate, baseline and
checker binaries match those records, including the discovery probe. No broader
speed or soundness claim follows from this rejected experiment.
