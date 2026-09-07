# Preserve learned-clause quality state across equivalence rebuilding

Successful SCC substitution inside the solve path replaced the solver without
transferring its preallocated `level_seen` scratch array or `levels_capacity`.
Subsequent LBD calculation silently returned zero because no assignment level
fit the replacement solver's zero capacity. Every learned clause was then
classified as glue, preventing ordinary learned-clause reduction and removing
the positive-LBD signal used by focused moving-average restarts.
Here LBD counts distinct non-root assignment levels in the learned clause at
analysis time.

The fix transfers the array and its capacity at the successful replacement
commit point, alongside the existing input and proof ownership transfers. The
discarded solver is detached from the array before cleanup. Failed or budgeted
substitution attempts leave the original ownership intact. No extra allocation
or array copy is needed. The path without successful substitution is unchanged.

This was a heuristic-state defect. Its discovery does not establish incorrect
SAT/UNSAT answers in earlier versions: models and completed UNSAT proofs were
independently checked. It does mean affected historical performance measurements
used incorrect clause-quality scores, and new tuning should use the corrected
state.

## Reproduction and regression coverage

A six-variable formula combines an independent binary equivalence with a search
conflict that learns a binary over two distinct decision levels. The old binary
reports a null level-mark array, capacity zero, and learned LBD zero. The fixed
binary reports the array present, capacity eight and LBD two.

The permanent regression checks both heap and VMTF search, with and without
chronology and substitution. It also checks cleared scratch marks, repeated
solves, assumptions and cleanup. It failed on the old implementation before
the fix. The same formula is added to the independent model/proof validator.

One mutation removes the transfer and reproduces the bad score. A second leaves
ownership with the discarded solver; AddressSanitizer detects the resulting
heap use-after-free. The fix is exercised by the existing SCC budget rollback,
large-cycle, model reconstruction and API reuse tests.

Before/after evidence is in
`benchmark_results/equiv-lbd-reproduction-20260907.json` and
`equiv-lbd-regression-20260907.json`; mutation commands and diagnostics are in
`equiv-lbd-mutations-20260907.json`.

## Scope of the observed problem

In the preceding [held-out screen](CHRONOLOGICAL_HELDOUT.md), chronological BSAT
substituted variables on `grs-fp-comm`, oddball weighing and hardware model
checking. All their learned clauses were marked glue, maximum LBD was zero,
and no learned clauses were deleted. The other three families performed no
substitution and had nonzero LBDs and ordinary deletions. The state loss therefore
does not explain every unsolved input or establish a single cause for the whole
competition gap.

## Validation

Release and ASan/UBSan builds each passed 46 C test executables, 4,611 independent
validator solves (87 configurations, seed 20261307), and 63 short-deadline cases.
The largest observed CPU overrun was 0.001 seconds in each build. Both mutations
were detected. Twelve default trace cases matched the previous binary and the
zero-SCC-budget profile in status, return code, 24 selected counters and proof
bytes. Incomplete trace proofs are not certificates.

Commands, source/binary hashes and full validation logs are in
`benchmark_results/equiv-lbd-validation-20260907.json`. Trace details are in
`equiv-lbd-default-traces-20260907.json`.

## Development performance comparison

Four affected development inputs were run serially with one repetition per
binary, seed 20261308. Both binaries used chronology, congruence, SCC with a
100-million-work budget, alternating VMTF, and proof output. Each run had a
30-second solving CPU budget and a 35-second external wall limit; independent
checking ran outside timing. Process CPU below includes parsing and proof work.
These reused inputs are not a new held-out evaluation.

| Family | Before | Fixed | Peak RSS before → fixed |
| --- | --- | --- | --- |
| Hardware model checking | UNKNOWN | SAT verified, 24.761 s CPU | 777.1 → 176.8 MB |
| grs-fp-comm | UNKNOWN | UNKNOWN | 1,040.5 → 324.6 MB |
| Oddball weighing | UNKNOWN | UNKNOWN | 169.9 → 117.7 MB |
| Random circuits | SAT verified, 22.758 s CPU | UNKNOWN | 105.2 → 74.4 MB |

Both binaries solved one of four cases, with no benchmark errors. Mean wall
PAR-2 slightly worsened from 58.530 to 59.197 seconds. This is a substantial
memory improvement and a hardware solve gain, but not an overall speed win:
the random-circuit solve regressed to a timeout. One repetition does not measure
timing variance. UNKNOWN cases provide no answer certificate.

Corrected runs had positive maximum LBDs and ordinary learned-clause deletion;
the old runs had zero for both. On grs-fp-comm the fix deleted 169,335 clauses
and reduced peak RSS by about 69%. Restoring the scores also changes restart
behavior, so subsequent tuning must evaluate explicit retention and restart
policies with correct bookkeeping. Retaining the fix avoids silently treating
every learned clause as glue; it does not establish competition performance.
Full commands, hashes, timings and counters are in
`benchmark_results/equiv-lbd-initial-20260907.json`.
