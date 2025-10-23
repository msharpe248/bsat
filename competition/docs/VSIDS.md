# VSIDS: Variable State Independent Decaying Sum

**Technique**: Dynamic variable ordering heuristic
**Impact**: Critical for CDCL performance - guides search toward conflict areas
**Status**: ✅ Implemented in both Python and C solvers

---

## Overview

VSIDS (Variable State Independent Decaying Sum) is the variable ordering heuristic used in modern CDCL solvers. It dynamically adjusts variable priorities based on their participation in recent conflicts, focusing the search on "active" parts of the problem.

**Key Idea**: Variables involved in recent conflicts are more likely to be relevant to future conflicts. By prioritizing these variables for branching decisions, the solver explores the most promising parts of the search space first.

---

## The Problem: Variable Ordering

In DPLL/CDCL, we must choose which variable to assign next (branching decision). Poor choices lead to:
- Deep, unproductive search branches
- Many conflicts before finding solution
- Exponential slowdown

**Question**: How do we choose which variable to branch on?

**Naive approaches**:
- **Fixed order**: Assign variables in order (x1, x2, x3, ...)
  - Simple but ignores problem structure
- **Most constrained first**: Variable in most clauses
  - Static, doesn't adapt to search
- **Random**: Pick randomly
  - No guidance, terrible performance

**VSIDS answer**: Maintain dynamic activity scores, prioritize variables involved in recent conflicts.

---

## Algorithm

### Core Components

1. **Activity Scores**: Each variable has a floating-point activity score
   ```c
   double activity[num_vars];  // Activity score per variable
   ```

2. **Heap Data Structure**: Variables ordered by activity (max-heap)
   ```c
   Var heap[num_vars];    // Binary max-heap
   activity[heap[0]] >= activity[heap[1]]  // Root has highest activity
   ```

3. **Activity Increment**: Global increment value, grows over time
   ```c
   double var_inc = 1.0;  // Initial increment
   ```

4. **Decay Factor**: Exponential decay of old activities
   ```c
   double var_decay = 0.95;  // Default decay factor
   ```

### Operations

#### 1. Variable Selection (Branching)

**Goal**: Pick unassigned variable with highest activity

```c
Var select_variable() {
    while (!heap_empty()) {
        Var v = heap_extract_max();  // O(log n)
        if (value[v] == UNDEF) {
            return v;  // Found unassigned variable
        }
    }
    return INVALID_VAR;  // All variables assigned
}
```

**Complexity**: O(log n) per decision

#### 2. Activity Bumping (After Conflict)

**Goal**: Increase activity of variables in conflict analysis

```c
void bump_activity(Var v) {
    activity[v] += var_inc;

    // Rescale if overflow risk
    if (activity[v] > 1e100) {
        rescale_activities();
    }

    // Update heap position (bubble up if needed)
    heap_decrease(v);  // O(log n)
}
```

**When**: After analyzing a conflict, bump all variables in the learned clause

**Effect**: Variables involved in recent conflicts have higher priority

#### 3. Activity Decay

**Goal**: Exponentially decay importance of old conflicts

```c
void decay_activities() {
    var_inc *= (1.0 / var_decay);  // Increase future bumps
}
```

**When**: After every conflict

**Effect**: Recent conflicts weighted exponentially more than old ones

**Math**:
- First conflict bumps variable by: 1.0
- Second conflict bumps by: 1.0 / 0.95 ≈ 1.053
- Third conflict bumps by: 1.053 / 0.95 ≈ 1.108
- After 100 conflicts: bump by ~131
- **Result**: Recent conflicts ~100× more important than ancient conflicts

#### 4. Activity Rescaling

**Goal**: Prevent floating-point overflow

```c
void rescale_activities() {
    for (Var v = 1; v <= num_vars; v++) {
        activity[v] *= 1e-100;
    }
    var_inc *= 1e-100;
}
```

**When**: When any activity exceeds 1e100

**Effect**: All activities scaled down proportionally (preserves relative order)

---

## Heap Implementation

### Why a Heap?

