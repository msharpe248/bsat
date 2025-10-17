# New SAT Solver Research Ideas - October 2025

**Date**: October 17, 2025
**Status**: Planning Phase → Ready for Implementation
**Context**: After discovering all 4 existing research solvers (CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT) have clear prior art, we're developing genuinely new approaches.

## Executive Summary

This document outlines **8 novel SAT solver approaches** for implementation and empirical evaluation. Each combines established CDCL foundations with new heuristics for variable selection, clause learning, or search space exploration.

**Implementation Priority**:
- **Tier 1** (Highest Priority): CQP-SAT, VPL-SAT, MAB-SAT
- **Tier 2** (Medium Priority): TPM-SAT, CCG-SAT, HAS-SAT
- **Tier 3** (Exploratory): SSTA-SAT, CEGP-SAT

**Timeline**: 8 weeks for complete implementation and benchmarking

---

## 1. CQP-SAT: Clause Quality Prediction SAT Solver

### ⭐ **Top Priority Implementation**

### Core Idea

Not all learned clauses are equally valuable. Some lead to rapid conflict resolution; others clutter the database. **CQP-SAT predicts clause quality** using machine learning on clause features, then:
- **Aggressively keeps** high-quality clauses
- **Aggressively deletes** low-quality clauses
- **Uses quality scores** to guide restart decisions

### Algorithm Overview

```python
# Phase 1: Feature Extraction (per learned clause)
features = {
    'lbd': literal_block_distance(clause),           # Glucose metric
    'size': len(clause),
    'depth': decision_level_when_learned,
    'activity': sum(var_activity[v] for v in clause),
    'propagation_rate': times_used_for_propagation / age,
    'conflict_participation': times_in_conflict_analysis / age,
    'glue_stability': variance_of_lbd_over_time,
    'subsumption_potential': count_subsumed_clauses,
}

# Phase 2: Quality Prediction
quality_score = predict_quality(features)  # ML model or heuristic formula

# Phase 3: Adaptive Clause Management
if quality_score > HIGH_THRESHOLD:
    mark_as_protected(clause)              # Never delete
    boost_activity(clause)                  # Use more in decisions
elif quality_score < LOW_THRESHOLD:
    delete_immediately(clause)              # Aggressive deletion
```

### Key Innovation

**Previous work** (Glucose, MiniSat): Use static metrics (LBD, activity) for clause deletion.

**CQP-SAT novelty**:
1. **Predict future usefulness** rather than measure past usage
2. **Multi-feature learning** instead of single metrics
3. **Dynamic quality tracking** as clauses age

### Implementation Specification

**Directory**: `research/cqp_sat/`

**Files**:
- `clause_features.py` - Extract 8-12 features per clause
- `quality_predictor.py` - ML model or weighted heuristic
- `clause_database.py` - Quality-aware clause management
- `cqp_solver.py` - Main CDCL solver integration
- `README.md` - Documentation with novelty assessment

**ML Approach Options**:
1. **Lightweight Online Learning**: Update linear model during solving
2. **Pre-trained Model**: Train on benchmark suite offline
3. **Heuristic Formula**: Weighted combination (no ML, faster)

**Recommended First Implementation**: Heuristic formula with weights tuned on medium test suite.

### Literature Search Queries

Before implementation, search for:
- `"clause quality prediction" SAT`
- `"learned clause usefulness" SAT`
- `"machine learning" "clause deletion" SAT`
- `"predictive clause database management"`

### Expected Performance

**Best suited for**:
- Large instances with many learned clauses (>10k clauses)
- Long-running solves (>30 seconds)
- Problems where Glucose/MiniSat learn many low-quality clauses

**Expected improvement over CDCL**:
- 20-40% reduction in clause database size
- 10-30% speedup on instances with clause bloat
- Better memory efficiency

### Educational Value

**High** - demonstrates how ML/prediction can improve classical SAT algorithms.

---

## 2. VPL-SAT: Variable Phase Learning SAT Solver

### Core Idea

When we backtrack from conflicts, we learn which variable assignments were wrong. But **we throw away the polarity information**! VPL-SAT maintains a **phase preference history**:

- Track which phase (True/False) led to conflicts
- Learn from repeated mistakes: "x=True always causes conflicts → prefer x=False"
- Dynamically adjust phase preferences based on recent search history

### Algorithm Overview

```python
# Track phase performance per variable
phase_stats = {
    var: {
        'true_conflicts': 0,
        'false_conflicts': 0,
        'true_success': 0,      # Led to satisfying assignments
        'false_success': 0,
        'recent_window': deque(maxlen=100)  # Last 100 decisions
    }
    for var in variables
}

# On conflict at decision level d:
for lit in conflict_clause:
    var, polarity = abs(lit), (lit > 0)
    phase_stats[var][f'{polarity}_conflicts'] += 1
    phase_stats[var]['recent_window'].append(('conflict', polarity))

# Variable selection with learned phase preference:
def choose_phase(var):
    stats = phase_stats[var]

    # Compute recent conflict ratio
    true_conflict_rate = stats['true_conflicts'] / (stats['true_conflicts'] + stats['true_success'] + 1)
    false_conflict_rate = stats['false_conflicts'] / (stats['false_conflicts'] + stats['false_success'] + 1)

    # Prefer phase with lower conflict rate
    if true_conflict_rate < false_conflict_rate - THRESHOLD:
        return True
    elif false_conflict_rate < true_conflict_rate - THRESHOLD:
        return False
    else:
        return vsids_phase_saving(var)  # Fall back to VSIDS
```

