# Retained independent incremental reference

The differential-history runner now optionally loads an independent IPASIR
library with `--incremental-reference`, retaining its instance through every
addition and query of each BSAT flag combination. The reference does not rebuild
when BSAT checkpoints. Assumptions are resubmitted on every solve, including after
cancellation; no previous assumption list is silently retained by the harness.

CI pins CaDiCaL at `c60730422e758ef1cebe7aeddf2dda31c996bf04`, built with
`./configure -shared && make`. Its [upstream IPASIR implementation](https://github.com/arminbiere/cadical/blob/c60730422e758ef1cebe7aeddf2dda31c996bf04/src/ipasir.cpp)
provides addition, assumption, solve, value and cancellation calls. The report
records library hashes and its runtime signature. No upstream sources are
modified or vendored. CaDiCaL's IPASIR adapter itself disables factoring.

`--growing-blocks` starts with 1,024 variables, adds 256 every eight queries,
and reaches 9,216 after 256 queries. Planted random 3-CNF blocks of 32 variables
are linked by equality clauses. This is a reproducible larger API stress history,
not evidence of solving difficult industrial problems. Signed assumptions vary;
contradictory assumptions, conflict-limited slices, cancellation, checkpointing,
and cancelled-checkpoint retries are mixed into the history. All four public
flag combinations replay the same seed and permanent additions.

Every final result must agree with both retained CaDiCaL and a fresh pinned
Kissat solve of the exact snapshot. Every SAT model from all three is checked
against every original clause and assumption. Unassigned reference values are
completed positively and independently checked. Every reference UNSAT proof,
BSAT failed core and certified BSAT UNSAT export is independently verified by
DRAT-to-LRAT conversion followed by the CakeML checker. Certified exports must
also exactly match the original formula and current assumptions.

The reference is allowed to return a conclusive answer before polling a pending
cancellation, as IPASIR permits; any such answer must equal its retry and BSAT.
The report records reference cancellation outcomes and callback counts. BSAT's
stronger immediate cancellation contract remains asserted separately.

The [release history](benchmark_results/retained-incremental-reference-20260908.json)
passes all 1,024 queries: 944 SAT and 80 UNSAT, reaching 9,216 variables and
37,438 permanent clauses. It includes 64 BSAT cancellations, 36 ordinary
checkpoints, 24 cancelled-checkpoint retries and 36 actual UNKNOWN conflict
slices. All 64 cancelled reference calls return UNKNOWN; their retries agree.
The 80 fresh-reference UNSAT proofs, 80 BSAT failed cores and 40 certified BSAT
UNSAT exports all pass independent verification. All three solvers' SAT models
and exact exported contexts pass.

The [ASan/UBSan history](benchmark_results/retained-incremental-debug-20260908.json)
passes another 256 queries (240 SAT, 16 UNSAT), growing to 3,072 variables and
12,478 clauses, including eight cancelled-checkpoint retries. On macOS the real
Python executable must be launched with `DYLD_INSERT_LIBRARIES` pointing at the
compiler's ASan runtime; loading a sanitized library late through ctypes is
rejected by ASan. A pyenv shell shim can strip this environment setting. The
initial loader rejection was resolved with the direct interpreter and is not
counted as a solver test failure or a successful sanitizer run.

A [mutation check](benchmark_results/retained-reference-mutation-20260908.json)
replaces the reference's solve method with a call omitting all assumptions. The
comparison rejects query zero, establishing that the new reference actually
participates in acceptance. The runtime signature is `cadical-3.0.1-c607304`.

CI now builds the pinned shared reference and runs the full growing history,
plus the public C checkpoint-policy smoke and independent exports. Local runs
are complete; remote results are pending at this snapshot. These histories test
incremental semantics and certificate plumbing, not competition performance.
