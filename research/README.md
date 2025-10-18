# BSAT Research

This directory contains experimental and research implementations of novel SAT solving algorithms. These solvers are designed to explore new approaches to Boolean satisfiability that go beyond traditional algorithms.

## Purpose

The research directory serves as a sandbox for:

- **Novel Algorithm Development**: Implementing and testing new SAT solving approaches
- **Theoretical Exploration**: Investigating algorithms that exploit problem structure
- **Educational Value**: Demonstrating advanced concepts in SAT solving
- **Performance Experiments**: Comparing new approaches against established solvers

---

## Research Projects

### Original Research Suite (Established Solvers)

#### 1. Community-Based Decomposition SAT (CoBD-SAT)

**Status**: ✅ Working (with recent critical bug fix!)

**Location**: `cobd_sat/`

**Description**: A novel SAT solver that exploits community structure in the variable-clause interaction graph. The algorithm decomposes the problem into semi-independent communities and coordinates their solutions through interface variable enumeration.

**Key Features**:
- Graph-based problem decomposition using Louvain modularity optimization
- Modularity-based community detection (Q > 0.2 for decomposition)
- Independent community solving with CDCL
- Interface variable coordination through enumeration
- Potential for exponential speedups on modular problems

**Recent Improvements (October 2025)**:
- ✅ **Fixed bipartite modularity bug**: Q=0.00 → Q=0.586 on modular problems
- ✅ **CDCL fallback**: Faster fallback when decomposition not beneficial

**Performance**: 127× speedup on Random 3-SAT (12 vars, 40 clauses)

**See**: `cobd_sat/README.md` for detailed algorithm description

---

#### 2. Backbone-Based CDCL (BB-CDCL)

**Status**: ✅ Working (with massive UNSAT speedup!)

**Location**: `bb_cdcl/`

**Description**: Combines statistical sampling (WalkSAT) with systematic search (CDCL) to identify and exploit backbone variables—variables that must have the same value in all solutions.

**Key Features**:
- WalkSAT-based backbone detection with configurable confidence threshold
- Adaptive sample count based on problem difficulty
- Early UNSAT detection with timeout
- Conflict-driven backbone unfixing for robustness
- Exponential search space reduction on backbone-rich problems

**Recent Improvements (October 2025)**:
- ✅ **Early UNSAT detection**: 6.3s → 0.0005s (12,600× speedup!)
- ✅ **Adaptive sampling**: 10-110 samples based on difficulty (6× speedup on easy)

**Performance**: 93% backbone detection accuracy, 12,600× speedup on UNSAT

**See**: `bb_cdcl/README.md` for detailed algorithm description

---

#### 3. Lookahead-Enhanced CDCL (LA-CDCL)

**Status**: ✅ Working

**Location**: `la_cdcl/`

**Description**: Enhances CDCL with shallow lookahead (2-3 steps) to predict which variable assignments lead to more unit propagations and fewer conflicts, resulting in better initial decisions.

**Key Features**:
- Shallow lookahead on top VSIDS candidates
- Adaptive lookahead frequency based on conflict rate
- Minimal overhead (5-10% on average)
- 20-50% conflict reduction on hard instances
- Compatible with all standard CDCL improvements

**Recent Improvements (October 2025)**:
- ✅ **Adaptive lookahead frequency**: freq 1-8 based on conflict rate
- Reduces overhead on easy problems while maintaining effectiveness

**Performance**: 122× speedup on Random 3-SAT (12 vars, 40 clauses)

**See**: `la_cdcl/README.md` for detailed algorithm description

---

#### 4. Conflict Graph Pattern Mining SAT (CGPM-SAT)

**Status**: ✅ Working (with efficient caching!)

**Location**: `cgpm_sat/`

**Description**: Builds a meta-level graph of variable conflicts and uses graph centrality metrics (PageRank, betweenness) to identify structurally important variables for better decision ordering.

**Key Features**:
- Conflict graph with incremental updates
- PageRank-based variable importance scoring
- Combines structural (graph) and reactive (VSIDS) heuristics
- Cached metric computation for efficiency
- Effective on structured and industrial benchmarks

**Recent Improvements (October 2025)**:
- ✅ **Graph metrics caching**: 89% cache hit rate, overhead < 1%

**Performance**: 186× speedup on Random 3-SAT (12 vars, 40 clauses) - best research solver!

**See**: `cgpm_sat/README.md` for detailed algorithm description

---

### New Research Suite (Advanced CDCL Variants)

This suite explores 8 different approaches to SAT solving, from novel research ideas to educational reimplementations of state-of-the-art techniques.