### Key Innovation

**Previous work**:
- VSIDS phase saving: Remember last assigned phase (Pipatsrisawat & Darwiche 2007)
- Polarity heuristics: Static bias toward True or False

**VPL-SAT novelty**:
1. **Dynamic learning** from conflict history
2. **Per-variable tracking** with decay/windows
3. **Success vs. conflict ratio** rather than just last assignment

### Implementation Specification

**Directory**: `research/vpl_sat/`

**Files**:
- `phase_tracker.py` - Track phase performance statistics
- `phase_selector.py` - Learned phase selection heuristic
- `vpl_solver.py` - CDCL integration
- `README.md` - Documentation

**Parameters**:
- `window_size`: Recent history length (default: 100)
- `threshold`: Minimum difference to override VSIDS (default: 0.2)
- `decay_factor`: Exponential decay for old statistics (default: 0.95)

### Literature Search Queries

- `"phase selection" "conflict history" SAT`
- `"polarity learning" SAT solver`
- `"dynamic phase preference" CDCL`

### Expected Performance

**Best suited for**:
- Problems with phase-dependent structure (planning, scheduling)
- Instances where VSIDS phase saving is suboptimal
- Problems with strong polarity asymmetries

**Expected improvement**:
- 15-35% fewer conflicts on phase-sensitive instances
- 10-25% overall speedup on planning/scheduling problems

### Educational Value

**High** - shows how to learn from mistakes in the search tree.

---

## 3. MAB-SAT: Multi-Armed Bandit Variable Selection

### Core Idea

Variable selection is an **exploration vs. exploitation** problem:
- **Exploit**: Choose variables that historically led to progress
- **Explore**: Try different variables to discover better strategies

Treat each variable as an "arm" in a multi-armed bandit problem. Use **UCB1 (Upper Confidence Bound)** to balance exploration and exploitation.

### Algorithm Overview

```python
# Track performance per variable
var_stats = {
    var: {
        'times_selected': 0,
        'total_reward': 0.0,
        'avg_reward': 0.0,
    }
    for var in variables
}

# Reward function (after decision):
def compute_reward(var, decision_result):
    if decision_result == 'propagations':
        return len(propagations) / 10.0  # More propagations = better
    elif decision_result == 'conflict':
        return -5.0  # Immediate conflict = bad
    elif decision_result == 'backtrack':
        levels_jumped = current_level - backtrack_level
        return -1.0 * levels_jumped  # Deep backtrack = worse
    else:  # 'sat_found'
        return 100.0  # Solution found = best!

# UCB1 Variable Selection:
def select_variable_ucb1():
    total_selections = sum(stats['times_selected'] for stats in var_stats.values())

    best_var = None
    best_ucb = -inf

    for var in unassigned_variables:
        stats = var_stats[var]

        if stats['times_selected'] == 0:
            return var  # Always try unexplored variables first

        # UCB1 formula: exploitation + exploration
        exploitation = stats['avg_reward']
        exploration = sqrt(2 * log(total_selections) / stats['times_selected'])
        ucb_score = exploitation + exploration

        if ucb_score > best_ucb:
            best_ucb = ucb_score
            best_var = var

    return best_var
```

### Key Innovation

**Previous work**:
- VSIDS: Activity-based (exploit recent conflicts)
- Random selection: Pure exploration
- Hybrid approaches: Fixed exploration rate

**MAB-SAT novelty**:
1. **Principled exploration/exploitation** via UCB1
2. **Adaptive exploration** based on search progress
3. **Reward-based learning** from decision outcomes

### Implementation Specification

**Directory**: `research/mab_sat/`

**Files**:
- `bandit_tracker.py` - UCB1 statistics and reward computation
- `reward_functions.py` - Different reward signal options
- `mab_solver.py` - CDCL integration
- `README.md` - Documentation

**Reward Function Variants**:
1. **Propagation-based**: Reward immediate propagations
2. **Conflict-based**: Penalize conflicts, reward progress
3. **Hybrid**: Combine multiple signals

### Literature Search Queries

- `"multi-armed bandit" SAT variable selection`
- `"UCB1" satisfiability solver`
- `"exploration exploitation" CDCL`
- `"reinforcement learning" SAT branching`

### Expected Performance

**Best suited for**:
- Problems where VSIDS gets stuck in local optima
- Instances requiring diverse exploration
- Problems with multiple solution clusters

**Expected improvement**:
- 20-40% speedup on exploration-sensitive instances
- More robust on difficult random SAT
- Better restart behavior

### Educational Value

**Very High** - connects SAT solving to reinforcement learning and bandit algorithms.

---

## 4. TPM-SAT: Temporal Pattern Mining SAT Solver

### Core Idea

