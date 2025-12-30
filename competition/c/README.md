# BSAT C Solver - Production-Ready CDCL Implementation

A high-performance SAT solver implementing modern CDCL (Conflict-Driven Clause Learning) with state-of-the-art optimizations including LRB branching, target phase rephasing, local search hybridization, and DRAT proof logging.

## Status: Production Ready

**Complete CDCL implementation with competition-level features**

### Test Results
- **100% success rate** on medium test suite (53/53 instances)
- **100-226x faster than Python** on hard instances
- **Solves hard instances in 0.01s** that Python takes 2+ seconds

---

## Quick Start

```bash
# Build the solver
make

# Solve a CNF instance
./bin/bsat instance.cnf

# With all optimizations enabled (LRB + local search)
./bin/bsat --lrb --local-search instance.cnf

# Generate DRAT proof for UNSAT instances
./bin/bsat --proof proof.drat instance.cnf

# Test on medium test suite (53 instances)
./scripts/test_medium_suite.sh
```

---

## Features

### Core CDCL Algorithm
- **Two-watched literals** for efficient unit propagation
- **Conflict analysis** with 1-UIP learning scheme
- **MiniSat-style clause minimization** with abstract level pruning (67% literal reduction)
- **Chronological backtracking** for improved search efficiency

### Modern Optimizations

#### Variable Ordering
| Feature | Default | Flag | Description |
|---------|---------|------|-------------|
| **VSIDS** | ON | `--vsids` | Dynamic activity scoring with decay |
| **LRB/CHB** | OFF | `--lrb` | Learning Rate Branching with recency weighting |

#### Restart Strategies
| Strategy | Default | Flag | Description |
|----------|---------|------|-------------|
| **Luby Sequence** | ON (fallback) | - | Provably good for all instance types |
| **Glucose EMA** | ON | `--glucose-restart-ema` | Conservative, paper-accurate |
| **Glucose AVG** | OFF | `--glucose-restart-avg` | Aggressive, Python-style |

#### Phase Management
| Feature | Default | Flag | Description |
|---------|---------|------|-------------|
| **Phase Saving** | ON | `--no-phase-saving` | Saves polarities across restarts |
| **Target Rephasing** | ON | `--no-rephase` | Kissat-style periodic phase reset |
| **Random Phase** | ON (1%) | `--random-prob <f>` | Prevents stuck states |

#### Clause Management
| Feature | Default | Flag | Description |
|---------|---------|------|-------------|
| **Clause Minimization** | ON | `--no-minimize` | MiniSat-style recursive minimization |
| **On-the-fly Subsumption** | ON | `--no-subsumption` | Removes subsumed clauses |
| **LBD-based Reduction** | ON | `--reduce-interval <n>` | Keeps best 50% of clauses |
| **Glue Protection** | ON | `--glue-lbd <n>` | Never deletes LBD <= 2 |

#### Preprocessing
| Feature | Default | Flag | Description |
|---------|---------|------|-------------|
| **Failed Literal Probing** | ON | `--no-probing` | Discovers implied units |
| **Blocked Clause Elimination** | OFF | `--bce` | Removes redundant clauses |
| **Bounded Variable Elimination** | OFF | `--elim` | SatELite-style BVE |

#### Inprocessing
| Feature | Default | Flag | Description |
|---------|---------|------|-------------|
| **Vivification** | OFF | `--inprocess` | Strengthens learned clauses |

#### Local Search Hybridization
| Feature | Default | Flag | Description |
|---------|---------|------|-------------|
| **WalkSAT** | OFF | `--local-search` | Periodic local search for SAT |

#### Proof Logging
| Feature | Default | Flag | Description |
|---------|---------|------|-------------|
| **DRAT Proofs** | OFF | `--proof <file>` | Generates verifiable proofs |

---

## Command-Line Reference

### Basic Usage
```bash
./bin/bsat [OPTIONS] <input.cnf>
```

### Output Control
| Flag | Default | Description |
|------|---------|-------------|
| `-h, --help` | - | Show help message |
| `-v, --verbose` | OFF | Verbose runtime diagnostics |
| `--debug` | OFF | Debug output |
| `-q, --quiet` | OFF | Suppress all output except result |
| `-s, --stats` | ON | Print statistics |

### Resource Limits
| Flag | Default | Description |
|------|---------|-------------|
| `-c, --conflicts <n>` | unlimited | Maximum number of conflicts |
| `-d, --decisions <n>` | unlimited | Maximum number of decisions |
| `-t, --time <sec>` | unlimited | Time limit in seconds |

### Branching Heuristic
| Flag | Default | Description |
|------|---------|-------------|
| `--vsids` | ON | Use VSIDS variable ordering |
| `--lrb` | OFF | Use LRB/CHB (Learning Rate Branching) |
| `--var-decay <f>` | 0.95 | Variable activity decay for VSIDS |
| `--var-inc <f>` | 1.0 | Variable activity increment |

