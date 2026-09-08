# Broader repeated evaluation of the hardened C solver

September 8 update: embedding, conservative learned reuse, conditional-certificate
bundles and broader assurance now have implementations and validation recorded in
[PRODUCTION_GAPS.md](PRODUCTION_GAPS.md). Capability limits below describe this
earlier milestone where they conflict with the current API contract.

BSAT solved **one of twelve inputs in both repetitions**; pinned Kissat solved
**two of twelve in both repetitions**. BSAT has 2/24 verified runs and Kissat
4/24. All conclusive answers in this screen are SAT and their original-input
models pass independent checking. There are no execution or invalid-answer
errors. UNKNOWN remains unfinished, not evidence of an incorrect answer.

The screen establishes a broader baseline, not competition readiness or a speed
gain from the error-handling changes. In particular, Kissat is about 6.15 times
faster on Summle in these two repetitions and additionally solves the selected
Erdős-discrepancy instance.

## Frozen selection and execution

The selector first shuffles families, then files within each family, with seed
20261324. It selects one eligible input from each of twelve families, excluding
CNF names and content hashes found in 387 preceding JSON records and excluding
duplicate selected content. Unlike the previous smallest-first policy, families
with many files do not receive extra entry opportunities. Six selector tests and
three manifest tests passed before execution.

The eligible size range is 1–30 MB. Selected files span 1.206–29.475 MB,
171–509,532 variables and 28,101–1,508,766 clauses. This is a bounded, family-balanced
sample of the local corpus, unused according to recorded history; it is not the
full competition suite or a guarantee that unrecorded historical usage never
occurred. The inputs become development data after this evaluation.

The command policy and input manifest were frozen before timing. Seed 20261325
shuffles all 48 serial jobs: twelve inputs, two solvers and two repetitions.
Both solvers receive an equal **60-second external wall limit**, including
parsing and proof output. BSAT has no separate internal CPU cutoff in this
screen. Both produce binary DRAT. Independent verification is outside timing,
with a 600-second checker allowance and retention of unverified conclusive
answers/errors. No builds, tests or profiling ran on the benchmark host concurrently with
timing. Hosted CI used independent machines.

BSAT uses `--chrono --congruence --equiv --equiv-budget 100000000 --alternating
--vmtf --binary-proof`; growing reduction intervals remain disabled. This is the
experimental competition profile, not the CLI defaults. Runtime source and
headers match `268cebe` throughout the screen. Documentation/CI-only commits
during execution do not alter the pinned executable. The report's dirty-worktree
flag includes documentation/artifact work and the unrelated local index directory;
the executable hash and final source audit establish which solver was measured.
Kissat is pinned at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.

## Results

Times are whole-process wall seconds. Each UNKNOWN entry represents both
repetitions exhausting the 60-second allowance.

| Family | BSAT | Kissat |
| --- | --- | --- |
| Belpyramid puzzle | UNKNOWN ×2 | UNKNOWN ×2 |
| Circuit equivalence checking | UNKNOWN ×2 | UNKNOWN ×2 |
| Erdős discrepancy | UNKNOWN ×2 | SAT verified, 30.751 / 30.772 s |
| Miter | UNKNOWN ×2 | UNKNOWN ×2 |
| Oddball weighing | UNKNOWN ×2 | UNKNOWN ×2 |
| Ordering principle XOR | UNKNOWN ×2 | UNKNOWN ×2 |
| Ramsey numbers | UNKNOWN ×2 | UNKNOWN ×2 |
| RISC instruction removal | UNKNOWN ×2 | UNKNOWN ×2 |
| Rooks | UNKNOWN ×2 | UNKNOWN ×2 |
| Summle | SAT verified, 58.406 / 58.202 s | SAT verified, 9.484 / 9.473 s |
| Tseitin formulas | UNKNOWN ×2 | UNKNOWN ×2 |
| Waerden | UNKNOWN ×2 | UNKNOWN ×2 |

Mean wall PAR-2 is **114.859 seconds for BSAT** and **103.353 for Kissat**,
assigning 120 seconds to each unfinished run. The many timeouts dominate this
score; it is not a general throughput ratio. Peak observed RSS is 719.6 MB for
BSAT and 1,032.1 MB for Kissat. These peaks reflect different amounts of search
and are not equal-work memory measurements. The timed solver processes consumed
2,717.4 seconds of wall time in total. Two repetitions expose obvious variation
but do not establish narrow statistical confidence intervals.

## Additional large UNSAT certificate check

After timing completed, the final BSAT binary solved the previously used hardware
instance `7bdf3e5401cc263951003b569bcd689c.cnf` with both text and binary proofs.
The pinned external checker verified both certificates. Exact commands, input and
proof hashes, preserved proof paths and checker transcripts are recorded in
`benchmark_results/readiness-large-unsat-20260907.json`. This is development
validation, excluded from the held-out score. It adds a large current-binary
UNSAT check to a fresh screen whose completed answers were all SAT.

## Acceptance evidence and limits

The final audit checks the complete unique 48-run grid, input/binary/checker
hashes, recomputed summaries, independent acceptance of every conclusive answer,
and unchanged runtime source. The four hosted Linux/macOS release/ASan-UBSan
jobs all passed for the same solver source and workflow in
[CI run 34179106951](https://github.com/msharpe248/bsat/actions/runs/34179106951).
The observed macOS sanitizer timeout was resolved by allowing 60 minutes without
removing test cases. The final results commit only adds documentation/evidence.

This batch closes the identified CI, fault-testing, long-search, API-contract,
profiling, benchmark-method and stale-documentation gaps. It does not formally
verify the C implementation, exhaust all failure paths or option combinations,
add concurrent embedding/conditional proofs/learned-clause reuse, or establish
competition-level performance. The supported API boundary is explicit in
[API_CONTRACT.md](API_CONTRACT.md). The next measured optimization candidates
are described in [READINESS_PROFILING.md](READINESS_PROFILING.md), especially
binary proof encoding and propagation. No policy is tuned on this screen and
then presented as held-out performance.

## Records

- `readiness-fresh-corpus-20260907.json`: frozen inputs and exclusion history.
- `readiness-fresh-policy-20260907.json`: frozen command and tool hashes.
- `readiness-fresh-20260907.json`: all raw runs and aggregate results.
- `readiness-large-unsat-20260907.json`: two independent hardware certificates.
- `readiness-ci-20260907.json`: successful hosted jobs and step metadata.
- `readiness-final-audit-20260907.json`: final integrity and completion audit.

All records are under `benchmark_results/`.