Conflicts follow **temporal patterns**:
- "After choosing x=True, we always hit a conflict involving variables {y, z} within 5 decisions"
- "Sequences like [a=T, b=F, c=T] → conflict 80% of the time"

**TPM-SAT mines these patterns** and avoids repeating them.

### Algorithm Overview

```python
# Track decision sequences leading to conflicts
pattern_database = {
    # pattern (tuple of decisions) → conflict info
}

# On conflict:
def record_conflict_pattern(decision_trail, conflict_clause):
    # Extract last N decisions before conflict
    pattern = tuple(decision_trail[-5:])  # Window size = 5

    if pattern not in pattern_database:
        pattern_database[pattern] = {
            'conflict_count': 0,
            'times_seen': 0,
            'avg_conflict_depth': 0,
        }

    pattern_database[pattern]['conflict_count'] += 1
    pattern_database[pattern]['times_seen'] += 1

# Decision making:
def select_variable_avoiding_patterns():
    var = vsids_select()  # Base selection

    # Check if current trail + this decision matches bad pattern
    for phase in [True, False]:
        candidate_pattern = tuple(decision_trail[-4:] + [(var, phase)])

        if candidate_pattern in pattern_database:
            stats = pattern_database[candidate_pattern]
            conflict_rate = stats['conflict_count'] / stats['times_seen']

            if conflict_rate > 0.8:  # 80% of the time → conflict
                # Try opposite phase or different variable
                phase = not phase

    return var, phase
```

### Key Innovation

**Previous work**:
- Clause learning: Learn **static** conflict clauses
- VSIDS: Activity-based, no sequence awareness

**TPM-SAT novelty**:
1. **Temporal/sequential** pattern mining
2. **Predictive avoidance** based on history
3. **Sequence-based heuristics** rather than static features

### Implementation Specification

**Directory**: `research/tpm_sat/`

**Files**:
- `pattern_miner.py` - Extract and store conflict patterns
- `pattern_matcher.py` - Check if current trail matches patterns
- `tpm_solver.py` - CDCL integration
- `README.md` - Documentation

**Parameters**:
- `window_size`: Pattern length (default: 5)
- `conflict_threshold`: Min conflict rate to avoid pattern (default: 0.8)
- `max_patterns`: Database size limit (default: 10000)

### Literature Search Queries

- `"temporal pattern mining" SAT`
- `"sequence mining" constraint satisfaction`
- `"decision sequence" conflict prediction SAT`

### Expected Performance

**Best suited for**:
- Problems with repetitive structure
- Instances where same mistake patterns repeat
- Planning problems with temporal dependencies

**Expected improvement**:
- 15-30% fewer repeated conflicts
- Better learning from long-running solves

### Educational Value

**Medium-High** - connects SAT to sequence mining and temporal reasoning.

---

## 5. CCG-SAT: Conflict Causality Graph SAT Solver

### Core Idea

Conflicts don't happen in isolation - they have **causal chains**:
- Conflict C1 → learned clause L1 → used in conflict C2 → learned clause L2 → ...

Build a **causality graph** where:
- **Nodes**: Conflicts and learned clauses
- **Edges**: "Clause L was used to derive conflict C"

Analyze this graph to:
1. Identify **root cause conflicts** (high out-degree)
2. Find **symptom conflicts** (caused by bad early decisions)
3. Guide restarts to avoid repeating causal chains

### Algorithm Overview

```python
# Causality graph
causality_graph = nx.DiGraph()

# On conflict:
def analyze_conflict_causality(conflict, learned_clause):
    # Add conflict node
    conflict_id = f"C{conflict_count}"
    causality_graph.add_node(conflict_id, type='conflict', clause=conflict)

    # Find which learned clauses participated
    for clause in learned_clauses_in_conflict_analysis:
        clause_id = learned_clause_ids[clause]
        causality_graph.add_edge(clause_id, conflict_id)

    # Add learned clause node
    clause_id = f"L{len(learned_clauses)}"
    causality_graph.add_node(clause_id, type='learned', clause=learned_clause)
    causality_graph.add_edge(conflict_id, clause_id)

# Identify root causes:
def find_root_causes():
    # Compute PageRank or out-degree centrality
    centrality = nx.pagerank(causality_graph)

    # High centrality clauses = root causes
    root_clauses = sorted(centrality.items(), key=lambda x: -x[1])[:10]
    return root_clauses

# Restart heuristic:
def should_restart():
    if conflict_count % 1000 == 0:
        root_causes = find_root_causes()

        # If root causes are very old, restart to re-explore
        if all(clause_age[c] > 5000 for c, _ in root_causes):
            return True
    return standard_restart_policy()
```

### Key Innovation

**Previous work**:
- Conflict graphs exist (implication graphs)
- Clause learning analyzes single conflicts

**CCG-SAT novelty**:
1. **Multi-conflict causality** tracking
2. **Graph analysis** (centrality, paths) for restart decisions
3. **Root cause identification** for better learning

### Implementation Specification

**Directory**: `research/ccg_sat/`

**Files**:
- `causality_graph.py` - Build and maintain conflict→clause graph
- `root_cause_analysis.py` - Centrality and path analysis
- `ccg_solver.py` - CDCL integration
- `README.md` - Documentation

