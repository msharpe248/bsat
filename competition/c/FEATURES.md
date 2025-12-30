# BSAT C Solver - Feature Implementation Summary

This document provides a comprehensive overview of all CDCL optimizations implemented in the C solver, including default states and command-line flags.

## Feature Quick Reference

| Feature | Default | Enable | Disable |
|---------|---------|--------|---------|
| VSIDS branching | ON | `--vsids` | `--lrb` |
| LRB/CHB branching | OFF | `--lrb` | `--vsids` |
| Glucose EMA restarts | ON | `--glucose-restart-ema` | `--no-restarts` |
| Phase saving | ON | - | `--no-phase-saving` |
| Target rephasing | ON | - | `--no-rephase` |
| Random phase | ON | `--random-phase` | - |
| Clause minimization | ON | - | `--no-minimize` |
| On-the-fly subsumption | ON | - | `--no-subsumption` |
| Failed literal probing | ON | - | `--no-probing` |
| Blocked clause elimination | OFF | `--bce` | `--no-bce` |
| Bounded variable elimination | OFF | `--elim` | `--no-elim` |
| Vivification inprocessing | OFF | `--inprocess` | - |
| Local search | OFF | `--local-search` | - |
| DRAT proof logging | OFF | `--proof <file>` | - |

---

## Core Algorithm: CDCL (Conflict-Driven Clause Learning)

### 1. **VSIDS (Variable State Independent Decaying Sum)** - DEFAULT: ON
- **Purpose**: Intelligently choose which variable to assign next
- **Implementation**: Binary max-heap ordered by activity scores
- **Activity decay**: 0.95 (configurable via `--var-decay`)
- **Bump strategy**: Increase activity for variables in conflict clauses

**Configuration:**
```bash
--vsids              # Use VSIDS (default)
--var-decay <f>      # Activity decay factor (default: 0.95)
--var-inc <f>        # Activity increment (default: 1.0)
```

### 2. **LRB/CHB (Learning Rate Branching)** - DEFAULT: OFF
- **Purpose**: Alternative to VSIDS based on learning rate
- **Implementation**: Hybrid VSIDS+LRB with recency-weighted bumps
- **Strategy**: Variables recently involved in conflicts get higher weight
- **Reference**: Liang et al. (2016) - "Learning Rate Based Branching Heuristic"

**Configuration:**
```bash
--lrb                # Enable LRB/CHB branching
--vsids              # Disable LRB, use VSIDS (default)
```

**Impact**: Different behavior on different instances - neither universally better.

### 3. **Two-Watched Literals** - ALWAYS ON
- **Purpose**: O(1) propagation overhead per clause
- **Implementation**: Each clause watched by exactly 2 literals
- **Watch update**: Only when a watched literal becomes false

### 4. **First-UIP Clause Learning** - ALWAYS ON
- **Purpose**: Learn high-quality clauses from conflicts
- **Strategy**: First Unique Implication Point (asserting clause)
- **LBD tracking**: Literal Block Distance for clause quality measurement

---

## Phase Management

### 5. **Phase Saving** - DEFAULT: ON
- **Purpose**: Remember variable polarities across restarts
- **Strategy**: Save last assigned polarity for each variable
- **Benefit**: Quickly return to promising search areas

**Configuration:**
```bash
--no-phase-saving    # Disable phase saving
```

### 6. **Target Phase Rephasing (Kissat-style)** - DEFAULT: ON
- **Purpose**: Escape local search areas by resetting phases
- **Strategy**: Track best assignment seen (most variables assigned before conflict)
- **Action**: Periodically reset saved phases to best phases
- **Interval**: Every 1000 conflicts (configurable)
- **Reference**: Biere et al. (2020) - Kissat solver

**Configuration:**
```bash
--no-rephase             # Disable rephasing
--rephase-interval <n>   # Conflicts between rephases (default: 1000)
```

**Impact**: Up to 58% fewer conflicts on some instances.

### 7. **Random Phase Selection** - DEFAULT: ON (1%)
- **Purpose**: Escape local minima and prevent stuck states
- **Probability**: 1% random phase by default
- **Adaptive**: Increases randomness when solver is stuck

**Configuration:**
```bash
--random-phase           # Enable random phase (default: on)
--random-prob <f>        # Random phase probability (default: 0.01)
```

---

## Restart Strategies

### 8. **Glucose Adaptive Restarts (EMA)** - DEFAULT: ON
- **Purpose**: Restart when learning quality degrades
- **Strategy**: Compare fast vs slow exponential moving averages of LBD
- **Restart when**: `fast_MA > slow_MA`
- **Reference**: Audemard & Simon (2009)

**Configuration:**
```bash
--glucose-restart        # Enable Glucose restarts (default)
--glucose-restart-ema    # Glucose with EMA (conservative)
--glucose-fast-alpha <f> # Fast MA decay (default: 0.8)
--glucose-slow-alpha <f> # Slow MA decay (default: 0.9999)
--glucose-min-conflicts <n>  # Min conflicts before Glucose (default: 100)
```

