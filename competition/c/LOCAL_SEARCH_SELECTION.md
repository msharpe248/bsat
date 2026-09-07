# Ordered local-search clause selection

## Change and tradeoff

The previous implementation selected a random unsatisfied-clause rank, then
scanned all clauses from the start to find that rank. Each flip could therefore
inspect the entire formula, even when only a few clauses changed satisfaction.

A one-based Fenwick tree now stores unsatisfied-clause counts in original clause
order. It is built in linear time at walk initialization. Clauses entering or
leaving the unsatisfied set update their ancestor counts in logarithmic time;
rank selection also takes logarithmic time. The random draw, modulo operation,
clause ordering, and subsequent variable selection are preserved. No new option
or default change is introduced.

The additional allocation is `4 * (num_clauses + 1)` bytes, plus one pointer in the
local-search state. Allocation failure follows the existing cleanup path, and the
allocation size is checked before adding the one-based sentinel. The tree is
rebuilt on each walk and freed with the state. This trades extra initialization
and update work for cheaper selection; it need not help every formula. Local
search remains opt-in, so default CDCL does not allocate this tree.

## Verification

- All 24 C executable suites pass in release and ASan/UBSan builds.
- The new regression compares 2,040 walk prefixes against a reference that scans
  the formula and recomputes scores. It checks assignments, RNG state, flip
  counts, unsatisfied counts, and every tree node. Clause counts span 4 through
  1,025, including both sides of powers of two; walks include greedy, mixed and
  random selection, including a zero RNG seed and all four assignments of the
  contradictory core. Repeated runs also exercise tree reconstruction.
- The existing 996-prefix score oracle and root-preservation tests still pass.
- Independent validation passes 416 model/proof checks per build.
- Both builds pass 36 short-deadline cases, with maximum observed CPU overruns of
  0.002 seconds in release and 0.041 seconds in debug. This is observed coverage,
  not a worst-case bound on initialization or clause scanning.
- On the six committed planted formulas with three seeds, both implementations
  produce 18/18 independently checked models with identical flip counts. See
  [planted comparison](benchmark_results/walk-tree-planted-20260907.json).

## Fixed-work performance

Baseline is `fcb82cf`, using the same standalone driver and release flags as the
candidate. The generator adds a contradictory two-variable core to random 3-SAT
at 4.2 clauses per variable. This forces all 100,000 flips to execute. UNKNOWN is
expected: these runs measure work, not certified UNSAT solving. They are synthetic
development inputs, not competition instances.

Three repetitions per implementation/input, seed 1 and noise 0.5, were run serially
without concurrent builds or validation. Internal walk CPU excludes startup,
parsing, and state allocation, but includes assignment and tree initialization.

| Variables | Clauses | Scan median CPU | Tree median CPU | Speed ratio |
| ---: | ---: | ---: | ---: | ---: |
| 1,000 | 4,204 | 0.065766 s | 0.014294 s | 4.60x |
| 10,000 | 42,004 | 0.751043 s | 0.030755 s | 24.42x |

Both versions execute exactly 100,000 flips, ending with 10 and 223 unsatisfied
clauses respectively. These final counts supplement the prefix oracle; matching
counts alone would not prove matching trajectories. The harness PAR-2 summary is
uninformative for these deliberately unfinished walks; use the recorded `Walk
CPU time` statistic. [Raw results](benchmark_results/walk-tree-fixed-work-20260907.json)
include executable/input hashes and all repetitions.

Generate the fixed-work corpus and run the candidate from the repository root:

```sh
python3 competition/c/tests/generate_walk_selection_corpus.py /tmp/bsat-walk-selection-corpus
make -C competition/c local-search-benchmark MODE=release
python3 competition/c/tests/benchmark.py \
  --solver 'tree=competition/c/bin/local_search_benchmark_release {input} 100000 1' \
  --manifest /tmp/bsat-walk-selection-corpus/manifest.json \
  --repeats 3 --timeout 60 --output /tmp/walk-tree.json
```

## Full-solver performance

The four recent development targets were run three times each with VMTF, trail
reuse, local search every 5,000 conflicts, and 1,000 flips per attempt. Limits were
ten solving CPU seconds and fifteen wall seconds. SAT models and UNSAT proofs
were independently verified outside timing.

| Target | Scan median process CPU | Tree median process CPU |
| --- | ---: | ---: |
| Battleship `ed6d842f` | 0.919508 s | 0.884991 s |
| Belpyramid `5831f356` | 3.220599 s | 3.000048 s |
| Hamiltonian `a497d784` | 0.594580 s | 0.583166 s |
| Multiplier `90bec6dc` | 0.832807 s | 0.810548 s |

Both versions certify 12/12 runs, with zero errors. Every paired non-timing solver
statistic is identical, including decisions, conflicts, propagations, local-search
calls/wins, and literal inspections. Battleship records a local-search win; the
other targets finish through CDCL. Median process CPU is approximately 2–7% lower on
these four targets. Wall timing is noisy; mean wall PAR-2 is 2.1584 versus 2.0766
seconds, which does not establish a reliable general speedup. See the
[complete comparison](benchmark_results/walk-tree-hybrid-20260907.json).

Retain the change for the measured reduction in local-search work and preserved
search behavior. These selected targets establish no additional competition
solves or competition-wide speedup. They also do not justify enabling local
search by default or increasing its flip budget without a separate evaluation.
