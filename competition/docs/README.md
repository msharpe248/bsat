# SAT Solver Documentation Index

This directory explains the core CDCL techniques used in the BSAT competition solvers and keeps the benchmark reports, bug write-ups, and progress logs from the Python prototype.

**Scope and age.** Almost everything here was written in October 2025 for the Python prototype (`competition/python/`) and the first C prototype. The technique explanations remain valid as introductions, but their code blocks are simplified illustrations and the reports are historical records. The current C solver (`competition/c/`) has many features not covered here (VMTF/VSIDS mode alternation, inprocessing, GF(2) Gauss, symmetry breaking, proofs, portfolio, ...); its authoritative documentation is [`../c/README.md`](../c/README.md) and [`../c/FEATURES.md`](../c/FEATURES.md), with design notes and experiment reports in [`../c/docs/`](../c/docs/).

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

**Implementation**: `competition/python/cdcl_optimized.py` (`WatchedLiteralManager`); C: `solver_propagate` in `competition/c/src/solver.c` and `competition/c/src/watch.c`

---

### 2. [Conflict Analysis and Clause Learning](CONFLICT_ANALYSIS.md)
**Topic**: 1-UIP learning scheme and clause minimization
**Impact**: Transforms exponential search to polynomial on many instances

**Key concepts**:
- Analyze implication graph to find 1-UIP (First Unique Implication Point)
- Learn asserting clause that blocks conflict path
- Minimize learned clause to remove redundant literals (10-30% reduction)
- Non-chronological backtracking to second-highest decision level

**Implementation**: Python: `_analyze_conflict`, `_minimize_clause`; C: `solver_analyze` in `competition/c/src/solver.c` and `competition/c/src/minimize.c`

---

### 3. [VSIDS Variable Ordering](VSIDS.md)
**Topic**: Dynamic variable selection heuristic
**Impact**: 10-1000× speedup over static ordering

**Key concepts**:
- Maintain activity scores for each variable
- Bump activity when variable involved in conflict
- Exponential decay favors recent conflicts (decay factor: 0.95)
- Max-heap for O(log n) variable selection (the C solver uses a 4-ary heap and, by default, alternates VSIDS with a VMTF queue; the Python prototype scans for the maximum)

**Implementation**: Python: `_pick_branching_variable`; C: `heap_*` functions and `solver_decide` in `competition/c/src/solver.c`, VMTF in `competition/c/src/vmtf.c`

---

### 4. [LBD and Clause Management](LBD_AND_CLAUSE_MANAGEMENT.md)
**Topic**: Clause quality assessment and database reduction
**Impact**: Prevents memory explosion, keeps high-quality clauses

**Key concepts**:
- LBD (Literal Block Distance) = number of distinct decision levels in clause
- Glue clauses (LBD ≤ 2) are extremely valuable, never delete
- Periodic reduction: Keep best 50% of learned clauses
- On-the-fly subsumption during conflict analysis

**Implementation**: Python: `_compute_lbd`, `_reduce_learned_clauses`; C: `calc_lbd` and `solver_reduce_db` in `competition/c/src/solver.c`, `competition/c/src/reduction_sort.c`

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

**Implementation**: Python: `_luby`, `_should_restart`; C: `luby_sequence` and `solver_should_restart` in `competition/c/src/solver.c`. Glucose-style restarts are the default in both solvers (the C solver uses LBD-EMA restarts in focused mode and reluctant doubling in stable mode); Luby is optional

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

**Implementation**: Python: `saved_phase` in `_pick_branching_variable`; C: `solver_decide` in `competition/c/src/solver.c` (plus target phases and rephasing in `solver_rephase`)

---

### 7. [Random Phase Selection](random_phase_selection.md)
**Topic**: Occasional random polarity to prevent stuck states
**Impact**: Prevents catastrophic performance on some instances

**Key concepts**:
- Current C policy: opt-in random phases, with 1% probability when enabled
- Changes search trajectories and can help or hurt time to solution
- A performance heuristic, not a requirement for checking SAT models or UNSAT proofs

**Implementation**: Python: `random_phase_freq` in `_pick_branching_variable`; C: `--random-phase` / `--random-prob`, applied in `solver_decide` in `competition/c/src/solver.c`

---

### 8. [Adaptive Random Phase](adaptive_random_phase.md)
**Topic**: Boost randomness when solver is stuck
**Impact**: Escapes local minima during search

**Key concepts**:
- Detect stuck state (after 1000 conflicts, restarts/conflicts ratio ≥ 20%)
- Enable 5% random phase selection for the rest of the search
- Python prototype only (on by default there); the C solver has no automatic stuck-state adaptation

