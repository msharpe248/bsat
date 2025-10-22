# Glucose Sliding Window Implementation Status

## Completed

### 1. Data Structures (solver.h) ✅
- Added `glucose_use_ema` to SolverOpts
- Added `glucose_window_size` and `glucose_k` parameters
- Added sliding window fields to restart state:
  - `recent_lbds` (circular buffer)
  - `recent_lbds_count`, `recent_lbds_head`
  - `lbd_sum`, `lbd_count`

### 2. Solver Logic (solver.c) ✅
- Initialized sliding window buffer in `solver_new_with_opts()` (line 261-271)
- Added buffer freeing in `solver_free()` (line 299)
- Implemented dual-mode Glucose in `solver_should_restart()` (line 1078-1109):
  - EMA mode: Uses fast_ma/slow_ma
  - AVG mode: Uses sliding window averages
- Updated LBD tracking in conflict analysis (line 2020-2044, 2048-2090):
  - Tracks LBDs in circular buffer for AVG mode
  - Maintains lbd_sum and lbd_count for long-term average

## Remaining Tasks

### 3. Command-Line Flags (main.c) - IN PROGRESS
Need to add:
```c
// In long_options array (around line 97):
{"glucose-restart-ema", no_argument, 0, 0},
{"glucose-restart-avg", no_argument, 0, 0},

// In option handling (around line 176):
else if (strcmp(long_options[option_index].name, "glucose-restart-ema") == 0) {
    opts.glucose_restart = true;
    opts.glucose_use_ema = true;
    opts.luby_restart = false;
}
else if (strcmp(long_options[option_index].name, "glucose-restart-avg") == 0) {
    opts.glucose_restart = true;
    opts.glucose_use_ema = false;
    opts.luby_restart = false;
}

// In help text (around line 47):
printf("  --glucose-restart-ema     Use Glucose adaptive (EMA, conservative)\n");
printf("  --glucose-restart-avg     Use Glucose adaptive (sliding window, Python-style)\n");
```

### 4. Testing
Once flags are added, test:
```bash
make
./bin/bsat --glucose-restart-avg ../../dataset/medium_tests/medium_suite/hard_3sat_v108_c0461.cnf
```
Expected: Should solve in ~323 conflicts like Python (vs timeout with EMA)

## Key Differences

| Feature | EMA Mode | AVG Mode (Python-style) |
|---------|----------|-------------------------|
| Algorithm | Exponential moving averages | Sliding window averages |
| Parameters | alpha_fast=0.8, alpha_slow=0.9999 | window=50, K=0.8 |
| Restart freq | Conservative (rarely triggers) | Aggressive (~1 per conflict) |
| Performance | Good for industrial | Good for random 3-SAT |
| Flag | `--glucose-restart-ema` | `--glucose-restart-avg` |
