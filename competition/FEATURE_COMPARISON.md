# Feature Comparison: C vs Python Competition Solvers

Current feature comparison between the C competition solver ([`competition/c/`](c/README.md)) and the Python reference competition solver ([`competition/python/`](python/README.md)).

## Summary

- **C Solver (`competition/c/bin/bsat`)**: Full modern CDCL solver with tick-budgeted inprocessing, dynamic GF(2) Gaussian elimination, exact BreakID-style symmetry breaking, SBVA, AMO sequential counters, multi-core clause-sharing portfolio (`-j N`), DRAT proofs, and opaque C/IPASIR embedding API.
- **Python Solver (`competition/python/cdcl_optimized.py`)**: Clean educational/reference CDCL implementation with watched literals, 1-UIP learning, VSIDS, Glucose/Luby restarts, phase saving, and basic preprocessing.

---

## Feature Matrix

| Feature | C Implementation | Python Implementation | Notes |
|---|---|---|---|
| **Core CDCL & BCP** | | | |
| Two-Watched Literals + Blocking Literals | ✅ | ✅ | C adds literal-indexed `lit_values[]`, binary-first watches, 1-ahead prefetch, circular scan |
| First-UIP Conflict Analysis | ✅ | ✅ | C includes on-the-fly self-subsumption (OTFS) |
| Chronological Backtracking | ✅ (default ON) | ✅ | C configurable via `--chrono` / `--chrono-levels` |
| Clause Minimization & Shrinking | ✅ (default ON) | ✅ (basic) | C: 3-state binary-aware DFS + block all-UIP shrinking + binary-resolution minimization |
| LBD & 3-Tier Learned Clause DB | ✅ (default ON) | ✅ | C: dynamic LBD updates, Tier-1 glue (`<=2`), used-protected Tier-2 (`<=6`), portable reduction sort |
| On-the-Fly Subsumption | ✅ (default ON) | ✅ | |
| **Branching, Restarts & Phases** | | | |
| Alternating Focused (VMTF) / Stable (4-ary VSIDS) | ✅ (default ON) | ❌ (VSIDS only) | C also supports `--vmtf`, `--lrb`, `--vsids` |
| Glucose EMA / AVG + Reluctant + Luby Restarts | ✅ (default ON) | ✅ | C uses Biere–Fröhlich LBD EMA in Focused mode and reluctant doubling in Stable mode |
| Restart Trail Reuse | ✅ (default ON) | ❌ | `--reuse-trail` / `--no-reuse-trail` |
| Phase Saving & 6-Step Target Rephasing | ✅ (default ON) | ✅ (phase saving) | C uses Kissat-style target rephasing + ProbSAT phase seeding |
| **Preprocessing & Inprocessing** | | | |
| Failed-Literal Probing, HBR & Intersection | ✅ (default ON) | ✅ (basic probing) | C derives RUP-certified HBR binaries and double-lookahead backbone units |
| Gate Congruence & Tarjan SCC | ✅ (default ON) | ❌ | C: iterative fixpoint 2-input AND/XOR/ITE + N-ary AND + 3-input `MAJ3`/`XOR3` + in-place SCC |
| Global Gate-DAG Simulation & Mini-Kitten Sweep | ✅ (default ON) | ❌ | 64-bit simulation + dual-cone CDCL SAT sweeping with RUP lemma projection |
| Model-Guided Global Backbone | ✅ (default ON) | ❌ | RUP-certified root units |
| Forward Subsumption, SSR & Gate/Semantic BVE | ✅ (default ON) | ❌ | Cascading min-degree BVE (AND/XOR/ITE + Mini-CDCL semantic definitions) + model reconstruction |
| Transitive Reduction & Prefix Vivification | ✅ (default ON) | ❌ | Post-BVE binary transitive reduction + O(n) prefix-sharing vivification |
| Dynamic GF(2) XOR Gaussian Elimination | ✅ (DRAT proof-producing: root RREF & bounded live in-search) | ❌ | 2..7-var XOR, subsumed recovery, Tseitin chain stitching, XOR-chain DRAT root RREF & live Bird/Han-Jiang matrices |
| Exact Syntactic Symmetry Breaking (`--sym`) | ✅ (DRAT/RAT proof-producing) | ❌ | BreakID-style verified automorphisms + O(n) single-pair peeling / insertion-sort DRAT proofs |
| AMO Sequential Counters & Structured BVA (`--sbva`) | ✅ (DRAT/RAT proof-producing) | ❌ | Sinz counters + 3-hop Structured BVA, both emitting RAT-valid DRAT proofs under `--proof` |
| **Proofs, Parallelism & Embedding** | | | |
| Text & Binary DRAT Proof Logging | ✅ | ❌ | Independently verified with `drat-trim` and `cake_lpr` |
| Multi-Core Clause-Sharing Portfolio (`-j N`) | ✅ | ❌ | 8 diversified pthread profiles, LBD-filtered sharing (`size <= 8, LBD <= 3`), unit ring, hash dedup |
| Opaque ABI v1, IPASIR 1.0, Python `ctypes` & Frozen Vars | ✅ | ❌ | `bsat.h`, `ipasir.h`, `bsat.solve_c`/`CSATSolver`, `BSAT_REUSE_LEARNTS`, `BSAT_CERTIFICATES`, `solver_freeze` |

See [`competition/c/FEATURES.md`](c/FEATURES.md) for the complete C CLI reference and [`competition/c/README.md`](c/README.md) for build and benchmarking instructions.
