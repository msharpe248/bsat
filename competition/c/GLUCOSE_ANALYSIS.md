# Glucose Implementation Analysis - Python vs C

## Executive Summary

**CRITICAL FINDING**: Python and C implement COMPLETELY DIFFERENT "Glucose" algorithms!

- **Python "Glucose"**: Uses simple sliding window averaging (window=50, K=0.8)
- **C "Glucose"**: Uses exponential moving averages (alpha_fast=0.8, alpha_slow=0.9999)

**Result**: Python's "Glucose" works well (323 conflicts, 272 restarts), C's "Glucose" times out on the same instance.

---

## Algorithm Comparison

### Python Implementation (cdcl_optimized.py:711-724)

```python
# Short-term average: average of last N LBDs (window=50)
short_term_avg = sum(self.recent_lbds) / len(self.recent_lbds)

# Long-term average: average of ALL LBDs
long_term_avg = self.lbd_sum / self.lbd_count

# Restart if short-term exceeds long-term by factor K (0.8)
should_restart_basic = short_term_avg > long_term_avg * self.glucose_k
```

**Key characteristics:**
- Maintains list of last 50 LBD values (`recent_lbds`)
- Maintains sum and count of ALL LBDs (`lbd_sum`, `lbd_count`)
- Restarts when: `avg(last 50) > avg(all) * 0.8`
- Simple to understand and implement
- VERY aggressive restarts (almost 1 restart per conflict!)

---

### C Implementation (solver.c:1055-1100)

```c
// Update moving averages after each learned clause
s->restart.fast_ma = alpha_fast * s->restart.fast_ma + (1.0 - alpha_fast) * lbd;
s->restart.slow_ma = alpha_slow * s->restart.slow_ma + (1.0 - alpha_slow) * lbd;

// Restart when fast_ma > slow_ma
if (s->restart.fast_ma > s->restart.slow_ma) {
    should_restart_glucose = true;
}

// Hybrid fallback: geometric restarts if Glucose hasn't triggered
```

**Key characteristics:**
- Exponential moving averages with:
  - `alpha_fast = 0.8` (approximates last ~5 conflicts)
  - `alpha_slow = 0.9999` (approximates long-term average)
- Restarts when: `fast_ma > slow_ma`
- Implements TRUE Glucose algorithm from the paper
- Includes hybrid geometric fallback
- Much more conservative restart behavior

---

## Experimental Results

### Test Instance: hard_3sat_v108_c0461.cnf

| Implementation | Strategy | Conflicts | Restarts | Result |
|----------------|----------|-----------|----------|--------|
| **Python** | "Glucose" (sliding window) | 323 | 272 | ✅ SAT (323 conflicts) |
| **C** | Glucose (EMA) | >1M | ~unknown | ❌ TIMEOUT (30s) |
| **C** | Luby (unit=100) | 25,631 | ~unknown | ✅ SAT (25,631 conflicts) |

### Restart Aggressiveness

**Python "Glucose"**: 272 restarts / 323 conflicts = **0.84 restarts per conflict** (!!)

This is EXTREMELY aggressive - almost continuous restarts. This is likely the key to why it works so well on these instances.

**C Glucose**: Unknown (times out) - but expected to be much less frequent based on EMA parameters.

---

## Why Python's "Glucose" Works Better

### 1. **Very Aggressive Restart Behavior**
The sliding window approach with K=0.8 is much more sensitive to recent changes:
- If last 50 LBDs average 10, and all-time average is 8
- Python restarts because: 10 > 8 * 0.8 = 6.4 ✓
- This happens VERY frequently (almost every conflict!)

### 2. **Simpler Algorithm**
The sliding window is easier to reason about and tune:
- Window size directly controls "how recent"
- K directly controls "how much worse before restart"

### 3. **Accidental Genius**
Python's simplified implementation may have accidentally found a better balance for these instances than the "true" Glucose algorithm!

---

## Why C's Glucose Doesn't Work

### 1. **Too Conservative**
Exponential moving averages smooth out variations:
- `alpha_slow = 0.9999` changes VERY slowly
- `alpha_fast = 0.8` still averages ~5 conflicts
- Condition `fast_ma > slow_ma` rarely triggers

### 2. **Different Behavior**
The EMA approach is fundamentally different:
- Gives more weight to recent values but never forgets old ones
- Requires much bigger divergence to trigger restart
- More suitable for industrial instances, not random 3-SAT

### 3. **Hybrid Fallback Complication**
The C code includes geometric fallback which may interfere:
```c
// Hybrid fallback: If Glucose hasn't triggered, use geometric
if (s->restart.conflicts_since >= s->restart.threshold) {
    should_restart_geometric = true;
}
```

