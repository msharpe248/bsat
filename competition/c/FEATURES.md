# BSAT C Solver — Features & CLI Reference

Authoritative reference of currently implemented solver features, defaults
(matching `default_opts()` in `src/solver.c`), and command-line flags (matching
`./bin/bsat --help` in `src/main.c`).

---

## Feature Summary

| Feature | Default | Enable | Disable |
|---|---:|---|---|
| Alternating Focused (VMTF) / Stable (4-ary VSIDS) | ON | `--alternating` | `--no-alternating` |
| VMTF in all modes | OFF | `--vmtf` | `--vsids` |
| LRB/CHB branching | OFF | `--lrb` | `--vsids` |
| Chronological backtracking (`--chrono-levels 100`) | ON | `--chrono` | `--no-chrono` |
| Restart trail reuse | ON | `--reuse-trail` | `--no-reuse-trail` |
| Glucose EMA restarts (Focused) / Reluctant (Stable) | ON | `--glucose-restart-ema` | `--luby-restart` / `--no-restarts` |
| Phase saving | ON | — | `--no-phase-saving` |
| 6-step Kissat-style target rephasing | ON | — | `--no-rephase` |
| Random phase diversification | OFF | `--random-phase` | `--no-random-phase` |
| Circular replacement watch search | ON | — | `--no-circular` |
| 3-state binary-aware clause minimization | ON | — | `--no-minimize` |
| Block-level all-UIP shrinking | ON | `--shrink` | `--no-shrink` |
| Binary-resolution minimization | ON | `--binary-minimize` | `--no-binary-minimize` |
| Dynamic LBD updates | ON | `--dynamic-lbd` | `--no-dynamic-lbd` |
| 3-tier used-clause retention | ON | `--protect-used` | `--no-protect-used` |
| On-the-fly subsumption & OTFS | ON | — | `--no-subsumption` |
| Failed-literal probing, HBR & probing intersection | ON | — | `--no-probing` |
| Tick-budgeted inprocessing pipeline (SCC, congruence, Kitten sweep, global backbone, SSR, gate BVE, transitive reduction, vivify, ProbSAT seeding, dynamic GF(2) Gauss, AMO sequential counters) | ON | `--inprocess` | `--no-inprocess` |
| Exact syntactic symmetry breaking (proof-producing DRAT/RAT) | ON | `--sym` | `--no-sym` |
| Structured Bounded Variable Addition (proof-producing DRAT/RAT) | ON | `--sbva` | `--no-sbva` |
| Standalone blocked clause elimination (BCE) | OFF | `--bce` | `--no-bce` |
| Standalone legacy BVE / SCC / Congruence / Factoring | OFF | `--elim` / `--equiv` / `--congruence` / `--factor` | `--no-elim` |
| Standalone WalkSAT local search hybridization | OFF | `--local-search` | — |
| Multi-core clause-sharing portfolio | 1 thread | `-j <n>` / `--threads <n>` | `-j 1` |
| Focused-then-alternating sequential portfolio | OFF | `--portfolio <sec>` | — |
| DRAT proof logging (text / binary) | OFF | `--proof <file>` / `--binary-proof` | — |
| Frozen-variable protection (C API) | API | `solver_freeze(s, v)` | `solver_melt(s, v)` |

---

## Core Engine Components

### 1. Propagation & Memory Layout (`src/solver.c`, `src/arena.c`, `src/watch.c`)
- **Two-watched literals** with 16-byte `ClauseHeader`, blocking literals,
  binary-first watch partitioning, implicit/tagged binary watch dispatch,
  1-ahead long-clause prefetching, and two-pass circular watch scanning.
- **Literal-indexed `lit_values[]`** shadow byte array during CDCL search for
  branchless single-load literal evaluation, backed by authoritative `values[]`
  and 4-byte `Trail` entries.
- **Forwarding arena garbage collection** (`solver_collect_garbage`) with
  bounded copy checkpoints and O(1) lazy watch sweeping.