**Graph Analysis Algorithms**:
- PageRank for root cause ranking
- Shortest path analysis for conflict chains
- Strongly connected components for cyclic patterns

### Literature Search Queries

- `"conflict causality" SAT`
- `"causal graph" constraint satisfaction`
- `"root cause analysis" SAT solver`

### Expected Performance

**Best suited for**:
- Complex instances with long conflict chains
- Problems where early bad decisions cascade
- Instances requiring intelligent restarts

**Expected improvement**:
- Better restart decisions (20-30% fewer wasted restarts)
- 10-20% speedup on complex structured instances

### Educational Value

**High** - demonstrates graph-based reasoning about solver behavior.

---

## 6. HAS-SAT: Hierarchical Abstraction SAT Solver

### Core Idea

Large SAT instances have **hierarchical structure**:
- High-level decisions (abstract variables)
- Low-level implications (derived variables)

**HAS-SAT builds abstraction hierarchy**:
1. Cluster variables into abstraction levels
2. Solve high-level (abstract) problem first
3. Refine solution by solving low-level details
4. Backtrack at appropriate abstraction level

### Algorithm Overview

```python
# Phase 1: Build abstraction hierarchy
def build_hierarchy(cnf):
    # Level 0: Original variables
    levels = [set(cnf.variables)]

    # Build variable influence graph
    influence_graph = build_influence_graph(cnf)

    # Cluster using Louvain (or hierarchical clustering)
    while len(levels[-1]) > 10:  # Until top level is small
        communities = louvain_clustering(influence_graph, level=len(levels))

        # Create abstract variables (one per community)
        abstract_vars = create_abstract_variables(communities)
        levels.append(abstract_vars)

    return levels

# Phase 2: Hierarchical solving
def solve_hierarchical():
    # Start at highest abstraction
    for level in reversed(hierarchy_levels):
        abstract_formula = project_to_level(cnf, level)

        # Solve at this level
        solution = cdcl_solve(abstract_formula)

        if solution is None:
            return UNSAT  # Unsolvable at abstract level → UNSAT

        # Refine: Fix abstract variables, solve next level down
        fix_variables(solution)

    return get_full_solution()
```

### Key Innovation

**Previous work**:
- Variable abstraction in model checking
- Hierarchical solving in constraint satisfaction

**HAS-SAT novelty for SAT**:
1. **Automated hierarchy construction** from CNF structure
2. **Hierarchical backtracking** (backtrack at right level)
3. **Refinement-based solving** (coarse → fine)

### Implementation Specification

**Directory**: `research/has_sat/`

**Files**:
- `abstraction_builder.py` - Construct variable hierarchy
- `hierarchical_solver.py` - Multi-level CDCL
- `refinement.py` - Solution refinement logic
- `has_solver.py` - Main solver
- `README.md` - Documentation

### Literature Search Queries

- `"hierarchical abstraction" SAT`
- `"abstraction refinement" satisfiability`
- `"multi-level" SAT solving`

### Expected Performance

**Best suited for**:
- Very large instances (>10k variables)
- Problems with clear hierarchical structure
- Circuit verification, hardware problems

**Expected improvement**:
- 2-5× speedup on hierarchical instances
- Better scalability to large problems

### Educational Value

**Medium** - demonstrates abstraction techniques from model checking.

---

## 7. SSTA-SAT: Solution Space Topology Analysis SAT Solver

### 🔬 **Exploratory / Advanced**

### Core Idea

The solution space has **topological structure**:
- Dense regions (many nearby solutions)
- Sparse regions (isolated solutions)
- Connectivity (paths between solutions)

**SSTA-SAT analyzes this topology** using sampling and graph analysis:
1. Sample multiple solutions (WalkSAT)
2. Compute Hamming distances → solution graph
3. Identify clusters and connectivity
4. Guide CDCL toward dense/connected regions

### Algorithm Overview

```python
# Phase 1: Sample solution space
solutions = []
for seed in range(100):
    sol = walksat(cnf, seed)
    if sol:
        solutions.append(sol)

# Phase 2: Build solution graph
solution_graph = nx.Graph()
for i, sol1 in enumerate(solutions):
    solution_graph.add_node(i, solution=sol1)

    for j, sol2 in enumerate(solutions[i+1:], start=i+1):
        hamming_dist = compute_hamming_distance(sol1, sol2)

        if hamming_dist < THRESHOLD:  # Close solutions
            solution_graph.add_edge(i, j, weight=hamming_dist)

# Phase 3: Topology analysis
def analyze_topology():
    # Identify dense clusters (many similar solutions)
    clusters = community_detection(solution_graph)

    # Compute centrality (most "central" solutions)
    centrality = nx.betweenness_centrality(solution_graph)

    # Find bridging variables (flip these → move between clusters)
    bridging_vars = find_bridge_variables(clusters)

    return {
        'clusters': clusters,
        'central_solutions': centrality,
        'bridges': bridging_vars,
    }

# Phase 4: Topology-guided CDCL
def select_variable_topology_guided():
    topo = analyze_topology()

    # Prefer variables that:
    # 1. Appear in central solutions
    # 2. Bridge between clusters (for exploration)
    # 3. Are consistent within dense clusters (for exploitation)

    var = choose_based_on_topology(topo)
    return var
```

