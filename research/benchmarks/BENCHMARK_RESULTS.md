# Benchmark Results

Comprehensive benchmarking results for all SAT solvers on medium-difficulty test instances.

## Test Suite: Medium Difficulty Instances

**Dataset**: `medium_tests` - 53 generated instances
**Date**: October 17, 2025
**Timeout**: 30 seconds per solver
**Solvers**: the six solvers that existed at the time (DPLL, CDCL, CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT)
**Raw data**: `../../dataset/results/medium_suite_results_oct_2025/`

> **Historical results - read before quoting.** This run predates the soundness fixes made to the
> baseline `bsat` CDCL solver on October 20, 2025 (false-UNSAT answers and learned clauses that were
> never used). In this data set CDCL returns UNSAT on the satisfiable `easy_3sat_v020_c0084.cnf`
> and times out on 27/53 instances; neither reflects the current CDCL solver. The CDCL row, and
> every conclusion below about research solvers beating "traditional CDCL", therefore describe the
> October 17, 2025 code only. The suite has not been re-run with all current solvers; use the
> commands at the end of this file to regenerate current numbers.
>
> For orientation only, a partial re-run on the current code (2026-11-25; DPLL, CDCL, LA-CDCL and
> CGPM-SAT only, 10 s timeout instead of 30 s) gave: CDCL 50 first places and 0 timeouts; CGPM-SAT 2
> first places and 2 timeouts; LA-CDCL 1 first place and 8 timeouts; DPLL 0 first places and 9
> timeouts, with no SAT/UNSAT disagreements. The ranking below is therefore reversed for CDCL.
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
- **Consistent speed**: Solve times ranged from 0.0004s to 11.4s
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

**CoBD-SAT** (Community-Based Decomposition):
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
easy_3sat_v018_c0075.cnf: 0.0012s (RANK=1)
easy_3sat_v020_c0084.cnf: 0.0016s (RANK=1)
```

Challenging instances:
```
hard_3sat_v090_c0384.cnf: 0.8225s (RANK=1, SAT)
hard_3sat_v099_c0422.cnf: 1.4183s (RANK=1, SAT)
hard_3sat_v096_c0409.cnf: 3.9783s (RANK=1, UNSAT)
hard_3sat_v102_c0435.cnf: 4.3292s (RANK=1, UNSAT)
```

### DPLL Wins

DPLL excelled on easier instances with simple structure:
```
easy_3sat_v012_c0050.cnf: 0.0003s (UNSAT)
easy_3sat_v024_c0100.cnf: 0.0007s (SAT)
sat_known_v040.cnf: 0.0008s (SAT)
```

## Conclusions

### Production Recommendations

1. **For general use**: **CGPM-SAT** - best overall performance, no timeouts
2. **For simple problems (< 30 vars)**: **DPLL** - extremely fast, minimal overhead
3. **For structured problems**: **LA-CDCL** - lookahead helps on specific instance types

### Research Insights

- **Graph-based heuristics** (CGPM) show significant advantage over traditional approaches
- **Backbone analysis** (BB-CDCL) adds overhead that doesn't pay off on these instances
- **Pure CDCL** performed poorly in this run; the solver already had VSIDS and restarts, and its poor showing was later traced to the soundness bugs noted at the top of this file

### Future Work

- Test on larger instances (200+ variables)
- Compare with industrial solvers (MiniSat, Glucose, CryptoMiniSat)
- Analyze why CGPM-SAT excels: graph structure correlation?
- Optimize CDCL implementation with industrial techniques

## Running the Benchmarks

The test suite is checked in under `../../dataset/medium_tests/medium_suite/`. To regenerate it
(optional; deletes and recreates `../../dataset/medium_tests`), from `research/benchmarks`:
```bash
./generate_medium_tests.py
```

Run benchmarks:
```bash
./run_all_benchmarks.py ../../dataset/medium_tests -t 30
```

Results are saved in `../../dataset/results/results-YYYY-MM-DD-HH-MM-SS/`

See `BENCHMARK_RUNNER_GUIDE.md` for detailed usage instructions.
