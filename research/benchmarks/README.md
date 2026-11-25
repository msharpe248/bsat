# SAT Solver Benchmarking Suite

Comprehensive benchmarking tools and test generators for evaluating SAT solver performance.

## Quick Start

### Run Benchmarks on Medium Test Suite
```bash
cd research/benchmarks   # the generators use paths relative to this directory

# The medium suite is already checked in at ../../dataset/medium_tests/medium_suite/.
# Optional: regenerate it (deletes and recreates ../../dataset/medium_tests; seeds are fixed)
./generate_medium_tests.py

# Run all solvers
./run_all_benchmarks.py ../../dataset/medium_tests -t 30

# Run specific solvers
./run_all_benchmarks.py ../../dataset/medium_tests -t 30 -s DPLL CDCL CGPM-SAT
```

Results are saved in `../../dataset/results/results-YYYY-MM-DD-HH-MM-SS/`

`run_all_benchmarks.py` treats each subdirectory of the dataset directory as a "family" and
runs every `*.cnf` file in it; CNF files placed directly in the dataset directory are ignored.

Solver names accepted by `-s`: `DPLL`, `CDCL`, `CoBD-SAT`, `BB-CDCL`, `LA-CDCL`, `CGPM-SAT`,
`TPM-SAT`, `SSTA-SAT`, `VPL-SAT`, `CQP-SAT`, `MAB-SAT`, `CCG-SAT`, `HAS-SAT`, `CEGP-SAT`,
`MARKET-SAT`, `PHYSARUM-SAT`, `FOLD-SAT`, `P-BIT` (18 in total; all are run when `-s` is omitted).

**Dependencies**: `run_all_benchmarks.py` and the generators need only the standard library.
`benchmark.py` and the scripts that import it (`validate_correctness.py`,
`statistical_benchmark.py`, `profile_solvers.py`, `competition_benchmark.py`) use the third-party
`tabulate` package for table formatting if it is installed and fall back to a built-in formatter otherwise.

## Test Suites

### Medium Tests (Recommended)
**53 instances** ranging from easy to moderately challenging

- **Location**: `../../dataset/medium_tests/medium_suite/` (checked in)
- **Regenerate** (optional): `./generate_medium_tests.py`
- **Run**: `./run_all_benchmarks.py ../../dataset/medium_tests -t 30`
- **Difficulty**: 10-120 variables, solve times 0.001s to ~10s

### Simple Tests
**9 very easy instances** for quick validation

- **Location**: `../../dataset/simple_tests/simple_suite/` (checked in)
- **Regenerate** (optional): `./generate_simple_tests.py` - note that this deletes
  `../../dataset/simple_tests` and writes the CNF files directly into it, so afterwards move
  them into a family directory: `mkdir ../../dataset/simple_tests/simple_suite && mv ../../dataset/simple_tests/*.cnf ../../dataset/simple_tests/simple_suite/`
- **Run**: `./run_all_benchmarks.py ../../dataset/simple_tests -t 10`
- **Difficulty**: 5-30 variables, all solve in < 0.1s

### SAT Competition 2025 Benchmarks
**399 real competition instances** - extremely challenging

- **Warning**: These are world-class hard problems designed for industrial solvers
- **Not checked in**: `../../dataset/sat_competition2025/` contains only a README; the instances
  must be downloaded into per-family subdirectories first (see that README)
- **Run**: `./run_all_benchmarks.py ../../dataset/sat_competition2025 -t 120`
- **Expected**: Many timeouts on basic implementations

### SAT Competition 2025 Small Subset
**8 smallest competition instances** - still very hard

- **Generate**: `./create_small_test_set.py` (requires the full competition download above;
  creates `../../dataset/sat_competition2025_small/`)
- **Run**: `./run_all_benchmarks.py ../../dataset/sat_competition2025_small -t 60`
- **Difficulty**: 149-500 variables, most will timeout

## Available Tools

