# SAT Solver Documentation Index

This directory contains comprehensive documentation for all major techniques used in the BSAT competition solvers (both Python and C implementations).

---

## Core CDCL Techniques

### 1. [Two-Watched Literals](two_watched_literals.md)
**Topic**: Efficient unit propagation
**Complexity**: O(1) amortized per assignment
**Impact**: 50-100× speedup on propagation (70-80% of solver time)

**Key concepts**:
- Watch exactly 2 literals per clause
- Only check clauses when a watch becomes false
- Avoids scanning all clauses on every assignment

**Implementation**: `competition/python/cdcl_optimized.py`, `competition/c/src/solver.c:588-714`

---

### 2. [Conflict Analysis and Clause Learning](CONFLICT_ANALYSIS.md)
**Topic**: 1-UIP learning scheme and clause minimization
**Impact**: Transforms exponential search to polynomial on many instances

**Key concepts**:
- Analyze implication graph to find 1-UIP (First Unique Implication Point)
- Learn asserting clause that blocks conflict path
- Minimize learned clause to remove redundant literals (10-30% reduction)
- Non-chronological backtracking to second-highest decision level

**Implementation**: `competition/c/src/solver.c:1731-1961`

---

### 3. [VSIDS Variable Ordering](VSIDS.md)
**Topic**: Dynamic variable selection heuristic
**Impact**: 10-1000× speedup over static ordering

**Key concepts**:
- Maintain activity scores for each variable
- Bump activity when variable involved in conflict
- Exponential decay favors recent conflicts (decay factor: 0.95)
- Binary max-heap for O(log n) variable selection

**Implementation**: `competition/c/src/solver.c:716-805`, `competition/c/include/solver.h:239-252`

---

### 4. [LBD and Clause Management](LBD_AND_CLAUSE_MANAGEMENT.md)
**Topic**: Clause quality assessment and database reduction
**Impact**: Prevents memory explosion, keeps high-quality clauses

**Key concepts**:
- LBD (Literal Block Distance) = number of distinct decision levels in clause
- Glue clauses (LBD ≤ 2) are extremely valuable, never delete
- Periodic reduction: Keep best 50% of learned clauses
- On-the-fly subsumption during conflict analysis

**Implementation**: `competition/c/src/solver.c:898-1002`, `competition/c/src/solver.c:1963-1985`

---

## Restart Strategies

### 5. [Adaptive Restarts](adaptive_restarts.md)
**Topic**: Luby sequence vs Glucose adaptive restarts
**Impact**: Escape unproductive search regions

**Key concepts**:
- **Luby**: Fixed geometric sequence (1, 1, 2, 1, 1, 2, 4, 8, ...)
  - Provably optimal for unknown runtime distributions
  - Default: 100 conflicts × Luby value

- **Glucose**: Adaptive based on LBD trends
  - Restart when short-term LBD > long-term LBD × threshold
  - More restarts when stuck, fewer when progressing

**Implementation**: `competition/c/src/solver.c:827-856` (Luby), `competition/c/src/solver.c:1057-1109` (Glucose)

---

## Phase Selection

### 6. [Phase Saving](phase_saving.md)
**Topic**: Remember variable polarities across restarts
**Impact**: Huge performance boost on structured instances

**Key concepts**:
- Save last polarity of each variable
- Reuse saved phase after restart
- Preserves good partial assignments
- Reduces thrashing between similar states

**Implementation**: `competition/c/src/solver.c:1399-1410`, `competition/c/src/solver.c:1502-1525`

---

### 7. [Random Phase Selection](random_phase_selection.md)
**Topic**: Occasional random polarity to prevent stuck states
**Impact**: Prevents catastrophic performance on some instances

**Key concepts**:
- 1% probability of random phase (default)
- Prevents infinite loops and stuck states
- Essential for completeness on some instances

**Implementation**: `competition/c/src/solver.c:1143-1160`

---

### 8. [Adaptive Random Phase](adaptive_random_phase.md)
**Topic**: Boost randomness when solver is stuck
**Impact**: Escapes local minima during search

**Key concepts**:
- Detect stuck state (many conflicts at low decision levels)
- Increase random phase probability to 20% when stuck
- Reset when reaching higher decision levels

