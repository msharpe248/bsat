# Preserve CDCL state across unsuccessful local searches

## Problem and implementation

Every local-search attempt previously backtracked CDCL to level zero, even if
the walk failed. These backtracks were not counted as restarts, interfered with
priority-based trail reuse, and could substantially change search performance.
The local walker already has its own assignment array, so a failed attempt does
not require changing the CDCL assignment.

The caller now retains the CDCL trail while the walk runs. An unsuccessful walk
returns to the same CDCL assignment; a successful walk installs the complete
model using the coherent transfer implemented in the preceding milestone.
The original-input model check still runs before returning SAT.

Root-level assignments initialize the walk ahead of saved phases and remain
fixed during greedy and random flips. Temporary non-root assignments remain
movable. Without root-fixed variables, variable selection retains its previous
random draws and choices. An unsatisfied clause with no movable variable causes
the walk to fail; it does not return a global UNSAT answer. No additional
per-variable storage is needed.

Root preservation is a constrained-walk policy, not evidence that allowing
transient root violations previously produced incorrect SAT answers. Its role
here is to carry the existing root constraints into an isolated walk that can
start while CDCL has non-root decisions. Local search remains opt-in.

The pinned Kissat [walker](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/walk.c)
was inspected: it constructs its walk at root and uses original root values when
connecting clauses. BSAT's decision to preserve a non-root CDCL trail on failure
is specific to its separate assignment representation, not a claim to duplicate
Kissat's implementation.

## Regression and certificate coverage

The new root tests fail on `80ff549`: a propagated root solution with stale
saved phases is not recognized with a zero-flip budget. The fixed walker
recognizes it immediately. Further tests cover 64 seeded random walks that must
never flip a fixed variable, greedy exclusion, an all-fixed unsatisfied clause,
and exact seeded choices when assignments are non-root and therefore movable.

The failed-walk regression compares heap and VMTF runs on pigeonhole formulas,
with and without zero-flip unsuccessful walks. It failed with forced root
backtracking; it now requires identical conflicts, decisions, propagations,
learned literals, restart counts and literal work. A separate integration case
checks successful local search after a non-root backjump and validates the
complete transferred model and metadata.

All 22 C test executables pass in release and ASan/UBSan builds. The focused
local-search validator passes 416 model/proof checks per build across eight
configurations (two fixed and 50 random formulas, seed 20261004). The deadline
suite now includes local search and passes 36 cases per build, including heap and VMTF walks. Maximum observed
CPU overrun is 0.002 seconds in release and 0.049 seconds in debug. These short
checks do not establish a worst-case bound for all local-search initialization
and scanning work.

## Performance evaluation

Use the four recent development targets, twice per profile, ten solving CPU
seconds and fifteen wall seconds. All profiles select VMTF and trail reuse.
Hybrid profiles enable local search every 5,000 conflicts with 1,000 flips per
attempt. Timing runs serially after builds and verification; SAT models and
UNSAT proofs are independently checked outside solver timing. These selected,
reused inputs are not held-out competition evidence.

Root preservation alone retained the old forced backtrack and certified 4/8
runs, matching the previous implementation. Both missed the multiplier circuit
within ten CPU seconds. That
[initial diagnostic](benchmark_results/local-search-roots-20260907.json)
led to the state-preserving caller change. To reconstruct that initial variant
from the final implementation, restore `solver_backtrack(s, 0)` immediately
before `solver_try_local_search(s)` in the local-search scheduling branch.

The final comparison includes the old hybrid at `80ff549`, the state-preserving
hybrid, and plain CDCL as an overhead control. New `Local search calls` and
`Local search wins` statistics report actual activation. [Complete final comparison](benchmark_results/local-search-preserve-trail-20260907.json)
and [paired summary](benchmark_results/local-search-preserve-trail-summary-20260907.json).

| Profile | Certified runs | Mean wall PAR-2 | Errors |
| --- | ---: | ---: | ---: |
| Old hybrid | 4/8 | 16.1504 | 0 |
| State-preserving hybrid | 6/8 | 8.7828 | 0 |
| Plain CDCL | 6/8 | 8.7421 | 0 |

The circuit times out at ten solving CPU seconds in both old-hybrid repeats;
the new hybrid solves it in 0.789–0.836 process CPU seconds, after 18,183
conflicts and three unsuccessful local-search calls. Both battleship repeats
remain unresolved for all profiles. Belpyramid and Hamiltonian remain certified
in both repeats for all profiles.

Every completed new-hybrid/CDCL pair has identical non-timing, non-local-search
statistics (six pairs). The new hybrid records 7 failed walks on belpyramid,
6 on Hamiltonian, and 3 on the circuit; there are no local-search wins in these
runs. Thus the recovered circuit solve comes from preserving CDCL progress,
not from WalkSAT finding a model. Local-search overhead remains, and these
results do not justify enabling it by default.

Retain root-aware initialization and flips, the state-preserving caller, and
activation counters. This makes the existing opt-in hybrid behave more predictably
and removes the measured timeout regression. It does not establish competition
performance or a benefit over plain CDCL on the tested inputs.

Final measured release executable SHA256:
`e91cf123f14ee9455309a95cc9bd0c7a1d2c94e9c271087c846608b8e157fd37`.
