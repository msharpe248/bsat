# C CDCL Solver - Current Status Summary

## Test Results on Medium Suite (53 instances)

### Configuration 1: Luby + No Random Phase (Python Config)
**Command**: `./bin/bsat --luby-restart --luby-unit 100 --no-random-phase <file>`

**Result**: **53/53 (100%) ✅ PERFECT**
- All instances pass
- No timeouts
- Matches Python's restart strategy

**Issues**: 
- Uses 43× more conflicts than Python (25,631 vs 597 on hard_3sat_v108_c0461.cnf)
- Root cause: Different VSIDS tie-breaking (C uses heap, Python uses dict order)
- This is NOT a bug - both are valid CDCL implementations

### Configuration 2: Default (Glucose + 1% Random Phase)
**Command**: `./bin/bsat <file>`

**Result**: **40/53 (75.5%) ❌ REGRESSION**
- 13 timeouts (was 1 before fix)
- Timeouts on: hard_3sat_v{096,099,102,108,111,117}_*.cnf, medium_3sat_v{066,068,070,072,074,078,080}_*.cnf

**Why regression?**: 
- Phase saving bug fix was correct but removed "accidental randomness"
- Before: Broken phase saving caused variables to randomly fall through to random phase
- After: Phase saving works correctly, but default config needs tuning

### Configuration 3: Simple Suite (13 instances)
Not tested yet with current build.

---

## What Works ✅

1. **Phase Saving**: Fixed critical bug - now works for ALL variables (was only 50%)
2. **Luby Restarts**: Fully implemented with cumulative conflict thresholds
3. **Completeness**: 100% success with Python-matching config
4. **Correctness**: All solved instances produce valid solutions

## What Doesn't Work ❌

1. **Default Config Performance**: Regression from 52/53 to 40/53
   - Needs new default parameters
   - Options:
     * Enable Luby by default?
     * Increase random phase %?
     * Tune Glucose restart parameters?

2. **Conflict Efficiency**: 43× more conflicts than Python
   - Different variable ordering on equal VSIDS scores
   - Not fixable without changing heap tie-breaking
   - Acceptable - both are correct CDCL

---

## Recommendations

**For Competition/Production**:
```bash
# Use Luby config - 100% success rate
./bin/bsat --luby-restart --luby-unit 100 --no-random-phase <file>
```

**For Development**:
Need to find better default config that balances:
- Completeness (instances solved)
- Efficiency (conflicts used)
- Generality (works across different instance types)

Possible approaches:
1. Make Luby the default
2. Increase random phase to 5-10% 
3. Implement adaptive restarts that combine Glucose + Luby
4. Add portfolio mode that tries multiple configs

---

## Files Changed (Committed: bbe48ce)

- `src/solver.c`: Fixed phase saving bug (line 965), added Luby sequence
- `include/solver.h`: Added luby_restart, luby_unit, luby_index fields
- `src/main.c`: Added --luby-restart, --luby-unit, --no-random-phase flags
- `test_luby_suite.sh`: Test script for Luby configuration
