# Competition SAT Solver

A competition-strength SAT solver combining state-of-the-art CDCL with novel algorithms (CGPM-SAT and CoBD-SAT).

## Goal

Build a SAT solver capable of competing with Kissat/CaDiCaL, with competitive advantage in specialized tracks through:
- **CGPM-SAT**: Conflict Graph Pattern Mining with PageRank-based variable selection
- **CoBD-SAT**: Community-Based Decomposition for modular instances
- **Adaptive Strategy**: Automatically selects best approach based on instance structure

## Project Structure

```
competition/
├── python/          # Phase 1: Optimized Python prototypes (Months 1-3)
├── c/               # Phase 2-4: C implementation (Months 4-16)
├── benchmarks/      # SAT Competition instances and results
└── docs/            # Design documents and optimization notes
```

## Development Phases

### Phase 1: Python Optimization & Validation (Months 1-3)
**Status**: 🚧 In Progress

Optimize Python CDCL and validate that CGPM/CoBD algorithms work at competition scale:
- ✅ Two-watched literals (50-100× propagation speedup)
- ⏳ LBD clause management
- ⏳ Inprocessing
- ⏳ Scale up CGPM-SAT and CoBD-SAT

**Deliverable**: Python solver handling 1000-5000 variable instances

### Phase 2: C Core Development (Months 4-9)
**Status**: ⏸️ Not Started

Build competition-grade CDCL engine in C:
- Study Kissat source code
- Implement modern CDCL with two-watched literals, VSIDS, LBD
- Add state-of-the-art optimizations (inprocessing, advanced branching)

**Target**: Within 2× of Kissat on general instances

### Phase 3: Novel Algorithm Integration (Months 10-13)
**Status**: ⏸️ Not Started

Integrate CGPM and CoBD in C:
- Incremental PageRank on conflict graph
- Community detection and parallel solving
- Adaptive strategy selection

**Target**: Beat Kissat on 30% of modular/structured instances

### Phase 4: Competition Readiness (Months 14-16)
**Status**: ⏸️ Not Started

Polish for competition submission:
- Parallel portfolio solver
- Parameter tuning
- DRAT proof generation

**Target**: Top-10 in specialized track, top-20 overall

## Current Focus

**Week 1-2**: Implementing two-watched literals in Python CDCL

This is the single most important optimization (50-100× speedup on unit propagation, which is 70-80% of solver time). Once validated in Python, this becomes the foundation for the C implementation.

## Quick Start

### Python Solver (Current)

```bash
cd competition/python
python cdcl_optimized.py instance.cnf
```

### C Solver (Future)

```bash
cd competition/c
make
./solver instance.cnf
```

## Benchmarking

```bash
cd competition/benchmarks
./download_benchmarks.sh  # Download SAT Competition instances
python run_benchmark.py   # Run full benchmark suite
```

## References

- **Kissat**: https://github.com/arminbiere/kissat (Study this for C implementation)
- **SAT Competition**: https://satcompetition.github.io/
- **CGPM-SAT**: `../research/cgpm_sat/README.md`
- **CoBD-SAT**: `../research/cobd_sat/README.md`

## Progress Tracking

See `docs/progress.md` for detailed week-by-week progress updates.
