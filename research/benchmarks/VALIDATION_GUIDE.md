# Validation Guide

This guide explains how to reproduce and validate the performance claims made in `BENCHMARKS.md`.

## Quick Start

```bash
cd research/benchmarks
./reproduce_validation.sh
```

This runs all validation levels and generates a complete validation report.

## What Gets Validated

Our validation framework has **4 levels** that comprehensively prove the performance claims:

### Level 1: Correctness Verification ✅ CRITICAL

**File:** `validate_correctness.py`

Verifies that all benchmark results are **actually correct**:
- **SAT results**: Checks that every solution truly satisfies all clauses
- **UNSAT results**: Confirms with independent solvers (DPLL, CDCL)
- **Inconsistencies**: Documents when different solvers return different results

**Why this matters:** A solver that returns wrong answers quickly is useless. This proves our solvers are **correct first, fast second**.

**Run separately:**
```bash
python3 validate_correctness.py
```

**Exit code 0** = All results verified correct
**Exit code 1** = Errors detected (fix before proceeding!)

---

### Level 2: Statistical Validation ✅ REPRODUCIBILITY

**File:** `statistical_benchmark.py`

Proves speedups are **reproducible and statistically significant**:
- Runs each solver **10 times** on each problem (configurable)
- Computes **mean, median, standard deviation**
- Calculates **95% confidence intervals**
- Measures **coefficient of variation** (stability metric)
- Reports **speedup with confidence bounds**

**Why this matters:** A single lucky run proves nothing. This shows speedups are **stable and reproducible**.

**Run separately:**
```bash
python3 statistical_benchmark.py --runs 10 --output statistical_results.csv
```

**Key metrics:**
- **CV < 10%** = Very stable ✅
- **CV < 20%** = Stable ✓
- **CV > 30%** = High variance ⚠️

---

### Level 3: Profiling Analysis ✅ ALGORITHMIC

**File:** `profile_solvers.py`

Profiles solvers to understand **where time is spent**:
- Function-level time breakdown (cProfile)
- Memory usage tracking (tracemalloc)
- Hotspot identification
- Call count analysis

**Why this matters:** This proves speedups come from **genuine algorithmic improvements**, not measurement artifacts or bugs.

**Run separately:**
```bash
python3 profile_solvers.py --output profile_report.md
```

**What to look for:**
- Different solvers should show **different function profiles**
- Research solvers should spend time in their **specialized algorithms**
- No excessive memory usage (would indicate bugs)

---

### Level 4: Full Benchmark Suite ✅ COMPREHENSIVE

**File:** `run_full_benchmark.py`

Runs the complete benchmark across **all problem types**:
- 15 different problems
- 7 different solvers
- Multiple problem generators (modular, backbone, chain, circuit, random)

**Why this matters:** Shows solvers excel on **different problem types** (not just cherry-picked examples).

**Run separately:**
```bash
python3 run_full_benchmark.py
```

**Output:** `benchmark_results_full.md` with complete results table

---

## Validation Modes

### Full Validation (Recommended)

Complete validation with maximum confidence:

```bash
./reproduce_validation.sh
```

**Runtime:** ~5-10 minutes
**Output:** `validation_results/` directory with all reports

### Quick Validation

Faster validation with reduced iterations:

```bash
./reproduce_validation.sh --quick
```

**Runtime:** ~2-3 minutes
**Statistical runs:** 5 instead of 10

---

## Understanding the Output

### Directory Structure

After running `./reproduce_validation.sh`, you'll have:

```
validation_results/
├── correctness_validation.log      # Level 1 results
├── statistical_validation.log      # Level 2 console output
├── statistical_results.csv         # Level 2 CSV data
├── profiling.log                   # Level 3 console output
├── profile_report.md               # Level 3 markdown report
├── full_benchmark.log              # Level 4 console output
└── benchmark_results_full.md       # Level 4 results table
```

### Key Files to Review

1. **correctness_validation.log** - MUST show ✅ for all solvers
2. **statistical_results.csv** - Import into spreadsheet for analysis
3. **profile_report.md** - Review function-level breakdowns
4. **benchmark_results_full.md** - Complete benchmark results

