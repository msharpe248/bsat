# SAT Solver Benchmarking Suite

Comprehensive benchmarking tools and test generators for evaluating SAT solver performance.

## Quick Start

### Run Benchmarks on Medium Test Suite
```bash
# Generate medium-difficulty test instances (recommended)
./generate_medium_tests.py

# Run all solvers
./run_all_benchmarks.py ../../dataset/medium_tests -t 30

# Run specific solvers
./run_all_benchmarks.py ../../dataset/medium_tests -t 30 -s DPLL CDCL CGPM-SAT
```

Results are saved in `../../dataset/results/results-YYYY-MM-DD-HH-MM-SS/`

## Test Suites

### Medium Tests (Recommended)
**53 instances** ranging from easy to moderately challenging

- **Generate**: `./generate_medium_tests.py`
- **Run**: `./run_all_benchmarks.py ../../dataset/medium_tests -t 30`
- **Difficulty**: 10-120 variables, solve times 0.001s to ~10s

### Simple Tests
**9 very easy instances** for quick validation

- **Generate**: `./generate_simple_tests.py`
- **Run**: `./run_all_benchmarks.py ../../dataset/simple_tests -t 10`
- **Difficulty**: 5-30 variables, all solve in < 0.1s

### SAT Competition 2025 Benchmarks
**399 real competition instances** - extremely challenging

- **Warning**: These are world-class hard problems designed for industrial solvers
- **Run**: `./run_all_benchmarks.py ../../dataset/sat_competition2025 -t 120`
- **Expected**: Many timeouts on basic implementations

### SAT Competition 2025 Small Subset
**8 smallest competition instances** - still very hard

- **Generate**: `./create_small_test_set.py`
- **Run**: `./run_all_benchmarks.py ../../dataset/sat_competition2025_small -t 60`
- **Difficulty**: 149-500 variables, most will timeout

## Available Tools

### Benchmarking
- `run_all_benchmarks.py` - **Main benchmark runner** with timeout support
- `benchmark.py` - Original comprehensive benchmarking tool
- `statistical_benchmark.py` - Statistical analysis with multiple runs
- `profile_solvers.py` - Performance profiling and bottleneck analysis

### Test Generation
- `generate_medium_tests.py` - Generate 53 medium-difficulty instances
- `generate_simple_tests.py` - Generate 9 easy instances
- `create_small_test_set.py` - Extract small subset from competition benchmarks

### Validation
- `validate_correctness.py` - Verify solver correctness on test suite
- See `VALIDATION_GUIDE.md` for details

### Competition Benchmarks
- `competition_benchmark.py` - Run against real SAT competition instances
- `run_competition_instances.py` - Test specific competition files

## Latest Benchmark Results

See `BENCHMARK_RESULTS.md` for comprehensive results from the medium test suite.

### Highlights (Medium Test Suite, 53 instances)

| Solver | Win Rate | Timeouts | Best For |
|--------|----------|----------|----------|
| **CGPM-SAT** | 47% (25/53) | 0 | General use - consistently fast |
| DPLL | 34% (18/53) | 6 | Simple problems (< 30 vars) |
| LA-CDCL | 11% (6/53) | 6 | Structured instances |
| CDCL | 8% (4/53) | 27 | Very small instances only |

**Winner**: CGPM-SAT with graph-based heuristics shows dominant performance

## Documentation

- `BENCHMARK_RESULTS.md` - Latest benchmark results and analysis
- `BENCHMARK_RUNNER_GUIDE.md` - Detailed guide for run_all_benchmarks.py
- `VALIDATION_GUIDE.md` - Correctness validation procedures
- `README.md` - This file

## Usage Examples

### Generate and Run Custom Tests
```bash
# Create 53 medium-difficulty instances
./generate_medium_tests.py

# Benchmark all solvers with 30s timeout
./run_all_benchmarks.py ../../dataset/medium_tests -t 30

# View results
cat ../../dataset/results/results-*/overall_rankings.txt
```

### Quick Validation
```bash
# Generate simple tests
./generate_simple_tests.py

# Validate all solvers complete successfully
./run_all_benchmarks.py ../../dataset/simple_tests -t 10 -s DPLL CDCL CGPM-SAT
```

### Profile Specific Solver
```bash
# Profile CGPM-SAT on medium instances
python3 profile_solvers.py
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
