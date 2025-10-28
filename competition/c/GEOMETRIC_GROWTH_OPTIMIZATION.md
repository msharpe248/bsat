# Geometric Growth Optimization for Variable Arrays

## Problem

The original `solver_new_var()` implementation performed **6 realloc() calls** plus a watch manager recreation **every time a variable was added**, growing each array by exactly 1 element:

```c
// OLD CODE - O(n²) complexity
realloc(s->vars, (s->num_vars + 1) * sizeof(VarInfo))
realloc(s->trail, (s->num_vars + 1) * sizeof(Trail))
realloc(s->trail_lims, (s->num_vars + 1) * sizeof(Level))
realloc(s->order.heap, (s->num_vars + 1) * sizeof(Var))
realloc(s->seen, (s->num_vars + 1) * sizeof(uint8_t))
realloc(s->analyze_stack, (s->num_vars + 1) * sizeof(Lit))
watch_free() + watch_init()
```

**Performance Impact:**
- For n variables: **n realloc calls per array** = ~6n total reallocations
- Each realloc copies all previous elements: **O(n²) memory operations**
- Particularly bad when loading DIMACS files that call `solver_new_var()` in a tight loop

## Solution

Implement **geometric growth strategy** (similar to std::vector in C++):

1. **Track capacity separately** from size (`var_capacity` vs `num_vars`)
2. **Allocate in chunks**: Initial size = 128 variables
3. **Double capacity** when full: 128 → 256 → 512 → 1024 → ...
4. **Centralize reallocation** in `grow_var_arrays()` helper function

```c
// NEW CODE - O(n) amortized complexity
if (s->num_vars > s->var_capacity) {
    uint32_t new_capacity = s->var_capacity * 2;  // Double it
    grow_var_arrays(s, new_capacity);
    s->var_capacity = new_capacity;
}
```

## Implementation Details

### Configuration (src/solver.c)

```c
// Can be overridden at compile time for testing:
// cc -DVAR_INITIAL_CAPACITY=1 -DVAR_GROWTH_FACTOR=1 ...

#ifndef VAR_INITIAL_CAPACITY
#define VAR_INITIAL_CAPACITY 128
#endif

#ifndef VAR_GROWTH_FACTOR
#define VAR_GROWTH_FACTOR 2
#endif
```

### Modified Files

1. **include/solver.h**: Added `uint32_t var_capacity` field to Solver struct
2. **src/solver.c**:
   - Added growth configuration defines
   - Added `grow_var_arrays()` helper function (lines 327-368)
   - Rewrote `solver_new_var()` to use geometric growth (lines 370-417)
   - Initialize `var_capacity = 0` in `solver_new_with_opts()` (line 273)

### Helper Function

```c
static bool grow_var_arrays(Solver* s, uint32_t new_capacity) {
    // Reallocate all 6 variable-related arrays
    // Recreate watch manager
    // Returns true on success, false on allocation failure
}
```

## Performance Results

### Test Results (200 variables)

**Default Configuration (CAPACITY=128, FACTOR=2):**
```
Reallocations: 2 (at var 1 and var 129)
Linear growth: 200 reallocations
Improvement: 100× fewer reallocations
Wasted memory: 56 slots (21.9%)
```

**Growth Sequence:**
```
Var 1:   0 → 128   (initial allocation)
Var 129: 128 → 256 (first doubling)
Var 257: 256 → 512 (second doubling)
...
```

### Complexity Analysis

| Instance Size | Old (Linear) | New (Geometric) | Improvement |
|---------------|--------------|-----------------|-------------|
| 10 vars       | 10 reallocs  | 1 realloc       | 10×         |
| 100 vars      | 100 reallocs | 1 realloc       | 100×        |
| 1000 vars     | 1000 reallocs| 4 reallocs      | 250×        |

**Theoretical Complexity:**
- **Old**: O(n²) memory operations (n reallocs × average n/2 copies)
- **New**: O(n) amortized (log₂(n) reallocs × geometric series sums to O(n))

### Memory Overhead

- **Worst case**: 50% wasted (e.g., need 1025 vars, allocated 2048)
- **Best case**: <1% wasted (e.g., need 2048 vars, allocated 2048)
- **Average**: ~25% wasted
- **Trade-off**: Acceptable for massive performance gain

## Testing

### Correctness Testing

```bash
cd tests
./test_medium_suite.sh
```

All 13 medium instances pass with geometric growth enabled.

### Performance Testing

```bash
# Compile and run geometric growth test
cc -Iinclude -o bin/test_geometric_growth \
   tests/test_geometric_growth.c \
   build/solver.o build/arena.o build/watch.o -lm

./bin/test_geometric_growth
```

Output:
```
✅ SUCCESS: Geometric growth working correctly!
Improvement: 100.0x fewer reallocs
```

### Edge Case Testing (Debug Mode)

Compile with linear growth (CAPACITY=1, FACTOR=1) to test edge cases:

```bash
cc -DVAR_INITIAL_CAPACITY=1 -DVAR_GROWTH_FACTOR=1 \
   -Iinclude -c -o build/solver_debug.o src/solver.c

cc -Iinclude -o bin/test_growth_debug \
   tests/test_geometric_growth.c \
   build/solver_debug.o build/arena.o build/watch.o -lm

./bin/test_growth_debug
```

This reverts to the old O(n²) behavior for debugging/testing.

## Benefits

✅ **Performance**: 100-250× fewer reallocations for typical instances
✅ **Scalability**: O(n²) → O(n) complexity
✅ **Configurable**: Can override via compile-time flags
✅ **Tested**: All existing tests pass
✅ **Standard practice**: Matches std::vector, Vec in Rust, etc.

## Design Decisions

### Why INITIAL_CAPACITY = 128?

- **Medium tests** have ~40 variables → handled without reallocation
- **Large instances** need a few doublings (128 → 256 → 512 → 1024)
- **Memory overhead**: Only ~4KB wasted on small instances (negligible)

Could use 32-64 with minimal performance difference (saves 1 reallocation for 1000+ var instances).

### Why GROWTH_FACTOR = 2?

- **Factor 2.0**: ~50% max waste, standard in most implementations
- **Factor 1.5**: ~33% max waste, but more reallocations (7 vs 4 for 1000 vars)
- **Factor 2.0** is optimal for most use cases

### Why not pre-allocate based on DIMACS header?

The DIMACS parser already calls `solver_new_var()` after reading the header (dimacs.c:145-147), so geometric growth handles this efficiently without special-casing. Pre-allocation would require:
- Passing expected size through function calls
- Handling cases without headers (lenient parsing)
- More complex code

Geometric growth is simpler and handles all cases uniformly.

## Future Improvements

Potential optimizations:
1. **Shrink capacity** on solver reset (if reusing solver instances)
2. **Tune INITIAL_CAPACITY** based on profiling competition instances
3. **Watch manager resize**: Implement efficient watch manager growth instead of recreate

## References

- **MiniSat**: Uses geometric growth for variable arrays (factor 2)
- **Glucose**: Uses geometric growth (factor 2)
- **C++ std::vector**: Typical growth factor 1.5-2.0
- **Rust Vec**: Growth factor 2.0
- **Python list**: Growth factor ~1.125 (optimized for small lists)

## Author

Optimization implemented by Claude Code, October 2025
