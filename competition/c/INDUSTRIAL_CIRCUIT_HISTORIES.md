# Real circuit histories — 2026-09-08

192 public-ABI queries passed independent validation across four upstream
verification circuits and all four supported flag combinations. BSAT returned
96 SAT, 92 UNSAT and four UNKNOWN results. Retained CaDiCaL and fresh Kissat both
resolved all queries; there were no contradictory answers.

This adds real circuit structure beyond the generated linked-CNF and BMC tests.
It is a generated bounded-model-checking history over published, preprocessed
circuits, not a captured customer transaction log or an unbounded safety proof.

## Provenance and scope

Inputs come from the HWMCC19 `fold_fraigy_orchestrate` collection in the pinned
[CAV 2025 artifact](https://github.com/TechnionFV/CAV_2025_artifact/tree/7df3ce27c569615bdd205dbd6b473ab2c33953a7).
The repo stores download URLs, sizes and SHA-256 values, not copies of the circuits.

| Circuit | Inputs | Latches | AND gates |
|---|---:|---:|---:|
| cal3 | 55 | 23 | 453 |
| usb_phy | 291 | 76 | 621 |
| vgasim_imgfifo-p039 | 689 | 593 | 3,962 |
| gen23 | 1,350 | 774 | 7,176 |

The official [AIGER converter](https://github.com/arminbiere/aiger/tree/039ec1a2cc37d3093ac35c4b6df65336b346f409)
decodes binary AIG files to ASCII. The strict local reader accepts one legacy
safety output, topological AND gates and reset values 0, 1 or the latch itself
(uninitialized). It rejects extended property/constraint/justice/fairness fields.
This prevents accidentally dropping constraints or interpreting liveness as safety.

Histories grow through frames 0, 1, 2, 4, 8 and 16, retaining each solver between
queries. Each depth queries the output and its negation as temporary assumptions.
The largest snapshot has 158,101 variables and 391,536 permanent clauses.
Every positive bad-state query is UNSAT in the two references at these depths;
SAT counts refer to the complementary assumptions, not observed bugs in hardware.

## Acceptance and limits

Every SAT model is checked against the exact original CNF plus assumption units,
then independently simulated from primary inputs and initial latch values.
Simulation computes gate outputs and next states instead of trusting model gates.
The encoder has exhaustive small-transition tests covering inversion, constants,
known/unknown reset, and mutation detection for wrong gate/latch values.

Every reference UNSAT has DRAT converted to LRAT and accepted by CakeML's
`cake_lpr`. Every conclusive certified BSAT query exports exactly the retained
permanent clauses plus the current assumptions; its UNSAT proof is checked by the
same independent chain. UNKNOWN is recorded without a correctness claim.

Limits are five process-CPU seconds for each BSAT solve, ten cooperative wall
seconds for retained CaDiCaL, ten external wall seconds for fresh Kissat, and
60 wall seconds per checker stage. These are validation budgets, not equal-budget
competitive solver rankings. Serial release API timings exclude encoding, input
addition, export and checking. Debug results are correctness evidence only.

All four BSAT timeouts are `cal3`, positive output, depth 16. Both reference
solvers prove these UNSAT. This is a concrete unresolved performance gap.
At the common conclusive queries, aggregate BSAT query CPU was 0.766/0.787 s
without certification (flags 0/1) and 1.424/1.151 s with certification (flags 2/3).
These single-history measurements justify investigating certification overhead;
they do not isolate proof I/O from preprocessing and search changes. The maximum
observed certified journal was about 31.3 MB, including learning from a timed-out
query that remained in the retained session.

## Reproduction and continuous checks

Build pinned AIGER with `./configure.sh && make aigtoaig`, then:

```sh
python3 competition/c/tests/prepare_industrial_aig.py --aigtoaig /path/to/aigtoaig --output /tmp/circuits
make -C competition/c shared
python3 competition/c/tests/industrial_histories.py \
  --library /path/to/libbsat_release.so --reference /path/to/kissat \
  --incremental-reference /path/to/libcadical.so --circuits /tmp/circuits \
  --output /tmp/industrial.json
```

Set `BSAT_DRAT_TRIM` and `BSAT_CAKE_LPR` to the independently built tools.
The integration workflow builds all pinned dependencies and runs all four flags
through depth 4; the recorded local release campaign goes through depth 16.
Reports include circuit, converter, solver and checker hashes:
[release](benchmark_results/industrial-histories-20260908.json) and
[ASan/UBSan smoke](benchmark_results/industrial-histories-debug-20260908.json).
