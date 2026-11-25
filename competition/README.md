# Competition SAT Solvers

CDCL SAT solvers aimed at SAT Competition-style workloads: an actively developed
C solver and the older Python prototype it grew out of.

The competition solver has two goals: to be genuinely competitive, and to stay
educational. The techniques it uses, and the experiments that shaped it
(including the ones that were rejected), are written up alongside the code.

## Status

| Implementation | Status | Use Case |
|----------------|--------|----------|
| **C solver** ([`c/`](c/README.md)) | Actively developed, version `1.0.0-dev`. See [c/PRODUCTION_READINESS.md](c/PRODUCTION_READINESS.md) for the capability matrix, verification evidence, and release gates. | Real solving, proofs, embedding |
| **Python prototype** ([`python/`](python/README.md)) | Algorithm prototype from October–December 2025; no longer the focus of development. Orders of magnitude slower than the C solver. | Prototyping, education |

---

## Quick Start

### C Solver (Recommended)

```bash
cd c
make
./bin/bsat instance.cnf
```

See **[C Solver Documentation](c/README.md)** for build modes, the CLI, proofs,
the portfolio mode, and the embedding API, and **[c/FEATURES.md](c/FEATURES.md)**
for every flag and default (`./bin/bsat --help`).

### Python Prototype

```bash
cd python
python competition_solver.py instance.cnf
```

See **[Python Solver Documentation](python/README.md)** for usage details.

---

## Directory Layout

```
competition/
├── c/                          # C11 solver (current work)
│   ├── src/, include/          # Solver sources and headers
│   ├── tests/                  # C unit tests, validation and benchmark scripts
│   ├── scripts/                # Corpus fetch/suite runner, acceptance gate
│   ├── examples/               # Embedding / service examples
│   ├── docs/                   # C design notes, experiment and milestone reports
│   ├── README.md               # Build, run, architecture overview
│   ├── FEATURES.md             # Feature list and full CLI reference
│   ├── PRODUCTION_READINESS.md # Capability matrix & release gates
│   ├── API_CONTRACT.md         # Public C/C++ ABI v1 contract
│   └── INSTALLING.md           # Install & packaging guide
│
├── python/                     # Python prototype
│   ├── cdcl_optimized.py       # CDCL solver (watched literals, LBD, restarts, phases)
│   ├── inprocessing.py         # Subsumption / BVE (opt-in, slow in Python)
│   ├── competition_solver.py   # DIMACS command-line wrapper
│   ├── benchmark_*.py          # Benchmark scripts behind the reports in docs/
│   ├── test_common_fixtures.py # Runs the shared fixtures in tests/
│   ├── tests/                  # Prototype test and debugging scripts
│   └── README.md               # Python solver documentation
│
├── docs/                       # Technique explanations and historical reports,
│                               #   mostly from the Python prototype (Oct 2025)
├── tests/                      # Shared DIMACS fixtures + expected results
│                               #   used by both solvers
├── FEATURE_COMPARISON.md       # C vs Python feature matrix
├── TESTING.md                  # How to run the C, Python, and cross-validation tests
├── FEATURE_TESTING.md          # Guide: choosing instances that exercise a feature
└── README.md                   # This file
```

---

## Features

The C solver is a full modern CDCL solver (alternating VMTF/VSIDS search,
tick-budgeted inprocessing, GF(2) Gaussian elimination, symmetry breaking,
DRAT/LRAT proofs, a multi-core portfolio, MaxSAT/PB/XOR input, and an embedding
API). Its feature list changes often and is maintained in one place:

- [c/README.md](c/README.md) — architecture and current defaults
- [c/FEATURES.md](c/FEATURES.md) — feature summary and complete CLI reference
- [FEATURE_COMPARISON.md](FEATURE_COMPARISON.md) — side-by-side C vs Python matrix

The Python prototype implements the core CDCL techniques only (two-watched
literals, 1-UIP learning with clause minimization, VSIDS, LBD clause management,
Luby/Glucose restarts, phase saving, plus several opt-in simplifications); see
[python/README.md](python/README.md).

---

## Performance

For current C solver measurements (SAT Competition subsets, SATLIB corpora,
proof-checked runs) see
[c/PRODUCTION_READINESS.md](c/PRODUCTION_READINESS.md#current-verification--benchmark-evidence).

### C vs Python (historical)

Measured in December 2025 on the repository's `dataset/medium_tests/medium_suite`
3-SAT instances; not re-measured since.

| Test Set | C Solver | Python Solver | Speedup |
|----------|----------|---------------|---------|
| 5 hard instances | 0.010s | 2.258s | **226×** |
| 53 all instances | 0.082s | ~60s | **700×+** |

The C solver still solves all 53 medium-suite instances.

---

## Building and Testing

```bash
cd c
make              # Optimized build
make MODE=debug   # Debug build with sanitizers
make pgo          # Profile-Guided Optimization (Clang/LLVM)
make test         # Build and run the C unit tests
```

The Python prototype needs no build: Python 3 with no external dependencies
(it imports the `bsat` package from the repository's `src/`).

See [TESTING.md](TESTING.md) for the shared-fixture, Python, and C-vs-Python
cross-validation tests.

---

## Documentation

| Document | Description |
|----------|-------------|
| [C Solver README](c/README.md) | Build, run, architecture, embedding |
| [C Solver Features](c/FEATURES.md) | Feature summary and CLI reference |
| [C Production Readiness](c/PRODUCTION_READINESS.md) | Capability matrix, evidence, release gates |
| [Python Solver README](python/README.md) | Prototype usage and development timeline |
| [Feature Comparison](FEATURE_COMPARISON.md) | C vs Python feature matrix |
| [Testing Guide](TESTING.md) | Running the test suites |
| [Feature Testing Guide](FEATURE_TESTING.md) | Picking instances that trigger specific features |
| [docs/](docs/README.md) | Technique explanations and historical Python-prototype reports |
| [tests/](tests/README.md) | Shared fixture documentation |

---

## References

- **MiniSat**: Eén & Sörensson (2003) - Original modern CDCL
- **Glucose**: Audemard & Simon (2009) - LBD and adaptive restarts
- **MapleSat**: Liang et al. (2016) - LRB/CHB branching heuristic
- **Kissat**: Biere et al. (2020) - State-of-the-art techniques
- **SatELite**: Eén & Biere (2005) - Variable elimination

---

**Last updated**: November 2026
