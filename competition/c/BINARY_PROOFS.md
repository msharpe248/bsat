# Buffered binary DRAT encoding

Binary clauses now encode into bounded 4 KB chunks and use `fwrite`, replacing
one locked stdio call per output byte. The existing stdio stream still buffers
across clauses. The encoder adds no persistent allocation or solver option.
Five-byte literals, record terminators, additions and deletions preserve their
exact previous bytes. Short writes mark the solver failed, and final flush
failures still prevent a conclusive answer. Text encoding uses the shared write
helper with unchanged bytes.

## Validation

Release and ASan/UBSan each passed all 48 C executables, including 336 exact
encoding cases against independent reference encoders, 22 immediate write cutoffs
and two deferred flush failures. New cases straddle the five-byte literal/chunk
boundary. Each build also passed all 24 structured/metamorphic long-search cases
with independent models and text/binary UNSAT certificates, plus six actual
process interruption/file-exhaustion cases.

Thirty-six serial fixed-work runs compare the preserved `8d11ef5` baseline with
the candidate: diagnosis and station repacking, text/binary/no proof, three
repetitions, 20,000 conflicts, identical full-profile options. All selected search
counters and proof hashes match across binaries/repetitions. UNKNOWN prefixes
are determinism evidence, not certificates. `tests/compare_work.py` freezes its
policy before timing and reports process CPU per million propagated literals;
this includes parsing and proof output and is not isolated propagation time.

Mean binary-output CPU improves from 1.674 to 1.503 seconds on diagnosis (10.2%)
and 0.596 to 0.435 seconds on station repacking (27.1%). Text controls also vary
by about 10–13%, and proof-disabled controls vary by about -2.4% to +1.0%; do
not attribute all fixed-work differences to the binary encoder or infer precise
confidence intervals from three short samples.

## Completed development solves

A separately frozen comparison uses binary proofs for both executables, equal
75-second wall limits, two repetitions, seed 20261327 and external checking with
a 600-second allowance. No builds/tests/profiling compete with timing.

| Input | Baseline wall seconds | Chunked wall seconds | Verification |
| --- | --- | --- | --- |
| Summle | 57.527 / 59.364 | 52.035 / 53.383 | All original SAT models pass |
| Hardware 7bdf3e54 | 1.441 / 1.435 | 1.384 / 1.385 | All UNSAT proofs pass |

Both solve 4/4 runs. Mean wall time improves from 29.942 to 27.047 seconds;
Summle mean process CPU improves from 55.541 to 50.331 seconds (about 9.4%).
This supports retaining the change on these development inputs, not a new
held-out solve-count claim or competition readiness.

`benchmark_results/binary-proof-work-20260907.json` and its `.policy.json`,
`binary-proof-solves-20260907.json`, `binary-proof-solves-policy-20260907.json`
and `binary-proof-validation-20260907.json` contain commands, hashes, raw
measurements, full test logs and certificate/model evidence.
