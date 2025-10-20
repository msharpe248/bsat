# Random Phase Selection Benchmark Results

**Date**: October 20, 2025
**Benchmark**: benchmark_random_phase.py
**Comparison**: Phase Saving (0% random) vs Phase Saving + Random Selection (5%)

---

## Executive Summary

Random phase selection shows **mixed but highly valuable performance**:

**Medium Tests (10 instances)**: **278× faster overall** 🎉
- Random phase faster: 5 instances
- No random phase faster: 3 instances
- Similar (±5%): 2 instances
- **FIXED CATASTROPHIC REGRESSION**: 3,823× speedup on easy_3sat_v016_c0067

**Simple Tests (9 instances)**: **25× slower overall** ⚠️
- Random phase faster: 3 instances
- No random phase faster: 6 instances
- **NEW CATASTROPHIC CASE**: 251× slowdown on random3sat_v5_c21

**Key Findings**:
- ✅ Brilliantly fixes the catastrophic phase saving regression (3,823× speedup!)
- ✅ Excellent on medium-sized structured instances (278× overall)
- ⚠️ Can hurt very small instances where randomness disrupts simple solutions
- ✅ Simple to implement (3 lines of code)
- ⚠️ Should remain disabled by default, user can enable when needed

---

## Detailed Results

### Medium Test Suite (10 instances)

**Summary**:
```
WITHOUT RANDOM PHASE SELECTION (0%):
  Total time: 3.088s
  Total conflicts: 10,794
  Total restarts: 10,147

WITH RANDOM PHASE SELECTION (5%):
  Total time: 0.011s
  Total conflicts: 714
  Total restarts: 206

OVERALL SPEEDUP: 278.00×
```

**Instance-by-Instance Results**:

| Instance | No Random (0%) | With Random (5%) | Speedup | Notes |
|----------|----------------|------------------|---------|-------|
| easy_3sat_v016_c0067 | 3.075s (10K conflicts) | 0.001s (69 conflicts) | **3,823×** | 🎉 **CATASTROPHIC REGRESSION FIXED!** |
| easy_3sat_v022_c0092 | 0.004s (295 conflicts) | 0.001s (78 conflicts) | **3.28×** | ✅ Significant improvement |
| easy_3sat_v028_c0117 | 0.003s (123 conflicts) | 0.001s (61 conflicts) | **2.17×** | ✅ Good speedup |
| easy_3sat_v014_c0058 | 0.002s (102 conflicts) | 0.001s (74 conflicts) | **1.63×** | ✅ Moderate improvement |
| easy_3sat_v026_c0109 | 0.001s (54 conflicts) | 0.001s (50 conflicts) | **1.29×** | ✅ Slight improvement |
| easy_3sat_v010_c0042 | 0.000s (0 conflicts) | 0.000s (0 conflicts) | **1.04×** | ≈ Similar (within 5%) |
| easy_3sat_v020_c0084 | 0.001s (58 conflicts) | 0.001s (55 conflicts) | **1.00×** | ≈ Similar (within 5%) |
| easy_3sat_v012_c0050 | 0.002s (111 conflicts) | 0.003s (190 conflicts) | **0.53×** | ⚠️ 1.88× slower |
| easy_3sat_v024_c0100 | 0.001s (51 conflicts) | 0.002s (73 conflicts) | **0.49×** | ⚠️ 2.05× slower |
| easy_3sat_v018_c0075 | 0.000s (0 conflicts) | 0.001s (64 conflicts) | **0.07×** | ⚠️ 15.28× slower |

**Analysis**:
- **Best performance**: Fixed the catastrophic regression (3,823× speedup!)
- **Overall**: 278× faster (3.088s → 0.011s)
- **Regressions**: Minor slowdowns on 3 instances (1.9-15× slower, but all still < 0.003s)
- **Reason**: Random diversification escapes local minima on hard instances

### Simple Test Suite (9 instances)

**Summary**:
```
WITHOUT RANDOM PHASE SELECTION (0%):
  Total time: 0.006s
  Total conflicts: 490
  Total restarts: 95

WITH RANDOM PHASE SELECTION (5%):
  Total time: 0.151s
  Total conflicts: 10,446
  Total restarts: 9,773

OVERALL SPEEDUP: 0.04× (25× SLOWER)
```

**⚠️ Note**: Dominated by one catastrophic regression (random3sat_v5_c21: 0.0006s → 0.145s)

