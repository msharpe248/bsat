# Retention cutoff diagnostic and ranking regression

The default solver remains unchanged from `7263acb`. Removing the effective
maximum-LBD cutoff did not solve the larger hardware-verification target in
repeated 30-CPU-second runs, so these measurements do not justify a new default.
This milestone retains a C regression for reduction selection and relocation.

## Search evidence

Input: `dataset/sat_competition2025/hardware-verification/d672bdbca3b1fb26d585e78add79537e.cnf`,
17,710 declared variables and 304,026 clauses. Prior measurements showed long
learned clauses and deletion of nearly all learned clauses, motivating a check
of retention before changing the C policy.

The [short probe](benchmark_results/retention-cutoff-probe-20260907.json) uses
one run per configuration, ten solving CPU seconds, a 15-second wall limit and
seed 20261025. All three return UNKNOWN, with zero errors:

| Configuration | Conflicts | Learned literals | Deleted clauses | Glue clauses |
| --- | ---: | ---: | ---: | ---: |
| Default cutoff 30 | 216,419 | 45,361,183 | 213,657 | 289 |
| Cutoff 255 | 224,814 | 39,507,560 | 220,806 | 946 |
| Used protection + dynamic LBD | 198,584 | 43,878,153 | 194,892 | 129 |

More glue clauses and fewer learned literals with the higher cutoff motivated
an extension, but those counters alone are not evidence of progress toward a
solution. The [longer comparison](benchmark_results/retention-cutoff-long-20260907.json)
uses two repetitions, 30 solving CPU seconds, a 40-second wall limit and seed
20261026. The rank-only profile uses `--max-lbd 4294967295`, retaining the existing
quality ordering, keep fraction and binary/glue/locked protection while removing
the effective absolute cutoff for this input.

| Profile | Conflicts, run 1 / run 2 | Verified solves | Mean wall PAR-2 | Maximum process RSS |
| --- | --- | ---: | ---: | ---: |
| Cutoff 30 | 677,193 / 674,129 | 0/2 | 80.0 s | 82,640,896 bytes |
| Rank only | 705,869 / 706,275 | 0/2 | 80.0 s | 82,329,600 bytes |

All four results are UNKNOWN, with zero reported errors. No completed answer
exists to certify in either screen, so these runs add search evidence rather
than independent SAT/UNSAT soundness evidence. Reject promoting rank-only
retention on this basis; the existing `--max-lbd` control remains available.
This is not proof that the profile can never help on another input or budget.

Runs are serial and precede builds/tests. Original models and proofs would be
checked outside solver timing, but no run completed here. Process CPU includes
startup, parsing and proof output; the internal solving limit excludes parsing.
Unverified answers receive twice the wall limit as PAR-2. Executable and checker
hashes still match the raw records after testing. This is one reused development
input, not a competition or held-out evaluation.

## C regression

`tests/test_reduction_ranking.c` constructs two protected clauses and six eligible
clauses with a strict LBD/activity quality order. It checks all 720 insertion
permutations, nine keep fractions and eight cutoffs: **51,840 cases**. A fixed
expected rank list and integer quota calculation are independent of the solver's
sort implementation. The cutoff set includes exact boundaries and `UINT32_MAX`.

Original unit clauses entail every synthetic learned clause. Binary and glue
clauses survive all cutoffs, while protected clauses do not contribute to the
eligible keep quota. The regression checks exact surviving IDs, deletion counts,
LBD preservation and activity decay only on eligible survivors. A deleted arena
prefix forces collection in every case, including when all clauses survive.
It checks relocated arena-backed binary and long watches, absence of deleted
clauses from their watch lists, and the original model after collection.
Existing clause-lock and used-protection tests cover those additional policies.

Both release and ASan/UBSan builds pass all **35 C test executables**, including
the final 51,840-case test. No runtime code or default changed, and the benchmark
executable remains byte-identical; a repeated full external solver-validation
suite was not needed for this test-only implementation.

Three temporary mutant solver builds demonstrate that the new assertions detect
reversed activity ranking, a keep quota incorrectly including protected clauses,
and deletion at equality with the cutoff. All three abort on assertions; exact
replacements and diagnostics are in the
[mutation record](benchmark_results/reduction-ranking-mutations-20260907.json).
Mutants never replace the production source or executable. These checks establish
regression sensitivity, not universal soundness.

## Reproduction

```sh
make -C competition/c all test
make -C competition/c all test MODE=debug
python3 competition/c/tests/benchmark.py --checker /tmp/bsat-drat-trim \
  --solver 'cutoff=competition/c/bin/bsat --time 30 --proof {proof} {input}' \
  --solver 'rank-only=competition/c/bin/bsat --max-lbd 4294967295 --time 30 --proof {proof} {input}' \
  --timeout 40 --repeats 2 --seed 20261026 --output /tmp/retention-repeat.json \
  dataset/sat_competition2025/hardware-verification/d672bdbca3b1fb26d585e78add79537e.cnf
```

To repeat mutation checks, apply each recorded replacement separately to a
temporary copy of `src/solver.c`, compile it with release flags, and link the
new test with that object plus the remaining release core objects (exclude the
original `solver.o` and `main.o`). Each resulting executable should abort. The
ordinary test executable should pass without replacements.