#### 5. TPM-SAT (Temporal Pattern Mining) ⭐⭐ NOVEL

**Status**: ✅ Working

**Location**: `tpm_sat/`

**Novelty**: **Highly Novel** - Pattern mining from conflict history for SAT

**Description**: Mines temporal decision patterns from conflict history to identify and avoid sequences that repeatedly lead to conflicts. Uses anti-pattern matching to guide variable selection.

**Key Features**:
- Conflict sequence extraction and pattern mining
- Anti-pattern database for avoiding bad decisions
- Pattern-aware variable selection with VSIDS integration
- Supports sequences of length 2-5
- Adaptive pattern matching with similarity thresholds

**See**: `tpm_sat/README.md` for detailed algorithm description

---

#### 6. SSTA-SAT (Solution Space Topology Analysis) ⭐⭐ NOVEL

**Status**: ✅ Working

**Location**: `ssta_sat/`

**Novelty**: **Highly Novel** - Topology-guided search using solution sampling

**Description**: Samples multiple solutions using WalkSAT, builds a topology graph using Hamming distance, identifies solution clusters, and guides CDCL search toward solution-dense regions.

**Key Features**:
- WalkSAT-based solution sampling
- Hamming distance topology graph construction
- Cluster detection and centroid computation
- Topology-aware variable selection
- Focuses search on solution-dense regions

**See**: `ssta_sat/README.md` for detailed algorithm description

---

#### 7. VPL-SAT (Variable Phase Learning) ⭐ PARTIALLY NOVEL

**Status**: ✅ Working

**Location**: `vpl_sat/`

**Novelty**: **Partially Novel** - Dynamic phase learning from conflict patterns

**Description**: Learns optimal phase (polarity) for each variable by tracking conflict/success history. Implements multiple learning strategies (conflict-based, success-based, hybrid).

**Key Features**:
- Per-variable phase statistics tracking
- Three learning strategies (conflict, success, hybrid)
- Dynamic phase selection based on history
- Integration with CDCL decision making
- Adaptive to problem structure

**See**: `vpl_sat/README.md` for detailed algorithm description

---

#### 8. CQP-SAT (Clause Quality Prediction) 📚 EDUCATIONAL

**Status**: ✅ Working

**Location**: `cqp_sat/`

**Novelty**: **Educational Reimplementation** of Glucose LBD approach

**Description**: Educational reimplementation of Glucose's Literal Block Distance (LBD) clause quality metric. Identifies "glue" clauses (LBD ≤ 2) for preferential retention.

**Key Features**:
- LBD computation for learned clauses
- Glue clause detection and preservation
- Quality-based clause database management
- Activity tracking and aging
- Glucose-inspired learned clause management

**Citation**: Based on Audemard & Simon (2009) - Glucose SAT Solver

**See**: `cqp_sat/README.md` for detailed algorithm description

---

#### 9. MAB-SAT (Multi-Armed Bandit) 📚 EDUCATIONAL

**Status**: ✅ Working

**Location**: `mab_sat/`

**Novelty**: **Educational Reimplementation** of MapleSAT/Kissat UCB1 approach

**Description**: Educational reimplementation of multi-armed bandit (UCB1) variable selection from MapleSAT and Kissat. Balances exploration (trying new variables) with exploitation (choosing historically good variables).

**Key Features**:
- UCB1 algorithm for variable selection
- Reward-based learning from decision outcomes
- Exploration/exploitation balance
- Multiple reward strategies (propagation, progress, hybrid)
- Adaptive variable selection

**Citation**: Based on MapleSAT Learning Rate Branching (LRB) and Kissat adaptive heuristics

**See**: `mab_sat/README.md` for detailed algorithm description

---

#### 10. CCG-SAT (Conflict Causality Graph) ⭐ PARTIALLY NOVEL

**Status**: ✅ Working

**Location**: `ccg_sat/`