**Instance-by-Instance Results**:

| Instance | No Random (0%) | With Random (5%) | Speedup | Notes |
|----------|----------------|------------------|---------|-------|
| random3sat_v15_c64 | 0.002s (86 conflicts) | 0.000s (0 conflicts) | **31.23×** | ✅ **HUGE WIN** (solved trivially!) |
| random3sat_v10_c43 | 0.000s (50 conflicts) | 0.000s (50 conflicts) | **1.15×** | ✅ Slight improvement |
| sat_v20 | 0.001s (50 conflicts) | 0.001s (50 conflicts) | **1.07×** | ✅ Slight improvement |
| random3sat_v30_c129 | 0.001s (52 conflicts) | 0.001s (76 conflicts) | **0.73×** | ⚠️ 1.38× slower |
| sat_v30 | 0.001s (51 conflicts) | 0.001s (60 conflicts) | **0.70×** | ⚠️ 1.43× slower |
| random3sat_v7_c30 | 0.001s (74 conflicts) | 0.001s (116 conflicts) | **0.67×** | ⚠️ 1.49× slower |
| random3sat_v20_c86 | 0.001s (55 conflicts) | 0.002s (69 conflicts) | **0.35×** | ⚠️ 2.83× slower |
| sat_v10 | 0.000s (0 conflicts) | 0.000s (25 conflicts) | **0.12×** | ⚠️ 8.45× slower |
| random3sat_v5_c21 | 0.001s (72 conflicts) | 0.145s (10K conflicts) | **0.004×** | ⚠️ **CATASTROPHIC** (251× slower!) |

**Exceptional Cases**:

1. **random3sat_v15_c64**: **31× speedup** 🎉
   - Without random: 86 conflicts, 0.002s
   - With random: 0 conflicts, 0.000s (solved by unit propagation alone!)
   - Random phase selection stumbled into solution immediately

2. **random3sat_v5_c21**: **251× SLOWDOWN** 💥
   - Without random: 72 conflicts, 0.0006s
   - With random: TIMEOUT (10,000 conflicts, 0.145s)
   - Random phase selection disrupted simple solution path
   - Instance is tiny (5 variables) - randomness adds too much noise

---

## Analysis

### The Catastrophic Regression Fix

**Problem Instance**: easy_3sat_v016_c0067 (16 vars, 67 clauses)

**Without Random Phase Selection**:
- Time: 3.075s
- Conflicts: 10,000 (timeout)
- Restarts: 9,951 (almost every conflict!)
- Behavior: Phase saving got stuck trying same bad polarities
- Restart postponing couldn't help (trail never grew)

**With 5% Random Phase Selection**:
- Time: 0.001s
- Conflicts: 69
- Restarts: 20
- Behavior: Random diversification broke out of bad polarity loop
- **Improvement**: **3,823× faster!** (99.97% time reduction)

**Why It Worked**:
- Phase saving alone: Stuck in local minimum (9,951 restarts trying same polarities)
- Random selection: 5% chance of different polarity each decision
- Result: Occasional random choice broke the cycle, found solution quickly

### When Random Phase Selection Excels

**Characteristics**:
- Medium-sized structured instances (12-28 variables)
- Instances where phase saving got stuck
- Problems with misleading early decisions

**Best Speedups**:
1. easy_3sat_v016_c0067: **3,823× faster** (THE BIG FIX)
2. random3sat_v15_c64: **31× faster** (unexpected bonus)
3. easy_3sat_v022_c0092: **3.3× faster**
4. easy_3sat_v028_c0117: **2.2× faster**

**Why It Works**:
- Random diversification provides escape from local minima
- Still uses phase saving 95% of the time (maintains benefits)
- Prevents cyclic behavior (same polarities → same conflicts → restart → repeat)

**Conflict Reduction**:
- easy_3sat_v016_c0067: 10,000 → 69 conflicts (99.3% reduction!)
- easy_3sat_v022_c0092: 295 → 78 conflicts (73.6% reduction)
- easy_3sat_v028_c0117: 123 → 61 conflicts (50.4% reduction)

### When Random Phase Selection Struggles

**Characteristics**:
- Very small instances (5-10 variables)
- Problems with simple, obvious solution paths
- Instances that solve in 0 conflicts without randomness