### Restart Parameters
| Flag | Default | Description |
|------|---------|-------------|
| `--restart-first <n>` | 100 | First restart interval |
| `--restart-inc <f>` | 1.5 | Restart interval multiplier |
| `--glucose-restart` | ON | Use Glucose adaptive restarts (EMA mode) |
| `--glucose-restart-ema` | ON | Glucose with EMA (conservative) |
| `--glucose-restart-avg` | OFF | Glucose with sliding window (aggressive) |
| `--no-restarts` | - | Disable all restarts |

### Glucose EMA Tuning (with `--glucose-restart` or `--glucose-restart-ema`)
| Flag | Default | Description |
|------|---------|-------------|
| `--glucose-fast-alpha <f>` | 0.8 | Fast MA decay factor |
| `--glucose-slow-alpha <f>` | 0.9999 | Slow MA decay factor |
| `--glucose-min-conflicts <n>` | 100 | Min conflicts before Glucose kicks in |

### Glucose AVG Tuning (with `--glucose-restart-avg`)
| Flag | Default | Description |
|------|---------|-------------|
| `--glucose-window-size <n>` | 50 | Window size for short-term average |
| `--glucose-k <f>` | 0.8 | Threshold multiplier |

### Phase Saving
| Flag | Default | Description |
|------|---------|-------------|
| `--no-phase-saving` | - | Disable phase saving (ON by default) |
| `--random-phase` | ON | Enable random phase selection |
| `--random-prob <f>` | 0.01 | Random phase probability (1%) |
| `--no-rephase` | - | Disable target phase rephasing (ON by default) |
| `--rephase-interval <n>` | 1000 | Conflicts between rephases |

### Clause Management
| Flag | Default | Description |
|------|---------|-------------|
| `--max-lbd <n>` | 30 | Max LBD for keeping learned clauses |
| `--glue-lbd <n>` | 2 | LBD threshold for glue clauses |
| `--reduce-fraction <f>` | 0.5 | Fraction of clauses to keep |
| `--reduce-interval <n>` | 2000 | Conflicts between reductions |
| `--no-minimize` | - | Disable clause minimization (ON by default) |
| `--no-subsumption` | - | Disable on-the-fly subsumption (ON by default) |

### Preprocessing
| Flag | Default | Description |
|------|---------|-------------|
| `--no-bce` | - | Disable blocked clause elimination (OFF by default) |
| `--elim` | OFF | Enable bounded variable elimination (BVE) |
| `--no-elim` | ON | Disable BVE (default) |
| `--elim-max-occ <n>` | 10 | Max occurrences to consider for BVE |
| `--elim-grow <n>` | 0 | Max clause growth allowed for BVE |
| `--no-probing` | - | Disable failed literal probing (ON by default) |

### Inprocessing
| Flag | Default | Description |
|------|---------|-------------|
| `--inprocess` | OFF | Enable inprocessing (vivification) |
| `--inprocess-interval <n>` | 10000 | Conflicts between inprocessing |

### Local Search Hybridization
| Flag | Default | Description |
|------|---------|-------------|
| `--local-search` | OFF | Enable WalkSAT-style local search |
| `--ls-interval <n>` | 5000 | Conflicts between local search calls |
| `--ls-max-flips <n>` | 100000 | Max flips per local search call |
| `--ls-noise <f>` | 0.5 | WalkSAT noise parameter (0.0-1.0) |

### Proof Logging
| Flag | Default | Description |
|------|---------|-------------|
| `--proof <file>` | OFF | Write DRAT proof to file |
| `--binary-proof` | OFF | Use binary DRAT format (more compact) |

---

## Default Configuration Summary

### Features ON by Default
- VSIDS variable ordering
- Glucose EMA restarts
- Phase saving
- Target phase rephasing (every 1000 conflicts)
- Random phase selection (1% probability)
- Clause minimization (MiniSat-style)
- On-the-fly subsumption
- Failed literal probing
- LBD-based clause reduction

### Features OFF by Default (opt-in)
- LRB/CHB branching (`--lrb`)
- Bounded Variable Elimination (`--elim`)
- Blocked Clause Elimination (`--bce`)
- Vivification inprocessing (`--inprocess`)
- Local search hybridization (`--local-search`)
- DRAT proof logging (`--proof <file>`)

---

## Output Format

Standard DIMACS output format:

