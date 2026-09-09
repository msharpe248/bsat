# Bounded reason-side branching experiment — 2026-09-08

This experiment changes BSAT's VSIDS signal, not its learned-clause derivation.
After minimization, inspect one-hop reasons of the non-asserting learned literals,
and reward assigned, non-root antecedent variables absent from the learned clause.
Deduplicate rewards, reject future/self references by trail position, and inspect
at most 10,000 reason literals per conflict. Implicit binary and explicit arena
reasons are supported. VMTF/LRB modes bypass the experiment. Existing VSIDS
rescaling and heap updates apply. Scratch marks are cleared on all exits.

This is not an exact port of CaDiCaL: BSAT excludes the final learned clause's
variables, while CaDiCaL can exclude its entire analyzed set and uses additional
scheduling/depth controls. It also differs from the previously rejected
quality/glue reason rewards, which rewarded propagated variables using reason LBD.
No public option or ABI extension is introduced for this prototype.

## Why this experiment

The same-input CaDiCaL plain controls all check UNSAT twice: baseline 2.581 CPU
seconds, no on-the-fly self-subsumption 2.399, no reason-side bumping 2.208.
Neither feature alone explains BSAT's performance gap. The selected hypothesis
is narrower: BSAT's current activity signal may benefit from including variables
just outside the final resolvent. The controls do not establish this benefit;
the implementation must earn it in direct baseline comparisons.

The [archived patch](benchmark_results/cal3-reason-side-prototype-20260908.patch)
applies to `8cc95da`. Compile with `BSAT_REASON_SIDE_BUMP` for the candidate;
`BSAT_REASON_SIDE_TRACE` emits per-conflict size, LBD, eligible side-variable
count, decisions and propagations. Trace-only control scans without rewarding.
All source and test changes are in the patch, with build commands/hashes in the
adjacent policy and validation records.

## Validation and traces

Both candidate builds pass 66 C executables, including 64 focused cases spanning
budget boundaries, shared binary/arena dependencies, reward deduplication, scratch
cleanup, unchanged clauses/reasons/assignments, disabled ordering modes,
interruption and large-score rescaling. Release independent validation adds
3,956 solves (seed 2026090902); ASan/UBSan adds 2,576 (seed 2026090903), checking
truth-table answers, original models and text/binary RUP/DRAT proofs. These finite
checks do not prove correctness for all inputs.

At 10,000 conflicts, the trace-only control has exactly the original binary's
proof-prefix hash. The candidate's conflict summary first diverges at conflict 4.
The control finds eligible side variables at 9,949 conflicts, with 766,553 total
unique-per-conflict candidates. Its learned-literal total is 766,096, versus
737,169 for the candidate. Both traces remain UNKNOWN. Instrumented traces are
not performance measurements or accepted UNSAT proofs.

## Performance decision

**Rejected and removed from production sources.** Both candidate runs remain
UNKNOWN at sixty CPU seconds; both unchanged-baseline runs produce independently
checked UNSAT. No conclusive run fails verification. The raw
[uninstrumented comparison](benchmark_results/cal3-side-target-20260908.json)
records identical inputs, exact commands and hashes, seed 2026090904 and two
serial repetitions at 60 CPU / 90 wall seconds.

| Configuration | Checked / runs | Median process CPU |
|---|---:|---:|
| Unchanged BSAT | 2/2 | 21.844 s |
| Reason-side candidate | 0/2 | 60.080 s, UNKNOWN |
| CaDiCaL | 2/2 | 0.620 s |
| Kissat | 2/2 | 1.045 s |

BSAT baseline proof checking adds a median 7.719 wall seconds outside solver
timing. Every conclusive answer passes DRAT-to-LRAT/CakeML checking. The candidate
fails the target gate, so the conditional confirmation corpus is not run; no
parameters are retuned after inspecting the result. The promotion threshold and
confirmation subset were frozen in the adjacent prototype policy before timing.

The original source files are restored exactly, and the production executable
is byte-identical to `/tmp/bsat-cal3-before`. The candidate-specific tests and
implementation remain together in the archived patch, not as an inert public
option or a claim of improved performance. Reapply the patch to `8cc95da` in a
separate checkout to reproduce it. Normal production coverage remains 65 C
executables; 66 refers only to this archived candidate's test build.

The useful next lead comes from the [matched-host investigation](CAL3_MATCHED_HOSTS.md):
BSAT's fixed-conflict traces differ across hosts while the two reference solvers'
proof prefixes match. Investigate the first divergent event before proposing a
platform-specific speed explanation or another branching reward.
