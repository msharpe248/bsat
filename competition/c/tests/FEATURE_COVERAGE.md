# Feature Test Coverage Matrix

## All Implemented Features vs. Test Coverage

| # | Feature | Status | Testable? | Test Method | Stats to Check | Current Test |
|---|---------|--------|-----------|-------------|----------------|--------------|
| **Core CDCL** |
| 1 | Two-Watched Literals | ✅ | ⚠️ Indirect | Correctness + performance | propagations | test_unit_propagation |
| 2 | First UIP Learning | ✅ | ✅ Yes | Conflicts generate clauses | conflicts, learned_clauses | test_clause_learning |
| 3 | VSIDS Heuristic | ✅ | ✅ Yes | Decisions made | decisions | test_vsids_decisions |
| 4 | Non-chronological Backtracking | ✅ | ⚠️ Indirect | Solve complex instances | conflicts | test_clause_learning |
| 5 | Binary Clause Optimization | ✅ | ⚠️ Indirect | Memory efficiency | N/A | (via correctness) |
| **Preprocessing** |
| 6 | Blocked Clause Elimination (BCE) | ✅ | ✅ Yes | Clauses eliminated | blocked_clauses | test_bce_preprocessing |
| **Search Optimizations** |
| 7 | LBD Calculation | ✅ | ✅ Yes | Max LBD tracked | max_lbd | test_lbd_calculation |
| 8 | Clause Database Reduction | ✅ | ✅ Yes | Learned clauses deleted | deleted_clauses | ❌ MISSING |
| 9 | Glue Clause Protection | ✅ | ✅ Yes | Glue clauses tracked | glue_clauses | ❌ MISSING |
| 10 | Phase Saving | ✅ | ⚠️ Hard | Check polarity memory | N/A | ❌ MISSING |
| 11 | Random Phase Selection | ✅ | ⚠️ Hard | Statistical variance | N/A | ❌ MISSING |
| 12 | Adaptive Random Phase | ✅ | ⚠️ Hard | Boost when stuck | N/A | ❌ MISSING |
| 13 | Hybrid Restarts | ✅ | ✅ Yes | Restarts occur | restarts | test_restarts |
| 14 | Restart Postponing | ✅ | ⚠️ Hard | Need internal state | N/A | ❌ MISSING |
| 15 | On-the-Fly Subsumption | ✅ | ✅ Yes | Clauses subsumed | subsumed_clauses | test_subsumption |
| 16 | Recursive Clause Minimization | ✅ | ✅ Yes | Literals minimized | minimized_literals | test_clause_minimization |
| 17 | Vivification | ✅ | ✅ Yes | With --inprocess flag | N/A | ❌ MISSING |
| 18 | Chronological Backtracking | ✅ | ⚠️ Hard | Need internal state | N/A | ❌ MISSING |

## Legend

**Testability**:
- ✅ **Yes**: Can be tested via statistics or direct observation
- ⚠️ **Indirect**: Can be tested indirectly (correctness, performance)
- ⚠️ **Hard**: Requires internal state inspection or complex setup

**Status**:
- ✅ **Has Test**: Feature has dedicated test
- ❌ **MISSING**: Feature implemented but no test yet

## Summary

### Implemented Features: 18
### Directly Testable: 11
### Has Tests: 8
### Missing Tests: 3 (high priority)

## Missing High-Priority Tests

### 1. Clause Database Reduction
**Why Important**: Core CDCL optimization for long-running instances

**Test Strategy**:
```c
void test_database_reduction() {
    // Use instance that generates many learned clauses
    // Check that deleted_clauses > 0
    assert(s->stats.learned_clauses > 10000);  // Trigger reduction
    assert(s->stats.deleted_clauses > 0);
}
```

**Good Instance**: Medium/hard random 3-SAT requiring long search

**Stats**: `deleted_clauses > 0`

---

### 2. Glue Clause Protection
**Why Important**: Ensures quality clauses (LBD ≤ 2) are never deleted

**Test Strategy**:
```c
void test_glue_clause_protection() {
    // Solve instance that generates glue clauses
    // Check that glue_clauses > 0
    // Verify they weren't deleted (indirect check)
    assert(s->stats.glue_clauses > 0);
}
```

**Good Instance**: Structured SAT instances (graph coloring, planning)

**Stats**: `glue_clauses > 0`

---

### 3. Vivification (--inprocess)
**Why Important**: Advanced inprocessing optimization

**Test Strategy**:
```c
void test_vivification() {
    // Create solver with inprocess option
    SolverOpts opts = default_opts();
    opts.inprocess_freq = 1000;  // Enable vivification

    Solver* s = solver_new_with_opts(&opts);
    // Solve instance
    // Check vivification occurred (need stat)
}
```

**Good Instance**: Industrial instances with redundancy

**Stats**: Need vivification stat (currently missing?)

---

## Tests That Are Hard to Implement

### Phase Saving
**Challenge**: Need to track variable polarities across multiple restarts

**Possible Approach**:
- Solve same instance twice
- Check if decisions are more efficient in second solve
- Very indirect, hard to make deterministic

### Random Phase Selection
**Challenge**: Testing randomness requires statistical methods

**Possible Approach**:
- Run multiple times with same seed
- Check for determinism
- Run with different random rates, check decision variance

### Chronological Backtracking
**Challenge**: Need internal backtrack level information

**Possible Approach**:
- Compare with/without chronological BT
- Check decision/conflict ratio changes
- Requires ability to disable feature

## Recommendations

### Priority 1: Add Missing Direct Tests
1. **Database Reduction** - Easy to add, just needs harder instance
2. **Glue Clause Protection** - Easy to add, check glue_clauses stat
3. **Vivification** - Add if stat available, use --inprocess flag

### Priority 2: Document Indirect Testing
Some features are tested indirectly through:
- **Correctness tests**: Two-watched literals, binary clause opt
- **Performance tests**: Chronological BT, phase saving
- **Comparison tests**: With/without feature flags

### Priority 3: Add Feature Flags
To better test features, consider adding flags:
- `--no-phase-saving`: Disable phase saving
- `--no-chrono-bt`: Disable chronological backtracking
- `--no-glue-protection`: Disable glue clause protection

Then test: solving time/conflicts with vs. without each feature.

## Test Instance Requirements

| Feature | Instance Type | Min Size | Why |
|---------|---------------|----------|-----|
| Learning | Random 3-SAT | 5 vars | Requires conflicts |
| VSIDS | Unconstrained SAT | 10 vars | Multiple valid decisions |
| Restarts | Hard SAT/UNSAT | 50+ vars | Long search |
| LBD | Structured instances | 20+ vars | Varied decision levels |
| DB Reduction | Long-running | 100+ vars | Generate >10k learned clauses |
| Glue Clauses | Structured | 50+ vars | Generate low-LBD clauses |
| BCE | Specific structure | Any | Contains blocked clauses |
| Subsumption | Redundant clauses | Any | Subsumption opportunities |
| Minimization | Any with conflicts | 10+ vars | Learned clauses to minimize |

## Next Steps

1. **Add 3 missing high-priority tests** to `test_features.c`
2. **Use dataset instances** instead of fixtures (they're too simple)
3. **Document expected behavior** for each test
4. **Add feature flag tests** if possible (with/without comparisons)
5. **Create integration tests** that verify features work together

## Related Documentation

- See `FEATURE_TESTING.md` for how to choose test instances
- See `test_features.c` for current implementation
- See `../README.md` for feature descriptions

Last updated: 2025-10-21
