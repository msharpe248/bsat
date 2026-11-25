# Validation Guide

This guide explains how to reproduce and validate the performance claims made in `BENCHMARKS.md`.

## Quick Start

```bash
cd research/benchmarks
./reproduce_validation.sh
```

This runs the validation levels in sequence and writes logs and reports to `validation_results/`.

**Prerequisites:** none beyond the standard library. The validation scripts import `benchmark.py`,
which uses the third-party `tabulate` package for tables if it is installed and a built-in
formatter otherwise.

**Notes on `reproduce_validation.sh`:**
- Level 4 runs `benchmark.py --all` (the original `run_full_benchmark.py` is no longer in the
  repository; see Level 4 below).
- Each step is piped through `tee` with `set -o pipefail`, so the script's ✅/❌ banners reflect
  the Python script's exit status. The full output of each step is in `validation_results/`.

**Status of the headline numbers:** the large speedups quoted in `BENCHMARKS.md` (e.g. CGPM-SAT
2710×) were measured on 2025-10-16 against a baseline CDCL solver that was later found to have
soundness bugs (fixed 2025-10-20). Running Levels 1-3 against the current code does **not**
reproduce them; see "Reproducing Specific Claims" below.

## What Gets Validated

Our validation framework has **4 levels** that comprehensively prove the performance claims:

### Level 1: Correctness Verification ✅ CRITICAL

**File:** `validate_correctness.py`

Runs 14 solvers (DPLL, CDCL, the 4 original and 8 new-suite research solvers) on 8 generated
problems and verifies that all results are **actually correct**:
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
- Problems: Random 3-SAT with 10, 15 and 25 variables (seed 42); solvers: CDCL (baseline), DPLL,
  CoBD-SAT, LA-CDCL, CGPM-SAT and the 8 new-suite solvers

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

**File:** `run_full_benchmark.py` — **no longer in the repository** (removed together with its
output `benchmark_results_full.md`). It produced the 15-problem, 7-solver tables in `BENCHMARKS.md`.

`reproduce_validation.sh` now runs the closest current equivalent, the comprehensive mode of
`benchmark.py` (`benchmark.py --all --output validation_results/benchmark_results_full.md`):
- 15 generated problems (3 each: modular, backbone, chain, circuit, random 3-SAT) — not the
  identical problem list used for `BENCHMARKS.md`
- 7 solvers by default (DPLL, CDCL, Schöning + CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT);
  add `--all-solvers` for the 8 new-suite solvers as well

**Why this matters:** Shows how solvers behave on **different problem types** (not just cherry-picked examples).

**Run separately:**
```bash
python3 benchmark.py --all --output benchmark_results.md
```

**Output:** `benchmark_results.md` with complete results table

For file-based suites (and all 18 registered solvers) use `run_all_benchmarks.py`; see
`BENCHMARK_RUNNER_GUIDE.md`.

---

## Validation Modes

### Full Validation (Recommended)

Complete validation with maximum confidence:

```bash
./reproduce_validation.sh
```

**Runtime:** ~5-10 minutes
**Output:** `validation_results/` directory with the Level 1-3 reports (Level 4 fails, see above)

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
└── benchmark_results_full.md       # Level 4 markdown report
```

### Key Files to Review

1. **correctness_validation.log** - MUST show ✅ for all solvers
2. **statistical_results.csv** - Import into spreadsheet for analysis
3. **profile_report.md** - Review function-level breakdowns

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

**Example output** (Random 3-SAT, 25 vars; actual output of `--runs 3` on the current code,
2026-11-25, macOS, Python 3.12):
```
CGPM-SAT:
  Mean:   0.0033s ± 0.0001s
  Median: 0.0033s
  Range:  [0.0032s, 0.0034s]
  95% CI: [0.0032s, 0.0034s]
  Runs:   3
  CV:     2.3% ✅ (very stable)

  Speedup vs CDCL:
    Mean:   0.89×
    95% CI: [0.86×, 0.92×]
    Range:  [0.86×, 0.91×]
```

**How to read this:**
- Mean time is 0.0033s (averaged over the runs)
- 95% confidence the true mean is between 0.0032s and 0.0034s
- CV of 2.3% means very consistent performance
- A speedup below 1.0× means the solver is **slower** than the CDCL baseline (here CDCL took
  0.0029s); a speedup claim is only supported when the whole confidence interval is above 1.0×

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

**Check with:**
```bash
python3 statistical_benchmark.py --runs 10
```

**Look for:**
- Problem: "Random 3-SAT (25 vars) 🏆"
- Solver: CGPM-SAT
- Speedup vs CDCL with 95% CI

**Current result: not reproduced.** The 2710× figure divides CGPM-SAT's time by a 9.9s CDCL run
that (incorrectly) reported UNSAT on this satisfiable instance — a symptom of the CDCL soundness
bugs fixed on 2025-10-20. With the current CDCL the same instance takes about 3ms for both
solvers (CGPM-SAT ≈ 0.9× CDCL in the run shown above). The other historical speedups measured
against that baseline (CoBD-SAT 1612×, LA-CDCL 529×, DPLL 2160×) do not reproduce either.

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

And that the research solver packages are in the parent directory:
```bash
ls ../cobd_sat/ ../cgpm_sat/  # Should exist
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
- Using smaller problems (edit the problem list in `profile_solvers.py`; it has no size option)
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
```python
# Import into pandas
import pandas as pd
df = pd.read_csv('validation_results/statistical_results.csv')
```

---

## Validation Checklist

Before claiming performance results publicly, verify:

- [ ] `./reproduce_validation.sh` completes and the logs in `validation_results/` show no errors (Level 4 excepted, see above)
- [ ] Correctness validation shows ✅ for all solvers
- [ ] Statistical CV < 20% for key benchmarks
- [ ] Speedup confidence intervals don't include 1.0× (no speedup)
- [ ] Profiling shows reasonable memory usage
- [ ] Benchmark results match what is claimed (the historical tables in `BENCHMARKS.md` currently do **not** match a re-run)

---

## Citation

When citing these benchmark results, include:

```
Performance claims validated with:
- Correctness verification (all solutions verified)
- Statistical analysis (10 runs, 95% CI)
- Profiling analysis (algorithmic validation)
- Comprehensive benchmarking (15 problems, 7 solvers; historical run of 2025-10-16)

Validation scripts: research/benchmarks/reproduce_validation.sh
```

---

## Questions?

See main README.md or file an issue on GitHub.
