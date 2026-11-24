# Retain unchecked certificates and require checker success

The fresh reduction-growth screen exposed a verification gap: Kissat reported
UNSAT on diagnosis in 16.794 process CPU seconds, but its external proof check
exceeded 120 seconds. The benchmark then discarded the temporary proof,
preventing a direct retry. The timed record correctly remained unverified.

`tests/benchmark.py --retain-unverified DIR` now copies the exact input, proof
when present, solver stdout/stderr, available checker stdout/stderr, and hashed
metadata into a unique directory before cleanup. Repeated runs cannot overwrite
each other. The metadata includes the executed command, checker and executable
hashes, file hashes/sizes and the original unverified result. Copying happens
outside solver timing and does not change the recorded CPU, memory or PAR-2.
Verified answers and UNKNOWN runs are not retained by this option. Storage is
opt-in; large proofs consume disk space in the chosen directory.

Both the independent validator and benchmark require an exact `s VERIFIED`
line and a recognized checker exit. Previously, a substring anywhere in stdout
was sufficient regardless of exit status. Exit zero is accepted; exit one is
accepted only with the additional exact `c trivial UNSAT` marker. Other exits,
conflicting status lines and text merely mentioning the success marker fail.

The first stricter rule required exit zero exclusively. Subsequent full
independent validation caught its false rejection of an empty input clause:
the pinned drat-trim emits both success markers but exits one on that path.
This is a checker compatibility correction, not a C solver wrong answer.
Permanent regressions cover synthetic failed-process/status combinations and,
when `DRAT_TRIM` is provided, real empty-clause, contradictory-unit and root
propagation contradictions with text and binary proofs.

## Validation

The initial retention milestone's nine harness tests cover timeout preservation, exact binary proof bytes and
hashes after temporary cleanup, repeated-run isolation, invalid SAT models,
disabled retention, verified/UNKNOWN behavior, CLI integration, manifest
validation, and strict checker acceptance. Synthetic checker output in these
unit tests tests control flow; it is not an answer certificate.

Six additional real solver runs check SAT and UNSAT versions of an exhaustive
three-variable formula, using default, binary-proof and combined experimental
settings. Truth-table answers, original SAT models and proof steps pass, and
real drat-trim UNSAT checks pass the stricter acceptance rule. No C solver
runtime code changes in this milestone. Commands, source/binary hashes and
results are recorded in
`benchmark_results/certificate-artifact-validation-20260907.json`.

The checker compatibility follow-up passes ten harness tests with
`DRAT_TRIM=/tmp/bsat-drat-trim`, including actual trivial-UNSAT proofs, and
4,876 independent restored-release solves across 92 configurations. The
original false rejection, checker output, source hashes and final validation
are recorded in `benchmark_results/checker-trivial-compatibility-20260907.json`.

## Diagnosis certificate follow-up

A separate regeneration uses the same pinned Kissat and input as the fresh
screen, with a 60-second solver wall allowance and a deliberately short
one-second checking allowance to exercise retention. It produces UNSAT and
saves a 91,559,304-byte proof plus the 12,429,871-byte input. This is a
certificate-generation follow-up, not a new matched performance comparison.
The frozen screen and its scores remain unchanged.

The proof is retained locally under
`/private/tmp/bsat-diagnosis-certificate/unverified-0ivwlt6i/` with SHA256
`885281e97fc4e4867be1fe1067119d4a5218968cef4c1f01a080c8bea1600c50`.
The artifacts are not committed; the repository record preserves their hashes
and location in `benchmark_results/diagnosis-certificate-regeneration-20260907.json`.
The separate check used a 600-second allowance and **verified successfully in
161.954 wall seconds** (161.917 seconds reported by drat-trim). It exited zero
and printed the exact success marker. The checker processed 30,812,389
resolution steps and 17,654 RAT lemmas in the core. This independently
establishes UNSAT for the exact diagnosis input; the original 120-second
checking budget was insufficient for this proof on this run.

The successful check is recorded with complete checker output, artifact
metadata, commands and hashes in
`benchmark_results/diagnosis-certificate-check-20260907.json`. The original
discarded proof cannot be compared byte-for-byte with the regenerated proof,
so the frozen benchmark is not retroactively marked verified. Kissat's
diagnosis answer is independently confirmed by this separate certificate.
