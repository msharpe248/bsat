# Proof compaction complete-cost experiment — 2026-09-09

Decision: retain the ordinary direct LRAT/CakeML acceptance path. Pre-trimming
substantially shrinks the saved DRAT artifact, but fails the frozen complete-CPU
gate. No default, public API or certificate trust change is made.

Exact certified retained cal3 depth-16 and cal100 depth-4 exports are compared in
serial direct/trim/trim/direct order. Trimming runs pinned DRAT-trim with
`-l compact.drat -C`, then independently verifies that output against the same
original query through the ordinary LRAT/CakeML chain. Direct conversion skips the
extra trimming pass. Both get 600 seconds per stage and 2048/512 MiB checker
heap/stack. All eight final acceptance paths verify; the extra pass is fully charged,
including parent orchestration CPU for copying/hashing and child CPU for tools.

| Query | Original → compact DRAT | Direct → trim+check mean CPU | Relative CPU change |
|---|---:|---:|---:|
| cal3 | 131.13 → 16.08 MB | 21.927 → 21.920 s | -0.03% |
| cal100 | 69.16 → 28.30 MB | 7.742 → 10.268 s | +32.63% |

Artifact bytes fall about 87.7% and 59.1%, respectively, but this is not faster
acceptance. Wall means are 21.949 → 21.943 seconds on cal3 and 7.754 → 10.283 on
cal100; minor wall fluctuation does not override the predeclared CPU gate. Peak
child RSS is cumulative across subprocesses, so it cannot establish an isolated
memory improvement. Trimming also needs additional temporary files.

The [pinned converter source](https://raw.githubusercontent.com/marijnheule/drat-trim/2e3b2dc0ecf938addbd779d42877b6ed69d9a985/drat-trim.c)
disables its literal-removal branch while emitting LRAT, motivating the separate
DRAT trimming hypothesis. The result shows why preprocessing/checking work must
be counted rather than judging certificate byte count alone. The raw journal
still grows during solving; this experiment does not provide journal compaction
safe for future assumptions or additions.

Reproduce with `tests/benchmark_proof_compaction.py` and explicit input, proof,
converter, checker and new output directory. Evidence under benchmark_results:
`proof-compaction-{cal3,cal100,decision}-20260909.json`. Hashes and full converter
and independent checker logs are preserved. The large temporary proof copies are
not committed; the frozen baseline/diagnostic histories reproduce the exports.


The initial eight-path screen also verifies but recorded child CPU separately
without parent CPU. Those reports remain under `*-initial-20260909.json`. The
final repetition above closes that accounting gap. Its negligible cal3 difference
is measurement noise, and cal100 clearly fails the frozen gate. No result was
selected by rerunning until a positive outcome.
