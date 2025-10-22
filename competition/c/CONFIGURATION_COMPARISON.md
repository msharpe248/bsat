# C CDCL Solver - Configuration Comparison Results

## Test Date: 2025-10-22

Tested on 53 instances from `dataset/medium_tests/medium_suite/` with 30-second timeout per instance.

---

## Configuration Results Summary

| Configuration | Success Rate | Timeouts | Command |
|--------------|--------------|----------|---------|
| **Luby + No Random (NEW DEFAULT)** | **53/53 (100%)** ✅ | **0** | `./bin/bsat <file>` |
| Glucose + 5% Random | 41/53 (77.3%) | 12 | `./bin/bsat --no-luby-restart --random-prob 0.05 <file>` |
| Glucose + 10% Random | 41/53 (77.3%) | 12 | `./bin/bsat --no-luby-restart --random-prob 0.10 <file>` |

---

## Winner: Luby Restarts + No Random Phase

**Current Default Configuration** (as of this commit):
```bash
./bin/bsat <file>
```

This uses:
- **Luby restart sequence** (1, 1, 2, 1, 1, 2, 4, 8, ...)
- **Luby unit = 100 conflicts** (matches Python reference solver)
- **Phase saving enabled** (remembers polarities across restarts)
- **No random phase** (deterministic decisions)

**Result: 100% completeness on test suite**

---

## Detailed Results

### Configuration 1: Luby + No Random (NEW DEFAULT)
**Command**: `./bin/bsat <file>`

```
✅ PASS: All 53 instances
⏱️ TIMEOUT: None
```

**Perfect Score: 53/53 (100%)**

This matches the Python reference solver's configuration and achieves identical completeness.

---

### Configuration 2: Glucose + 5% Random Phase
**Command**: `./bin/bsat --no-luby-restart --random-prob 0.05 <file>`

```
✅ PASS: 41 instances
❌ TIMEOUT: 12 instances

Timeouts:
  hard_3sat_v096_c0409.cnf
  hard_3sat_v099_c0422.cnf
  hard_3sat_v108_c0461.cnf
  hard_3sat_v111_c0473.cnf
  hard_3sat_v117_c0499.cnf
  medium_3sat_v066_c0281.cnf
  medium_3sat_v068_c0289.cnf
  medium_3sat_v070_c0298.cnf
  medium_3sat_v072_c0306.cnf
  medium_3sat_v074_c0315.cnf
  medium_3sat_v078_c0332.cnf
  medium_3sat_v080_c0340.cnf
```

**Score: 41/53 (77.3%)**

---

### Configuration 3: Glucose + 10% Random Phase
**Command**: `./bin/bsat --no-luby-restart --random-prob 0.10 <file>`

```
✅ PASS: 41 instances
❌ TIMEOUT: 12 instances (SAME AS 5%)

Timeouts: (identical to 5% random)
  hard_3sat_v096_c0409.cnf
  hard_3sat_v099_c0422.cnf
  hard_3sat_v108_c0461.cnf
  hard_3sat_v111_c0473.cnf
  hard_3sat_v117_c0499.cnf
  medium_3sat_v066_c0281.cnf
  medium_3sat_v068_c0289.cnf
  medium_3sat_v070_c0298.cnf
  medium_3sat_v072_c0306.cnf
  medium_3sat_v074_c0315.cnf
  medium_3sat_v078_c0332.cnf
  medium_3sat_v080_c0340.cnf
```

**Score: 41/53 (77.3%)**

**Observation**: Increasing random phase from 5% to 10% had no effect - same instances timeout. This suggests these instances fundamentally need Luby's restart strategy, not more randomness.

---

## Analysis

### Why Luby Wins

1. **Frequent Short Restarts**: Luby sequence has many short restarts (1, 1, 2, 1, 1, 2, ...) that help escape local minima early

2. **Matches Python's Strategy**: Python reference solver uses Luby with unit=100, achieving 100% on these instances

3. **No Random Phase Needed**: Phase saving alone provides sufficient diversity when combined with aggressive restarts

4. **Deterministic**: Reproducible results, easier to debug

### Why Glucose + Random Fails

1. **Adaptive Restarts Too Conservative**: Glucose waits for LBD moving averages to diverge, which may not happen quickly enough

2. **Random Phase Hurts**: On these structured instances, random phase disrupts learned phase information without providing enough benefit

3. **Same Failures at 5% and 10%**: The problem is fundamental, not just insufficient randomness

---

## Recommendations

### For Production/Competition
Use the default configuration:
```bash
./bin/bsat <file>
```

No flags needed - Luby is now the default.

### For Experimentation
If you want to test Glucose adaptive restarts:
```bash
./bin/bsat --no-luby-restart <file>
```

To use geometric restarts (legacy):
```bash
./bin/bsat --no-luby-restart --restart-first 100 --restart-inc 1.5 <file>
```

### Advanced Tuning

All command-line options:
- `--luby-restart`: Use Luby sequence (DEFAULT)
- `--no-luby-restart`: Disable Luby, enable Glucose adaptive
- `--luby-unit N`: Luby unit size in conflicts (default: 100)
- `--random-prob F`: Random phase probability 0.0-1.0 (default: 0.0)
- `--random-phase`: Enable random phase (with --random-prob)
- `--no-random-phase`: Disable random phase (DEFAULT)
- `--phase-saving` / `--no-phase-saving`: Control phase saving (enabled by default)
- `--restart-first N`: First restart interval for geometric (default: 100)
- `--restart-inc F`: Restart multiplier for geometric (default: 1.5)

---

## Key Changes Made

1. **Default Configuration** (`src/solver.c:70-74`):
   ```c
   .luby_restart = true,       // NOW DEFAULT
   .luby_unit = 100,           // Matches Python
   .glucose_restart = false,   // Disabled when Luby enabled
   .random_phase = false,      // No random phase by default
   .phase_saving = true,       // Phase saving enabled
   ```

2. **New Flags** (`src/main.c`):
   - Added `--no-luby-restart` to switch back to Glucose
   - Existing `--random-prob` flag can be used for random phase experiments

3. **Bug Fixed**: Phase saving now works for ALL variables (was only 50%)

---

## Conflict Efficiency Note

C solver still uses ~43× more conflicts than Python on some instances (e.g., 25,631 vs 597 on hard_3sat_v108_c0461.cnf). This is due to different VSIDS tie-breaking:
- **Python**: Alphabetical dict order picks x1 first when scores are equal
- **C**: Heap structure picks variables by numeric order

Both are correct CDCL implementations - they just explore different search spaces. The important metric is **completeness** (100% achieved) and **time-to-solution** (which is fast due to C implementation).

---

## Test Scripts

- `./test_luby_suite.sh` - Test default Luby configuration
- `./test_glucose_random.sh` - Test Glucose with 5% and 10% random phase
- `./test_medium_suite.sh` - Quick test of current default

---

## Conclusion

**Luby restart sequence is the clear winner** for this test suite, achieving perfect 100% completeness. This configuration is now the default and requires no special flags to use.