### 2. Branching, Restarts & Phases (`src/solver.c`, `src/vmtf.c`)
- **Mode alternation** (`--alternating`, default ON): alternates tick-budgeted
  **Focused** (VMTF queue, Biere–Fröhlich LBD EMA restarts, trail reuse, saved
  phases) and **Stable** (4-ary max-heap VSIDS with dynamic decay `0.80 → 0.95`,
  reluctant doubling restarts, target phases) modes.
- **Reason-side bumping**: boosts non-learned antecedents during conflict
  analysis on larger formulas (`num_vars >= 300`).
- **6-step Stable-mode target rephasing**: cycles Kissat-style `B/W/I/B/W/O`
  (best-trail, bounded ProbSAT walk, inverted, original) polarities at level 0
  in Stable mode with growing intervals (`--rephase-interval`, default `1000`),
  syncing separated `target_phase[]` without wiping unassigned entries.
- **Alternative heuristics**: pure VMTF (`--vmtf`), recency-weighted LRB/CHB
  (`--lrb`), pure VSIDS (`--no-alternating --vsids`), sliding-window Glucose
  restarts (`--glucose-restart-avg`), and Luby restarts (`--luby-restart`).

### 3. Conflict Analysis & Clause Database (`src/solver.c`, `src/minimize.c`, `src/reduction_sort.c`)
- **1-UIP conflict analysis** with **chronological backtracking** (`--chrono`,
  threshold `--chrono-levels 100`) and **on-the-fly self-subsumption (OTFS)**.
- **3-stage learned clause reduction**: 3-state binary-aware DFS minimization,
  block-level all-UIP shrinking (`--shrink`), and binary-resolution minimization
  (`--binary-minimize`).
- **3-tier dynamic clause database**: permanent Tier-1 glue clauses
  (`LBD <= 2`), used-protected Tier-2 clauses (`LBD <= 6`, `--protect-used`),
  and LBD/activity-ranked Tier-3 reduction with deterministic portable sorting
  (`--reduce-interval 1000`, `--reduce-increment 100`, `--reduce-fraction 0.5`).

### 4. Inprocessing Engine (`src/inprocess.c`, `src/congruence.c`, `src/elim.c`)
- **Probing, HBR & Probing Intersection**: root-prioritized failed-literal
  probing, hyper-binary resolution, and RUP-certified double-lookahead backbone
  unit extraction.
- **Gate Congruence & Tarjan SCC**: preprocessing (scaled up to `800k` clauses)
  and periodic iterative fixpoint congruence across 2-input AND/XOR/ITE, N-ary
  AND (`k = 2..8`), and 3-input `MAJ3`/`XOR3` full-adder gates (1-split/2-split
  RUP), paired with in-place Tarjan SCC equivalence substitution.
- **Global Gate-DAG Simulation & Mini-Kitten Sweeping**: 64-bit bit-parallel
  gate-DAG simulation to identify cross-cone equivalence/unit candidates, proven
  via an embedded CDCL Mini-Kitten sweeper with RUP lemma projection.
- **Model-Guided Global Backbone**: probes candidate backbone literals guided by
  saved phases/models to derive RUP-verified root units.
- **Forward Subsumption, SSR, Gate-Aware + Semantic BVE & Transitive Reduction**:
  64-bit signature-filtered subsumption/SSR, cascading min-degree BVE with
  syntactic AND/XOR/ITE gate extraction and bounded Mini-CDCL UNSAT-core
  semantic definition extraction plus compact reconstruction records, followed
  by post-BVE binary transitive reduction.
- **Prefix-Sharing Vivification**: O(n) frequency-ordered prefix-sharing clause
  strengthening on irredundant and Tier-1/Tier-2 learned clauses.
- **ProbSAT Phase Seeding**: precomputed `(1 + break)^(-2.3)` polynomial break
  weights with zero-break fast path, active-var/clause gating (`<= 150k` live
  vars / `<= 450k` live clauses), explicit tick budget, and low-energy basin
  filtering before updating saved polarities and target phases.

### 5. Proof-Producing Structural Engines (`src/sbva.c`, `src/gauss.c`, `src/symmetry.c`, `src/inprocess.c`)
All four engines emit standard DRAT/RAT proofs in `--proof` mode (and stay
disabled in append-only `BSAT_CERTIFICATES` journals, assumption calls, and
`BSAT_REUSE_LEARNTS`):
- **SBVA (`--sbva`, `src/sbva.c`)**: arbitrary-length Structured BVA with 3-hop
  structural tie-breaking and RAT-valid DRAT clause emission.
