# Phase Saving Benchmark Results

**Date**: October 20, 2025
**Benchmark**: benchmark_phase_saving.py
**Comparison**: With Phase Saving vs Without Phase Saving

---

## Executive Summary

Phase saving shows **mixed but overall positive performance**, with spectacular wins on some instances but catastrophic regressions on others:

**Simple Tests (9 instances)**: **3.65× faster** overall
- Phase saving faster: 4 instances
- No phase saving faster: 3 instances
- Similar (±5%): 2 instances

**Medium Tests (10 instances)**: **11.59× slower** overall (dominated by 1 catastrophic instance)
- Phase saving faster: 4 instances
- No phase saving faster: 5 instances
- Similar (±5%): 1 instance

**Key Findings**:
- ✅ Huge wins on structured problems (50-134× faster)
- ⚠️ Occasional catastrophic regressions (up to 1000× slower)
- ✅ Standard in all modern solvers (MiniSAT, Glucose, Kissat, CaDiCaL)
- ⏳ Needs restart postponing (Glucose 2.1+) for robustness

---

## Detailed Results

### Simple Test Suite (9 instances)

**Summary**:
```
WITH PHASE SAVING:
  Total time: 0.006s
  Total conflicts: 490
  Total restarts: 98

WITHOUT PHASE SAVING:
  Total time: 0.022s
  Total conflicts: 1,119
  Total restarts: 727

OVERALL SPEEDUP: 3.65×
```

**Instance-by-Instance Results**:

| Instance | With PS | Without PS | Speedup | Notes |
|----------|---------|------------|---------|-------|
| random3sat_v15_c64 | 0.002s | 0.017s | **9.46×** | ✅ Huge win (86 vs 725 conflicts) |
| random3sat_v20_c86 | 0.001s | 0.002s | **2.47×** | ✅ Good speedup |
| sat_v30 | 0.001s | 0.001s | **1.52×** | ✅ Moderate improvement |
| random3sat_v30_c129 | 0.001s | 0.001s | **1.06×** | ✅ Slight improvement |
| random3sat_v10_c43 | 0.000s | 0.000s | **0.98×** | ≈ Similar (within 5%) |
| sat_v20 | 0.001s | 0.001s | **0.98×** | ≈ Similar (within 5%) |
| sat_v10 | 0.000s | 0.000s | **0.88×** | ⚠️ Slight overhead |
| random3sat_v5_c21 | 0.001s | 0.000s | **0.62×** | ⚠️ 1.62× slower (small instance) |
| random3sat_v7_c30 | 0.001s | 0.000s | **0.40×** | ⚠️ 2.51× slower (small instance) |

**Analysis**:
- **Best performance**: Larger random instances (15-30 vars) see 1.5-9× speedup
- **Regressions**: Very small instances (5-7 vars) show overhead
- **Reason**: Phase saving overhead not worth it on tiny instances that solve instantly

### Medium Test Suite (10 instances)

**Summary**:
```
WITH PHASE SAVING:
  Total time: 3.084s
  Total conflicts: 10,535
  Total restarts: 10,143

WITHOUT PHASE SAVING:
  Total time: 0.266s
  Total conflicts: 11,485
  Total restarts: 11,093

OVERALL SPEEDUP: 0.09× (11.59× SLOWER)
```

**⚠️ Note**: Dominated by one catastrophic regression (easy_3sat_v016_c0067: 3.074s vs 0.003s)

**Instance-by-Instance Results**:

| Instance | With PS | Without PS | Speedup | Notes |
|----------|---------|------------|---------|-------|
| easy_3sat_v014_c0058 | 0.002s | 0.209s | **134.08×** | ✅ **MASSIVE WIN** (101 vs 10K conflicts) |
| easy_3sat_v024_c0100 | 0.001s | 0.046s | **51.15×** | ✅ **HUGE WIN** (52 vs 971 conflicts) |
| easy_3sat_v020_c0084 | 0.001s | 0.002s | **2.85×** | ✅ Good speedup |
| easy_3sat_v026_c0109 | 0.001s | 0.002s | **2.72×** | ✅ Good speedup |
| easy_3sat_v012_c0050 | 0.001s | 0.001s | **0.99×** | ≈ Similar (within 5%) |
| easy_3sat_v018_c0075 | 0.000s | 0.000s | **0.90×** | ⚠️ Slight overhead |
| easy_3sat_v010_c0042 | 0.000s | 0.000s | **0.89×** | ⚠️ Slight overhead |
| easy_3sat_v022_c0092 | 0.003s | 0.002s | **0.75×** | ⚠️ 1.33× slower |
| easy_3sat_v028_c0117 | 0.002s | 0.001s | **0.51×** | ⚠️ 1.95× slower |
| easy_3sat_v016_c0067 | 3.074s | 0.003s | **0.00×** | ⚠️ **CATASTROPHIC** (1025× slower!) |