**Implementation**: `competition/python/cdcl_optimized.py` (`adaptive_random_phase`, in `solve`)

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

**Implementation**: Python: opt-in `enable_bce`, `enable_probing`, `enable_inprocessing`; C: the preprocessing/inprocessing pipeline in `competition/c/src/inprocess.c` and `competition/c/src/elim.c`, which goes well beyond this document (probing, subsumption, BVE, vivification, ...; BCE is opt-in via `--bce`; there is no separate pure-literal pass)

---

## C-Specific Implementation Details

These three design notes live in `competition/c/docs/` and describe the C solver's basic structures; for current defaults and the full feature set use [`../c/FEATURES.md`](../c/FEATURES.md).

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
- Glucose EMA mode (default) and sliding-window AVG mode
- Clause database reduction algorithm
- On-the-fly subsumption
- VSIDS activity management
- Performance comparison and tuning guidelines

---

## Results and Analysis

Historical records of the Python prototype (October 2025). Numbers and "next steps" in these files reflect that date.

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
| **Luby restarts** | Good for random | O(1) per check | Optional in both solvers |
| **Glucose restarts** | Good for structured | O(1) per check | Default in both solvers |
| **Phase saving** | Huge boost on structured | O(1) | Always |
| **Random phase** | Can prevent stuck states | O(1) | Opt-in (C: `--random-phase`, 1% when enabled) |
| **Adaptive random** | Escapes local minima | O(1) | Python prototype only (default on) |
| **Preprocessing** | 10-50% reduction | O(m²) worst case | Structured instances |

---

## Implementation Comparison

### Python vs C

| Feature | Python Location | C Location | Notes |
|---------|----------------|------------|-------|
| Two-watched literals | `WatchedLiteralManager` | `solver_propagate` (`src/solver.c`), `src/watch.c` | C adds binary-first watches, prefetching, circular scan |
| Conflict analysis | `_analyze_conflict`, `_minimize_clause` | `solver_analyze` (`src/solver.c`), `src/minimize.c` | C adds all-UIP shrinking, binary minimization, OTFS |
| VSIDS | `_pick_branching_variable` | `heap_*`, `solver_decide` (`src/solver.c`), `src/vmtf.c` | C: 4-ary heap, alternating with VMTF |
| LBD computation | `_compute_lbd` | `calc_lbd` (`src/solver.c`) | Same idea; C also updates LBD dynamically |
| Clause reduction | `_reduce_learned_clauses` | `solver_reduce_db` (`src/solver.c`), `src/reduction_sort.c` | C: 3-tier database |
| Glucose restarts | `_should_restart` | `solver_should_restart` (`src/solver.c`) | AVG default in Python; EMA default in C |
| Phase saving | `_pick_branching_variable` | `solver_decide` (`src/solver.c`) | C adds target phases and rephasing |

Python names are methods in `competition/python/cdcl_optimized.py`; C paths are relative to `competition/c/`.

**Performance**: The C solver is orders of magnitude faster than the Python prototype (see [`../README.md`](../README.md#performance))

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
A: Glucose-style adaptive restarts are the default in both solvers and work well on structured instances; Luby is available as an option (`restart_strategy='luby'` in Python, `--luby-restart` in C).

**Q: How often should I reduce the clause database?**
A: The C solver first reduces after 1000 conflicts and then at intervals growing by 100 (`--reduce-interval`, `--reduce-increment`); the Python prototype reduces when more than 10,000 learned clauses exist (`learned_clause_limit`). Adjust based on memory constraints.

**Q: What's a good LBD threshold for glue clauses?**
A: 2 (standard in Glucose). Never delete clauses with LBD ≤ 2.

**Q: Should I use preprocessing?**
A: Yes, always do unit propagation and pure literals (cheap). BCE is good for structured instances but expensive.

**Q: What's the difference between Python and C solvers?**
A: The Python solver is a prototype of the core techniques documented here. The C solver implements the same core far more efficiently and adds a large set of further techniques; see [`../FEATURE_COMPARISON.md`](../FEATURE_COMPARISON.md).

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

**Last Updated**: November 2026 (index and code references; the linked write-ups date from October 2025)

**Coverage**: Core CDCL techniques shared by both solvers. Techniques added to the C solver since October 2025 (variable elimination, probing, vivification, portfolio solving, proofs, and more) are documented under [`../c/`](../c/README.md), not here.

**C Solver Status**: Actively developed, `1.0.0-dev`; see [`../c/PRODUCTION_READINESS.md`](../c/PRODUCTION_READINESS.md)

**Python Solver Status**: Prototype, no longer actively developed; see [`../python/README.md`](../python/README.md)
