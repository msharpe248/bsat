# Native profiles of three unsolved development workloads

Historical profile of revision `268cebe`; later proof buffering and Linux planning
profiles supersede its implementation-specific observations. Current status:
[PRODUCTION_READINESS.md](PRODUCTION_READINESS.md).

The `268cebe` release binary was sampled on diagnosis, scheduling and
station repacking, with the chronology/congruence/SCC/alternating/VMTF profile.
Each input was run with text and binary proofs. Native macOS `sample` observed
five seconds from startup at a requested 1 ms interval; BSAT had a 12-second
solving CPU budget. All six runs ended UNKNOWN. No profiling run supplies a
conclusive answer or a performance score. Builds/tests and timed benchmarks did
not run concurrently with profiling.

The table reports **leaf stack samples** as percentages of all main-thread
samples. These are approximate observations of this window, not exclusive
whole-run CPU accounting. Inlining and time blocked in I/O affect interpretation.

| Input | Proof format | Samples in propagation | Samples in kernel write |
| --- | --- | ---: | ---: |
| Diagnosis | Text | 66.8% | 1.5% |
| Diagnosis | Binary | 39.5% | 29.1% |
| Scheduling | Text | 45.6% | 26.3% |
| Scheduling | Binary | 41.2% | 24.7% |
| Station repacking | Text | 28.3% | 31.7% |
| Station repacking | Binary | 18.9% | 28.4% |

Propagation is a substantial cost on all three inputs. Conflict analysis also
appears prominently. Binary-output call stacks contain per-byte `fputc`, stdio
locking and flush/write paths, consistent with the implementation in
`src/solver.c::proof_clause`. Text output already uses chunked encoding.
Several percent of samples also reach clock/resource queries. These observations
identify concrete follow-ups: measure chunked binary encoding, propagation cache
behavior, and deadline polling cost. They do not establish that changing any of
those will improve solved counts.

Text/binary samples are not matched search histories: time limits permit different
amounts of search, startup costs are included, and the sampling process perturbs
execution. A smaller proof is not automatically a faster proof writer. Any
encoding change must preserve exact bytes and I/O-failure behavior and be measured
at equal search work before being promoted. No runtime optimization is introduced
from these profiles alone.

## Reproduction and evidence

`tests/profile_solver.py --solver bin/bsat --output profile.json INPUT...` runs
both encodings serially and records input/binary hashes, commands, outputs and
raw native stacks. It requires macOS process-sampling access to its own children.
Sandboxed sampling could not inspect the process; an authorized unsandboxed retry
worked. No remote service or privileged system modification was needed.

`benchmark_results/readiness-profile-20260907.json` contains the six completed
profiles. The original one-input sampling probe is excluded from this comparison.
The fresh application benchmark runs separately, with equal 60-second wall limits,
binary proofs for both solvers and external certificate checking outside timing.
