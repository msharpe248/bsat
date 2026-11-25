# Feature Comparison: C vs Python Competition Solvers

Current feature comparison between the C competition solver ([`competition/c/`](c/README.md)) and the Python prototype competition solver ([`competition/python/`](python/README.md)). The C column summarizes [`c/FEATURES.md`](c/FEATURES.md), which is authoritative for C flags and defaults; the Python column reflects the constructor options of `CDCLSolver` in `python/cdcl_optimized.py`.

## Summary

- **C Solver (`competition/c/bin/bsat`)**: Full modern CDCL solver with tick-budgeted inprocessing, dynamic GF(2) Gaussian elimination, exact BreakID-style symmetry breaking, SBVA, AMO sequential counters, multi-core clause-sharing portfolio (`-j N`), DRAT and native LRAT proofs, MaxSAT / pseudo-Boolean / XOR-CNF input, and opaque C/IPASIR embedding API.
- **Python Solver (`competition/python/cdcl_optimized.py`)**: Educational/prototype CDCL implementation with watched literals (with blocking literals), 1-UIP learning, clause minimization, VSIDS, LBD clause management, Glucose/Luby restarts, phase saving with adaptive random phases, and opt-in (default OFF) probing, BCE, vivification, chronological backtracking, and subsumption/BVE inprocessing.

---

## Feature Matrix