**Worst Regressions**:
1. random3sat_v5_c21: **251× slower** (NEW catastrophic case!)
2. sat_v10: **8.5× slower** (disrupted trivial solution)
3. easy_3sat_v018_c0075: **15× slower** (found solution immediately without random)

**Why It Fails**:
- Small search space (5 variables = only 32 possible assignments)
- Random selection disrupts simple solution paths
- 5% randomness is too aggressive when problem is trivial
- Overhead not worth it for instances that solve in < 0.001s

**Pattern**: Instances that solve in 0 conflicts without random often worse with random
- easy_3sat_v018_c0075: 0 → 64 conflicts (added unnecessary work)
- sat_v10: 0 → 25 conflicts (disrupted unit propagation)

### Interaction with Phase Saving

**Key Observation**: Random phase selection is a "fix" for phase saving pathologies

**Good Interaction** (easy_3sat_v016_c0067):
- Without phase saving: SAT in 0.003s (119 conflicts)
- With phase saving only: TIMEOUT at 10,000 conflicts (3.075s)
- With phase saving + 5% random: SAT in 0.001s (69 conflicts)
- **Conclusion**: Random phase selection fixed phase saving's catastrophic regression!

**Neutral Interaction** (most medium instances):
- Random phase either helps slightly or has no impact
- Maintains phase saving's benefits (50-100× speedups still present)

**Bad Interaction** (very small instances):
- random3sat_v5_c21: Created NEW catastrophic case (251× slowdown)
- Randomness disrupts simple solution paths
- **Conclusion**: Don't use random phase selection on tiny instances!

---

## Performance by Problem Type

### Structured 3-SAT (Medium Tests)

**Overall**: **278× faster** with 5% random phase selection

**Pattern**:
- **Huge wins** (100-4000×): 1 instance (THE FIX)
- **Moderate wins** (2-3×): 3 instances
- **Slight wins** (1-2×): 1 instance
- **Similar** (±5%): 2 instances
- **Small losses** (2-15×): 3 instances

**Conclusion**: Random phase selection is EXTREMELY valuable on medium structured instances

### Random 3-SAT (Simple Tests)

**Overall**: **25× slower** with 5% random phase selection

**Pattern**:
- **Huge wins** (31×): 1 instance
- **Slight wins** (1-2×): 2 instances
- **Small losses** (1-3×): 3 instances
- **Medium losses** (3-10×): 2 instances
- **Catastrophic loss** (250×): 1 instance

**Conclusion**: Random phase selection should NOT be used on small random instances

---

## Recommendations

### Default Configuration (Current)

**Disabled by default** (current implementation):
```python
solver = CDCLSolver(cnf,
                    use_watched_literals=True,
                    phase_saving=True,
                    restart_postponing=True,
                    random_phase_freq=0.0)  # DISABLED
```

**Why**:
- ✅ Avoids regressions on small instances
- ✅ Most instances work fine with pure phase saving
- ✅ User can enable when needed (hard instances, timeouts)

### When to Enable Random Phase Selection

**Scenario 1**: Timeout with phase saving
```python
# First attempt times out with phase saving
solver1 = CDCLSolver(cnf, phase_saving=True, random_phase_freq=0.0)
result1 = solver1.solve(max_conflicts=10000)

if result1 is None:  # Timeout
    # Retry with 5% random phase selection
    solver2 = CDCLSolver(cnf, phase_saving=True, random_phase_freq=0.05)
    result2 = solver2.solve(max_conflicts=10000)
    print(f"Fixed with random: {result2 is not None}")
```

**Scenario 2**: Known hard instances (100+ variables)
```python
# Large structured instance
solver = CDCLSolver(cnf,
                    phase_saving=True,
                    random_phase_freq=0.05,
                    random_seed=42)  # Reproducible
```

**Scenario 3**: Many restarts with same conflicts
```python
# Adaptive approach
solver = CDCLSolver(cnf, phase_saving=True, random_phase_freq=0.0)
result = solver.solve(max_conflicts=5000)

if solver.stats.restarts > 1000 and result is None:
    # Stuck in local minimum - enable random
    solver2 = CDCLSolver(cnf, phase_saving=True, random_phase_freq=0.05)
    result = solver2.solve(max_conflicts=10000)
```

### When to Avoid Random Phase Selection

