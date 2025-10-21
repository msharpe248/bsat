# BSAT Competition Solver - C Implementation

A modern, high-performance SAT solver implementing state-of-the-art CDCL with advanced optimizations.

## Features

### Core CDCL Engine
- ✅ **Two-Watched Literals**: O(1) amortized unit propagation with blocking literals
- ⏳ **First UIP Learning**: Conflict analysis with on-the-fly minimization
- ⏳ **VSIDS Heuristic**: Variable State Independent Decaying Sum for decisions
- ⏳ **Non-chronological Backtracking**: Jump to asserting level

### Advanced Optimizations
- ⏳ **LBD Clause Management**: Literal Block Distance for clause quality
- ⏳ **Adaptive Restarts**: Glucose-style moving averages
- ⏳ **Phase Saving**: Remember variable polarities
- ⏳ **Random Phase Selection**: Escape local minima
- ⏳ **Restart Postponing**: Block restarts when making progress

### Inprocessing (Future)
- ⏳ **Variable Elimination**: Bounded variable elimination
- ⏳ **Subsumption**: Remove redundant clauses
- ⏳ **Failed Literal Probing**: Detect failed literals

## Architecture

### Data Structures
- **Arena Allocator**: Single contiguous memory region for all clauses
- **Compact Variables**: 2-bit values, efficient packing
- **Binary Heap**: For VSIDS variable ordering
- **Segmented Trail**: Decision stack with level markers

### Key Design Principles
1. **Cache-friendly**: Optimized data layouts for modern CPUs
2. **Memory-efficient**: Compact representations, arena allocation
3. **Modular**: Clean separation of concerns
4. **Fast**: Every hot path optimized

## Building

```bash
# Release build (optimized)
make

# Debug build (with sanitizers)
make MODE=debug

# Profile build
make MODE=profile

# Run tests
make test

# Clean
make clean
```

## Usage

```bash
# Solve a CNF file
./bin/bsat problem.cnf

# With options
./bin/bsat --verbose --stats problem.cnf

# With limits
./bin/bsat --max-conflicts=10000 --max-time=60 problem.cnf
```

## Performance Targets

- **Correctness**: 100% sound (no invalid solutions)
- **Speed**: Within 5x of Kissat on standard benchmarks
- **Memory**: <2GB for 1M variable instances
- **Scalability**: Support up to 500M variables

## Implementation Status

### Completed ✅
- Core type system and data structures
- Arena memory allocator
- Watch list infrastructure
- DIMACS parser
- Build system

### In Progress ⏳
- Two-watched literal propagation
- Conflict analysis
- VSIDS decision heuristic

### TODO 📝
- Restart strategies
- Clause database reduction
- Inprocessing techniques
- Parallel solving

## Testing

The solver includes comprehensive testing:
- Unit tests for each module
- Integration tests with small CNFs
- Regression tests against Python solver
- Benchmark suite from SAT competitions

## References

- **Kissat**: State-of-the-art solver by Armin Biere
- **Satch**: Educational solver with clean code
- **Glucose**: Adaptive restart strategies
- **MiniSat**: Original two-watched literal implementation

## License

MIT License - See LICENSE file for details.