### Benchmarking
- `run_all_benchmarks.py` - **Main benchmark runner** with timeout support
- `benchmark.py` - Original comprehensive benchmarking tool on generated problems (`--formula`, `--file`, `--random`, `--all`; solver sets via `--original-suite`, `--new-suite`, `--all-solvers`)
- `statistical_benchmark.py` - Statistical analysis with multiple runs (`--runs`, `--output`)
- `profile_solvers.py` - Performance profiling and bottleneck analysis (`--output`)

### Test Generation
- `generate_medium_tests.py` - Generate 53 medium-difficulty instances
- `generate_simple_tests.py` - Generate 9 easy instances
- `create_small_test_set.py` - Extract small subset from competition benchmarks

### Validation
- `validate_correctness.py` - Verify solver correctness on 8 generated problems (no arguments)
- `reproduce_validation.sh` - Runs the validation scripts in sequence (`--quick`, `--help`); its
  final step runs `benchmark.py --all`
- See `VALIDATION_GUIDE.md` for details

### Competition Benchmarks
- `competition_benchmark.py` - Run against generated competition-style instances (random 3-SAT, graph coloring, pigeonhole)
- `run_competition_instances.py` - Test a hard-coded list of real SAT Competition 2025 files (requires the downloaded dataset)

## Latest Benchmark Results

See `BENCHMARK_RESULTS.md` for comprehensive results from the medium test suite.

### Highlights (Medium Test Suite, 53 instances; historical run of October 17, 2025)

> These figures predate the October 20, 2025 soundness fixes to the baseline CDCL solver, whose
> 27 timeouts (and one false UNSAT) in this run were symptoms of those bugs. A partial re-run on the
> current code has CDCL fastest on 50 of 53 instances with no timeouts; see `BENCHMARK_RESULTS.md`.

| Solver | Win Rate | Timeouts | Best For |
|--------|----------|----------|----------|
| **CGPM-SAT** | 47% (25/53) | 0 | General use - consistently fast |
| DPLL | 34% (18/53) | 6 | Simple problems (< 30 vars) |
| LA-CDCL | 11% (6/53) | 6 | Structured instances |
| CDCL | 8% (4/53) | 27 | Very small instances only |

**Winner (in that run)**: CGPM-SAT with graph-based heuristics

## Documentation

- `BENCHMARK_RESULTS.md` - Latest benchmark results and analysis
- `BENCHMARK_RUNNER_GUIDE.md` - Detailed guide for run_all_benchmarks.py
- `VALIDATION_GUIDE.md` - Correctness validation procedures
- `README.md` - This file

## Usage Examples

### Generate and Run Custom Tests
```bash
# (Optional) regenerate the 53 medium-difficulty instances
./generate_medium_tests.py

# Benchmark all solvers with 30s timeout
./run_all_benchmarks.py ../../dataset/medium_tests -t 30

# View results
cat ../../dataset/results/results-*/overall_rankings.txt
```

### Quick Validation
```bash
# Simple tests are checked in at ../../dataset/simple_tests/simple_suite/

# Validate all solvers complete successfully
./run_all_benchmarks.py ../../dataset/simple_tests -t 10 -s DPLL CDCL CGPM-SAT
```

### Profile Solvers
```bash
# Profile all available solvers on two small random 3-SAT problems (10 and 15 vars)
python3 profile_solvers.py --output profile_report.md
```

## Result Files

After running benchmarks, results are saved in:
```
dataset/results/results-YYYY-MM-DD-HH-MM-SS/
├── master_summary.csv          # All results in CSV format
├── overall_rankings.txt         # Win/place/timeout summary
├── <family>.results.txt         # Detailed per-instance results
└── <family>.summary.txt         # Per-family rankings
```

## Tips

1. **Start with simple tests** to verify solvers work correctly
2. **Use medium tests** for meaningful performance comparison
3. **Adjust timeout** based on instance difficulty (-t flag)
4. **Select specific solvers** to reduce benchmark time (-s flag)
5. **Competition benchmarks** are aspirational - expect many timeouts

## Contributing

When adding new solvers or test generators:
1. Update solver list in `run_all_benchmarks.py`
2. Add test generator to this directory
3. Run benchmarks and update `BENCHMARK_RESULTS.md`
4. Update this README with new tools/results
