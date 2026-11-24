# Competition SAT Solver

Modern CDCL SAT solvers with both C and Python implementations, featuring state-of-the-art conflict-driven clause learning algorithms.

## Status: Production Ready

Both implementations are feature-complete with competition-level optimizations:

| Implementation | Performance | Use Case |
|----------------|-------------|----------|
| **C Solver** | 100-700× faster than Python | Production, competitions |
| **Python Solver** | Reference implementation | Prototyping, education |

---

## Quick Start

### C Solver (Recommended)

```bash
cd c
make
./bin/bsat instance.cnf
```

See **[C Solver Documentation](c/README.md)** for full CLI reference and options.

### Python Solver

```bash
cd python
python competition_solver.py instance.cnf
```

See **[Python Solver Documentation](python/README.md)** for usage details.

---

## Project Structure

```
competition/
├── c/                          # C Implementation
│   ├── src/                    # Source files
│   ├── include/                # Header files
│   ├── tests/                  # Unit tests
│   ├── scripts/                # Test and benchmark scripts
│   ├── README.md               # C solver documentation
│   └── FEATURES.md             # Detailed feature list
│
├── python/                     # Python Implementation
│   ├── cdcl_optimized.py       # Optimized CDCL solver
│   ├── competition_solver.py   # Competition-format wrapper
│   └── README.md               # Python solver documentation
│
└── README.md                   # This file
```

---

## Features Implemented

### Core CDCL Algorithm

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Two-Watched Literals | ✅ | ✅ | Binary-first watches, blocking literals, 1-ahead prefetch, circular scan |
| First UIP Conflict Analysis | ✅ | ✅ | Standard 1-UIP scheme with On-the-Fly Self-Subsumption (OTFS) |
| Clause Minimization & Shrinking | ✅ | ✅ | 3-state binary-aware DFS + block-level all-UIP shrinking + binary resolution |
| LBD & 3-Tier Clause DB | ✅ | ✅ | Dynamic LBD updates, adaptive Tier-1/Tier-2 retention, lazy watch sweep |
| On-the-Fly Subsumption | ✅ | ✅ | Remove subsumed clauses |

### Variable Ordering

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| 4-ary Heap VSIDS + VMTF Alternation | ✅ | ✅ (VSIDS) | Tick-based Focused (VMTF) / Stable (4-ary VSIDS) alternation + reason bumping |
| LRB/CHB | ✅ | ❌ | Learning Rate Branching (`--lrb`) |

### Phase Management

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Phase Saving | ✅ | ✅ | Remember variable polarities |
| 6-Step Target Rephasing | ✅ | ❌ | Kissat-style cycling target/original/inverted/flipped rephasing |
| Random Phase Selection | ✅ | ✅ | Opt-in diversification (`--random-phase`) |

### Restart Strategies

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Glucose EMA + Reluctant Doubling | ✅ | ✅ (EMA) | Fast/slow Biere-Fröhlich LBD EMA (Focused) + reluctant doubling (Stable) |
| Luby Sequence | ✅ | ✅ | Optional (`--luby-restart`) |
| Glucose AVG | ✅ | ✅ | Optional sliding window (`--glucose-restart-avg`) |

### Preprocessing & Inprocessing

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Failed Literal Probing & HBR | ✅ | ✅ (probing) | Root-prioritized probing + Hyper-Binary Resolution |
| Tarjan SCC & Congruence Closure | ✅ | ❌ | In-place equivalence substitution + AND/XOR/ITE gate congruence |
| Mini-Kitten CDCL SAT Sweeping | ✅ | ❌ | Embedded local CDCL sweeper with 16-model partitioning & RUP proof projection |
| Gate-Aware BVE & SSR/Subsumption | ✅ | ❌ | Min-degree cascading BVE (AND/XOR/ITE) + 64-bit bloom SSR |
| Prefix-Sharing Vivification & Transitive Reduction | ✅ | ❌ | O(n) prefix vivification + post-BVE binary transitive reduction |
| GF(2) XOR Gaussian Elimination | ✅ | ❌ | Exact 2..6-var XOR extraction & bit-packed RREF (non-proof mode) |
| Blocked Clause Elimination & Factoring (BVA) | ✅ | ❌ | Linear BCE (`--bce`) + bounded clause factoring (`--factor` / adaptive) |

### Advanced Features

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Local Search Hybridization | ✅ | ❌ | Periodic WalkSAT phase seeding + standalone WalkSAT (`--local-search`) |
| DRAT Proof Logging | ✅ | ❌ | Independently verifiable text & binary DRAT proofs |
| Chronological Backtracking | ✅ | ✅ | Enabled by default (`--chrono` / `--no-chrono`) |
| Profile-Guided Optimization (PGO) | ✅ | ❌ | `make -C competition/c pgo` (Clang/LLVM) |

---

## Performance

### C vs Python Comparison

| Test Set | C Solver | Python Solver | Speedup |
|----------|----------|---------------|---------|
| 5 hard instances | 0.010s | 2.258s | **226×** |
| 53 all instances | 0.082s | ~60s | **700×+** |

### Test Results

- **C Solver**: 53/53 medium test instances pass
- **Python Solver**: All correctness tests pass
- Both produce identical SAT/UNSAT results

---

## Building

### C Solver

```bash
cd c
make              # Optimized build
make MODE=debug   # Debug build with sanitizers
make pgo          # Profile-Guided Optimization
make test         # Run unit tests
```

### Python Solver

No build required. Uses Python 3.7+ with no external dependencies.

---

## Documentation

| Document | Description |
|----------|-------------|
| [C Solver README](c/README.md) | Complete CLI reference, build options |
| [C Solver Features](c/FEATURES.md) | Detailed feature documentation |
| [Python Solver README](python/README.md) | Usage, benchmarks, development timeline |

---

## References

- **MiniSat**: Eén & Sörensson (2003) - Original modern CDCL
- **Glucose**: Audemard & Simon (2009) - LBD and adaptive restarts
- **MapleSat**: Liang et al. (2016) - LRB/CHB branching heuristic
- **Kissat**: Biere et al. (2020) - State-of-the-art techniques
- **SatELite**: Eén & Biere (2005) - Variable elimination

---

**Last updated**: December 2025
