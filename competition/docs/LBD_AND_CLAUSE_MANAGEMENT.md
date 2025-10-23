# LBD and Clause Database Management

**Technique**: Literal Block Distance (LBD) for clause quality assessment
**Impact**: Prevents memory explosion, keeps high-quality clauses
**Status**: ✅ Implemented in both Python and C solvers

---

## Overview

Modern CDCL solvers learn thousands to millions of clauses during search. Without management, this leads to:
- Memory explosion (GB to TB)
- Slower propagation (more clauses to check)
- Cache thrashing (poor locality)

**Solution**: The **Glucose** algorithm uses **LBD (Literal Block Distance)** to measure clause quality and delete low-quality clauses.

**Key Idea**: Clauses with low LBD (spanning few decision levels) are more useful than clauses with high LBD (spanning many levels).

---

## The Problem: Clause Database Growth

### Naive Clause Learning

**Behavior**: Keep all learned clauses forever
```
Conflict 1:    Learn clause C1  → 1 clause
Conflict 100:  Learn clause C100 → 100 clauses
Conflict 1000: Learn clause C1000 → 1000 clauses
Conflict 10000: → 10,000 clauses (megabytes of memory)
Conflict 100000: → 100,000 clauses (hundreds of MB)
```

**Problems**:
1. **Memory**: Unlimited growth, eventually runs out of RAM
2. **Performance**: Slower propagation (more clauses to watch/check)
3. **Quality**: Most learned clauses are never used again

**Observation** (Glucose, 2009): Only ~10-20% of learned clauses are actually useful!

---

## LBD: Literal Block Distance

### Definition

**LBD of a clause** = Number of distinct decision levels in the clause

**Example**:
```
Clause: (¬x1 ∨ ¬x2 ∨ ¬x3 ∨ ¬x4)

Assignment levels:
  x1 assigned at level 1
  x2 assigned at level 1
  x3 assigned at level 2
  x4 assigned at level 3

Distinct levels: {1, 2, 3}
LBD = 3
```

### Interpretation

**Low LBD** (e.g., 1-2):
- Clause spans few decision levels
- Literals are "closely related" in search tree
- High quality, likely to be reused
- **Keep these clauses!**

**High LBD** (e.g., 10+):
- Clause spans many decision levels
- Literals are "loosely related"
- Low quality, rarely reused
- **Candidates for deletion**

**Glue clauses** (LBD ≤ 2):
- Extremely high quality
- "Glue" together different parts of search
- **NEVER delete**

---

## Computing LBD

### Algorithm

```c
uint32_t compute_lbd(Clause* clause, VarInfo* vars) {
    static bool level_seen[256];  // Reusable array
    memset(level_seen, 0, sizeof(level_seen));

    uint32_t lbd = 0;

    for (uint32_t i = 0; i < clause_size(clause); i++) {
        Lit lit = clause_lits(clause)[i];
        Level level = vars[var(lit)].level;

        if (!level_seen[level]) {
            level_seen[level] = true;
            lbd++;
        }
    }

    return lbd;
}
```

**When computed**: During conflict analysis, when learning a new clause

**Complexity**: O(n) where n = clause length

**Storage**: LBD stored in clause header (8 bits, max 255)

---

## Clause Database Reduction

### When to Reduce

**Trigger**: Every N conflicts (default: N = 2000)

```c
if (stats.conflicts % opts.reduce_interval == 0) {
    solver_reduce_db(solver);
}
```

**Alternative triggers**:
- Database size exceeds threshold
- Memory usage exceeds limit
- Propagation becomes slow

### Reduction Algorithm

**Goal**: Keep highest quality clauses, delete the rest

**Steps**:

1. **Sort clauses** by quality (LBD ascending, activity descending)
   ```c
   qsort(learnts, num_learnts, sizeof(CRef), compare_clauses);
   ```

2. **Determine keep threshold** (default: keep best 50%)
   ```c
   uint32_t keep_count = num_learnts * reduce_fraction;  // 0.5
   ```

