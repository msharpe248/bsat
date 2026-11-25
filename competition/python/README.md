# Competition SAT Solver — Python Prototype

Python CDCL prototype implementing two-watched literals, LBD clause management, adaptive restarts, and phase saving. It was the algorithm test bed (October–December 2025) for the competition effort; the solver that actually targets Kissat/CaDiCaL-class performance is the C implementation in [`../c/`](../c/README.md). See [`../FEATURE_COMPARISON.md`](../FEATURE_COMPARISON.md) for a side-by-side feature matrix.

## Directory Structure

```
competition/python/
├── cdcl_optimized.py          # Main CDCL solver with two-watched literals
├── inprocessing.py            # Inprocessing algorithms (subsumption, BVE)
├── competition_solver.py      # DIMACS command-line wrapper (competition output format)
├── benchmark_speedup.py       # Main performance benchmark script
├── benchmark_restarts.py      # Luby vs Glucose (see ../docs/restart_comparison_results.md)
├── benchmark_phase_saving.py  # Phase saving (see ../docs/phase_saving_results.md)
├── benchmark_random_phase.py  # Random phase (see ../docs/random_phase_results.md)
├── benchmark_adaptive.py      # Adaptive random phase (see ../docs/adaptive_random_phase.md)
├── test_common_fixtures.py    # Shared fixture tests (../tests/fixtures), see ../TESTING.md
├── tests/                     # Test suite and debugging scripts (see tests/README.md)
│   ├── test_simple.py
│   ├── test_correctness.py
│   ├── test_comprehensive.py
│   ├── test_competition_solver.py
│   ├── test_soundness.py
│   ├── test_inprocessing.py
│   ├── test_inprocessing_soundness.py
│   ├── benchmark_inprocessing.py
│   └── debug_simple.py, trace_bug.py, test_watch_key_bug.py (historical debugging)
└── README.md                  # This file
```

## Core Files

### cdcl_optimized.py
Main competition solver implementation with:
- **Two-watched literals** with blocking literals: O(1) amortized unit propagation (100-600× speedup)
- **LBD clause management**: Glucose-style learned clause quality heuristic
- **Restarts**: Glucose sliding-window AVG (default), Glucose EMA, or Luby (`restart_strategy`), with restart postponing
- **Phase saving**: Remember variable polarities across restarts (50-134× speedup on structured problems)
- **Random phase selection**: off by default; adaptive mode (default on) enables 5% random phases when the search looks stuck
- **VSIDS heuristic**: Dynamic variable ordering
- **1UIP clause learning** with MiniSat-style recursive clause minimization and on-the-fly subsumption
- **Opt-in (default off)**: failed literal probing (`enable_probing`), blocked clause elimination (`enable_bce`), chronological backtracking (`enable_chrono_bt`), vivification (`enable_vivification`)
- **Initial unit clause propagation**: Fixed soundness bug (October 20, 2025)

**Status**: Prototype. The default configuration (`use_watched_literals=True`) passes the shared fixtures and agrees with the C solver on them; 114-188× average speedup over the original `src/bsat/cdcl.py` was measured in October 2025 on medium/competition instances. The legacy naive propagation mode (`use_watched_literals=False`) is kept for comparison; it agrees with the default mode on all 10 instances of `tests/test_soundness.py` (see [tests/README.md](tests/README.md)).

### competition_solver.py
Command-line wrapper that reads a DIMACS file and prints competition-format output (`s SATISFIABLE` / `s UNSATISFIABLE` / `s UNKNOWN`, `v ... 0`):

```bash
python competition_solver.py instance.cnf [--verbose] [--max-conflicts N] [--output FILE]
```

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
# Solve a DIMACS file
python competition_solver.py ../../dataset/simple_tests/simple_suite/sat_v10.cnf

# Run the shared fixture tests (10 tests)
python test_common_fixtures.py

# Run prototype tests
cd tests/
python test_correctness.py

# Run main benchmark (takes ~8 minutes)
python ../benchmark_speedup.py
cd ..

# Import and use solver (from competition/python)
python
>>> from cdcl_optimized import CDCLSolver   # also puts ../../src on sys.path
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
- Competition Tests (3 instances): **187.8× average speedup** (the `dataset/sat_competition2025_small` set used here is not in the repository, so `benchmark_speedup.py` finds no instances for this suite)
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

**Week 7** (Complete): ✅
- Adaptive random frequency implementation (auto-enable when stuck)
- Detects stuck state (high restart rate > 20%)
- BEST OF BOTH WORLDS: No overhead on easy, automatic help on hard
- 22.70× faster than static on simple suite, 50.20× faster than baseline on medium

**December 2025** (Complete): ✅
- MiniSat-style clause minimization and blocking literals
- Opt-in failed literal probing, blocked clause elimination, chronological backtracking, and vivification (added for parity with the C solver of the time)

**Afterwards**:
- Development moved to the C implementation in [`../c/`](../c/README.md), which has since gone far beyond this prototype. The earlier plans for this prototype (scale testing on 1000-5000 variable instances, Python profiling, competition instance benchmarking) were not carried out in Python.

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
- `adaptive_random_phase.md`: Adaptive random phase (auto-enable when stuck)
- `week1_summary.md`: Week 1 progress summary
- `progress.md`: Week-by-week tracking

## Contributing

Tests are in `tests/` (see [tests/README.md](tests/README.md) for what each script does and its current result) plus `test_common_fixtures.py` in this directory.

Run before committing changes:
```bash
python test_common_fixtures.py     # 10 shared fixtures, all pass
cd tests/
python test_correctness.py         # 13 formulas vs the original solver, all pass
python test_comprehensive.py       # 32 formulas, all pass
```