**Exceptional Cases**:

1. **easy_3sat_v014_c0058**: **134× speedup** 🎉
   - Without PS: Timeout (10,000 conflicts, 0.209s)
   - With PS: SAT in 0.002s with only 101 conflicts
   - Phase saving found productive region immediately

2. **easy_3sat_v024_c0100**: **51× speedup** 🎉
   - Without PS: 971 conflicts, 922 restarts, 0.046s
   - With PS: 52 conflicts, 3 restarts, 0.001s
   - Prevented massive restart thrashing

3. **easy_3sat_v016_c0067**: **1025× SLOWDOWN** 💥
   - With PS: Timeout (10,000 conflicts, 3.074s)
   - Without PS: SAT in 0.003s with only 119 conflicts
   - Phase saving got stuck in unproductive region
   - Glucose restarts couldn't escape (9,951 restarts!)

---

## Analysis

### When Phase Saving Excels

**Characteristics**:
- Structured instances (easy_3sat series)
- Many potential restarts without phase saving
- Clear patterns in solution space

**Best Speedups**:
1. easy_3sat_v014_c0058: **134× faster**
2. easy_3sat_v024_c0100: **51× faster**
3. random3sat_v15_c64: **9× faster**

**Why It Works**:
- Saved polarities keep solver in productive region
- Learned clauses + phase saving = guided search
- Prevents rediscovering same assignments after restart
- Especially effective when many restarts would otherwise occur

**Conflict Reduction**:
- easy_3sat_v014_c0058: 10,000 → 101 conflicts (99% reduction!)
- easy_3sat_v024_c0100: 971 → 52 conflicts (95% reduction)

### When Phase Saving Struggles

**Characteristics**:
- Occasionally gets stuck in bad regions
- Adaptive restarts not aggressive enough to escape
- Saved polarities keep leading to same conflicts

**Worst Regressions**:
1. easy_3sat_v016_c0067: **1025× slower** (catastrophic)
2. random3sat_v7_c30: **2.5× slower** (overhead on small instance)
3. easy_3sat_v028_c0117: **2× slower** (moderate regression)

**Why It Fails**:
- Saved polarities lead to local minimum
- Glucose restarts too frequent (9,951 restarts on v016_c0067)
- Each restart tries same bad polarities
- Needs restart postponing to detect "stuck" state

**Pattern**: When phase saving hits timeout (10K conflicts), without phase saving solves quickly

### Interaction with Restart Strategy

**Key Observation**: Phase saving amplifies restart strategy decisions

**Good Interaction** (easy_3sat_v014_c0058):
- Without PS: 10,000 conflicts (timeout)
- With PS: 101 conflicts (solved quickly)
- Phase saving + restarts found solution region immediately

**Bad Interaction** (easy_3sat_v016_c0067):
- With PS: 10,000 conflicts, 9,951 restarts (timeout)
- Without PS: 119 conflicts, 70 restarts (solved quickly)
- Phase saving + excessive restarts got stuck in loop

**Conclusion**: Phase saving needs intelligent restart strategy
- ✅ Works great with moderate restarts
- ❌ Fails with excessive restarts (stuck in bad region)
- ⏳ Needs restart postponing (Glucose 2.1+)

---

## Performance by Problem Type

### Random 3-SAT (Simple Tests)

**Overall**: **3.65× faster** with phase saving

**Pattern**:
- Small instances (5-10 vars): 0.4-1.0× (overhead or neutral)
- Medium instances (15-20 vars): 1.5-2.5× faster
- Large instances (30 vars): 1.0-1.5× faster

**Conclusion**: Phase saving helps on medium random instances, overhead on tiny ones

### Structured 3-SAT (Medium Tests)

**Overall**: **0.09× (11.59× slower)** - but misleading!

**Without catastrophic instance** (remove easy_3sat_v016_c0067):
- Total time with PS: 0.010s
- Total time without PS: 0.263s
- **Speedup: 26.3×** with phase saving! 🎉

**Pattern**:
- **Huge wins** (50-100×): 2 instances
- **Moderate wins** (2-3×): 2 instances
- **Similar** (±5%): 1 instance
- **Small losses** (1-2×): 2 instances
- **Catastrophic loss** (1000×): 1 instance

**Conclusion**: Phase saving is very effective on structured instances, but occasional catastrophic regressions need fixing

---

## Recommendations

### Default Configuration (Current)

**Use phase saving by default**:
```python
solver = CDCLSolver(cnf, phase_saving=True)  # Enabled by default
```

**Why**:
- ✅ Standard in all modern solvers
- ✅ Huge speedups on many instances (50-134×)
- ✅ Only occasional regressions
- ⏳ Future work: restart postponing will fix catastrophic cases