### Key Innovation

**Previous work**:
- Sampling for backbone detection (BB-CDCL)
- Solution counting (model counting tools)

**SSTA-SAT novelty**:
1. **Topological analysis** of solution space structure
2. **Graph-based reasoning** about solution connectivity
3. **Topology-guided search** (exploit structure)

### Implementation Specification

**Directory**: `research/ssta_sat/`

**Files**:
- `solution_sampler.py` - WalkSAT-based sampling
- `topology_analyzer.py` - Build solution graph, compute metrics
- `cluster_detector.py` - Community detection in solution space
- `ssta_solver.py` - CDCL integration
- `README.md` - Documentation

**Topology Metrics**:
- Clustering coefficient
- Solution graph diameter
- Modularity (cluster quality)
- Betweenness centrality

### Literature Search Queries

- `"solution space topology" SAT`
- `"solution graph" satisfiability`
- `"topological analysis" constraint satisfaction`
- `"solution clustering" SAT`

### Expected Performance

**Best suited for**:
- SAT instances (not UNSAT)
- Problems with clustered solutions
- Instances where solution structure matters

**Expected improvement**:
- Uncertain (highly exploratory)
- Potentially 2-3× speedup on clustered solution spaces

### Educational Value

**Very High** - connects SAT to topology, graph theory, and solution space geometry.

---

## 8. CEGP-SAT: Clause Evolution via Genetic Programming

### 🔬 **Exploratory / Advanced**

### Core Idea

Instead of learning clauses via conflict analysis, **evolve better clauses** using genetic programming:
- Population: Current learned clauses
- Fitness: Propagation rate, conflict participation
- Evolution: Crossover (combine clauses), mutation (add/remove literals)

### Algorithm Overview

```python
# Population: Learned clauses
clause_population = []

# Fitness function
def fitness(clause):
    metrics = {
        'propagations': times_used_for_unit_prop,
        'conflicts': times_in_conflict_analysis,
        'size': len(clause),
        'lbd': literal_block_distance(clause),
    }

    # Reward useful, compact clauses
    return (metrics['propagations'] * 2 + metrics['conflicts']) / (metrics['size'] + 1)

# Genetic operators
def crossover(clause1, clause2):
    # Combine literals from both clauses
    child = set(clause1[:len(clause1)//2]) | set(clause2[len(clause2)//2:])
    return resolve_tautologies(child)

def mutate(clause):
    # Add or remove random literal
    if random() < 0.5:
        clause.add(random_literal())
    else:
        clause.remove(random.choice(clause))
    return clause

# Evolution step (every N conflicts)
def evolve_clauses():
    # Select best clauses (tournament selection)
    parents = tournament_selection(clause_population, k=10)

    # Create offspring via crossover and mutation
    offspring = []
    for _ in range(10):
        p1, p2 = random.sample(parents, 2)
        child = crossover(p1, p2)
        child = mutate(child)
        offspring.append(child)

    # Add offspring to population
    clause_population.extend(offspring)

    # Survival of the fittest
    clause_population.sort(key=fitness, reverse=True)
    clause_population = clause_population[:MAX_SIZE]
```

### Key Innovation

**Previous work**:
- Genetic algorithms for SAT (solver parameter tuning)
- Evolutionary algorithms for CSP

**CEGP-SAT novelty**:
1. **Evolve learned clauses** (not just tune parameters)
2. **Online evolution** during solving
3. **Genetic operators on clauses** (crossover, mutation)

### Implementation Specification

**Directory**: `research/cegp_sat/`

**Files**:
- `clause_evolution.py` - Genetic operators and evolution loop
- `fitness_functions.py` - Clause quality metrics
- `cegp_solver.py` - CDCL integration
- `README.md` - Documentation

### Literature Search Queries

- `"genetic programming" learned clauses SAT`
- `"evolutionary algorithms" clause learning`
- `"clause evolution" satisfiability`

### Expected Performance

**Highly uncertain** - this is very exploratory.

**Potential benefits**:
- Discover novel useful clauses not found by standard conflict analysis
- Better clause database diversity

**Risks**:
- High computational overhead
- Evolved clauses may be nonsensical or useless

### Educational Value

**Very High** - fun experiment connecting SAT to evolutionary computation!

---

## Implementation Plan

### Phase 1: Preparation (Week 1)

**Tasks**:
1. ✅ Create this analysis document
2. ⏳ Quick novelty checks (literature searches for all 8 approaches)
3. ⏳ Set up base infrastructure:
   - Common utilities for tracking statistics
   - Benchmark harness integration
   - Logging and visualization support

**Deliverables**:
- Novelty assessment updates to this document
- `research/common/solver_utils.py` (shared utilities)

### Phase 2: Tier 1 Implementation (Weeks 2-3)

**Priority**: CQP-SAT → VPL-SAT → MAB-SAT

**Week 2**: CQP-SAT
- Implement clause feature extraction
- Build heuristic quality predictor (no ML initially)
- Integrate with CDCL
- Test on medium suite

