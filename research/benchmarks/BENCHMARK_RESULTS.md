# Benchmark Results

Comprehensive benchmarking results for all SAT solvers on medium-difficulty test instances.

## Test Suite: Medium Difficulty Instances

**Dataset**: `medium_tests` - 53 generated instances
**Date**: October 17, 2025
**Timeout**: 30 seconds per solver
**Instance Range**: 10-120 variables, varying difficulty

### Instance Distribution

- **Easy** (10-30 vars): 11 instances - fast to solve
- **Medium** (40-80 vars): 21 instances - moderate challenge
- **Hard** (90-120 vars): 11 instances - several seconds
- **Mixed k-SAT**: 5 instances - 4-SAT and 5-SAT
- **Known SAT**: 5 instances - guaranteed satisfiable

## Overall Rankings

| Solver | 1st Place | 2nd Place | 3rd Place | Timeouts | Errors | Total |
|--------|-----------|-----------|-----------|----------|--------|-------|
| **CGPM-SAT** | **25** (47%) | 10 | 10 | 0 | 0 | 53 |
| **DPLL** | 18 (34%) | 17 | 10 | 6 | 0 | 53 |
| **LA-CDCL** | 6 (11%) | 8 | 2 | 6 | 0 | 53 |
| CDCL | 4 (8%) | 4 | 1 | 27 | 0 | 53 |
| CoBD-SAT | 0 | 5 | 23 | 6 | 0 | 53 |
| BB-CDCL | 0 | 6 | 2 | 14 | 0 | 53 |

## Key Findings

### CGPM-SAT: Clear Winner
- **Dominant performance**: 25 first-place finishes (47% win rate)
- **Perfect completion**: 0 timeouts across all 53 instances
- **Consistent speed**: Solved instances ranging from 0.0012s to 4.3s
- **Versatile**: Excelled on both SAT and UNSAT instances

### DPLL: Strong Baseline
- **Reliable performance**: 18 wins, especially on easier instances
- **Fast on simple problems**: Sub-millisecond on small instances
- **Few timeouts**: Only 6 timeouts (11%)
- **Production-ready**: Solid choice for small-to-medium problems

### LA-CDCL: Specialized Strength
- **6 first-place finishes** with lookahead heuristics
- **Good balance**: 8 second-place finishes
- **Moderate timeouts**: 6 instances

### Traditional CDCL: Struggles
- **High timeout rate**: 27/53 instances (51%)
- **Limited wins**: Only 4 first-place finishes
- **Best for**: Smaller instances where it can complete quickly

### Research Solvers

**CoBD-SAT** (Constraint-Based):
- Consistent third-place finisher (23 instances)
- No first-place finishes
- 6 timeouts

**BB-CDCL** (Backbone-Based):
- Heaviest timeout rate: 14 instances
- Limited to second/third place finishes
- Backbone computation overhead visible

## Performance Examples

### CGPM-SAT Wins

Fast SAT instances:
```
medium_3sat_v040_c0170.cnf: 0.0012s (RANK=1)
medium_3sat_v042_c0178.cnf: 0.0016s (RANK=1)
```

Challenging instances:
```
medium_3sat_v015_c0064.cnf: 0.8225s (RANK=1, UNSAT)
medium_3sat_v070_c0298.cnf: 1.4183s (RANK=1, SAT)
medium_3sat_v074_c0315.cnf: 3.9783s (RANK=1, UNSAT)
medium_3sat_v078_c0332.cnf: 4.3292s (RANK=1, UNSAT)
```

### DPLL Wins

DPLL excelled on easier instances with simple structure:
```
easy_3sat_v010_c0042.cnf: 0.0001s
easy_3sat_v020_c0084.cnf: 0.0008s
sat_known_v040.cnf: 0.0003s
```

## Conclusions

### Production Recommendations

1. **For general use**: **CGPM-SAT** - best overall performance, no timeouts
2. **For simple problems (< 30 vars)**: **DPLL** - extremely fast, minimal overhead
3. **For structured problems**: **LA-CDCL** - lookahead helps on specific instance types

### Research Insights

- **Graph-based heuristics** (CGPM) show significant advantage over traditional approaches
- **Backbone analysis** (BB-CDCL) adds overhead that doesn't pay off on these instances
- **Pure CDCL** needs optimizations (restarts, VSIDS, watched literals) to compete

### Future Work

- Test on larger instances (200+ variables)
- Compare with industrial solvers (MiniSat, Glucose, CryptoMiniSat)
- Analyze why CGPM-SAT excels: graph structure correlation?
- Optimize CDCL implementation with industrial techniques

## Running the Benchmarks

Generate test suite:
```bash
./generate_medium_tests.py
```

Run benchmarks:
```bash
./run_all_benchmarks.py ../../dataset/medium_tests -t 30
```

Results are saved in `../../dataset/results/results-YYYY-MM-DD-HH-MM-SS/`

See `BENCHMARK_RUNNER_GUIDE.md` for detailed usage instructions.