- **AMO Cliques (`src/inprocess.c`)**: linear Sinz sequential-counter
  re-encoding for `>=80%`-binary formulas (`<= 250k` clauses), emitting
  temporary backward RAT definitions + forward RAT/1-step RUP blocking binaries
  under `--proof`.
- **Exact Syntactic Symmetry Breaking (`--sym`, `src/symmetry.c`)**: exact
  clause hash set over level-0-reduced irredundant clauses, verified involution
  automorphisms, disjoint variable transposition chains, and BreakID-style
  matrix row-interchange lex-leader constraints; in `--proof` mode uses O(n)
  single-pair conditional-swap peeling + failed-literal RUP on mutex columns
  and insertion-sort conditional-swap chains on non-mutex columns with zero
  residual auxiliary variables.
- **GF(2) Gaussian Elimination (`src/gauss.c`)**: 2..7-var XOR extraction,
  subsumed/strengthened XOR recovery, Tseitin XOR-chain stitching,
  connected-component root Gauss-Jordan RREF (emitting linear Tseitin XOR-chain
  addition DRAT proofs from RREF history bitmasks under `--proof`), and live
  in-search Bird/Han-Jiang two-watched-column matrices with zero-undo
  backtracking, BVE protection, temporary 1-UIP reason clauses, and persistent
  row Tseitin XOR-chain DRAT proofs on compact matrices (`<= 128` rows,
  `<= 2,048` cumulative proof-row merges, with automatic level-0 CDCL fallback
  above budget).

### 6. Parallel Portfolio & Operational Controls (`src/portfolio.c`, `src/main.c`, `src/bsat.c`)
- **Multi-core portfolio (`-j N` / `--threads N`)**: runs `N` isolated worker
  threads (`1..64`, 8 MiB stacks); Worker 0 runs full BSAT (imports only) while
  Workers `1..N-1` run 8 diversified logically-equivalent profiles (including a
  zero-preprocessing fast-start profile on Worker 2) exporting units, binaries,
  and LBD-filtered learned clauses (`size <= 8, LBD <= 3`) through a TSan-clean
  dedicated unit ring + clause ring with 64-bit canonical hash deduplication and
  immediate atomic cancellation on first winner.
- **Frozen-variable API**: `solver_freeze(s, v)`, `solver_melt(s, v)`, and
  `solver_is_frozen(s, v)` protect interface variables from BVE, SCC
  substitution, BCE, and symmetry breaking (persisting across rebuilds).
- **Progress monitoring**: send `SIGUSR1` to the CLI process to print live
  statistics without stopping search.

---

## Complete Command-Line Reference

### Output & Diagnostics
```sh
-h, --help                    Show help message
-v, --verbose                 Verbose runtime diagnostics (same as BSAT_VERBOSE=1)
    --debug                   Debug output (same as DEBUG_CDCL=1)
-q, --quiet                   Suppress output except result
-s, --stats                   Print statistics (default: ON)
    --accounting              Report inclusive phase CPU and owned memory capacities
```

### Resource Limits & Parallelism
```sh
-c, --conflicts <n>           Maximum conflicts (default: unlimited)
-d, --decisions <n>           Maximum decisions (default: unlimited)
-t, --time <sec>              CPU time limit (wall time under -j N; default: unlimited)
-j, --threads <n>             Parallel portfolio threads, 1..64 (default: 1)
    --portfolio <sec>         Sequential focused slice then fresh alternating search
    --seed <n>                Deterministic per-solver random seed (default: 1)
```