**Week 3**: VPL-SAT & MAB-SAT
- VPL-SAT: Phase tracking and learned phase selection
- MAB-SAT: UCB1 variable selection with reward tracking
- Test both on medium suite

**Deliverables**:
- 3 working solvers with README documentation
- Initial benchmark results

### Phase 3: Tier 2 Implementation (Weeks 4-6)

**Priority**: TPM-SAT → CCG-SAT → HAS-SAT

**Week 4**: TPM-SAT
- Pattern mining from decision sequences
- Pattern matching during search
- Avoidance heuristics

**Week 5**: CCG-SAT
- Causality graph construction
- Root cause analysis (PageRank)
- Restart heuristics

**Week 6**: HAS-SAT
- Hierarchy construction (clustering)
- Hierarchical CDCL solver
- Refinement logic

**Deliverables**:
- 3 more working solvers with documentation
- Comparative benchmarks (now 6 new solvers)

### Phase 4: Tier 3 Implementation (Weeks 7-8)

**Priority**: SSTA-SAT → CEGP-SAT

**Week 7**: SSTA-SAT
- Solution sampling (reuse WalkSAT from BB-CDCL)
- Topology analysis (solution graph, clustering)
- Topology-guided variable selection

**Week 8**: CEGP-SAT
- Clause evolution framework
- Genetic operators (crossover, mutation)
- Fitness-based selection

**Deliverables**:
- Final 2 solvers (total 8 new implementations)
- Complete benchmark suite results

### Phase 5: Analysis & Documentation (Week 8+)

**Tasks**:
1. Comprehensive benchmarking on all test suites:
   - Simple tests (validation)
   - Medium tests (performance comparison)
   - Competition small subset (stress test)

2. Performance analysis:
   - Win rates and rankings
   - Per-family performance
   - Timeout rates

3. Documentation:
   - Update README files with empirical results
   - Create comparison tables
   - Write `research/RESULTS_SUMMARY.md`

4. Novelty confirmation:
   - Final literature review for any approaches that show promise
   - Update novelty assessments based on results

---

## Success Metrics

### Implementation Success
- ✅ All 8 solvers compile and run without errors
- ✅ Pass correctness tests (simple test suite)
- ✅ Complete medium test suite without crashes

### Performance Success (vs. baseline CDCL)
- 🎯 **Tier 1**: At least 1 solver shows >20% average speedup
- 🎯 **Tier 2**: At least 1 solver solves instances CDCL times out on
- 🎯 **Tier 3**: Demonstrate novel behavior (even if slower)

### Educational Success
- ✅ Each solver demonstrates a distinct technique
- ✅ Clear documentation explaining the approach
- ✅ Empirical results inform understanding of when/why techniques work

---

## Risk Assessment

### Low Risk (Likely to work)
- **CQP-SAT**: Clause quality prediction is well-motivated
- **VPL-SAT**: Phase learning should help on some instances
- **MAB-SAT**: UCB1 is proven in other domains

### Medium Risk (May need tuning)
- **TPM-SAT**: Pattern mining overhead might be too high
- **CCG-SAT**: Causality analysis benefit unclear
- **HAS-SAT**: Hierarchy construction is tricky

### High Risk (Exploratory)
- **SSTA-SAT**: Topology analysis might not guide search well
- **CEGP-SAT**: Evolutionary clause learning is highly speculative

**Mitigation**: Focus on Tier 1 first. If those work well, proceed with confidence. If not, adjust approach.

---

## Literature Search Status

**Status**: ✅ COMPLETE (October 17, 2025)

**Findings Summary**:
- **2 approaches have clear prior art** and are NOT novel
- **6 approaches are potentially novel** with varying degrees of prior work

---

## Detailed Novelty Assessment (Literature Review Results)

### 1. CQP-SAT: Clause Quality Prediction

**Verdict**: ❌ **NOT NOVEL**

**Prior Art Found**:
- **Audemard & Simon (2009)**: "Predicting Learnt Clauses Quality in Modern SAT Solvers" (IJCAI 2009)
  - **This is THE foundational work** on clause quality prediction
  - Introduced **Literal Block Distance (LBD)** metric
  - Implemented in **Glucose solver** - now industry standard
  - Uses static measure to predict future clause usefulness
  - Exactly what CQP-SAT proposes to do

**Recommendation**: ❌ **DO NOT IMPLEMENT** - this is well-established work. Glucose already does this and is highly optimized.

**Alternative**: Could implement as educational exercise showing how Glucose works, but must clearly cite Audemard & Simon (2009) and acknowledge this is a reimplementation.

---

### 2. VPL-SAT: Variable Phase Learning

**Verdict**: ⚠️ **PARTIALLY NOVEL** - May have research value

**Prior Art Found**:
- **ResearchGate (2020)**: "Designing New Phase Selection Heuristics"
  - Discusses various phase selection strategies
  - Mentions VSIDS phase saving, ACIDS, EVSIDS, VMTF
  - Focus on selecting variables that recently participated in conflicts
  - But **NOT specifically** learning from conflict history per-variable

**Pipatsrisawat & Darwiche (2007)**: Phase saving (last assigned value)

**Gap in Literature**:
- No clear evidence of **dynamic per-variable phase learning from conflict/success ratios**
- Existing work uses static heuristics or simple phase saving