### 9. **Glucose Adaptive Restarts (AVG)** - DEFAULT: OFF
- **Purpose**: More aggressive restart strategy
- **Strategy**: Sliding window averaging
- **Restart when**: Short-term average > threshold * long-term average

**Configuration:**
```bash
--glucose-restart-avg    # Enable aggressive mode
--glucose-window-size <n> # Window size (default: 50)
--glucose-k <f>          # Threshold multiplier (default: 0.8)
```

### 10. **Luby Sequence Restarts** - DEFAULT: ON (fallback)
- **Purpose**: Provably good restart sequence
- **Sequence**: 1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, ...

**Configuration:**
```bash
--restart-first <n>      # First restart interval (default: 100)
--restart-inc <f>        # Restart multiplier (default: 1.5)
--no-restarts            # Disable all restarts
```

---

## Clause Management

### 11. **MiniSat-Style Clause Minimization** - DEFAULT: ON
- **Purpose**: Remove redundant literals from learned clauses
- **Strategy**: Recursive analysis with abstract level pruning
- **Results**: **67% literal reduction** on test instances

**Configuration:**
```bash
--no-minimize            # Disable minimization
```

### 12. **On-the-Fly Backward Subsumption** - DEFAULT: ON
- **Purpose**: Remove redundant clauses during learning
- **Strategy**: Check if new learned clause subsumes existing clauses
- **Results**: **73% subsumption rate** on UNSAT instances

**Configuration:**
```bash
--no-subsumption         # Disable subsumption
```

### 13. **LBD-Based Clause Database Reduction** - DEFAULT: ON
- **Purpose**: Limit memory and focus on high-quality clauses
- **Strategy**: LBD-based sorting + activity tiebreaking
- **Keep**: Best 50% of learned clauses
- **Protection**: Never delete glue clauses (LBD <= 2)

**Configuration:**
```bash
--max-lbd <n>            # Max LBD for keeping clauses (default: 30)
--glue-lbd <n>           # Glue clause threshold (default: 2)
--reduce-fraction <f>    # Fraction to keep (default: 0.5)
--reduce-interval <n>    # Conflicts between reductions (default: 2000)
```

---

## Preprocessing

### 14. **Failed Literal Probing** - DEFAULT: ON
- **Purpose**: Discover implied unit clauses
- **Strategy**: For each variable, try both polarities and check for conflict
- **When**: Called at start before main CDCL loop

**Configuration:**
```bash
--no-probing             # Disable probing
```

### 15. **Blocked Clause Elimination (BCE)** - DEFAULT: OFF
- **Purpose**: Remove blocked clauses before search
- **Theory**: A clause is blocked if all resolvents are tautologies
- **Results**: 10-20% clause elimination on typical instances

**Configuration:**
```bash
--bce                    # Enable BCE (experimental)
--no-bce                 # Disable BCE (default)
```

### 16. **Bounded Variable Elimination (BVE)** - DEFAULT: OFF
- **Purpose**: SatELite-style variable elimination
- **Strategy**: Resolve away variables if resolvent count doesn't increase
- **Model Extension**: Reconstructs eliminated variables after solving
- **Reference**: Een & Biere (2005) - SatELite

**Configuration:**
```bash
--elim                   # Enable BVE (experimental)
--no-elim                # Disable BVE (default)
--elim-max-occ <n>       # Max occurrences to consider (default: 10)
--elim-grow <n>          # Max clause growth allowed (default: 0)
```

---

## Inprocessing

### 17. **Vivification** - DEFAULT: OFF
- **Purpose**: Strengthen clauses during search
- **Strategy**: Check if literals can be proven redundant via propagation
- **When**: Runs periodically during search

**Configuration:**
```bash
--inprocess              # Enable vivification
--inprocess-interval <n> # Conflicts between runs (default: 10000)
```

---

## Local Search Hybridization

### 18. **WalkSAT Local Search** - DEFAULT: OFF
- **Purpose**: Complement CDCL with local search for SAT instances
- **Strategy**: WalkSAT with configurable noise parameter
- **When**: Periodically called during CDCL search
- **Benefits**: Can find SAT solutions faster than pure CDCL

**Configuration:**
```bash
--local-search           # Enable local search
--ls-interval <n>        # Conflicts between calls (default: 5000)
--ls-max-flips <n>       # Max flips per call (default: 100000)
--ls-noise <f>           # Noise parameter 0.0-1.0 (default: 0.5)
```

**Impact**: Reduced conflicts on SAT instances (e.g., 500 vs 2114).

---

## Proof Logging

### 19. **DRAT Proof Logging** - DEFAULT: OFF
- **Purpose**: Generate verifiable proofs for UNSAT instances
- **Format**: DRAT (Deletion Resolution Asymmetric Tautology)
- **Usage**: Verify with drat-trim or other DRAT checkers

**Configuration:**
```bash
--proof <file>           # Write DRAT proof to file
--binary-proof           # Use binary format (more compact)
```

**Verification:**
```bash
./bin/bsat --proof proof.drat instance.cnf
drat-trim instance.cnf proof.drat
# Expected: s VERIFIED
```

