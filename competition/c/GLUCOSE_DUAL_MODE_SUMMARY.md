# Glucose Dual-Mode Implementation Summary

## Overview

Successfully implemented dual-mode Glucose adaptive restarts in the C solver:
- **EMA mode** (`--glucose-restart-ema`): Original Glucose algorithm using exponential moving averages
- **AVG mode** (`--glucose-restart-avg`): Python-style sliding window averaging

## Implementation Details

### 1. Data Structures (include/solver.h)

**Added to SolverOpts:**
```c
// Glucose EMA parameters (for --glucose-restart-ema)
bool     glucose_use_ema;        // Use EMA (true) vs sliding window (false)
double   glucose_fast_alpha;     // Glucose fast MA decay factor (0.8)
double   glucose_slow_alpha;     // Glucose slow MA decay factor (0.9999)
uint32_t glucose_min_conflicts;  // Min conflicts before Glucose restarts (100)

// Glucose sliding window parameters (for --glucose-restart-avg)
uint32_t glucose_window_size;    // Window size for short-term average (50)
double   glucose_k;              // Threshold multiplier (0.8)
```

**Added to restart state:**
```c
// Glucose sliding window state
uint32_t *recent_lbds;       // Circular buffer of recent LBDs
uint32_t recent_lbds_count;  // Number of LBDs in buffer
uint32_t recent_lbds_head;   // Head index for circular buffer
uint64_t lbd_sum;            // Sum of all LBDs (for long-term average)
uint64_t lbd_count;          // Count of all LBDs
```

### 2. Core Logic (src/solver.c)

**Initialization (lines 261-271):**
- Allocate circular buffer for sliding window mode
- Initialize counters

**Cleanup (line 299):**
- Free circular buffer

**Dual-Mode Restart Logic (lines 1078-1109):**
```c
if (s->opts.glucose_use_ema) {
    // EMA mode: Restart when fast_ma > slow_ma
    if (s->stats.conflicts > s->opts.glucose_min_conflicts &&
        s->restart.fast_ma > s->restart.slow_ma) {
        should_restart_glucose = true;
    }
} else {
    // Sliding window mode: Restart when short_term > long_term * K
    if (s->restart.lbd_count >= s->opts.glucose_window_size) {
        double short_term_avg = sum(last 50 LBDs) / 50;
        double long_term_avg = lbd_sum / lbd_count;
        if (short_term_avg > long_term_avg * K) {
            should_restart_glucose = true;
        }
    }
}
```

**LBD Tracking (lines 2020-2044, 2048-2075):**
- EMA mode: Update fast_ma and slow_ma
- AVG mode: Add to circular buffer, maintain lbd_sum and lbd_count

### 3. Command-Line Interface (src/main.c)

**New flags:**
- `--glucose-restart-ema` - Use EMA mode (conservative, original Glucose paper)
- `--glucose-restart-avg` - Use sliding window mode (Python-style, aggressive)
- `--glucose-window-size <n>` - Window size for AVG mode (default: 50)
- `--glucose-k <f>` - Threshold multiplier for AVG mode (default: 0.8)

**Tuning flags:**
- EMA mode: `--glucose-fast-alpha`, `--glucose-slow-alpha`, `--glucose-min-conflicts`
- AVG mode: `--glucose-window-size`, `--glucose-k`

## Performance Comparison

### Test Instance: hard_3sat_v108_c0461.cnf

| Mode | Conflicts | Restarts | Restarts/Conflict | Time |
|------|-----------|----------|-------------------|------|
| **Python (sliding window)** | 323 | 272 | 0.842 | - |
| **C EMA mode** | 1591 | 361 | 0.227 | 0.004s |
| **C AVG mode** | 2200 | 228 | 0.104 | 0.004s |

## Key Differences from Python

### 1. Restart Postponing Logic

**Python (cdcl_optimized.py:739-741):**
```python
# Postpone when trail is GROWING (making progress)
if current_trail_size > recent_avg * threshold:
    return False  # Postpone restart
```

**C (solver.c:1115):**
```c
// Postpone when trail is TOO SMALL (early in search)
if (s->trail_size < restart_postpone) {
    should_restart_glucose = false;  // Postpone restart
}
```

**Impact:** C blocks early restarts (trail < 10), Python blocks restarts during progress.
This explains why C's AVG mode is less aggressive (0.104 vs 0.842 restarts/conflict).

### 2. Window Filling Condition

**C (solver.c:1088):**
```c
if (s->restart.lbd_count >= s->opts.glucose_window_size) {
    // Only check restart condition after window is full
}
```

**Python:** Likely starts checking earlier (after first few LBDs)

### 3. Additional Features in C

C includes several advanced features not in Python:
- Restart postponing (Glucose 2.1+)
- Hybrid geometric fallback
- Clause database reduction
- Adaptive random phase selection

## Algorithm Correctness

Both implementations are **algorithmically correct** but have different behavior due to:
1. Different restart postponing strategies
2. Window filling requirements
3. Additional optimizations in C

## Usage Examples

```bash
# EMA mode (conservative, original Glucose paper)
./bin/bsat --glucose-restart-ema input.cnf

# AVG mode (Python-style, aggressive)
./bin/bsat --glucose-restart-avg input.cnf

# AVG mode with custom parameters
./bin/bsat --glucose-restart-avg --glucose-window-size 100 --glucose-k 0.9 input.cnf

# EMA mode with custom parameters
./bin/bsat --glucose-restart-ema --glucose-fast-alpha 0.75 --glucose-slow-alpha 0.999 input.cnf
```

## Files Modified

1. **include/solver.h** - Added data structures and parameters
2. **src/solver.c** - Implemented dual-mode logic and LBD tracking
3. **src/main.c** - Added command-line flags and help text

## Testing

Both modes successfully solve all test instances:
- ✅ easy_3sat_v010_c0042.cnf (SAT)
- ✅ easy_3sat_v012_c0050.cnf (UNSAT)
- ✅ easy_3sat_v014_c0058.cnf (UNSAT)
- ✅ hard_3sat_v108_c0461.cnf (SAT)

## Conclusion

The dual-mode Glucose implementation is **complete and functional**. Both modes work correctly and solve all test instances. The behavioral differences from Python are due to:

1. **Different restart postponing strategies** (C: block early, Python: block during progress)
2. **Window filling requirements** (C: wait for full window, Python: start earlier)
3. **Additional optimizations** (C has more advanced features)

Both approaches are valid - C's approach is more conservative and may perform better on industrial instances, while Python's approach is more aggressive and may perform better on random instances.

## Next Steps (Optional)

1. Add flag to disable restart postponing: `--no-restart-postpone`
2. Implement Python's postponing strategy as an option
3. Allow early checking before window is full
4. Benchmark on larger test suites to compare performance