**Requirements**:
- Fast extraction of max activity variable: O(log n)
- Fast activity updates (after bumping): O(log n)
- Must handle frequent updates (every conflict)

**Binary max-heap** is ideal:
- Array-based: cache-friendly
- O(log n) insert/extract/update
- Simple to implement

### Data Structure

```c
struct {
    Var*     heap;       // Array of variables (heap property)
    uint32_t size;       // Current heap size
    uint32_t* heap_pos;  // heap_pos[v] = position of v in heap
} order;
```

**Heap property**: `activity[heap[i]] >= activity[heap[left(i)]]` and `activity[heap[i]] >= activity[heap[right(i)]]`

**Position tracking**: `heap_pos[v]` allows O(1) lookup of variable's position in heap

### Operations

#### Insert Variable
```c
void heap_insert(Var v) {
    heap[size] = v;
    heap_pos[v] = size;
    size++;
    bubble_up(size - 1);  // Restore heap property
}
```

#### Extract Maximum
```c
Var heap_extract_max() {
    Var v = heap[0];        // Root has max activity
    heap[0] = heap[--size]; // Move last to root
    heap_pos[heap[0]] = 0;
    bubble_down(0);         // Restore heap property
    heap_pos[v] = INVALID;
    return v;
}
```

#### Update (After Activity Bump)
```c
void heap_decrease(Var v) {
    // Activity increased, so v may need to bubble UP
    uint32_t pos = heap_pos[v];
    if (pos != INVALID) {
        bubble_up(pos);
    }
}
```

---

## Performance Impact

### Without VSIDS (Fixed Order)

**Example**: 3SAT formula with 1000 variables
- Fixed order tries x1, x2, x3, ..., x1000
- May branch on irrelevant variables early
- Deep, fruitless search trees
- **Result**: Millions of conflicts, hours to solve

### With VSIDS

**Same formula**:
- VSIDS learns which variables are "active" (in conflicts)
- Focuses search on conflict-rich regions
- Quickly finds contradictions or solutions
- **Result**: Thousands of conflicts, seconds to solve

**Typical speedup**: 10-1000× depending on problem structure

---

## Tuning Parameters

### Decay Factor (`var_decay`)

**Default**: 0.95

**Effect of changing**:

**Lower decay** (e.g., 0.90):
- Faster decay of old activities
- More reactive to recent conflicts
- Pros: Quick adaptation
- Cons: May forget long-term structure

**Higher decay** (e.g., 0.99):
- Slower decay of old activities
- More stable ordering
- Pros: Remembers long-term patterns
- Cons: Slower adaptation

**Recommended range**: 0.90 - 0.99

**Best values**:
- Random instances: 0.95 (default)
- Structured instances: 0.95 - 0.99
- Industrial instances: 0.95

### Initial Increment (`var_inc`)

**Default**: 1.0

**Effect**: Only affects absolute scale, not relative ordering

**Tuning**: Usually not changed (1.0 is standard)

---

## Implementation Notes

### Python Implementation

```python
class CDCLSolver:
    def __init__(self, cnf):
        self.var_activity = {var: 0.0 for var in cnf.get_variables()}
        self.var_inc = 1.0
        self.var_decay = 0.95
        self.var_order_heap = []  # Binary heap (using heapq)

    def _select_variable(self):
        # Extract max from heap
        while self.var_order_heap:
            v = heapq.heappop(self.var_order_heap)
            if v not in self.assignment:
                return v
        return None

    def _bump_activity(self, var):
        self.var_activity[var] += self.var_inc
        if self.var_activity[var] > 1e100:
            self._rescale_activities()
        # Re-insert into heap with new activity
        heapq.heappush(self.var_order_heap, var)

    def _decay_activities(self):
        self.var_inc /= self.var_decay
```

### C Implementation

See `competition/c/src/solver.c:716-805` for complete implementation.

**Key differences**:
- Manual heap management (no stdlib heap in C)
- Heap operations: `heap_insert`, `heap_extract_max`, `heap_decrease`
- Position tracking via `heap_pos` array for O(1) lookup

---

## Advanced Variations

### VSIDS Variants

