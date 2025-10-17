# Benchmark Runner Guide

## Overview

`run_all_benchmarks.py` systematically runs all solvers on all SAT competition benchmarks with comprehensive result tracking and timeout handling.

## Features

- ✅ **Timeout per solver** - Configurable timeout (default 120s)
- ✅ **Continue on timeout** - If one solver times out, others still run
- ✅ **All solvers** - DPLL, CDCL, CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT
- ✅ **Configurable solver list** - Run only specific solvers
- ✅ **Progress tracking** - Shows current family/file/solver
- ✅ **Per-family results** - One result file per family
- ✅ **Master summary** - CSV with all results
- ✅ **Rankings** - 1st/2nd/3rd place counts per solver
- ✅ **Easy grepping** - Structured format for analysis

## Usage

### Run all benchmarks (default 120s timeout)
```bash
cd research/benchmarks
./run_all_benchmarks.py
```

### Run with 2-minute timeout
```bash
./run_all_benchmarks.py -t 120
```

### Run only specific solvers
```bash
./run_all_benchmarks.py -s DPLL CDCL
```

### Run with custom dataset path
```bash
./run_all_benchmarks.py /path/to/dataset -t 60
```

## Output Structure

Results are saved in `dataset/results/results-YYYY-MM-DD-HH-MM-SS/`:

```
dataset/
├── sat_competition2025/           # Benchmark files (not touched)
└── results/                       # All results go here
    └── results-2025-10-17-01-30-45/
        ├── scheduling.results.txt          # Per-family detailed results
        ├── scheduling.summary.txt          # Per-family rankings
        ├── argumentation.results.txt
        ├── argumentation.summary.txt
        ├── ...
        ├── master_summary.csv              # All results in CSV format
        └── overall_rankings.txt            # Overall solver rankings
```

## File Formats

### Family Results File (e.g., `scheduling.results.txt`)

```
Family: scheduling
Timeout: 120s
Files: 23
================================================================================

File: 081f111af59344b61346367a930e24f6.cnf
  Variables: 252
  Clauses: 1163
  Ratio: 4.62

  DPLL            SAT     0.0705s      RANK=1
  CDCL            SAT     0.7832s      RANK=5
  CoBD-SAT        SAT     0.0990s      RANK=2
  BB-CDCL         SAT     108.7843s    RANK=6
  LA-CDCL         SAT     0.3300s      RANK=4
  CGPM-SAT        SAT     0.1468s      RANK=3

File: 0e1d562093d5f4fc9013cf4a14a03f70.cnf
  ...
```

### Family Summary File (e.g., `scheduling.summary.txt`)

```
Summary: scheduling
================================================================================

Solver Rankings:
  DPLL           : 1st= 15  2nd=  5  3rd=  2  Timeout=  0  Error=  0
  CDCL           : 1st=  3  2nd=  8  3rd=  6  Timeout=  0  Error=  0
  CoBD-SAT       : 1st=  5  2nd= 10  3rd=  4  Timeout=  0  Error=  0
  BB-CDCL        : 1st=  0  2nd=  0  3rd=  0  Timeout= 12  Error=  0
  LA-CDCL        : 1st=  0  2nd=  0  3rd=  8  Timeout=  0  Error=  0
  CGPM-SAT       : 1st=  0  2nd=  0  3rd=  3  Timeout=  0  Error=  0
```

### Master Summary CSV (`master_summary.csv`)

```csv
family,file,variables,clauses,solver,result,time,rank
scheduling,081f111af59344b61346367a930e24f6.cnf,252,1163,DPLL,SAT,0.0705,1
scheduling,081f111af59344b61346367a930e24f6.cnf,252,1163,CDCL,SAT,0.7832,5
scheduling,081f111af59344b61346367a930e24f6.cnf,252,1163,CoBD-SAT,SAT,0.0990,2
...
```

### Overall Rankings (`overall_rankings.txt`)

```
Overall Solver Rankings
================================================================================

CDCL           : 1st=  45  2nd=  78  3rd=  89  Timeout=  12  Error=   0  Total= 399
CoBD-SAT       : 1st=  67  2nd=  56  3rd=  45  Timeout=   8  Error=   0  Total= 399
CGPM-SAT       : 1st=  23  2nd=  34  3rd=  56  Timeout=   0  Error=   0  Total= 399
DPLL           : 1st= 234  2nd=  89  3rd=  23  Timeout=   5  Error=   0  Total= 399
...
```

## Easy Grepping Examples

### Count 1st place finishes for DPLL
```bash
grep "DPLL" ../../dataset/results/results-*/overall_rankings.txt | grep -oP '1st=\s*\K\d+'
```

### Count timeouts for BB-CDCL
```bash
grep "BB-CDCL" ../../dataset/results/results-*/overall_rankings.txt | grep -oP 'Timeout=\s*\K\d+'
```

### Find all instances where DPLL won
```bash
grep "DPLL.*RANK=1" ../../dataset/results/results-*/scheduling.results.txt
```

### Get CSV of all DPLL results
```bash
grep "DPLL" ../../dataset/results/results-*/master_summary.csv
```

### Count families where DPLL won most instances
```bash
for f in ../../dataset/results/results-*/**.summary.txt; do
  echo "$f:"
  grep "DPLL" "$f" | awk '{print $3}'
done
```

## Progress Tracking

During execution, you'll see:
```
[1/99] Processing family: scheduling
================================================================================
  Found 23 CNF files

  [1/23] 081f111af59344b61346367a930e24f6.cnf
    Variables: 252, Clauses: 1163
    DPLL            ... SAT   in     0.0705s
    CDCL            ... SAT   in     0.7832s
    CoBD-SAT        ... SAT   in     0.0990s
    BB-CDCL         ... TIMEOUT (>120s)
    LA-CDCL         ... SAT   in     0.3300s
    CGPM-SAT        ... SAT   in     0.1468s

  [2/23] 0e1d562093d5f4fc9013cf4a14a03f70.cnf
    ...
```

## Tips

1. **Start small** - Test with `-s DPLL CDCL` and `-t 30` first
2. **Use screen/tmux** - For long runs (all 399 files)
3. **Monitor progress** - Check intermediate result files as they're created
4. **Analyze incrementally** - Each family summary is written immediately
5. **Compare families** - Use grep to compare solver performance across families

## Example Workflow

```bash
# Quick test on 2 solvers with 30s timeout
./run_all_benchmarks.py -s DPLL CDCL -t 30

# Check results
ls ../../dataset/results/results-*/

# Full run (will take hours!)
nohup ./run_all_benchmarks.py -t 120 > benchmark_run.log 2>&1 &

# Monitor progress
tail -f benchmark_run.log

# Quick analysis
grep "1st=" ../../dataset/results/results-*/overall_rankings.txt
```

## Performance Estimates

Assuming average solve time of 1s per solver:
- **One family (23 files):** ~2 minutes (with 6 solvers)
- **All families (399 files):** ~40 minutes (with 6 solvers, no timeouts)
- **With timeouts:** Can take much longer (hours)

Set timeout appropriately:
- `-t 30`: Quick survey
- `-t 120`: Standard testing
- `-t 600`: Thorough evaluation
