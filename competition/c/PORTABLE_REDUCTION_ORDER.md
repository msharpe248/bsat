# Portable learned-clause reduction order — 2026-09-09

The first cross-host cal3 divergence is caused by ordering equal reduction scores.
At conflict 2,000, macOS and Linux have identical candidate clauses, references,
LBDs and activity values. Their C-library `qsort` implementations produce different
tie orders. A tie crosses the keep/delete boundary, so subsequent learning differs.
This is permitted sorting behavior, not a C-library correctness failure.

## Causal evidence

Two repetitions at each boundary establish:

| Stop | Default Mac/Linux proof prefixes | With reduction disabled |
|---|---|---|
| 1,999 conflicts | Match | Match |
| 2,000 conflicts | Differ | Match |
| 2,001 conflicts | Differ | Match |
| 4,001 conflicts | Differ | Match |
| 10,000 conflicts | Differ | Match |

The first reduction has 1,696 eligible clauses and 126 tied-score groups. Its
pre-sort records are identical; its first post-sort difference is at index 8,
where both entries have LBD 3 and activity 34. A tied group at LBD 7/activity 2
straddles the 848-clause retention boundary. Three clauses survive only on Mac,
and three different clauses survive only on Linux. The ordinary protections for binary,
low-glue and locked clauses remain separate from this ranking.

[Raw boundary evidence](benchmark_results/reduction-boundaries-20260908/summary.json)
includes the pinned input and binary hashes, counters and selected compressed
proof/log records. These bounded UNKNOWN prefixes establish trace divergence;
they are not accepted UNSAT certificates.

## Implementation

`src/reduction_sort.c` specializes the Bentley/McIlroy partition policy published
in [Apple Libc at revision 71bbe350](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/stdlib/FreeBSD/qsort.c).
Typed struct swaps avoid generic byte-aliasing machinery. Small partitions use
insertion sort; median sampling and three-way partitioning define tie order.
The routine recurses into the smaller partition and limits depth to twice the
integer base-two logarithm of the input size. An allocation-free local heapsort
handles depth exhaustion, bounding worst-case work to O(n log n).

The key remains lower LBD, then higher activity. Equal keys use a fixed algorithmic
order; this is not a stable sort or a clause-age preference. The candidate retains
the observed faster Mac search instead of making the older Linux tie selection
canonical. It requires no additional score buffer beyond the existing reduction
allocation, and changes no clause derivation, proof rule or protection condition.

The bounded fallback is local and deterministic; it does not promise the same tie
order as every version of the Mac C library on every adversarial array. Likewise,
fixed sorting does not guarantee whole-solver cross-platform identity under every
floating-point environment or future option combination. The claims below are
limited to the measured traces and confirmation cases.

The BSD 3-clause notice is retained in the source and `THIRD_PARTY_NOTICES`.
Installation includes that notice under `share/doc/bsat`, checked by the installed
C/C++ consumer test.

## Validation and performance

The candidate passes 66 C executables in release and ASan/UBSan builds, including
192 golden-order array shapes and 384 forced-depth sorting cases. They check
thresholds around sizes 7 and 40, repeated/extreme keys, preservation of every
record, sortedness and deterministic fallback. The golden checksum was frozen
from the local C-library sort. Existing 51,840 reduction rankings exercise retention
fractions, maximum LBD, protected clauses, garbage collection and watched literals.
Independent release validation passes 3,036 truth-table/model/text/binary proof
solves (seed 2026090906). Finite tests are not a universal soundness proof.

**Promoted as the default reduction sort.** The original library sort is removed
from the reduction path; input-literal sorting is unaffected. All 20 candidate
boundary traces match across Linux and macOS and match the prior Mac baseline.
The complete cal3 proof also matches byte for byte across hosts.

| Linux, same runner and limits | Original library sort | Portable sort |
|---|---|---|
| Outcomes, two repetitions | 2 UNKNOWN | 2 checked UNSAT |
| Process CPU seconds | 60.005 / 60.009 | 32.816 / 33.559 |

Median complete-proof checking adds 10.832 wall seconds outside solver timing.
This is a solved-case improvement within the fixed budget; the old run times are
censored, so they do not establish an exact unrestricted speedup ratio. The
[hosted run](https://github.com/msharpe248/bsat/actions/runs/34312877985) and
[Linux evidence](benchmark_results/portable-sort-linux-20260908/summary.json)
record tool/input hashes, limits, counters and independent verification.

The [frozen local confirmation](benchmark_results/portable-sort-confirmation-20260908.json)
uses cal3, battleship, belpyramid, Hamiltonian and the 10.97-million-clause planning
input: two repetitions, 60 process CPU / 90 wall seconds, seed 2026090908. Both
versions check 10/10 runs, with identical proof bytes for every input across all
four runs. Mean CPU PAR-2 is 5.284 seconds before versus 5.241 after, a change
under 1%; this is preservation evidence, not a broad Mac speedup claim. No local
build/test/fuzz work overlaps timing. These are reused development cases, not
fresh competition holdouts. [Derived summary](benchmark_results/portable-sort-confirmation-summary-20260908.json).

Final default release and sanitizer builds pass all 66 C executables. The added
tied-boundary reduction regression checks exact survivors, relocated references,
watch removal and model preservation. Installed C/C++ clients and the third-party
notice check pass. Each build additionally checks 53 retained/rebuilt query
certificates and rejects an intentionally wrong query context. The [final default Linux rerun](benchmark_results/portable-sort-final-linux-20260909/timing.json)
checks all four answers on an AMD EPYC 9V45 runner: median CPU falls from
**50.470 to 20.764 seconds**, about **2.43× faster on this query**. Conflicts drop
from 1,125,799 to 519,320, identically in both repetitions. All portable complete
proofs match the Mac proof. This is the promoted source at `45f2389`, verified by
[the hosted workflow](https://github.com/msharpe248/bsat/actions/runs/34313559174).

The earlier candidate run used an AMD EPYC 7763 runner. Absolute times across
those different machines are not paired measurements; each old/new comparison
was serial on the same runner. The newer machine lets the old solver finish
inside sixty seconds, while the within-host improvement remains substantial.
The [final Mac executable](benchmark_results/portable-sort-final-mac-20260909.json)
also verifies the identical proof twice. [Final validation record](benchmark_results/portable-sort-final-validation-20260909.json).

## Reproduction and limits

`tests/reduction_host_trace.py` runs the fixed-conflict boundary controls. Define
`BSAT_REDUCTION_TRACE` only for diagnostic builds; it emits pre/post sort records
and does not change order. Normal builds always use the portable routine.
`.github/workflows/c-reduction-candidate.yml` builds the pre-fix source at
`67d80b51bfbae3e2c7be6b77726d5cc346e44a51` separately and compares it with the
current default. The historical candidate at `51e6c9a` required
`BSAT_PORTABLE_REDUCTION_SORT`; that temporary gate is no longer present.

This closes the identified reduction-order portability gap and improves Linux
cal3 solving. It does not close the remaining gap to CaDiCaL/Kissat or establish
representative production throughput, deployment SLOs, or universal soundness.
