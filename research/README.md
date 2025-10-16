# BSAT Research

This directory contains experimental and research implementations of novel SAT solving algorithms. These solvers are designed to explore new approaches to Boolean satisfiability that go beyond traditional algorithms.

## Purpose

The research directory serves as a sandbox for:

- **Novel Algorithm Development**: Implementing and testing new SAT solving approaches
- **Theoretical Exploration**: Investigating algorithms that exploit problem structure
- **Educational Value**: Demonstrating advanced concepts in SAT solving
- **Performance Experiments**: Comparing new approaches against established solvers

## Current Research Projects

### Community-Based Decomposition SAT (CoBD-SAT)

**Status**: In Development

**Location**: `cobd_sat/`

**Description**: A novel SAT solver that exploits community structure in the variable-clause interaction graph. The algorithm decomposes the problem into semi-independent communities and coordinates their solutions through message passing.

**Key Features**:
- Graph-based problem decomposition
- Modularity-based community detection
- Parallel/independent community solving
- Interface variable coordination
- Potential for sub-exponential performance on structured instances

**See**: `cobd_sat/README.md` for detailed algorithm description

## Analysis Documents

### Novel SAT Approaches Analysis

**Location**: `analysis/novel_sat_approaches.md`

A comprehensive analysis of potential novel SAT solving approaches, including:
1. Community-Based Decomposition SAT (CoBD-SAT)
2. Probabilistic Backbone Detection + CDCL (BB-CDCL)
3. Lookahead-Enhanced CDCL (LA-CDCL)
4. Conflict Graph Pattern Mining (CGPM-SAT)

Each approach is evaluated for theoretical soundness, practical potential, and implementation complexity.

## Usage

Research solvers can be imported from the `research` directory:

```python
from research.cobd_sat.cobd_solver import CoBDSATSolver
from bsat import CNFExpression

# Create a CNF formula
cnf = CNFExpression.parse("(a | b) & (b | c) & (c | d)")

# Solve using CoBD-SAT
solver = CoBDSATSolver(cnf)
result = solver.solve()

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

## Examples

See `examples/` directory for usage examples of each research solver.

## Integration with Visualization Server

Research solvers can be integrated into the visualization server to provide interactive demonstrations of their algorithms. This helps with:
- Understanding algorithm behavior
- Debugging and refinement
- Educational presentations
- Performance analysis

## Contributing

When adding new research algorithms:

1. Create a subdirectory for the algorithm (e.g., `new_algorithm/`)
2. Include a `README.md` with:
   - Algorithm description and pseudocode
   - Theoretical analysis (complexity, completeness)
   - Advantages/disadvantages
   - References to literature
3. Implement the solver following patterns from existing BSAT solvers
4. Add examples demonstrating the algorithm
5. Update this README with the new project

## Comparison with Production Solvers

These research implementations prioritize clarity and educational value over performance. For production SAT solving, consider:

- **MiniSat**: Classic CDCL implementation
- **CryptoMiniSat**: Advanced CDCL with XOR reasoning
- **Glucose**: Aggressive clause learning
- **Lingeling**: Highly optimized with inprocessing
- **CaDiCaL**: Modern CDCL with chronological backtracking

However, research solvers may outperform production solvers on specific structured instances where they can exploit problem characteristics.

## License

Same as BSAT package (MIT License)
