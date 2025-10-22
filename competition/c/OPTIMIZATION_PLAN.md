# BSAT C Solver Performance Optimization Plan

**Current Status**: C solver is only **2.09x faster** than Python on average
- C solver: 44/62 instances (70.9% coverage)
- Python solver: 49/62 instances (79.0% coverage)
- **Problem**: C times out on 11 instances Python solves quickly
- **Root cause**: Likely algorithmic issues + performance problems

## Phase 1: Diagnostic & Profiling (CURRENT - Essential)

**Goal**: Understand WHERE time is actually spent before optimizing

### 1.1 Add Instrumentation (IN PROGRESS)
- [x] Arena growth tracking (when/how often arena expands)
- [x] Better initial arena sizing based on problem size
- [ ] Propagation statistics (propagations per conflict, watch list sizes)
- [ ] Decision heuristic metrics (VSIDS heap operations)
- [ ] Clause database metrics (learned clause quality, reduction effectiveness)
- [ ] Restart analysis (frequency, effectiveness)

### 1.2 Profile with gprof/perf
- Build with `-pg` flag
- Run on representative instances
- Identify hot functions (expect: propagate 80%+, analyze 10%, decision 5%)

### 1.3 Compare C vs Python behavior
- Run both on timeout instances
- Compare search tree characteristics (decisions, conflicts, restarts)
- Identify WHY C times out (wrong heuristics? stuck in search?)

## Phase 2: Low-Hanging Fruit (Quick Wins - 2-5x improvement)

### 2.1 Compilation Optimizations (1-2 hours)
**Changes to Makefile**:
```makefile
OPTFLAGS = -O3 -march=native -flto -ffast-math -funroll-loops
```

**Add function attributes**:
- `__attribute__((hot))` to `solver_propagate()`
- `__attribute__((always_inline))` to critical helpers
- Verify LTO is working

### 2.2 Data Structure Optimization (2-4 hours)
**Problem**: VarInfo is ~40 bytes, causes cache misses

**Current structure** (40 bytes per variable):
```c
typedef struct VarInfo {
    lbool value;         // 4 bytes
    Level level;         // 4 bytes
    CRef reason;         // 4 bytes
    uint32_t trail_pos;  // 4 bytes
    bool polarity;       // 1 byte + 3 padding
    uint32_t last_polarity; // 4 bytes
    double activity;     // 8 bytes
    uint32_t heap_pos;   // 4 bytes
} VarInfo;
```

**Proposed optimization** (split hot/cold):
```c
// HOT PATH: 12 bytes (fits in 1 cache line with neighbors)
typedef struct VarAssignment {
    uint32_t value_level_reason; // Pack value(2bit) + level(14bit) + reason(16bit)
    uint32_t trail_pos;
    float activity;  // Use float instead of double (sufficient precision)
} VarAssignment;

// COLD PATH: 8 bytes (accessed rarely)
typedef struct VarMeta {
    uint32_t heap_pos;
    uint16_t polarity : 1;
    uint16_t last_polarity : 15;
} VarMeta;
```

**Expected impact**: 2-3x fewer cache misses in propagation loop

### 2.3 Propagation Loop Micro-optimizations (2-3 hours)
```c
// Add prefetching
__builtin_prefetch(&watches[i+1], 0, 3);

// Inline blocker check
static inline bool is_blocker_satisfied(const Solver* s, Lit blocker) {
    Var v = var(blocker);
    return s->vars[v].value == (sign(blocker) ? FALSE : TRUE);
}

// Remove all DEBUG code in release builds
#ifndef DEBUG
#define DEBUG_PRINT(...)
#else
#define DEBUG_PRINT(...) do { if (getenv("DEBUG_CDCL")) printf(__VA_ARGS__); } while(0)
#endif
```

**Expected impact**: 1.5-2x speedup in propagation

## Phase 3: Algorithmic Fixes (Critical - Fix timeouts)

### 3.1 Investigate Timeout Root Cause (3-5 hours)
Compare failing instances:
- Is C making bad decisions? (check decision heuristic)
- Is C learning worse clauses? (check LBD distribution)
- Is C restarting incorrectly? (compare restart frequencies)
- Is C getting stuck? (check adaptive random phase)

**Instrumentation needed**:
```c
// Per-instance diagnostics
typedef struct DiagnosticStats {
    uint64_t decisions_per_conflict;
    uint64_t propagations_per_conflict;
    uint32_t avg_lbd;
    uint32_t avg_learned_size;
    uint32_t restart_frequency;
    uint32_t stuck_detections;
} DiagnosticStats;
```