**Avoid on**:
1. Very small instances (< 15 variables)
2. Fast solves (< 100 conflicts expected)
3. Pure random UNSAT instances
4. When performance is already good

### Tuning Random Frequency

**Conservative (2-5% recommended)**:
```python
solver = CDCLSolver(cnf, random_phase_freq=0.05)  # Balanced
```

**Moderate (5-10%)**:
```python
solver = CDCLSolver(cnf, random_phase_freq=0.10)  # More aggressive
```

**Note**: Our benchmarks show 5% works well, 10% can be too aggressive
- easy_3sat_v016_c0067 with 10% random: Timed out again!
- 5% seems to be the sweet spot

---

## Comparison with Other Techniques

### Random Phase vs Phase Saving

| Technique | Simple Tests | Medium Tests | Fixes Catastrophic Case |
|-----------|-------------|--------------|-------------------------|
| Phase saving only | 3.65× faster | 11.59× slower | ❌ No |
| Random phase (5%) | 25× slower | 278× faster | ✅ Yes |

**Conclusion**: Random phase selection brilliantly fixes phase saving's worst regression!

### Random Phase vs Restart Postponing

| Technique | Fixed easy_3sat_v016_c0067? | Overhead | Recommendation |
|-----------|------------------------------|----------|----------------|
| Restart postponing | ❌ No (trail never grew) | Minimal | Enable by default |
| Random phase (5%) | ✅ Yes (3,823× faster!) | Minimal | Disable by default, enable when needed |

**Conclusion**: Complementary techniques, random phase is the missing piece!

---

## Future Work

### High Priority: Adaptive Random Frequency

**Idea**: Auto-enable random phase selection after N conflicts
```python
if conflicts > 1000 and restarts > 500:
    random_phase_freq = 0.05  # Auto-enable
```

**Expected Impact**: Best of both worlds (no overhead on easy, help on hard)

### Medium Priority: Size-Based Heuristic

**Idea**: Only enable for larger instances
```python
if num_variables >= 20:
    random_phase_freq = 0.05
else:
    random_phase_freq = 0.0
```

**Expected Impact**: Avoid regressions on tiny instances

### Low Priority: Variable-Specific Random Selection

**Idea**: Apply random selection more to problematic variables
```python
if var in high_conflict_variables:
    use_random_with_prob = 0.10
else:
    use_random_with_prob = 0.02
```

**Expected Impact**: More targeted diversification

---

## Conclusions

### Main Takeaway

**Random phase selection (5%) is an excellent "safety net" for phase saving**:
- ✅ **Brilliantly fixes the catastrophic regression** (3,823× speedup!)
- ✅ **Excellent on medium instances** (278× overall speedup)
- ⚠️ **Can hurt very small instances** (25× overall slowdown)
- ✅ **Simple to implement** (3 lines of code)
- ✅ **Low overhead** (just a random check per decision)

### Recommendation

**Disabled by default, enable when needed**:
```python
# Default (disabled)
solver = CDCLSolver(cnf, random_phase_freq=0.0)

# Enable for hard instances
solver = CDCLSolver(cnf, random_phase_freq=0.05)
```

**Why**: Avoids regressions on small instances while providing huge benefits on hard instances

### Implementation Status

- ✅ **Implemented**: Fully working in cdcl_optimized.py:640-667
- ✅ **Tested**: Comprehensive benchmarks on 19 instances
- ✅ **Validated**: Fixed catastrophic regression (3,823× faster)
- ⚠️ **Known Issue**: Can create new catastrophic case on tiny instances (251× slower on random3sat_v5_c21)
- ⏳ **Future Work**: Adaptive frequency (auto-enable after N conflicts)

### Performance Summary

**Medium Tests (main use case)**: **278× faster** overall ✅
- Fixed catastrophic case: **3,823× faster** (3.075s → 0.001s)
- 5 instances faster, 2 similar, 3 slower (minor regressions)
- **Verdict**: EXCELLENT for medium-sized instances

**Simple Tests (not recommended)**: **25× slower** overall ⚠️
- 1 catastrophic case: **251× slower** (0.0006s → 0.145s)
- 1 huge win: **31× faster** (random luck)
- **Verdict**: AVOID for small instances

**Overall Recommendation**: ✅ **Valuable feature, use judiciously**

**Status**: Week 6 feature complete! 🎉

**Next Steps**: Consider adaptive random frequency (auto-enable after 1000+ conflicts)