**Recommendation**: ✅ **IMPLEMENT** - This appears to have novelty potential. The specific approach of tracking conflict rates per phase per variable and learning preferences is not well-covered.

**Caution**: Do thorough literature review before publication. This may exist in some form.

---

### 3. MAB-SAT: Multi-Armed Bandit Variable Selection

**Verdict**: ❌ **NOT NOVEL**

**Prior Art Found**:
- **Learning Rate Branching (LRB)** in **MapleSAT**:
  - Uses multi-armed bandit framework for SAT branching
  - Variables scored using exponential recency-weighted average
  - Predicts which variables will be in more learnt clauses
  - Based on MAB framework in reinforcement learning

- **Kissat MAB**:
  - Modern SAT solver with bandit-based heuristic selection
  - Maintains scores for different heuristics (VSIDS, CHB)
  - Adaptive selection using bandit techniques

- **AAAI Paper**: "Multi-armed Bandit Algorithms for the Boolean Satisfiability Problem: A Survey"
  - Comprehensive survey of bandit applications to SAT

**Recommendation**: ❌ **DO NOT IMPLEMENT** - This is well-established. MapleSAT and Kissat already implement this approach.

**Alternative**: Could implement for educational purposes, but must cite MapleSAT (Liang et al.) and acknowledge prior work.

---

### 4. TPM-SAT: Temporal Pattern Mining

**Verdict**: ✅ **LIKELY NOVEL**

**Prior Art Found**:
- **None specific to SAT solving**
- Many papers on temporal pattern mining in general (healthcare, time series, event detection)
- **BUT: No papers found connecting temporal pattern mining to SAT variable selection**

**Gap in Literature**:
- Clause learning captures conflicts but not temporal sequences
- VSIDS tracks recent conflicts but no sequence patterns
- Decision sequence mining for conflict prediction appears unexplored

**Recommendation**: ✅ **IMPLEMENT - HIGH PRIORITY**

This appears genuinely novel. The idea of mining temporal patterns from decision sequences and using them to avoid repeating mistakes is not covered in SAT literature.

**Potential Issues**:
- Pattern database overhead
- Pattern matching may be expensive
- Unclear if patterns generalize or are too instance-specific

---

### 5. CCG-SAT: Conflict Causality Graph

**Verdict**: ⚠️ **PARTIALLY NOVEL** - Related work exists but different focus

**Prior Art Found**:
- **CausalSAT (Yang, 2023)**: "Explaining SAT Solving Using Causal Reasoning" (SAT 2023)
  - Uses causal reasoning to **explain solver behavior**
  - Causal graph with nodes = solver components/heuristics
  - Edges = causal relations between components
  - **BUT: Focus is on understanding solvers, not guiding decisions during solving**

- **Implication Graphs**: Standard in CDCL
  - Track literals and their propagation
  - Used for conflict analysis
  - **BUT: Single-conflict focused, not multi-conflict causality**

**Gap in Literature**:
- CausalSAT analyzes solver behavior (post-hoc analysis)
- CCG-SAT proposes **using causality during solving** for restart decisions
- Multi-conflict causal chains not explored

**Recommendation**: ✅ **IMPLEMENT** - This has novelty potential

The twist is using causality analysis **online during solving** rather than for post-hoc explanation. Must clearly distinguish from CausalSAT (Yang 2023).

---

### 6. HAS-SAT: Hierarchical Abstraction

**Verdict**: ⚠️ **PARTIALLY NOVEL** - Related work in model checking, less in pure SAT

**Prior Art Found**:
- **CEGAR (Counterexample-Guided Abstraction Refinement)**:
  - Well-established in model checking
  - Abstraction → refinement loop
  - Has been combined with SAT for some tasks

- **SAT-based CEGAR** (Clarke et al.):
  - Uses SAT solvers within CEGAR framework
  - **BUT: For model checking, not pure SAT solving**

- **Predicate Abstraction** (hardware verification):
  - Abstracts netlists to higher levels
  - **BUT: Requires external structure (RTL, hardware hierarchy)**

**Gap in Literature**:
- **Automated hierarchy construction from CNF structure alone**
- Pure SAT solving with abstraction refinement
- No external structure assumed

**Recommendation**: ✅ **IMPLEMENT** - Has novelty potential

The key novelty is **automated extraction** of hierarchy from flat CNF, not requiring external domain knowledge. Must cite CEGAR literature and position as applying to pure SAT.

**Challenges**:
- How to automatically build meaningful hierarchy?
- Louvain clustering may not capture semantic structure

---

### 7. SSTA-SAT: Solution Space Topology Analysis

**Verdict**: ✅ **LIKELY NOVEL**

**Prior Art Found**:
- **Recent Theory (2024)**: "An Intrinsic Barrier for Resolving P=NP (2-SAT as Flat, 3-SAT as High-Dimensional Void-Rich)"
  - Analyzes **topology of SAT solution spaces** theoretically
  - Uses Betti numbers to characterize voids
  - 2-SAT: flat, contractible (Betti = 0)
  - 3-SAT: high-dimensional voids, exponential Betti numbers
  - **BUT: Purely theoretical, not algorithmic**