3. **Delete low-quality clauses**
   ```c
   for (uint32_t i = keep_count; i < num_learnts; i++) {
       if (clause_lbd(learnts[i]) > glue_lbd) {  // Don't delete glue!
           clause_set_deleted(learnts[i], true);
           deleted++;
       }
   }
   ```

4. **Protect glue clauses**
   ```c
   // NEVER delete clauses with LBD ≤ 2
   if (clause_lbd(cr) <= opts.glue_lbd) {
       continue;  // Keep this clause
   }
   ```

5. **Compact array** (remove deleted clauses)
   ```c
   uint32_t j = 0;
   for (uint32_t i = 0; i < num_learnts; i++) {
       if (!clause_deleted(learnts[i])) {
           learnts[j++] = learnts[i];
       }
   }
   num_learnts = j;
   ```

### Sorting Key

**Primary key**: LBD (lower is better)
**Secondary key**: Activity (higher is better)

```c
int compare_clauses(const void* a, const void* b) {
    CRef cr1 = *(CRef*)a;
    CRef cr2 = *(CRef*)b;

    uint32_t lbd1 = clause_lbd(cr1);
    uint32_t lbd2 = clause_lbd(cr2);

    if (lbd1 < lbd2) return -1;  // cr1 is better
    if (lbd1 > lbd2) return 1;   // cr2 is better

    // LBD equal, tie-break by activity
    uint32_t act1 = clause_activity(cr1);
    uint32_t act2 = clause_activity(cr2);

    if (act1 > act2) return -1;  // cr1 more active
    if (act1 < act2) return 1;   // cr2 more active

    return 0;
}
```

---

## Clause Activity

### What is Activity?

**Activity** = Measure of recent usage in conflicts

**Bumping**: Increase activity when clause is used in conflict analysis

```c
void bump_clause_activity(CRef cr) {
    uint32_t activity = clause_activity(cr);
    activity += clause_inc;

    clause_set_activity(cr, activity);

    if (activity > UINT32_MAX / 2) {
        rescale_clause_activities();
    }
}
```

**Decay**: Periodically increase increment to favor recent usage

```c
void decay_clause_activity() {
    clause_inc *= (1.0 / clause_decay);  // Default: 1/0.999
}
```

**Effect**: Recently used clauses are less likely to be deleted

---

## Protection Rules

### 1. Glue Clauses (LBD ≤ 2)

**Rule**: NEVER delete glue clauses

```c
if (clause_lbd(cr) <= opts.glue_lbd) {  // Default: glue_lbd = 2
    continue;  // Skip deletion
}
```

**Reason**: Glue clauses are extremely high quality
- Span only 1-2 decision levels
- Almost always reused
- Critical for performance

**Impact**: ~5-10% of learned clauses are glue clauses

### 2. Low LBD Clauses (LBD ≤ 5)

**Rule**: Highly protected, usually kept

**Reason**: Still high quality, worth keeping

**Impact**: ~20-30% of learned clauses

### 3. High Activity Clauses

**Rule**: Recent conflicts boost activity → more likely to keep

**Reason**: Recently used clauses may be relevant to current search

---

## Tuning Parameters

### 1. Reduce Interval (`reduce_interval`)

**Default**: 2000 conflicts

**Effect**:
- **Smaller interval** (e.g., 1000): More frequent reduction
  - Pros: Lower memory, faster propagation
  - Cons: May delete useful clauses too early

- **Larger interval** (e.g., 5000): Less frequent reduction
  - Pros: More clauses available for search
  - Cons: Higher memory, slower propagation

**Recommendation**: 2000-3000 for most problems

### 2. Reduce Fraction (`reduce_fraction`)

**Default**: 0.5 (keep 50%)

**Effect**:
- **Lower fraction** (e.g., 0.3): Keep fewer clauses
  - Pros: Aggressive memory reduction
  - Cons: May delete useful clauses

- **Higher fraction** (e.g., 0.7): Keep more clauses
  - Pros: More clauses available
  - Cons: Higher memory usage

**Recommendation**: 0.4-0.6