**Implementation**: `competition/c/src/solver.c:1143-1160`

---

## Preprocessing

### 9. [Preprocessing Techniques](PREPROCESSING.md)
**Topic**: Formula simplification before search
**Impact**: 10-50% reduction in problem size

**Key concepts**:
- **Unit propagation**: Find and propagate unit clauses at level 0
- **Pure literal elimination**: Assign variables with single polarity
- **Blocked Clause Elimination (BCE)**: Remove irrelevant clauses
- Iterate to fixpoint (changes propagate through formula)

**Implementation**: `competition/c/src/solver.c` (preprocessing phase)

---

## C-Specific Implementation Details

### 10. [Data Structures](../c/docs/DATA_STRUCTURES.md)
**Topic**: Complete reference for C solver data structures
**Content**:
- Core types (Var, Lit, CRef, Level, lbool)
- VarInfo structure (value, level, reason, polarity, activity)
- Trail and decision level tracking
- Arena allocator for compact clause storage
- Watch lists and two-watched literals
- VSIDS heap implementation
- Memory layout examples

---

### 11. [Core Algorithms](../c/docs/CORE_ALGORITHMS.md)
**Topic**: Detailed C implementation of main CDCL algorithms
**Content**:
- Main search loop (solver_solve)
- Unit propagation with two-watched literals
- 1-UIP conflict analysis
- Clause minimization and self-subsumption
- Decision making (VSIDS + phase selection)
- Backtracking algorithm
- LBD computation
- Performance characteristics and complexity analysis

---

### 12. [Restarts and Clause Management](../c/docs/RESTARTS_AND_CLAUSE_MANAGEMENT.md)
**Topic**: C implementation of restart strategies and clause database management
**Content**:
- Luby sequence implementation
- Glucose EMA mode (conservative, α_fast=0.8, α_slow=0.9999)
- Glucose AVG mode (aggressive, window=50, K=0.8)
- Restart postponing technique (Glucose 2.1+)
- Clause database reduction algorithm
- On-the-fly subsumption
- VSIDS activity management
- Performance comparison and tuning guidelines

---

## Results and Analysis

### Benchmark Results
- [benchmark_results.md](benchmark_results.md) - Performance on test suites
- [restart_comparison_results.md](restart_comparison_results.md) - Luby vs Glucose comparison
- [phase_saving_results.md](phase_saving_results.md) - Phase saving impact
- [random_phase_results.md](random_phase_results.md) - Random phase analysis

### Bug Fixes and Debugging
- [BUG_FIX_SUMMARY.md](BUG_FIX_SUMMARY.md) - Summary of bug fixes
- [CONFLICT_ANALYSIS_BUG.md](CONFLICT_ANALYSIS_BUG.md) - Conflict analysis debugging
- [SOUNDNESS_BUG.md](SOUNDNESS_BUG.md) - Soundness issue and fix

### Development Progress
- [progress.md](progress.md) - Week-by-week development log
- [week1_summary.md](week1_summary.md) - Week 1: Two-watched literals
- [inprocessing_findings.md](inprocessing_findings.md) - Inprocessing exploration

---

## Technique Summary Table

| Technique | Impact | Complexity | When to Use |
|-----------|--------|------------|-------------|
| **Two-watched literals** | 50-100× faster propagation | O(1) amortized | Always |
| **1-UIP learning** | Exponential → polynomial | O(graph size) | Always |
| **VSIDS** | 10-1000× vs static | O(log n) per decision | Always |
| **LBD management** | Prevents memory explosion | O(m log m) per reduction | Always |
| **Luby restarts** | Good for random | O(1) per check | Default for random |
| **Glucose restarts** | Good for structured | O(1) per check | Default for industrial |
| **Phase saving** | Huge boost on structured | O(1) | Always |
| **Random phase** | Prevents stuck states | O(1) | Always (1% default) |
| **Adaptive random** | Escapes local minima | O(1) | When stuck detection needed |
| **Preprocessing** | 10-50% reduction | O(m²) worst case | Structured instances |

---

## Implementation Comparison

### Python vs C