- **Solution Clusters** (CSTheory StackExchange):
  - Definition: Satisfying assignments within Hamming distance ball
  - **BUT: No algorithms using topology to guide search**

- **Backbone Detection**: Uses sampling but not topology

**Gap in Literature**:
- **No work found using topological analysis to guide CDCL search**
- Theory exists, but no practical algorithms
- Solution graph construction and analysis for variable selection is unexplored

**Recommendation**: ✅ **IMPLEMENT - HIGH PRIORITY**

This appears genuinely novel and connects deep mathematical theory (topology) to practical SAT solving. Could be very interesting research contribution.

**Cite**: Must reference the 2024 topology theory paper and position this as algorithmic application.

---

### 8. CEGP-SAT: Clause Evolution via Genetic Programming

**Verdict**: ⚠️ **PARTIALLY NOVEL** - Related work but different focus

**Prior Art Found**:
- **CLASS (Fukunaga)**: Genetic programming to **discover SAT heuristics**
  - GP evolves local search heuristics
  - Competitive with WalkSAT variants
  - **BUT: Evolves heuristics, not clauses**

- **Hybrid CDCL + Local Search**:
  - CDCL shares learned clauses with local search
  - **BUT: No clause evolution**

- **Genetic Algorithms for SAT**:
  - Parameter tuning
  - Initialization of variable polarities
  - **BUT: No online clause evolution during solving**

**Gap in Literature**:
- **Evolving learned clauses during CDCL solving**
- Genetic operators (crossover, mutation) on clauses
- Online evolution loop

**Recommendation**: ✅ **IMPLEMENT** - Exploratory but potentially novel

This is highly experimental. GP has been used for SAT but not specifically for evolving learned clauses. Educational value is very high.

**Risks**:
- May generate useless/tautological clauses
- Computational overhead likely high
- Unclear if evolved clauses help

---

## Revised Implementation Priorities

Based on novelty findings:

### ❌ DO NOT IMPLEMENT (Not Novel):
1. **CQP-SAT** - Already done by Glucose (Audemard & Simon 2009)
2. **MAB-SAT** - Already done by MapleSAT and Kissat

### ✅ HIGH PRIORITY (Likely Novel):
1. **TPM-SAT** - Temporal pattern mining for SAT (no prior work found)
2. **SSTA-SAT** - Solution space topology analysis (theory exists, algorithm doesn't)

### ✅ MEDIUM PRIORITY (Partially Novel):
3. **VPL-SAT** - Phase learning from conflicts (gap in literature)
4. **CCG-SAT** - Causality for decisions (different from CausalSAT)
5. **HAS-SAT** - Automated hierarchy from CNF (different from CEGAR)

### ✅ EXPLORATORY:
6. **CEGP-SAT** - Clause evolution (experimental, educational)

### Revised Tier Structure:

**Tier 1 (Weeks 1-3)**: High-novelty approaches
- TPM-SAT (Temporal Pattern Mining) ⭐⭐
- SSTA-SAT (Solution Space Topology) ⭐⭐
- VPL-SAT (Variable Phase Learning) ⭐

**Tier 2 (Weeks 4-6)**: Medium-novelty approaches
- CCG-SAT (Conflict Causality Graph)
- HAS-SAT (Hierarchical Abstraction)

**Tier 3 (Weeks 7-8)**: Exploratory
- CEGP-SAT (Clause Evolution)

---

## Next Steps (Updated After Literature Review)

1. ✅ **Run literature searches** - COMPLETE
2. ✅ **Update novelty assessments** - COMPLETE
3. **Set up common infrastructure** - Create shared utilities for all solvers
4. **Implement TPM-SAT** (first solver, highest novelty priority)
5. **Implement SSTA-SAT** (second solver, high novelty priority)
6. **Implement VPL-SAT** (third solver, medium novelty)
7. **Benchmark all Tier 1 solvers** on medium suite
8. **Iterate** through Tier 2 and Tier 3 solvers

## Summary of Findings

**Great News**: We found **6 approaches with novelty potential** (dropped from 8 to 6):

**Dropped (Not Novel)**:
- ❌ CQP-SAT → Glucose (Audemard & Simon 2009)
- ❌ MAB-SAT → MapleSAT / Kissat

**Keeping (Novel or Partially Novel)**:
- ✅ **TPM-SAT** - Likely novel (no prior work found)
- ✅ **SSTA-SAT** - Likely novel (theory exists, no algorithms)
- ✅ **VPL-SAT** - Partially novel (gap in literature)
- ✅ **CCG-SAT** - Partially novel (different focus from CausalSAT)
- ✅ **HAS-SAT** - Partially novel (automated hierarchy for pure SAT)
- ✅ **CEGP-SAT** - Partially novel (exploratory)

---

## Notes

- **Focus on educational value**: Even if some approaches turn out to be non-novel or underperforming, they're valuable learning exercises
- **Benchmark early and often**: Test each solver as soon as it's implemented
- **Be honest about results**: Update novelty assessments and documentation based on literature findings and empirical performance
- **Have fun**: This is exploratory research - enjoy the process!

---

**End of Analysis Document**
