# Certificates for assumption queries

`tests/certify_query.py` creates a fresh artifact directory containing the exact
base CNF, assumptions, augmented query CNF, solver output, DRAT proof, and a JSON
binding with SHA-256 hashes. It solves the augmented input as a separate process;
an existing incremental solver session is not involved or mutated.

```sh
python3 competition/c/tests/certify_query.py base.cnf \
  --assume=-7 --assume=12 --solver competition/c/bin/bsat \
  --converter /path/to/drat-trim --checker /path/to/cake_lpr \
  --artifacts /tmp/new-query-certificate
```

The directory must not already exist. Assumptions must be nonzero DIMACS literals
within the base variable range. Repetitions and contradictions are supported.
SAT requires an independent model check against the base plus every assumption.
UNSAT requires DRAT-to-LRAT conversion followed by cake_lpr on the exact augmented
CNF. The certificate establishes `base AND assumptions` is UNSAT; it does not
establish unconditional UNSAT of the base. The internal C assumptions-plus-proof
API still returns UNKNOWN. This explicit snapshot workflow supplies conditional
certificates without silently changing that API contract.

Exit 10/20 means independently verified SAT/UNSAT of the augmented query. Exit 0
means no verified answer; exit 1 or argument failure indicates an error. Keep the
whole artifact bundle when sharing a certificate. Hashes bind bytes, not authorship.

Real-checker regressions exercise an initially satisfiable base, base/assumed SAT,
conditional UNSAT, contradictory and duplicated assumptions, invalid literals,
existing-directory protection, and rejection when conditional proofs are rebound
to the satisfiable base without their assumptions.

Verification reports include conversion/checker wall and CPU time, total wrapper
time, and cumulative child-process high-water RSS (not an isolated stage peak).
`benchmark.py` separately reports solver time, validation time and their sum;
PAR-2 remains based on solver time for continuity with previous reports. Snapshot
and hashing costs outside a reported timing scope are not implied to be included.
Timeout supervision terminates process groups, gives wrappers a short cleanup
window, and escalates to SIGKILL. Forced external SIGKILL of the supervisor itself
still needs OS-level process containment for a strict no-orphan guarantee.