| Feature | C Implementation | Python Implementation | Notes |
|---|---|---|---|
| **Core CDCL & BCP** | | | |
| Two-Watched Literals + Blocking Literals | ✅ | ✅ | C adds literal-indexed `lit_values[]`, binary-first watches, 1-ahead prefetch, circular scan |
| First-UIP Conflict Analysis | ✅ | ✅ | C includes on-the-fly self-subsumption (OTFS) |
| Chronological Backtracking | ✅ (default ON) | ✅ (opt-in, `enable_chrono_bt`) | C configurable via `--chrono` / `--chrono-levels` |
| Clause Minimization & Shrinking | ✅ (default ON) | ✅ (recursive minimization only) | C: 3-state binary-aware DFS + block all-UIP shrinking + binary-resolution minimization |
| LBD & 3-Tier Learned Clause DB | ✅ (default ON) | ✅ (LBD-ranked reduction with glue protection; no tiers) | C: dynamic LBD updates, Tier-1 glue (`<=2`), used-protected Tier-2 (`<=6`), portable reduction sort |
| On-the-Fly Subsumption | ✅ (default ON) | ✅ | |
| **Branching, Restarts & Phases** | | | |
| Alternating Focused (VMTF) / Stable (4-ary VSIDS) | ✅ (default ON) | ❌ (VSIDS only, linear scan for the maximum) | C also supports `--vmtf`, `--lrb`, `--vsids` |
| Glucose EMA / AVG + Reluctant + Luby Restarts | ✅ (default ON) | ✅ (no reluctant doubling) | C uses Biere–Fröhlich LBD EMA in Focused mode and reluctant doubling in Stable mode; Python defaults to Glucose AVG (`restart_strategy='glucose'`), with `'glucose_ema'` and `'luby'` selectable, plus restart postponing |
| Restart Trail Reuse | ✅ (default ON) | ❌ | `--reuse-trail` / `--no-reuse-trail` |
| Phase Saving & 6-Step Target Rephasing | ✅ (default ON) | ✅ (phase saving) | C uses Kissat-style target rephasing + ProbSAT phase seeding |
| Random Phase Selection | ✅ (opt-in, `--random-phase`, fixed 1% probability) | ✅ (adaptive, default ON) | Python auto-enables 5% random phases when the restart rate indicates a stuck search (`adaptive_random_phase`); C has no automatic stuck-state adaptation |
| **Preprocessing & Inprocessing** | | | |
| Failed-Literal Probing, HBR & Intersection | ✅ (default ON) | ✅ (basic probing, opt-in `enable_probing`) | C derives RUP-certified HBR binaries and double-lookahead backbone units |
| Gate Congruence & Tarjan SCC | ✅ (default ON) | ❌ | C: iterative fixpoint 2-input AND/XOR/ITE + N-ary AND + 3-input `MAJ3`/`XOR3` + in-place SCC |
| Global Gate-DAG Simulation & Mini-Kitten Sweep | ✅ (default ON) | ❌ | 64-bit simulation + dual-cone CDCL SAT sweeping with RUP lemma projection |
| Model-Guided Global Backbone | ✅ (default ON) | ❌ | RUP-certified root units |
| Forward Subsumption, SSR & Gate/Semantic BVE | ✅ (default ON) | ⚠️ (plain subsumption/SSR/BVE, opt-in `enable_inprocessing`, experimental) | C: cascading min-degree BVE (AND/XOR/ITE + Mini-CDCL semantic definitions) + model reconstruction. Python: `inprocessing.py`, default OFF because it is too slow in Python |
| Transitive Reduction & Prefix Vivification | ✅ (default ON) | ✅ (basic vivification only, opt-in `enable_vivification`) | C: post-BVE binary transitive reduction + O(n) prefix-sharing vivification |
| Blocked Clause Elimination (BCE) | ✅ (opt-in, `--bce`) | ✅ (opt-in, `enable_bce`) | Default OFF in both |
| Dynamic GF(2) XOR Gaussian Elimination | ✅ (DRAT proof-producing: root RREF & bounded live in-search) | ❌ | 2..7-var XOR, subsumed recovery, Tseitin chain stitching, XOR-chain DRAT root RREF & live Bird/Han-Jiang matrices |
| Exact Syntactic Symmetry Breaking (`--sym`) | ✅ (DRAT/RAT proof-producing) | ❌ | BreakID-style verified automorphisms + O(n) single-pair peeling / insertion-sort DRAT proofs |
| AMO Sequential Counters & Structured BVA (`--sbva`) | ✅ (DRAT/RAT proof-producing) | ❌ | Sinz counters + 3-hop Structured BVA, both emitting RAT-valid DRAT proofs under `--proof` |
| **Proofs, Parallelism & Embedding** | | | |
| Text & Binary DRAT + Native LRAT Proofs | ✅ (`--proof`, `--lrat`) | ❌ | DRAT verified with `drat-trim`/`cake_lpr`; native text & binary LRAT linearly checkable directly |
| Core-Guided MaxSAT (RC2/OLL), `.opb`, XOR-CNF | ✅ (`--maxsat`, `--opb`, `x` lines) | ❌ | Sinz, incremental Totalizer, Generalized Totalizer (GTE), binary-adder PB, WCNF classic & MSE 2022+ |
| AllSAT / Projected Enumeration, Backbone & MUS | ✅ (`--models`, `--backbone`, `--mus`) | ❌ | Flip-filtered exact backbone, core-trimmed RMR minimal MUS, projected blocking AllSAT |
| Multi-Core Clause-Sharing Portfolio (`-j N`) | ✅ | ❌ | 8 diversified pthread profiles (incl. fast-start & WalkSAT-hybridized SAT-hunter), LBD-filtered sharing, unit ring, hash dedup |
| Opaque ABI v1, IPASIR 1.0, Python `ctypes` & Frozen Vars | ✅ | ❌ | `bsat.h` (incl. `freeze`/`melt`, `set_threads`, `set_proof`/`lrat`, `add_xor`/`atmost`/`pb_le`, `backbone`, `mus`), `ipasir.h`, `bsat.solve_c`/`CSATSolver`, `BSAT_REUSE_LEARNTS`, `BSAT_CERTIFICATES` |

See [`competition/c/FEATURES.md`](c/FEATURES.md) for the complete C CLI reference and [`competition/c/README.md`](c/README.md) for build and benchmarking instructions.
