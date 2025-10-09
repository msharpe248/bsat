# BSAT Documentation

Welcome to the BSAT (Boolean Satisfiability) package documentation! This guide will help you understand SAT problems and how to solve them using this library.

## Table of Contents

1. [Introduction to SAT](introduction.md) - Start here if you're new to SAT problems
2. [CNF Expressions](cnf.md) - Understanding the data structures
3. [Solvers](solvers.md) - Overview of available solvers
   - [2SAT Solver](2sat-solver.md) - Polynomial-time algorithm for 2SAT
   - [DPLL Solver](dpll-solver.md) - Classic backtracking algorithm for general SAT
   - [Horn-SAT Solver](hornsat-solver.md) - Polynomial-time solver for Horn formulas
   - [XOR-SAT Solver](xorsat-solver.md) - Polynomial-time solver for XOR constraints
   - [WalkSAT Solver](walksat-solver.md) - Randomized local search (incomplete but fast)
   - [Advanced Solvers](advanced-solvers.md) - Coming soon: CDCL
4. [Examples & Tutorials](examples.md) - Practical usage examples
5. [Theory & References](theory.md) - Academic papers and further reading

## Quick Start

### Installation

```bash
pip install bsat
```

### Basic Usage

```python
from bsat import CNFExpression, solve_sat

# Parse a CNF formula
formula = "(x | y) & (~x | z) & (~y | ~z)"
cnf = CNFExpression.parse(formula)

# Solve it
result = solve_sat(cnf)

if result:
    print(f"SAT: {result}")
else:
    print("UNSAT")
```

## What is SAT?

The **Boolean Satisfiability Problem (SAT)** asks: given a Boolean formula, is there an assignment of truth values to variables that makes the formula true?

For example:
- Formula: `(x ∨ y) ∧ (¬x ∨ z)`
- Question: Can we find values for x, y, z that make this true?
- Answer: Yes! For instance, x=True, y=True, z=True works.

## Why is SAT Important?

1. **First NP-Complete Problem**: SAT was the first problem proven to be NP-complete (Cook-Levin theorem, 1971)
2. **Practical Applications**: Used in:
   - Hardware verification
   - Software testing
   - Automated reasoning
   - Planning and scheduling
   - Cryptography
   - AI and machine learning
3. **Foundation for Other Problems**: Many problems can be reduced to SAT

## Package Features

- ✅ **CNF Data Structures**: Clean, Pythonic representation of Boolean formulas
- ✅ **2SAT Solver**: O(n+m) polynomial-time solver using strongly connected components
- ✅ **DPLL Solver**: Classic backtracking algorithm for general SAT/3SAT
- ✅ **Horn-SAT Solver**: O(n+m) polynomial-time solver for Horn formulas (logic programming)
- ✅ **XOR-SAT Solver**: O(n³) polynomial-time solver using Gaussian elimination over GF(2)
- ✅ **WalkSAT Solver**: Randomized local search (incomplete but often very fast)
- 🚧 **Advanced Algorithms**: CDCL (coming soon)
- ✅ **Pretty Printing**: Unicode symbols (∧, ∨, ¬) for readable output
- ✅ **Multiple Input Formats**: Parse from text, JSON, or programmatic construction
- ✅ **Truth Tables**: Generate and compare truth tables

## Next Steps

- **New to SAT?** Start with [Introduction to SAT](introduction.md)
- **Ready to code?** Check out [Examples & Tutorials](examples.md)
- **Want to understand the algorithms?** See the individual solver pages
- **Looking for theory?** Visit [Theory & References](theory.md)

## Contributing

This is an educational package designed to help people learn about SAT solving. Contributions, examples, and documentation improvements are welcome!

## License

MIT License - See LICENSE file for details.