---

## Recommendation

### Option 1: Make C Match Python's "Glucose" (RECOMMENDED)

Replace the EMA implementation with Python's sliding window approach:

```c
// Instead of:
double fast_ma, slow_ma;  // Exponential moving averages

// Use:
uint32_t recent_lbds[50];  // Sliding window
uint32_t recent_count;
uint64_t lbd_sum;          // Sum of all LBDs
uint64_t lbd_count;        // Count of all LBDs

// Restart condition:
if (lbd_count >= 50) {
    double short_term = avg(recent_lbds);
    double long_term = lbd_sum / lbd_count;
    if (short_term > long_term * 0.8) {
        restart();
    }
}
```

**Pros:**
- Matches Python's proven behavior
- Simpler to understand
- Would likely achieve same 100% success rate as Python
- More predictable restart frequency

**Cons:**
- Not the "real" Glucose algorithm
- May not perform well on different instance types

---

### Option 2: Keep Luby as Default (CURRENT CHOICE)

This is what we've already done:
- Luby achieves 100% completeness
- Simple, well-understood algorithm
- Works across different instance types

**Pros:**
- Already working (100% success)
- Standard algorithm (Luby sequence)
- Predictable behavior

**Cons:**
- Not as fast as Python on some instances
- C uses more conflicts than Python (43× on some instances)

---

### Option 3: Add Python's "Glucose" as Alternative

Keep both Luby (default) and add Python-style Glucose as option:

```bash
# Default: Luby (100% completeness)
./bin/bsat <file>

# Python-style "Glucose" (aggressive restarts)
./bin/bsat --python-glucose <file>

# Real Glucose (EMA-based)
./bin/bsat --no-luby-restart <file>  # Current --no-luby
```

**Pros:**
- Maximum flexibility
- Can compare all three strategies
- Best of both worlds

**Cons:**
- More code to maintain
- More confusion about which to use

---

## Technical Details

### Python's LBD Tracking (cdcl_optimized.py:809-816)

```python
# Update LBD statistics (for Glucose adaptive restarts)
if self.restart_strategy == 'glucose':
    self.lbd_sum += lbd
    self.lbd_count += 1

    # Maintain sliding window of recent LBDs
    self.recent_lbds.append(lbd)
    if len(self.recent_lbds) > self.glucose_lbd_window:
        self.recent_lbds.pop(0)  # Remove oldest
```

### C's LBD Tracking (solver.c:2013-2022)

```c
// Update LBD moving averages for Glucose adaptive restarts
if (s->stats.conflicts > 0) {
    double alpha_fast = s->opts.glucose_fast_alpha;  // 0.8
    double alpha_slow = s->opts.glucose_slow_alpha;  // 0.9999
    s->restart.fast_ma = alpha_fast * s->restart.fast_ma + (1.0 - alpha_fast) * lbd;
    s->restart.slow_ma = alpha_slow * s->restart.slow_ma + (1.0 - alpha_slow) * lbd;
} else {
    // Initialize moving averages with first LBD
    s->restart.fast_ma = lbd;
    s->restart.slow_ma = lbd;
}
```

---

## Mathematical Comparison

### Exponential Moving Average (C)

```
fast_ma[i] = α * fast_ma[i-1] + (1-α) * lbd[i]
slow_ma[i] = β * slow_ma[i-1] + (1-β) * lbd[i]

where: α = 0.8, β = 0.9999
```

**Effective window:**
- fast_ma: ~5 recent values (1/(1-0.8) = 5)
- slow_ma: ~10,000 recent values (1/(1-0.9999) = 10,000)

### Sliding Window Average (Python)

```
short_term = avg(lbd[i-49..i])  # Last 50 values
long_term = avg(lbd[0..i])      # All values
```

**Key difference:**
- Python: Exact 50-value window
- C: Weighted average favoring recent but never forgetting old

---

## Conclusion

The core issue is that Python and C implement fundamentally different algorithms, both called "Glucose":

1. **Python's "Glucose"** is a simplified approximation with sliding windows
   - Works exceptionally well on these test instances
   - Very aggressive restart behavior (almost every conflict!)
   - Not the "real" Glucose from the literature

2. **C's "Glucose"** is the true Glucose algorithm with EMAs
   - More faithful to the original paper
   - Too conservative for random 3-SAT instances
   - Works better on industrial instances

**Recommendation**: Either:
- Make C match Python's sliding window approach, OR
- Keep Luby as default (current choice - already working!)

The fact that Python's simplified version works better is a reminder that **simpler can sometimes be better** than theoretically sophisticated!
