# Competition SAT Solvers

A CDCL SAT solver written in C11, aimed at SAT Competition-style workloads.

The competition solver has two goals: to be genuinely competitive, and to stay
educational. The techniques it uses, and the experiments that shaped it
(including the ones that were rejected), are written up alongside the code.

The solver grew out of a Python prototype. That prototype has been retired: its
core algorithms (two-watched literals, clause minimization, LBD clause
management, phase saving, restarts, and the simplification techniques) now live
in the educational Python package as
[`ModernCDCLSolver`](../docs/modern-cdcl.md) and
[`bsat.simplification`](../docs/simplification.md), each with its own
explanation in [`docs/`](../docs/README.md). Read those first if you want to
understand a technique before looking at the C implementation of it.

## Status

Actively developed, version `1.0.0-dev`. See
[c/PRODUCTION_READINESS.md](c/PRODUCTION_READINESS.md) for the capability
matrix, verification evidence, and release gates.

---

## Quick Start

```bash
cd c
make
./bin/bsat instance.cnf
```

See **[C Solver Documentation](c/README.md)** for build modes, the CLI, proofs,
the portfolio mode, and the embedding API, and **[c/FEATURES.md](c/FEATURES.md)**
for every flag and default (`./bin/bsat --help`).

---

## Directory Layout

```
competition/
├── c/                          # C11 solver
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
├── tests/                      # Shared DIMACS fixtures + expected results
├── TESTING.md                  # How to run the C and cross-validation tests
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

For readable Python versions of the core techniques, see
[docs/modern-cdcl.md](../docs/modern-cdcl.md) and
[docs/simplification.md](../docs/simplification.md).

---

## Performance

For current C solver measurements (SAT Competition subsets, SATLIB corpora,
proof-checked runs) see
[c/PRODUCTION_READINESS.md](c/PRODUCTION_READINESS.md#current-verification--benchmark-evidence).

---

## Building and Testing

```bash
cd c
make              # Optimized build
make MODE=debug   # Debug build with sanitizers
make pgo          # Profile-Guided Optimization (Clang/LLVM)
make test         # Build and run the C unit tests
```

See [TESTING.md](TESTING.md) for the shared-fixture and C-vs-Python
cross-validation tests.

---

## Documentation

| Document | Description |
|----------|-------------|
| [C Solver README](c/README.md) | Build, run, architecture, embedding |
| [C Solver Features](c/FEATURES.md) | Feature summary and CLI reference |
| [C Production Readiness](c/PRODUCTION_READINESS.md) | Capability matrix, evidence, release gates |
| [Testing Guide](TESTING.md) | Running the test suites |
| [Feature Testing Guide](FEATURE_TESTING.md) | Picking instances that trigger specific features |
| [Modern CDCL in Python](../docs/modern-cdcl.md) | Readable Python implementations of the core techniques |
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