**Novelty**: **Partially Novel** - Online causality analysis (vs. CausalSAT's post-hoc)

**Description**: Tracks multi-conflict causal chains during solving. Uses root cause analysis to identify when search is stuck in bad regions and triggers intelligent restarts.

**Key Features**:
- Directed causality graph (clause → conflict → clause)
- Root cause detection (high out-degree nodes)
- Age-based restart heuristic (old root causes = stuck)
- Online analysis during solving (not post-hoc)
- Adaptive restart decisions

**Distinction**: CausalSAT (Yang 2023) uses causality for post-hoc explanation; CCG-SAT uses it online for restart guidance

**See**: `ccg_sat/README.md` for detailed algorithm description

---

#### 11. HAS-SAT (Hierarchical Abstraction) 📚 EDUCATIONAL

**Status**: ✅ Working

**Location**: `has_sat/`

**Novelty**: **Educational** - Abstraction-refinement for SAT

**Description**: Educational demonstration of hierarchical abstraction-refinement. Builds variable clusters based on co-occurrence, solves at abstract level first, then refines to concrete.

**Key Features**:
- Variable clustering by co-occurrence
- Multi-level abstraction hierarchy
- Abstraction-refinement loop
- Solve abstract → refine to concrete
- Solution verification

**Related**: Abstraction-refinement from planning (HTN) and model checking (CEGAR)

**See**: `has_sat/README.md` for detailed algorithm description

---

#### 12. CEGP-SAT (Clause Evolution with Genetic Programming) 🧪 EXPERIMENTAL

**Status**: ✅ Working

**Location**: `cegp_sat/`

**Novelty**: **Experimental** - Genetic programming for SAT

**Description**: Experimental approach using genetic programming to evolve learned clauses. Applies crossover and mutation operators to create clause variants, selects based on fitness (propagation effectiveness).

**Key Features**:
- Genetic operators: crossover, mutation, selection
- Fitness evaluation (propagation + conflicts + size)
- Clause evolution during solving
- Tournament selection of high-fitness clauses
- Experimental/educational demonstration

**Note**: Experimental approach - traditional CDCL is more reliable for production

**See**: `cegp_sat/README.md` for detailed algorithm description

---

## Algorithm Comparison

### Original Research Suite

| Algorithm | Best For | Speedup Demonstrated | Status |
|-----------|----------|---------------------|--------|
| **CoBD-SAT** | Modular problems (circuits, planning) | 127× | ✅ Working |
| **BB-CDCL** | Backbone-rich (>30% forced vars) | 12,600× (UNSAT) | ✅ Working |
| **LA-CDCL** | Hard random SAT near phase transition | 122× | ✅ Working |
| **CGPM-SAT** | Structured conflicts (industrial) | 186× | ✅ Working |

### New Research Suite

| Algorithm | Type | Novelty | Status |
|-----------|------|---------|--------|
| **TPM-SAT** | Pattern Mining | ⭐⭐ Novel | ✅ Working |
| **SSTA-SAT** | Topology Analysis | ⭐⭐ Novel | ✅ Working |
| **VPL-SAT** | Phase Learning | ⭐ Partially Novel | ✅ Working |
| **CQP-SAT** | Clause Quality (Glucose) | 📚 Educational | ✅ Working |
| **MAB-SAT** | UCB1 Selection (MapleSAT) | 📚 Educational | ✅ Working |
| **CCG-SAT** | Causality Analysis | ⭐ Partially Novel | ✅ Working |
| **HAS-SAT** | Abstraction-Refinement | 📚 Educational | ✅ Working |
| **CEGP-SAT** | Genetic Programming | 🧪 Experimental | ✅ Working |

**Total**: 12 research solvers implemented and tested

**See** `ALGORITHM_SHOWCASE.md` for comprehensive performance analysis and real-world applications.

**See** `BENCHMARKS.md` for detailed benchmark results with rankings across all solvers.

---

## Directory Structure

```
research/
├── README.md                    # This file
├── ALGORITHM_SHOWCASE.md        # Comprehensive algorithm analysis
├── BENCHMARKS.md                # Detailed benchmark results
│
├── cobd_sat/                    # Community-Based Decomposition SAT
│   ├── README.md
│   ├── cobd_solver.py
│   └── community_detector.py
│
├── bb_cdcl/                     # Backbone-Based CDCL
│   ├── README.md
│   ├── bb_cdcl_solver.py
│   └── backbone_detector.py
│
├── la_cdcl/                     # Lookahead-Enhanced CDCL
│   ├── README.md
│   ├── la_cdcl_solver.py
│   └── lookahead_engine.py
│
├── cgpm_sat/                    # Conflict Graph Pattern Mining SAT
│   ├── README.md
│   ├── cgpm_solver.py
│   └── conflict_graph.py
│
├── tpm_sat/                     # Temporal Pattern Mining SAT ⭐⭐
│   ├── README.md
│   ├── pattern_miner.py
│   ├── pattern_matcher.py
│   ├── tpm_solver.py
│   └── example.py
│
├── ssta_sat/                    # Solution Space Topology Analysis ⭐⭐
│   ├── README.md
│   ├── solution_sampler.py
│   ├── topology_analyzer.py
│   ├── ssta_solver.py
│   └── example.py
│
├── vpl_sat/                     # Variable Phase Learning ⭐
│   ├── README.md
│   ├── phase_tracker.py
│   ├── phase_selector.py
│   ├── vpl_solver.py
│   └── example.py
│
├── cqp_sat/                     # Clause Quality Prediction 📚
│   ├── README.md
│   ├── clause_features.py
│   ├── quality_predictor.py
│   ├── cqp_solver.py
│   └── example.py
│
├── mab_sat/                     # Multi-Armed Bandit 📚
│   ├── README.md
│   ├── bandit_tracker.py
│   ├── reward_functions.py
│   ├── mab_solver.py
│   └── example.py
│
├── ccg_sat/                     # Conflict Causality Graph ⭐
│   ├── README.md
│   ├── causality_graph.py
│   ├── root_cause_analyzer.py
│   ├── ccg_solver.py
│   └── example.py
│
├── has_sat/                     # Hierarchical Abstraction 📚
│   ├── README.md
│   ├── abstraction_builder.py
│   ├── refinement_solver.py
│   ├── has_solver.py
│   └── example.py
│
├── cegp_sat/                    # Clause Evolution with GP 🧪
│   ├── README.md
│   ├── genetic_operators.py
│   ├── fitness_evaluator.py
│   ├── cegp_solver.py
│   └── example.py
│
├── benchmarks/                  # Benchmark scripts and results
│   ├── benchmark.py            # Core benchmark utilities
│   ├── run_full_benchmark.py   # Run all solvers
│   ├── run_simple_benchmark.py
│   ├── run_focused_benchmark.py
│   └── benchmark_results_full.md
│
├── bugs/                        # Test files for bug fixes
│   ├── test_cobd_modularity_fix.py
│   ├── test_bb_cdcl_unsat_fix.py
│   ├── test_bb_cdcl_adaptive_sampling.py
│   ├── test_la_cdcl_adaptive_lookahead.py
│   └── test_cgpm_caching.py
│
├── examples/                    # Usage examples
│   └── research_solver_demo.py
│
└── analysis/                    # Research documents
    └── novel_sat_approaches.md
```

---

## Usage Examples

### Original Research Suite

#### CoBD-SAT (Community-Based Decomposition)

```python
from research.cobd_sat import CoBDSATSolver
from bsat import CNFExpression

# Create a modular CNF formula (e.g., circuit verification)
cnf = CNFExpression.parse("(a | b) & (b | c) & (c | d)")

# Solve using CoBD-SAT
solver = CoBDSATSolver(cnf, min_communities=2, max_communities=8)
result = solver.solve()

# Get statistics
stats = solver.get_statistics()
print(f"Modularity Q: {stats['modularity']:.3f}")
print(f"Communities: {stats['num_communities']}")
print(f"Used decomposition: {stats['used_decomposition']}")
```

#### BB-CDCL (Backbone-Based CDCL)

```python
from research.bb_cdcl import BBCDCLSolver
from bsat import CNFExpression

# Create a CNF with backbone (e.g., planning problem)
cnf = CNFExpression.parse("(a) & (a | b) & (~b | c)")

# Solve using BB-CDCL with adaptive sampling
solver = BBCDCLSolver(
    cnf,
    adaptive_sampling=True,  # Adjusts samples based on difficulty
    confidence_threshold=0.95
)
result = solver.solve()

# Get statistics
stats = solver.get_statistics()
print(f"Samples used: {stats['num_samples']}")
print(f"Backbone detected: {stats['num_backbone_detected']} ({stats['backbone_percentage']:.1f}%)")
```

### New Research Suite

#### TPM-SAT (Temporal Pattern Mining)

```python
from research.tpm_sat import TPMSATSolver
from bsat import CNFExpression

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

# Solve with pattern mining
solver = TPMSATSolver(
    cnf,
    use_patterns=True,
    max_pattern_length=3,
    min_pattern_support=2
)
result = solver.solve()

# Get pattern statistics
stats = solver.get_pattern_statistics()
print(f"Patterns found: {stats['patterns_found']}")
print(f"Anti-patterns found: {stats['anti_patterns_found']}")
```

#### SSTA-SAT (Solution Space Topology)

```python
from research.ssta_sat import SSTASATSolver
from bsat import CNFExpression

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

# Solve with topology analysis
solver = SSTASATSolver(
    cnf,
    use_topology=True,
    num_samples=10,
    cluster_threshold=0.3
)
result = solver.solve()

# Get topology statistics
stats = solver.get_topology_statistics()
print(f"Solutions sampled: {stats['solutions_sampled']}")
print(f"Clusters found: {stats['clusters_found']}")
```

#### VPL-SAT (Variable Phase Learning)

```python
from research.vpl_sat import VPLSATSolver
from bsat import CNFExpression

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

# Solve with phase learning
solver = VPLSATSolver(
    cnf,
    use_phase_learning=True,
    learning_strategy='hybrid'  # 'conflict', 'success', or 'hybrid'
)
result = solver.solve()

# Get phase statistics
stats = solver.get_phase_statistics()
print(f"Phase learning enabled: {stats['enabled']}")
print(f"Learned phases used: {stats['learned_phases_used']}")
```

#### CCG-SAT (Conflict Causality Graph)

```python
from research.ccg_sat import CCGSATSolver
from bsat import CNFExpression

cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

# Solve with causality analysis
solver = CCGSATSolver(
    cnf,
    use_causality=True,
    old_age_threshold=5000  # Restart if root causes > 5000 conflicts old
)
result = solver.solve()

# Get causality statistics
stats = solver.get_causality_statistics()
print(f"Causality restarts: {stats['causality_restarts']}")
print(f"Root causes detected: {stats['root_causes_detected']}")
```

---

## Running Benchmarks

### Full Benchmark (All Solvers)

```bash
cd research/benchmarks
python run_full_benchmark.py
```

Benchmarks all solvers on multiple problem types.

### Simple Benchmark

```bash
cd research/benchmarks
python run_simple_benchmark.py
```

Quick benchmark on a subset of problems for rapid testing.

---

## Recent Improvements (October 2025)

### Phase 1: Critical Fixes
- ✅ **CoBD-SAT modularity**: Fixed bipartite graph bug (Q=0.00 → Q=0.586)
- ✅ **BB-CDCL UNSAT**: Early detection with timeout (6.3s → 0.0005s = **12,600× speedup**)

### Phase 2: Performance Optimizations
- ✅ **BB-CDCL adaptive sampling**: Adjusts 10-110 samples (6× speedup on easy)
- ✅ **LA-CDCL adaptive lookahead**: Adjusts freq 1-8 based on conflicts
- ✅ **CGPM-SAT caching**: 89% cache hit rate, overhead < 1%

### Phase 3: New Research Suite (8 Solvers)
- ✅ **TPM-SAT**: Temporal pattern mining from conflicts (Novel)
- ✅ **SSTA-SAT**: Solution topology analysis and clustering (Novel)
- ✅ **VPL-SAT**: Dynamic phase learning (Partially Novel)
- ✅ **CQP-SAT**: Glucose LBD reimplementation (Educational)
- ✅ **MAB-SAT**: MapleSAT UCB1 reimplementation (Educational)
- ✅ **CCG-SAT**: Online causality analysis for restarts (Partially Novel)
- ✅ **HAS-SAT**: Abstraction-refinement demonstration (Educational)
- ✅ **CEGP-SAT**: Genetic programming for clauses (Experimental)

All improvements are thoroughly tested and documented.

---

## Contributing

When adding new research algorithms:

1. Create a subdirectory for the algorithm (e.g., `new_algorithm/`)
2. Include a `README.md` with:
   - Algorithm description and pseudocode
   - Theoretical analysis (complexity, completeness)
   - Novelty assessment and citations
   - Advantages/disadvantages
   - References to literature
3. Implement the solver following patterns from existing BSAT solvers
4. Add `example.py` demonstrating the algorithm
5. Add test files to `bugs/` for any bug fixes
6. Update this README with the new project

---

## References

### Original Research Suite
- **CoBD-SAT**: Based on community detection in SAT (Louvain algorithm)
- **BB-CDCL**: Inspired by survey propagation and WalkSAT
- **LA-CDCL**: Based on lookahead techniques from DPLL solvers
- **CGPM-SAT**: Inspired by PageRank and graph centrality measures

### New Research Suite
- **TPM-SAT**: Novel pattern mining approach for SAT
- **SSTA-SAT**: Novel topology-guided search
- **VPL-SAT**: Related to conflict-driven phase selection
- **CQP-SAT**: Audemard & Simon (2009) - Glucose LBD
- **MAB-SAT**: MapleSAT LRB, Kissat adaptive heuristics, Auer et al. (2002) UCB1
- **CCG-SAT**: Related to CausalSAT (Yang 2023) but online vs. post-hoc
- **HAS-SAT**: Abstraction-refinement from HTN planning and CEGAR
- **CEGP-SAT**: Experimental genetic programming for SAT

---

## License

Same as BSAT package (MIT License)