---

## Additional Features

### 20. **Chronological Backtracking** - ALWAYS ON
- **Purpose**: Preserve more learned information
- **Strategy**: Backtrack one level at a time checking for unit clauses

### 21. **Signal-Based Progress Monitoring** - ALWAYS ON
- **Purpose**: Monitor solver progress without interrupting
- **Method**: Send SIGUSR1 to get current statistics

**Usage:**
```bash
./bin/bsat instance.cnf &
kill -USR1 <pid>         # Get progress update
```

---

## Complete Command-Line Reference

### Output Control
```bash
-h, --help               # Show help message
-v, --verbose            # Verbose runtime diagnostics (default: OFF)
    --debug              # Debug output (default: OFF)
-q, --quiet              # Suppress output except result (default: OFF)
-s, --stats              # Print statistics (default: ON)
```

### Resource Limits
```bash
-c, --conflicts <n>      # Max conflicts (default: unlimited)
-d, --decisions <n>      # Max decisions (default: unlimited)
-t, --time <sec>         # Time limit in seconds (default: unlimited)
```

### Branching Heuristic
```bash
--vsids                  # Use VSIDS (default: ON)
--lrb                    # Use LRB/CHB (default: OFF)
--var-decay <f>          # VSIDS decay factor (default: 0.95)
--var-inc <f>            # VSIDS increment (default: 1.0)
```

### Restart Parameters
```bash
--restart-first <n>      # First restart interval (default: 100)
--restart-inc <f>        # Restart multiplier (default: 1.5)
--glucose-restart        # Glucose EMA restarts (default: ON)
--glucose-restart-ema    # Glucose EMA mode (default: ON)
--glucose-restart-avg    # Glucose AVG mode (default: OFF)
--no-restarts            # Disable all restarts
```

### Glucose Tuning
```bash
--glucose-fast-alpha <f>     # Fast MA decay (default: 0.8)
--glucose-slow-alpha <f>     # Slow MA decay (default: 0.9999)
--glucose-min-conflicts <n>  # Min conflicts (default: 100)
--glucose-window-size <n>    # AVG window size (default: 50)
--glucose-k <f>              # AVG threshold (default: 0.8)
```

### Phase Management
```bash
--no-phase-saving        # Disable phase saving (default: ON)
--random-phase           # Enable random phase (default: ON)
--random-prob <f>        # Random probability (default: 0.01)
--no-rephase             # Disable rephasing (default: ON)
--rephase-interval <n>   # Rephase interval (default: 1000)
```

### Clause Management
```bash
--max-lbd <n>            # Max LBD (default: 30)
--glue-lbd <n>           # Glue threshold (default: 2)
--reduce-fraction <f>    # Keep fraction (default: 0.5)
--reduce-interval <n>    # Reduce interval (default: 2000)
--no-minimize            # Disable minimization (default: ON)
--no-subsumption         # Disable subsumption (default: ON)
```

### Preprocessing
```bash
--no-bce                 # Disable BCE (default: OFF)
--elim                   # Enable BVE (default: OFF)
--no-elim                # Disable BVE (default)
--elim-max-occ <n>       # BVE max occurrences (default: 10)
--elim-grow <n>          # BVE max growth (default: 0)
--no-probing             # Disable probing (default: ON)
```

### Inprocessing
```bash
--inprocess              # Enable vivification (default: OFF)
--inprocess-interval <n> # Vivification interval (default: 10000)
```

### Local Search
```bash
--local-search           # Enable local search (default: OFF)
--ls-interval <n>        # LS interval (default: 5000)
--ls-max-flips <n>       # Max flips (default: 100000)
--ls-noise <f>           # Noise parameter (default: 0.5)
```

### Proof Logging
```bash
--proof <file>           # Write DRAT proof (default: OFF)
--binary-proof           # Binary format (default: OFF)
```

---

## Performance Results

### Test Suite: 53 Medium/Hard Instances
- **All tests pass**: 53/53 passed
- **Correctness**: All SAT/UNSAT results verified
- **DRAT proofs**: All UNSAT proofs verify

### Performance vs Python CDCL

| Test Set | C Solver | Python Solver | Speedup |
|----------|----------|---------------|---------|
| 5 hard instances | 0.010s | 2.258s | **226x** |
| 53 all instances | 0.082s | ~60s | **700x+** |

### Feature Impact

| Feature | Impact |
|---------|--------|
| Clause minimization | 67% literal reduction |
| On-the-fly subsumption | 73% clauses subsumed |
| Target rephasing | Up to 58% fewer conflicts |
| Local search | Instant SAT solutions |

---

## References

- **MiniSat**: Eén & Sörensson (2003)
- **Glucose**: Audemard & Simon (2009)
- **Chaff/VSIDS**: Moskewicz et al. (2001)
- **MapleSat/LRB**: Liang et al. (2016)
- **Lingeling**: Biere (2010-2020)
- **Kissat**: Biere et al. (2020)
- **SatELite**: Eén & Biere (2005)

---

**Last Updated**: December 2025 | **Version**: 1.2