| Feature | Python Location | C Location | Notes |
|---------|----------------|------------|-------|
| Two-watched literals | `competition/python/cdcl_optimized.py` | `src/solver.c:588-714` | C is 10-100× faster |
| Conflict analysis | `competition/python/cdcl_optimized.py` | `src/solver.c:1731-1961` | Same algorithm |
| VSIDS | `competition/python/cdcl_optimized.py` | `src/solver.c:716-805` | C uses manual heap |
| LBD computation | `competition/python/cdcl_optimized.py` | `src/solver.c:1963-1985` | Same algorithm |
| Glucose restarts | `competition/python/cdcl_optimized.py` | `src/solver.c:1057-1109` | Dual mode in C |
| Phase saving | `competition/python/cdcl_optimized.py` | `src/solver.c:1399-1410` | Same approach |

**Performance**: C solver is 10-400× faster than Python on most instances

---

## Learning Path

### Beginner: Start Here
1. [Two-Watched Literals](two_watched_literals.md) - Most important optimization
2. [VSIDS](VSIDS.md) - Variable ordering
3. [Adaptive Restarts](adaptive_restarts.md) - Escape bad search regions

### Intermediate
4. [Conflict Analysis](CONFLICT_ANALYSIS.md) - 1-UIP learning and minimization
5. [LBD and Clause Management](LBD_AND_CLAUSE_MANAGEMENT.md) - Database reduction
6. [Phase Saving](phase_saving.md) - Remember polarities

### Advanced
7. [Preprocessing](PREPROCESSING.md) - Formula simplification
8. [C Data Structures](../c/docs/DATA_STRUCTURES.md) - Compact memory layout
9. [C Core Algorithms](../c/docs/CORE_ALGORITHMS.md) - Implementation details
10. [C Restarts and Management](../c/docs/RESTARTS_AND_CLAUSE_MANAGEMENT.md) - Advanced tuning

---

## External Resources

### Essential Papers
1. **GRASP** (Marques-Silva & Sakallah, 1996) - Original CDCL
2. **Chaff** (Moskewicz et al., 2001) - Two-watched literals, VSIDS
3. **MiniSat** (Eén & Sörensson, 2003) - Clean modern implementation
4. **Glucose** (Audemard & Simon, 2009) - LBD and adaptive restarts
5. **Handbook of Satisfiability** (2009) - Comprehensive reference

### Source Code
- **MiniSat**: https://github.com/niklasso/minisat (educational, clean)
- **Glucose**: http://www.labri.fr/perso/lsimon/glucose/ (competition-level)
- **Kissat**: https://github.com/arminbiere/kissat (highly optimized)
- **CaDiCaL**: https://github.com/arminbiere/cadical (well-documented)

### Competitions
- **SAT Competition**: http://www.satcompetition.org/
- **SAT Race**: http://sat-race.org/

---

## Quick Reference

### Common Questions

**Q: What's the single most important optimization?**
A: Two-watched literals. Without it, competition-level solving is impossible.

**Q: What restart strategy should I use?**
A: Luby (default) for universal good performance. Glucose for structured instances. Both are implemented.

**Q: How often should I reduce the clause database?**
A: Every 2000 conflicts (default). Adjust based on memory constraints.

**Q: What's a good LBD threshold for glue clauses?**
A: 2 (standard in Glucose). Never delete clauses with LBD ≤ 2.

**Q: Should I use preprocessing?**
A: Yes, always do unit propagation and pure literals (cheap). BCE is good for structured instances but expensive.

**Q: What's the difference between Python and C solvers?**
A: Same algorithms, but C is 10-400× faster due to memory layout and compiled code.

---

## Contributing

When adding new techniques, please:
1. Create a new .md file in this directory
2. Follow the existing format (Overview, Algorithm, Example, Performance, References)
3. Update this README.md index
4. Include code locations and complexity analysis
5. Add benchmark results if available

---

## Status

**Last Updated**: October 2025

**Documentation Completeness**: ✅ All major techniques documented

**C Solver Status**: ✅ Production ready (100% test success rate)

**Python Solver Status**: ✅ Reference implementation complete

**Next Steps**:
- Performance tuning for SAT competition instances
- Additional preprocessing techniques (variable elimination, failed literal probing)
- Parallel solving (portfolio approach)