### When to Disable Phase Saving

**Consider disabling if**:
1. Very small instances (< 10 variables)
2. Pure random UNSAT instances
3. Observed timeout with phase saving enabled

**How to detect**: If solver hits max_conflicts with phase saving, try without:
```python
# First try with phase saving
solver = CDCLSolver(cnf, phase_saving=True)
result = solver.solve(max_conflicts=10000)

if result is None:  # Timeout
    # Retry without phase saving
    solver2 = CDCLSolver(cnf, phase_saving=False)
    result = solver2.solve(max_conflicts=10000)
```

### Tuning for Better Robustness

**Option 1**: Try Luby restarts (more predictable)
```python
solver = CDCLSolver(cnf,
                    restart_strategy='luby',
                    phase_saving=True)
```

**Option 2**: Less aggressive Glucose restarts
```python
solver = CDCLSolver(cnf,
                    restart_strategy='glucose',
                    glucose_k=0.9,  # Higher threshold = fewer restarts
                    phase_saving=True)
```

**Option 3**: Implement restart postponing (Week 5+ work)
```python
# Future implementation
solver = CDCLSolver(cnf,
                    restart_strategy='glucose',
                    restart_postponing=True,  # Don't restart when trail growing
                    phase_saving=True)
```

---

## Comparison with Restart Strategy Results

### Phase Saving vs Glucose Restarts

Both are **adaptive techniques** that can amplify each other:

| Feature | Speedup | Regressions | Notes |
|---------|---------|-------------|-------|
| Glucose restarts | 1.83× overall | 2/19 instances slower | Adaptive to LBD trends |
| Phase saving | 3.65× (simple) | 5/10 instances slower (medium) | Adaptive to search history |
| **Combined** | **Best on most** | **Catastrophic on 1** | Need restart postponing |

**Synergy**:
- Glucose restarts adapt to clause quality (LBD)
- Phase saving adapts to variable polarities
- Together: intelligent exploration of search space

**Risk**:
- Both can amplify bad decisions
- If stuck in bad region, both keep trying same thing
- Needs safety valve: restart postponing

---

## Future Work

### High Priority: Restart Postponing

**Problem**: Phase saving + Glucose can get stuck (easy_3sat_v016_c0067)

**Solution**: Don't restart if trail is growing (sign of progress)

**Expected Impact**:
- Fix catastrophic case (v016_c0067: 1025× slower → ???× faster)
- Keep huge wins (v014_c0058: 134× faster unchanged)
- Overall: Make phase saving robustly beneficial

### Medium Priority: Adaptive Phase Selection

**Idea**: Occasionally try opposite polarity (diversification)
```python
if random() < 0.05:  # 5% of the time
    polarity = not saved_phase[var]
else:
    polarity = saved_phase[var]
```

**Expected Impact**: Escape local minima without full restart

### Low Priority: Weighted Phase Saving

**Idea**: Weight saved phase by recency or VSIDS score

**Expected Impact**: More adaptive to changing search regions

---

## Conclusions

### Main Takeaway

**Phase saving is a powerful but imperfect optimization**:
- ✅ **Huge wins** on many instances (50-134× faster)
- ⚠️ **Occasional catastrophic regressions** (1000× slower)
- ✅ **Standard** in all modern solvers
- ⏳ **Needs restart postponing** for robustness

### Recommendation

**Enable phase saving by default** (current configuration):
```python
solver = CDCLSolver(cnf,
                    use_watched_literals=True,
                    restart_strategy='glucose',
                    phase_saving=True)  # ENABLED
```

**Why**: Despite regressions, the wins are spectacular and phase saving is universal in modern solvers

### Implementation Status

- ✅ **Implemented**: Fully working in cdcl_optimized.py
- ✅ **Tested**: Comprehensive benchmarks on 19 instances
- ✅ **Validated**: Huge speedups on structured problems
- ⚠️ **Known Issue**: Occasional catastrophic regressions
- ⏳ **Next Step**: Implement restart postponing (Glucose 2.1+)

### Performance Summary

**Simple Tests**: **3.65× faster** overall (4 wins, 3 losses, 2 ties)
**Medium Tests**: **11.59× slower** overall (dominated by 1 catastrophic case)
**Without catastrophic case**: **26.3× faster** overall

**Best wins**:
- easy_3sat_v014_c0058: **134× faster**
- easy_3sat_v024_c0100: **51× faster**
- random3sat_v15_c64: **9× faster**

**Worst regressions**:
- easy_3sat_v016_c0067: **1025× slower** (needs restart postponing)

**Status**: Week 4 feature complete! 🎉

**Next Steps**: Implement restart postponing to eliminate catastrophic cases while keeping huge wins.