1. **Original VSIDS** (Chaff, 2001)
   - Multiplicative decay every 256 conflicts
   - Periodic rescaling

2. **Additive VSIDS** (MiniSat, 2003)
   - Additive increment with exponential growth (this implementation)
   - More common in modern solvers

3. **EVSIDS** (Exponential VSIDS, MapleSAT, 2016)
   - Exponential moving average
   - Faster adaptation

4. **ACIDS** (Average Conflict-Index Decision Score)
   - Averages conflict indices
   - Used in some modern solvers

### Hybrid Approaches

**Phase-based VSIDS** (Glucose, Kissat):
- Different decay rates in different search phases
- Rapid decay during rapid restarts
- Conservative decay during stable search

**Learning Rate Adaptation**:
- Adjust `var_decay` based on search progress
- More aggressive when stuck
- More conservative when progressing

---

## Comparison with Other Heuristics

| Heuristic | Complexity | Adaptability | Performance |
|-----------|------------|--------------|-------------|
| **Fixed order** | O(1) | None | Poor |
| **Most constrained** | O(n) | Static | Moderate |
| **VSIDS** | O(log n) | Dynamic | Excellent |
| **BerkMin** | O(log n) | Dynamic | Good |
| **VMTF** (Move-to-Front) | O(1) | Dynamic | Comparable |

**VSIDS is standard** in modern solvers (Glucose, Kissat, CaDiCaL, CryptoMiniSat)

**VMTF** is gaining popularity (simpler, faster, similar performance)

---

## Debugging and Analysis

### Visualizing Activity Evolution

```python
import matplotlib.pyplot as plt

# Track activities over time
activity_history = []

# During solving:
activity_history.append(list(solver.var_activity.values()))

# After solving:
for var_idx in range(num_vars):
    plt.plot([activities[var_idx] for activities in activity_history])

plt.xlabel('Conflict')
plt.ylabel('Activity')
plt.title('Variable Activity Evolution')
plt.show()
```

**Expected pattern**:
- Rapid growth for variables in many conflicts
- Decay for variables not in recent conflicts
- Periodic rescaling (vertical drops)

### Common Issues

**Problem**: All variables have similar activity
- **Cause**: Too many conflicts, excessive decay
- **Solution**: Decrease decay rate (higher `var_decay`)

**Problem**: Only a few variables have high activity
- **Cause**: Too few conflicts, insufficient mixing
- **Solution**: Check restart strategy

**Problem**: Solver seems stuck on irrelevant variables
- **Cause**: Bad early decisions, not enough restarts
- **Solution**: More aggressive restarts (e.g., Glucose adaptive)

---

## References

### Papers

1. **Original VSIDS** (Chaff, 2001)
   - Moskewicz, Madigan, Zhao, Zhang, Malik
   - "Chaff: Engineering an Efficient SAT Solver"

2. **MiniSat Implementation** (2003)
   - Eén & Sörensson
   - "An Extensible SAT-solver"

3. **Theoretical Analysis** (2009)
   - Pipatsrisawat & Darwiche
   - "On the Power of Clause-Learning SAT Solvers as Resolution Engines"

4. **Modern Variants** (2016+)
   - Liang, Ganesh, Poupart, Czarnecki (MapleSAT)
   - "Learning Rate Based Branching Heuristic for SAT Solvers"

### Source Code

- **MiniSat**: Classic, clean implementation
- **Glucose**: Enhanced with LBD integration
- **Kissat**: Highly optimized C implementation
- **CaDiCaL**: Modern, well-documented

---

## Summary

**VSIDS is essential** for modern CDCL performance:

✅ **Dynamic**: Adapts to problem structure during search
✅ **Efficient**: O(log n) variable selection
✅ **Effective**: 10-1000× speedup over static heuristics
✅ **Universal**: Works well on all problem types

**Key insight**: Focus on variables involved in recent conflicts - they're most likely to lead to progress.

**Implementation**: Binary max-heap with exponential activity decay.

**Tuning**: Decay factor (0.95 default) is main parameter, rarely needs adjustment.

This technique, combined with conflict-driven clause learning and restarts, forms the core of modern SAT solver performance.
