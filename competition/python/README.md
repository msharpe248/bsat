# Competition SAT Solver

Optimized CDCL solver for SAT Competition, implementing two-watched literals, LBD clause management, and targeting performance competitive with industrial solvers like Kissat/CaDiCaL.

## Directory Structure

```
competition/python/
├── cdcl_optimized.py          # Main CDCL solver with two-watched literals
├── inprocessing.py            # Inprocessing algorithms (subsumption, BVE)
├── benchmark_speedup.py       # Main performance benchmark script
├── tests/                     # Test suite and debugging scripts
│   ├── test_simple.py
│   ├── test_correctness.py
│   ├── test_comprehensive.py
│   ├── test_inprocessing.py
│   ├── test_inprocessing_soundness.py
│   ├── benchmark_inprocessing.py
│   └── debug_*.py, trace_*.py (historical debugging)
└── README.md                  # This file
```

## Core Files

### cdcl_optimized.py
Main competition solver implementation with:
- **Two-watched literals**: O(1) amortized unit propagation (100-600× speedup)
- **LBD clause management**: Glucose-style learned clause quality heuristic
- **Glucose adaptive restarts**: Dynamic restarts based on LBD trends
- **Phase saving**: Remember variable polarities across restarts (50-134× speedup on structured problems)
- **VSIDS heuristic**: Dynamic variable ordering
- **1UIP clause learning**: First Unique Implication Point learning scheme
- **Initial unit clause propagation**: Fixed soundness bug (October 20, 2025)

**Status**: Production-ready, 114-188× average speedup on medium/competition instances

### inprocessing.py
Preprocessing techniques applied during search:
- **Subsumption**: Remove clauses subsumed by others
- **Self-subsuming resolution**: Strengthen clauses
- **Bounded variable elimination (BVE)**: Eliminate variables with few occurrences

**Status**: Algorithms correct but disabled by default (O(n²) too slow in Python)

### benchmark_speedup.py
Main benchmark script comparing:
- Original naive CDCL (`src/bsat/cdcl.py`)
- Watched literals only
- Watched + LBD (default configuration)

**Usage**: `python benchmark_speedup.py`

## Quick Start

```bash
# Run core tests
cd tests/
python test_comprehensive.py

# Run main benchmark (takes ~8 minutes)
python ../benchmark_speedup.py

# Import and use solver
python
>>> from cdcl_optimized import CDCLSolver
>>> import sys; sys.path.insert(0, '../../src')
>>> from bsat import CNFExpression
>>> cnf = CNFExpression.parse("(a | b) & (~a | c)")
>>> solver = CDCLSolver(cnf, use_watched_literals=True)
>>> result = solver.solve()
>>> print(result)
```

## Performance Highlights

**Week 1 Results** (Post-Bug-Fix Validation, October 20, 2025):
- Simple Tests (9 instances): 0.6× (overhead on trivial instances)
- Medium Tests (10 instances): **113.9× average speedup**
- Competition Tests (3 instances): **187.8× average speedup**
- Best speedup: **150,696× on easy_3sat_v018_c0075**

See `../docs/benchmark_results.md` for detailed analysis.

## Development Timeline

**Week 1** (Complete): ✅
- Two-watched literals implementation
- LBD clause management
- Soundness bug fix (initial unit clause propagation)
- Benchmark validation (114-188× speedup)

**Week 2-3** (Complete): ✅
- Glucose-style adaptive restarts (1.83× speedup over Luby)
- Comprehensive restart strategy benchmarking

**Week 4** (Complete): ✅
- Phase saving implementation (50-134× speedup on structured problems)
- Identified interaction between phase saving and restart strategy
- Comprehensive benchmarking and documentation

**Week 5** (Complete): ✅
- Restart postponing (Glucose 2.1+) implementation
- Blocks restarts when trail is growing (sign of progress)
- Works correctly but couldn't fix catastrophic phase saving regression
- Trail never grew on problematic instance (different solution needed)

**Week 6** (Complete): ✅
- Random phase selection (5% diversification) implementation
- FIXED catastrophic phase saving regression (3,823× speedup!)
- Excellent on medium instances (278× overall speedup)
- Disabled by default (can hurt small instances), user can enable
- Comprehensive benchmarking and documentation

**Week 7+** (Planned): ⏳
- Adaptive random frequency (auto-enable after N conflicts)
- Scale testing on larger instances (1000-5000 variables)
- Python profiling

**Month 4-9** (Planned):
- C implementation (10-100× faster than Python)
- Novel algorithms (CGPM + CoBD)
- SAT Competition 2026 submission

## References

See `../docs/` for comprehensive documentation:
- `two_watched_literals.md`: Algorithm design and implementation
- `benchmark_results.md`: Performance analysis and validation
- `adaptive_restarts.md`: Glucose vs Luby restart strategies
- `restart_comparison_results.md`: Restart strategy benchmark results
- `phase_saving.md`: Phase saving algorithm and implementation
- `phase_saving_results.md`: Phase saving benchmark results
- `random_phase_selection.md`: Random phase selection algorithm and implementation
- `random_phase_results.md`: Random phase selection benchmark results
- `week1_summary.md`: Week 1 progress summary
- `progress.md`: Week-by-week tracking

## Contributing

Tests are in `tests/`. All core tests should pass before committing changes.

Run test suite:
```bash
cd tests/
python test_comprehensive.py
python test_correctness.py
```