```
c BSAT Competition Solver v1.1
c PID: 12345 (send SIGUSR1 for progress: kill -USR1 12345)
c Reading from instance.cnf
c Variables: 100
c Clauses:   400
c
s SATISFIABLE
v -1 2 -3 4 -5 6 ... 0
c
c CPU time:         0.042 s
c
c ========== Statistics ==========
c CPU time          : 0.042 s
c Decisions         : 1234
c Propagations      : 5678
c Conflicts         : 234
c Restarts          : 12
c Learned clauses   : 234
c Learned literals  : 1012
c Deleted clauses   : 0
c Blocked clauses   : 15
c Subsumed clauses  : 45
c Minimized literals: 89
c Glue clauses      : 23
c Max LBD           : 8
```

---

## Testing

### Full Test Suite
```bash
# Run all 53 medium test instances
./scripts/test_medium_suite.sh

# Expected output:
# Passed:  53
# Timeout: 0
# Total:   53
```

### Verify DRAT Proofs
```bash
# Generate proof for UNSAT instance
./bin/bsat --proof proof.drat unsat_instance.cnf

# Verify with drat-trim
drat-trim unsat_instance.cnf proof.drat
# Expected: s VERIFIED
```

### Performance Comparison
```bash
# Compare C vs Python on test instances
./scripts/compare_c_vs_python.sh

# Benchmark on custom instances
./scripts/benchmark.sh instance1.cnf instance2.cnf ...
```

---

## Architecture

### Directory Structure
```
competition/c/
├── src/                  # Source files
│   ├── main.c            # Main entry point and CLI
│   ├── solver.c          # Core CDCL solver implementation
│   ├── dimacs.c          # DIMACS format parser
│   ├── arena.c           # Memory arena for clause storage
│   ├── watch.c           # Two-watched literal manager
│   ├── elim.c            # Bounded variable elimination (BVE)
│   └── local_search.c    # WalkSAT local search
├── include/              # Header files
│   ├── solver.h          # Solver interface and options
│   ├── types.h           # Core type definitions
│   ├── dimacs.h          # DIMACS parser interface
│   ├── arena.h           # Arena allocator interface
│   ├── watch.h           # Watch manager interface
│   ├── elim.h            # BVE interface
│   └── local_search.h    # Local search interface
├── tests/                # Unit tests
├── scripts/              # Utility scripts
├── docs/                 # Detailed documentation
├── bin/                  # Compiled binaries
├── build/                # Build artifacts
├── FEATURES.md           # Detailed feature list
└── README.md             # This file
```

### Core Data Structures
- **VarInfo[]**: Per-variable info (value, level, reason, polarity, activity)
- **Trail**: Assignment stack with decision levels
- **WatchManager**: Two-watched literals for each clause
- **Arena**: Compact clause storage with reference-based access
- **LocalSearchState**: Occurrence lists and break counts for WalkSAT

---

## Performance

### vs Python Competition Solver

| Metric | C Solver | Python Solver | Speedup |
|--------|----------|---------------|---------|
| **Hard instances (5)** | 0.010s | 2.258s | **226x** |
| **All instances (53)** | 0.082s | ~60s | **700x+** |

### Feature Impact

| Optimization | Impact |
|--------------|--------|
| MiniSat clause minimization | 67% literal reduction |
| On-the-fly subsumption | 73% clauses subsumed (UNSAT) |
| Target rephasing | Up to 58% fewer conflicts |
| Local search | Instant SAT solutions |

---

## Build System

### Makefile Targets
```bash
make              # Build optimized binary (bin/bsat)
make MODE=debug   # Build with debug symbols and sanitizers
make MODE=profile # Build for profiling
make test         # Run all unit tests
make clean        # Remove build artifacts
make pgo          # Full Profile-Guided Optimization build
make help         # Show available targets
```

### Compiler Requirements
- C11 standard
- GCC or Clang (Clang recommended for PGO)
- Tested on macOS (Apple Clang) and Linux (GCC)

---

## References

### Academic Papers
1. **CDCL**: Marques-Silva & Sakallah (1996) - GRASP
2. **VSIDS**: Moskewicz et al. (2001) - Chaff
3. **Glucose**: Audemard & Simon (2009) - LBD and adaptive restarts
4. **MapleSat/LRB**: Liang et al. (2016) - Learning Rate Branching
5. **Phase Saving**: Pipatsrisawat & Darwiche (2007)
6. **SatELite/BVE**: Eén & Biere (2005) - Variable elimination
7. **Kissat**: Biere et al. (2020) - Target phases, state-of-the-art techniques

---

## Version History

### v1.2 (Current)
- LRB/CHB branching heuristic
- Target phase rephasing (Kissat-style)
- WalkSAT local search hybridization
- Bounded Variable Elimination (BVE)
- DRAT proof logging

### v1.1
- MiniSat-style clause minimization
- Failed literal probing
- Vivification inprocessing
- PGO build support

### v1.0
- Core CDCL with two-watched literals
- Glucose adaptive restarts
- Phase saving with random phase
- LBD-based clause management