### 3. Glue LBD Threshold (`glue_lbd`)

**Default**: 2

**Effect**:
- **Lower threshold** (e.g., 1): Only protect LBD=1 clauses
  - Pros: More aggressive deletion
  - Cons: May delete valuable LBD=2 clauses

- **Higher threshold** (e.g., 3): Protect up to LBD=3
  - Pros: Keep more high-quality clauses
  - Cons: Less memory savings

**Recommendation**: 2 (standard in Glucose)

### 4. Max LBD (`max_lbd`)

**Default**: 30

**Effect**: Don't learn clauses with LBD > max_lbd

**Reason**: Very high LBD clauses are almost never useful

**Recommendation**: 20-30

---

## Performance Impact

### Memory Usage

**Without reduction**:
- After 100k conflicts: ~500MB (50k learned clauses)
- Growth rate: ~5KB per conflict
- Eventually: Out of memory

**With reduction** (default settings):
- After 100k conflicts: ~50MB (5k learned clauses)
- Kept clauses: High-quality (low LBD, high activity)
- Memory savings: **~90%**

### Solving Performance

**Impact on time**:
- Reduction overhead: ~5% (sorting + compaction)
- Propagation speedup: Better cache locality
- Overall: **Net performance gain**

**Impact on success rate**:
- Keeps high-quality clauses → No degradation
- Sometimes improves (better focus on good clauses)

---

## On-the-Fly Subsumption

### What is Subsumption?

**Definition**: Clause C1 subsumes C2 if all literals of C1 are in C2

**Example**:
```
C1: (¬x1 ∨ ¬x2)
C2: (¬x1 ∨ ¬x2 ∨ ¬x3)

C1 subsumes C2 (C1 is shorter and stronger)
→ C2 is redundant, can be deleted
```

### Implementation

**When**: During conflict analysis

**Algorithm**:
```c
// While building learned clause
for (each literal in learned_clause) {
    CRef reason = vars[var(literal)].reason;

    if (reason != INVALID_CREF) {
        // Check if learned clause subsumes reason
        if (all_literals_in_reason_are_in_learned_clause(reason)) {
            // Mark reason for deletion
            clause_set_deleted(reason, true);
            stats.subsumed_clauses++;
        }
    }
}
```

**Impact**: Reduces database size by ~5-10% at no extra cost

---

## Implementation Example

### Complete Reduction Function

```c
void solver_reduce_db(Solver* s) {
    // 1. Sort learned clauses
    qsort(s->learnts, s->num_learnts, sizeof(CRef), compare_clauses);

    // 2. Determine keep threshold
    uint32_t keep_count = (uint32_t)(s->num_learnts * s->opts.reduce_fraction);

    // 3. Delete low-quality clauses
    uint32_t deleted = 0;
    for (uint32_t i = keep_count; i < s->num_learnts; i++) {
        CRef cr = s->learnts[i];

        // Protect glue clauses
        if (clause_lbd(cr) <= s->opts.glue_lbd) {
            continue;
        }

        // Mark for deletion
        clause_set_deleted(cr, true);
        deleted++;
        s->stats.deleted_clauses++;
    }

    // 4. Compact learnt clause array
    uint32_t j = 0;
    for (uint32_t i = 0; i < s->num_learnts; i++) {
        CRef cr = s->learnts[i];
        if (!clause_deleted(cr)) {
            s->learnts[j++] = cr;
        }
    }
    s->num_learnts = j;

    s->stats.reduces++;
}
```

### Integration with CDCL