### Branching, Restarts & Phases
```sh
--alternating / --no-alternating       Focused (VMTF) / Stable (VSIDS) alternation (default: ON)
--vmtf / --lrb / --vsids               Force VMTF, LRB/CHB, or VSIDS
--var-decay <f>                        VSIDS decay factor (default: 0.95)
--var-inc <f>                          VSIDS initial increment (default: 1.0)
--chrono / --no-chrono                 Chronological backtracking (default: ON)
--chrono-levels <n>                    Backjump gap threshold (default: 100)
--reuse-trail / --no-reuse-trail       Restart trail prefix reuse (default: ON)
--glucose-restart-ema                  LBD EMA restarts (default: ON)
--glucose-restart-avg                  Sliding-window LBD restarts (default: OFF)
--glucose-fast-alpha <f>               Fast EMA decay (default: 32/33 ≈ 0.970)
--glucose-slow-alpha <f>               Slow EMA decay (default: 0.99999)
--glucose-min-conflicts <n>            Min conflicts between restarts (default: 2)
--glucose-k <f>                        Threshold multiplier (default: 10/11 ≈ 0.909)
--glucose-window-size <n>              AVG window size (default: 50)
--luby-restart / --no-luby-restart     Luby sequence restarts (default: OFF)
--luby-unit <n>                        Base Luby unit (default: 100)
--restart-first <n>                    First geometric interval (default: 100)
--restart-inc <f>                      Geometric multiplier (default: 1.5)
--no-restarts                          Disable restarts
--no-phase-saving                      Disable phase saving (default: ON)
--random-phase / --no-random-phase     Random phase override (default: OFF)
--random-prob <f>                      Random phase probability (default: 0.01)
--no-rephase                           Disable target rephasing (default: ON)
--rephase-interval <n>                 Conflicts between rephases (default: 1000)
```

### Clause Management
```sh
--max-lbd <n>                          Max LBD kept (default: 30)
--glue-lbd <n>                         Glue clause LBD threshold (default: 2)
--reduce-fraction <f>                  Fraction kept on reduction (default: 0.5)
--reduce-interval <n>                  Initial reduction interval (default: 1000)
--reduce-increment <n>                 Interval growth per reduction (default: 100)
--protect-used / --no-protect-used     Tier-2 used-clause retention (default: ON)
--dynamic-lbd / --no-dynamic-lbd       Refresh LBD during analysis (default: ON)
--shrink / --no-shrink                 Block-level all-UIP shrinking (default: ON)
--binary-minimize / --no-binary-minimize  Binary-resolution minimization (default: ON)
--iterative-minimize                   Binary-aware DFS minimization flag
--minimize-budget <n>                  Work cap per minimization (default: 10000)
--no-minimize                          Disable clause minimization
--no-subsumption                       Disable on-the-fly subsumption
--subsume-budget <n>                   Subsumption candidate cap (default: 128)
--no-circular                          Disable circular watch scanning
```

### Preprocessing, Inprocessing, Local Search & Proofs
```sh
--inprocess / --no-inprocess           Full tick-budgeted inprocessing (default: ON)
--inprocess-interval <n>               Conflicts between rounds (default: 10000)
--preprocess-budget <n>                Literal-work budget; 0 disables (default: 1000000)
--sbva / --no-sbva                     Structured BVA, proof-producing DRAT/RAT (default: ON)
--sym / --no-sym                       Exact syntactic symmetry breaking (default: ON)
--no-probing                           Disable failed-literal probing
--bce / --no-bce                       Standalone BCE preprocessing (default: OFF)
--elim / --no-elim                     Standalone legacy BVE (default: OFF)
--elim-max-occ <n> / --elim-grow <n>   Legacy BVE bounds (defaults: 10 / 0)
--equiv / --equiv-budget <n>           Standalone SCC pass (default: OFF / 1000000)
--congruence / --congruence-budget <n> Standalone congruence pass (default: OFF / 100000000)
--factor / --factor-budget <n>         Proof-producing binary/ternary BVA (default: OFF)
--factor-max-variables <n>             Fresh-variable cap for --factor (default: 1024)
--factor-min-gain <n>                  Min net clause saving for --factor (default: 1)
--local-search                         Standalone WalkSAT hybridization (default: OFF)
--ls-interval <n> / --ls-max-flips <n> WalkSAT interval / flip cap (5000 / 100000)
--ls-noise <f> / --ls-save-phases      WalkSAT noise (0.5) & phase feedback
--proof <file>                         Write DRAT proof to file
--binary-proof                         Use binary DRAT format
```
