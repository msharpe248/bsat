# Keep heap branching as the default

The additional-instance screen does not support promoting VMTF to the default.
At ten solving CPU seconds, default heap branching verifies 3/16 inputs and
VMTF verifies 2/16, losing battleship and gaining none. Pinned Kissat verifies
3/16. There are zero answer/checker errors. The current C solver and branching
default are unchanged; this milestone improves benchmark selection and input
verification so future tuning is less dependent on the reused 28-input sample.

## Reproducible selection

`tests/select_corpus.py` selects the smallest eligible CNFs, using relative path
as the size tie-breaker, with at most one input per family. It excludes filenames
and SHA256 strings found anywhere in the configured directory's JSON reports,
and excludes byte-identical duplicates within the new selection. Hashes cover
file contents, so renaming a previously measured input does not make it new.
Malformed history, insufficient eligible families or invalid limits fail without
replacing the requested output. The output itself is excluded from history so
rerunning against unchanged history produces the same manifest.

This screen requested 16 families and a two-million-byte file-size ceiling.
The selected files are 32,558–162,107 bytes. The manifest records each path,
family, byte size and content hash, plus hashes of all 149 prior JSON reports.
Eligibility means absent by filename/content hash from that recorded C benchmark
history. It does not establish absence from all prior work. Families may have
appeared in earlier experiments; this is an additional-instance development
screen, not a held-out-family or competition score. Small-file selection is an
explicit sampling bias, and these short limits leave most cases unresolved.

`tests/benchmark.py --manifest FILE` accepts the selected input object directly.
It checks every input's SHA256 before launching any timed solver, resolves
relative input paths against the manifest directory, rejects duplicate resolved
paths, and records the manifest's own hash in the result. It rejects combining a
manifest with positional inputs. Existing positional input/directory runs remain
supported. This prevents accidentally benchmarking changed bytes under a prior
selection record. The original screen predates the new CLI option; its recorded
input hashes were explicitly compared with the selection manifest and all match.

For example, from the C directory:

```sh
python3 tests/select_corpus.py --dataset ../../dataset/sat_competition2025 \
  --reports benchmark_results --families 16 --max-bytes 2000000 \
  --output /tmp/next-corpus.json
python3 tests/benchmark.py --manifest /tmp/next-corpus.json \
  --solver 'heap=bin/bsat --time 10 --proof {proof} {input}' \
  --solver 'vmtf=bin/bsat --vmtf --time 10 --proof {proof} {input}' \
  --checker /path/to/drat-trim --timeout 15 --repeats 1 --output /tmp/results.json
```

The history changes after new results are recorded, so later selections can
legitimately differ. The archived manifest fixes this screen's exact selection.

## Comparison

All profiles use the same 16 inputs, once each, in seeded serial order. Both
BSAT profiles use the same executable built from `47d38cb`. Kissat is pinned to
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`. Solving limits are ten CPU seconds and
fifteen wall seconds; proof checking is outside solver timing. SAT models and
UNSAT certificates are independently checked. No local builds or tests ran
during timing. Binary hashes, input hashes and commands are in the report.

| Profile | Verified solves | Mean wall PAR-2 | Peak RSS (MB) |
| --- | ---: | ---: | ---: |
| Default heap | 3/16 | 24.8952 | 27.62 |
| VMTF | 2/16 | 26.2623 | 29.95 |
| Pinned Kissat | 3/16 | 24.3919 | 69.21 |

All solve p-center quickly. Heap and VMTF solve Hamiltonian in 0.343 and 0.135
process CPU seconds respectively; Kissat takes 0.060 seconds. Heap solves
battleship in 5.733 process CPU seconds, VMTF reaches its limit, and Kissat solves
it in 0.150 seconds. Every other input remains UNKNOWN in all three profiles.
Equal solve counts between heap and Kissat on this short sample do not establish
competition parity: the battleship runtime gap is large and thirteen inputs are
unresolved by every solver.

## Tool tests

Six Python tests cover selection exclusions across alternate history schemas,
renamed content, same-name files with changed contents, duplicate selected
content, family diversity, deterministic replay, limits, malformed history,
manifest-relative paths, manifest hashing, invalid/missing hashes, aliases,
missing files, and changed-input rejection before benchmark output is created.
They are included in the C CI workflow. No C runtime change was made, so the
existing C correctness matrix was not needlessly rerun for these Python changes.

- [Selection manifest and history hashes](benchmark_results/default-order-corpus-20260907.json)
- [Three-profile comparison](benchmark_results/default-order-screen-20260907.json)

## Repeated decision check

Three further repetitions on battleship and p-center use the new hash-checked
manifest path. Heap verifies 6/6 runs; VMTF verifies 3/6, with zero errors. Heap
solves battleship every time in 5.538–5.773 process CPU seconds and exactly
258382 conflicts. VMTF reaches the limit in all three runs at 378620–384002
conflicts. Both profiles solve the p-center control in all repetitions, with
independently verified UNSAT proofs. This makes the battleship loss repeatable
and supports keeping heap as the general default. VMTF remains available for
cases where prior measurements show it helps.

The large gap to Kissat on battleship remains a useful target for search and
preprocessing work. Merely switching the default order does not close it.

- [Repeat manifest](benchmark_results/default-order-repeat-corpus-20260907.json)
- [Repeated comparison](benchmark_results/default-order-repeat-20260907.json)

The existing positional-input CLI also passed a post-change smoke run on
p-center with an independently verified UNSAT certificate. The manifest CLI
regression additionally checks that mixing manifests and positional inputs is
rejected before any benchmark runs.
