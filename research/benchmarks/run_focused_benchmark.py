#!/usr/bin/env python3
"""
Focused benchmark showcasing each solver's strengths.
Smaller, faster problems that complete quickly.
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from benchmark import SolverBenchmark, ProblemGenerator
from bsat import CNFExpression

def main():
    """Run focused benchmark."""
    solvers = ["DPLL", "CDCL", "Schöning", "CoBD-SAT", "BB-CDCL", "LA-CDCL", "CGPM-SAT"]
    benchmark = SolverBenchmark(timeout=5.0)

    print("Running Focused Benchmark Suite (Fast)")
    print("=" * 70)

    # 1. Small modular problem (CoBD-SAT strength)
    cnf = ProblemGenerator.modular_problem(3, 3, 2, seed=42)
    benchmark.benchmark_problem("Modular (3 modules, 3 vars each)", cnf, solvers)

    # 2. Small backbone problem (BB-CDCL strength)
    cnf = ProblemGenerator.backbone_problem(15, 0.5, seed=42)
    benchmark.benchmark_problem("Backbone (15 vars, 50% backbone)", cnf, solvers)

    # 3. Chain problem (LA-CDCL strength)
    cnf = ProblemGenerator.chain_problem(8)
    benchmark.benchmark_problem("Chain (length=8)", cnf, solvers)

    # 4. Small circuit (CGPM-SAT strength)
    cnf = ProblemGenerator.circuit_problem(4, seed=42)
    benchmark.benchmark_problem("Circuit (4 gates)", cnf, solvers)

    # 5. Random 3-SAT (baseline)
    cnf = ProblemGenerator.random_3sat(12, 40, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (12 vars, 40 clauses)", cnf, solvers)

    # 6. Easy problem (show overhead)
    cnf = CNFExpression.parse("(a | b) & (c | d) & (e)")
    benchmark.benchmark_problem("Easy problem (3 clauses)", cnf, solvers)

    # Print results
    benchmark.print_summary_table()

    # Export markdown
    benchmark.export_markdown_report("benchmark_results.md")
    print(f"\n✅ Results saved to: benchmark_results.md")

if __name__ == "__main__":
    main()