---

## Interpreting Results

### Correctness Verification

**Good:**
```
✅ SAT (verified)
✅ UNSAT (confirmed)
```

**Bad:**
```
❌ SAT (INVALID SOLUTION!)
⚠️  UNSAT (not confirmed)
```

### Statistical Validation

**Example output:**
```
CGPM-SAT:
  Mean:   0.0023s ± 0.0002s
  Median: 0.0022s
  95% CI: [0.0021s, 0.0025s]
  CV:     8.7% ✅ (very stable)

  Speedup vs CDCL:
    Mean:   2710.34×
    95% CI: [2450.21×, 3012.67×]
```

**What this proves:**
- Mean time is 0.0023s (averaged over 10 runs)
- 95% confidence the true mean is between 0.0021s and 0.0025s
- CV of 8.7% means very consistent performance
- **Speedup is real and reproducible with high confidence**

### Profiling Analysis

**Look for:**
- Research solvers spending time in their specialized algorithms
- Different function profiles between solvers (proves different approaches)
- Reasonable memory usage (< 100 MB for small problems)

**Red flags:**
- Excessive memory usage (GB range for small problems)
- All time in one function (might indicate infinite loop)
- Identical profiles (solvers might not be using their algorithms)

---

## Reproducing Specific Claims

### Claim: "CGPM-SAT achieves 2710× speedup on Random 3-SAT (25 vars)"

**Validate with:**
```bash
python3 statistical_benchmark.py --runs 10
```

**Look for:**
- Problem: "Random 3-SAT (25 vars) 🏆"
- Solver: CGPM-SAT
- Speedup vs CDCL with 95% CI

### Claim: "All SAT solutions are correct"

**Validate with:**
```bash
python3 validate_correctness.py
```

**Must show:**
- ✅ SAT (verified) for all SAT results
- No ❌ INVALID SOLUTION errors

---

## Troubleshooting

### "Module not found" errors

Make sure you're in the right directory:
```bash
cd research/benchmarks
```

And that research solvers are in the parent directory:
```bash
ls ../cobd_sat.py ../cgpm_sat.py  # Should exist
```

### Correctness validation fails

**This is critical!** Do NOT proceed with other validations.

1. Check which solver failed
2. Review the error message
3. Fix the solver implementation
4. Re-run validation

### Statistical results show high variance (CV > 30%)

**Possible causes:**
- Background processes consuming CPU
- Thermal throttling
- Very small time values (measurement noise)

**Solutions:**
- Close other applications
- Run on a dedicated machine
- Increase problem size for more stable measurements

### Profiling crashes with memory errors

Profiling adds overhead. Try:
- Using smaller problems (`--quick` mode)
- Increasing available memory
- Profiling one solver at a time

---

## Advanced Usage

### Custom Statistical Runs

```bash
python3 statistical_benchmark.py --runs 20 --output my_results.csv
```

### Profile Single Problem

Edit `profile_solvers.py` to profile specific problems.

### Export Data for Analysis

Statistical results are in CSV format for easy import:
```bash
# Import into pandas
import pandas as pd
df = pd.read_csv('validation_results/statistical_results.csv')
```

---

## Validation Checklist

Before claiming performance results publicly, verify:

- [ ] `./reproduce_validation.sh` completes successfully
- [ ] Correctness validation shows ✅ for all solvers
- [ ] Statistical CV < 20% for key benchmarks
- [ ] Speedup confidence intervals don't include 1.0× (no speedup)
- [ ] Profiling shows reasonable memory usage
- [ ] Full benchmark matches results in `BENCHMARKS.md`

---

## Citation

When citing these benchmark results, include:

```
Performance claims validated with:
- Correctness verification (all solutions verified)
- Statistical analysis (10 runs, 95% CI)
- Profiling analysis (algorithmic validation)
- Comprehensive benchmarking (15 problems, 7 solvers)

Validation scripts: research/benchmarks/reproduce_validation.sh
```

---

## Questions?

See main README.md or file an issue on GitHub.
