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
| Two-Watched Literals | ✅ | ✅ | Blocking literal optimization |
| First UIP Conflict Analysis | ✅ | ✅ | Standard 1-UIP scheme |
| MiniSat Clause Minimization | ✅ | ✅ | 67% literal reduction |
| LBD Calculation | ✅ | ✅ | Clause quality metric |
| Clause Database Reduction | ✅ | ✅ | LBD-based with glue protection |
| On-the-Fly Subsumption | ✅ | ✅ | Remove subsumed clauses |

### Variable Ordering

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| VSIDS | ✅ | ✅ | Heap-based with activity decay |
| LRB/CHB | ✅ | ❌ | Learning Rate Branching |

### Phase Management

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Phase Saving | ✅ | ✅ | Remember variable polarities |
| Target Rephasing | ✅ | ❌ | Kissat-style periodic reset |
| Random Phase Selection | ✅ | ✅ | Escape local minima |

### Restart Strategies

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Luby Sequence | ✅ | ✅ | Provably good for all instances |
| Glucose EMA | ✅ | ✅ | Conservative adaptive restarts |
| Glucose AVG | ✅ | ✅ | Aggressive sliding window |

### Preprocessing

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Failed Literal Probing | ✅ | ✅ | Discover implied units |
| Blocked Clause Elimination | ✅ | ✅ | Remove redundant clauses |
| Bounded Variable Elimination | ✅ | ✅ | SatELite-style BVE |

### Advanced Features

| Feature | C | Python | Notes |
|---------|---|--------|-------|
| Vivification Inprocessing | ✅ | ❌ | Strengthen learned clauses |
| Local Search Hybridization | ✅ | ❌ | WalkSAT for SAT instances |
| DRAT Proof Logging | ✅ | ❌ | Verifiable UNSAT proofs |
| Chronological Backtracking | ✅ | ✅ | Preserve more assignments |

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