### 3.2 Tune Parameters (1-2 hours)
Current params may be suboptimal for small instances:
```c
// CURRENT
restart_first = 100      // May be too large for small instances
restart_inc = 1.5        // May be too aggressive
var_decay = 0.95         // Standard
reduce_interval = 2000   // May be wrong for small instances

// SUGGESTED (try MiniSAT/Glucose values)
restart_first = 25       // More aggressive initial restart
restart_inc = 1.1        // Gentler increase
var_decay = 0.95         // Keep
reduce_interval = 1000   // More frequent for small instances
```

### 3.3 Add Missing Optimizations
- Implement chronological backtracking (if not already)
- Improve random phase selection
- Better stuck detection

## Phase 4: Advanced Optimizations (10-50x potential)

### 4.1 Cache-Aware Watch Lists (2-4 hours)
```c
// BEFORE: ~12 bytes per watch
typedef struct Watch {
    CRef cref;      // 4 bytes
    Lit blocker;    // 4 bytes
} Watch;  // + padding = 12 bytes

// AFTER: 8 bytes per watch
typedef struct Watch {
    uint32_t cref;      // 4 bytes
    uint32_t blocker;   // 4 bytes (pack literal)
} Watch __attribute__((packed));  // Exactly 8 bytes
```

### 4.2 Profile-Guided Optimization (PGO) (2-3 hours)
```bash
# Step 1: Build with instrumentation
make clean
CFLAGS="-O3 -march=native -fprofile-generate" make

# Step 2: Run training workload
for f in dataset/medium_tests/medium_suite/*.cnf; do
    timeout 5 ./bin/bsat $f >/dev/null 2>&1
done

# Step 3: Rebuild with profile data
make clean
CFLAGS="-O3 -march=native -fprofile-use" make
```

**Expected**: 10-30% speedup with zero code changes

### 4.3 Better Memory Allocation (1-2 hours)
Options:
- Use jemalloc: `LD_PRELOAD=/usr/lib/libjemalloc.so ./bin/bsat`
- Use tcmalloc: `LD_PRELOAD=/usr/lib/libtcmalloc.so ./bin/bsat`
- Pre-allocate arena in large chunks (already doing this)

### 4.4 SIMD Optimizations (Advanced - 4-8 hours)
For clause evaluation (if profiling shows it's significant):
```c
#include <immintrin.h>  // AVX2

// Check 8 literals at once
bool check_clause_satisfied_simd(const Solver* s, const Lit* lits, uint32_t size) {
    // Load 8 literals at a time
    // Check their values in parallel
    // Early exit if any satisfied
}
```

## Phase 5: Ultimate Optimizations (50-1000x if successful)

### 5.1 Concurrent Search (Major undertaking - 40+ hours)
- Portfolio approach: run multiple strategies in parallel
- Work stealing between threads
- Clause sharing between solvers
- Lock-free data structures

### 5.2 GPU Acceleration (Research project - 100+ hours)
- Move propagation to GPU
- Massively parallel clause checking
- Memory bandwidth challenges

## Expected Results

| Phase | Time | Expected Speedup | Cumulative | Status |
|-------|------|------------------|------------|--------|
| 1. Profiling | 4h | 0x (diagnostic) | 1x | IN PROGRESS |
| 2. Quick Wins | 8h | 2-5x | 4-10x | PLANNED |
| 3. Fix Timeouts | 8h | 2x (coverage) | 8-20x | PLANNED |
| 4. Advanced | 12h | 2-4x | 16-80x | PLANNED |
| 5. Ultimate | 100h+ | 10-100x | 160-8000x | FUTURE |

**Immediate Focus**: Phases 1-3 (20 hours) should get us to **10-20x** speedup over Python and fix the timeout problem.

## Priority Order

1. **CURRENT**: Instrumentation and diagnostics
2. **CRITICAL**: Fix timeout issues (algorithmic problem)
3. **HIGH ROI**: Compilation flags + data structure optimization
4. **MEDIUM**: PGO and malloc optimization
5. **RESEARCH**: Concurrent/GPU (only after 1-4)

## Notes

- Benchmark showing only 2.09x average is concerning - suggests algorithmic issues
- C times out where Python doesn't - need to understand WHY
- Can't optimize what we haven't measured - profiling is essential
- Low-hanging fruit (flags, PGO) should be done early

Last updated: 2025-10-22