```c
lbool solver_solve(Solver* s) {
    while (true) {
        // Propagate
        CRef conflict = solver_propagate(s);

        if (conflict != INVALID_CREF) {
            // Conflict analysis
            Lit learnt[MAX_CLAUSE_SIZE];
            uint32_t learnt_size;
            Level bt_level;

            solver_analyze(s, conflict, learnt, &learnt_size, &bt_level);

            // Compute LBD
            uint32_t lbd = compute_lbd(learnt, learnt_size, s->vars);

            // Add clause with LBD
            if (lbd <= s->opts.max_lbd) {
                CRef cr = add_clause(s->arena, learnt, learnt_size);
                clause_set_lbd(cr, lbd);
                clause_set_learnt(cr, true);

                if (lbd <= s->opts.glue_lbd) {
                    clause_set_glue(cr, true);  // Protect from deletion
                }

                add_to_learnt_db(s, cr);
            }

            // Periodic reduction
            if (s->stats.conflicts % s->opts.reduce_interval == 0) {
                solver_reduce_db(s);
            }

            // Backtrack and continue
            solver_backtrack(s, bt_level);
            enqueue(s, learnt[0], cr);
        }

        // ... (rest of CDCL loop)
    }
}
```

---

## Advanced Techniques

### 1. Geometric Series (Glucose 2.0+)

**Idea**: Reduce more aggressively over time

```c
if (num_learnts > reduce_base * reduce_multiplier) {
    solver_reduce_db();
    reduce_multiplier *= 1.1;  // Geometric growth
}
```

**Effect**: More clauses kept early (exploration), fewer kept later (exploitation)

### 2. Tier-Based Management (Glucose 4.0+)

**Idea**: Classify clauses into tiers based on LBD

- **Tier 1** (LBD ≤ 2): Glue clauses, never delete
- **Tier 2** (LBD 3-5): High quality, rarely delete
- **Tier 3** (LBD > 5): Normal, delete regularly

**Effect**: More nuanced deletion policy

### 3. Dynamic LBD Update

**Idea**: Recompute LBD when clause is used

```c
if (clause_used_in_conflict(cr)) {
    uint32_t new_lbd = compute_lbd(cr, current_assignment);
    if (new_lbd < clause_lbd(cr)) {
        clause_set_lbd(cr, new_lbd);  // Improve quality
    }
}
```

**Effect**: Clauses that improve over time are protected

---

## Statistics and Debugging

### Key Metrics

```c
struct {
    uint64_t learned_clauses;    // Total learned
    uint64_t deleted_clauses;    // Total deleted
    uint64_t glue_clauses;       // Glue clauses (LBD ≤ 2)
    uint64_t reduces;            // Number of reductions
    uint32_t max_lbd;            // Maximum LBD seen
} stats;
```

### Analysis

**Healthy solver**:
- 5-10% of learned clauses are glue clauses
- 50% of clauses deleted during each reduction
- Average LBD: 5-10
- Max LBD: 20-30

**Problematic patterns**:
- Very few glue clauses (< 1%): Bad problem structure or poor search
- High average LBD (> 15): Search is unfocused
- Many clauses never deleted: Reduction not working

---

## References

### Papers

1. **Original Glucose** (2009)
   - Audemard & Simon
   - "Predicting Learnt Clauses Quality in Modern SAT Solvers"

2. **Glucose 2.1** (2012)
   - Audemard & Simon
   - "Refining Restarts Strategies for SAT and UNSAT"

3. **Theoretical Analysis** (2012)
   - Audemard & Simon
   - "Glucose 2.1: Aggressive but Reactive Clause Database Management"

### Implementation

- **Glucose source**: http://www.labri.fr/perso/lsimon/glucose/
- **MiniSat**: Early clause deletion strategy (less sophisticated)
- **Kissat**: Highly optimized Glucose-style management

---

## Summary

**LBD-based clause management** is essential for modern CDCL:

✅ **Prevents memory explosion**: Keeps database size manageable
✅ **Improves performance**: Better cache locality, faster propagation
✅ **Maintains quality**: Keeps high-quality clauses, deletes junk
✅ **Simple metric**: LBD is easy to compute and effective

**Key principles**:
- LBD measures clause quality (lower is better)
- Glue clauses (LBD ≤ 2) are precious, never delete
- Periodic reduction keeps database size bounded
- Activity-based tiebreaking favors recently used clauses

**Implementation**: Integrated into conflict analysis and main CDCL loop, minimal overhead (~5%).

This technique, combined with VSIDS and adaptive restarts, enables solving of industrial-scale SAT problems (millions of variables and clauses).
