# Exact local-search score maintenance

## Defect and algorithm

The field named `break_count` represents the change in unsatisfied-clause count
if a variable is flipped: break minus make. Initialization computed this quantity,
but the old incremental update recalculated only the flipped variable. Its
neighbours retained obsolete contributions from changed clauses, so greedy
selection stopped using the declared score after the first affected flip.
The final SAT model checker still protected reported answers; this is a score-
maintenance defect, not evidence of an incorrect reported SAT model.

Update contributions only for clauses containing the flipped variable:

- At zero to one true literal, remove every variable's make contribution and add
  the flipped variable's new break contribution.
- At one to zero, apply the inverse change.
- At one to two, remove the other true variable's break contribution.
- At two to one, add the remaining true variable's break contribution.
- When at least two literals stay true, the clause contributes no score change.

This relies on the existing normalized-clause invariant: each variable occurs at
most once per clause. It allocates nothing per flip and does not recompute the
whole formula. Longer affected clauses may need scanning at the transitions
above. The score is exact, but changing a heuristic's scores does not guarantee
faster solving on every input.

## Verification

The regression fails against `6d32c73` at a score-versus-recomputed-delta assertion.
It checks clause truth counts, unsatisfied counts, and every variable's score
against the actual objective difference from a temporary flip. Across 996 walk
prefixes it covers greedy, mixed and random selection, multiple phases/seeds,
binary through four-literal clauses, and a 64-literal clause whose sole remaining
true literal can be at the end. Contradictory subformulas keep walks active across
all requested prefixes, rather than ending before the incremental path is tested.

All 23 C test executables pass in release and ASan/UBSan builds. The focused
validator passes 416 model/proof checks per build. Both builds pass 36 deadline
cases; maximum observed CPU overrun is 0.002 seconds in release and 0.059 in debug.
These short tests do not prove a worst-case latency bound for every local-search
initialization or clause scan.

## Standalone walk evaluation

`tests/generate_walk_corpus.py` creates six planted 3-SAT development formulas:
two each at 50, 100 and 200 variables, with 4.2 clauses per variable. The committed
[manifest](benchmark_results/walk-score-corpus-20260907.json) includes hashes and
planted models. The inputs were fixed before timing; they are small synthetic
examples, not held-out competition evidence.

`make local-search-benchmark` builds a standalone driver that emits standard SAT
models or UNKNOWN after its flip budget. CI builds and smoke-tests this driver. The release and sanitizer smoke checks
both pass locally, and all six manifest planted models are independently checked.
The baseline links this driver with `local_search.c` from `6d32c73`; the other
solver objects are unchanged by this milestone. Each implementation gets three
walk seeds per formula, 20,000 flips, noise 0.5, and ten wall seconds. The benchmark
runner independently verifies every reported model.

[Complete standalone comparison](benchmark_results/walk-score-standalone-20260907.json):

| Implementation | Verified models | Median flips | Total flips |
| --- | ---: | ---: | ---: |
| Stale neighbour scores | 18/18 | 737 | 32,647 |
| Exact scores | 18/18 | 425 | 33,168 |

Median flips improve, but the total rises slightly. Summed internal walk CPU is
0.006011 versus 0.007023 seconds; these very short runs do not support a reliable
throughput claim. Both implementations solve all inputs, so no solved-count gain
is established by this standalone sample.

## Hybrid solver evaluation

The four recent development targets were run twice per implementation with VMTF,
trail reuse, local search every 5,000 conflicts and 1,000 flips per attempt. Limits
are ten solving CPU seconds and fifteen wall seconds. Timing is serial and
separate from builds and validation; model/proof verification is outside timing.

[Complete targeted comparison](benchmark_results/walk-score-hybrid-20260907.json):

| Scores | Certified runs | Mean wall PAR-2 | Errors |
| --- | ---: | ---: | ---: |
| Stale | 6/8 | 8.9344 | 0 |
| Exact | 8/8 | 1.8934 | 0 |

Both exact-score battleship runs produce a certified model through local search,
in 0.896–0.944 process CPU seconds. Both stale-score runs time out at ten solving
CPU seconds. The other three targets remain solved in both repeats; they record
no local-search wins. A [follow-up with seeds 2, 3 and 4](benchmark_results/walk-score-seeds-20260907.json)
solves battleship in all three exact-score runs (1.718, 0.375 and 4.107 process CPU
seconds, respectively), each through local search. Stale scores solve only seed 3
(4.944 CPU seconds); the other two time out. This supports the targeted result
across four seeds, while remaining evidence from one selected input.

The fixed 44-input development corpus compares the corrected hybrid with plain
VMTF CDCL plus trail reuse, using the same ten-CPU/fifteen-wall limits and one run
per profile/input. The [complete broad comparison](benchmark_results/walk-score-broad-20260907.json)
records 8/44 verified solves for each profile, with no errors. Mean wall PAR-2 is
24.9433 for CDCL and 24.7026 for hybrid (about 1% lower). Hybrid gains the battleship
input `ed6d842f96d10f3400bce251f9e95bfb`, but loses ensemble-computation
`67aa1a256f92020dccdf66b235de3986`: plain CDCL solves it at 9.981 process CPU
seconds, near the ten-second solving limit. Thus the broad sample establishes no
net solved-count gain. These are selected development inputs and single runs,
not a competition ranking or evidence for changing the defaults.

The implementation is retained because it repairs an independently reproduced
score invariant and improves the targeted hybrid results. Local search remains
off by default; enabling it has mixed effects across the broader corpus.
