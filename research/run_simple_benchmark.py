#!/usr/bin/env python3
"""
Simple focused benchmark (excludes slow solvers).
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from benchmark import SolverBenchmark, ProblemGenerator
from bsat import CNFExpression

def main():
    """Run simple benchmark."""
    # Exclude CGPM-SAT which has a solving loop issue
    solvers = ["DPLL", "CDCL", "Schöning", "CoBD-SAT", "BB-CDCL", "LA-CDCL"]
    benchmark = SolverBenchmark(timeout=5.0)

    print("Running Simple Benchmark Suite")
    print("=" * 70)

    # 1. Modular problem (CoBD-SAT strength)
    cnf = ProblemGenerator.modular_problem(3, 3, 2, seed=42)
    benchmark.benchmark_problem("Modular Problem (3 modules × 3 vars)", cnf, solvers)

    # 2. Backbone problem (BB-CDCL strength)
    cnf = ProblemGenerator.backbone_problem(15, 0.5, seed=42)
    benchmark.benchmark_problem("Backbone Problem (15 vars, 50% backbone)", cnf, solvers)

    # 3. Chain problem (LA-CDCL strength)
    cnf = ProblemGenerator.chain_problem(8)
    benchmark.benchmark_problem("Chain Problem (length=8)", cnf, solvers)

    # 4. Circuit problem
    cnf = ProblemGenerator.circuit_problem(4, seed=42)
    benchmark.benchmark_problem("Circuit Problem (4 gates)", cnf, solvers)

    # 5. Random 3-SAT
    cnf = ProblemGenerator.random_3sat(12, 40, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (12 vars, 40 clauses)", cnf, solvers)

    # 6. Random 3-SAT (harder)
    cnf = ProblemGenerator.random_3sat(15, 60, seed=43)
    benchmark.benchmark_problem("Random 3-SAT (15 vars, 60 clauses)", cnf, solvers)

    # 7. Easy problem
    cnf = CNFExpression.parse("(a | b) & (c | d) & (e)")
    benchmark.benchmark_problem("Easy Problem (3 simple clauses)", cnf, solvers)

    # 8. Backbone with higher percentage
    cnf = ProblemGenerator.backbone_problem(20, 0.7, seed=44)
    benchmark.benchmark_problem("Strong Backbone (20 vars, 70% backbone)", cnf, solvers)

    # Print results
    benchmark.print_summary_table()

    # Export markdown
    benchmark.export_markdown_report("benchmark_results.md")
    print(f"\n✅ Results saved to: benchmark_results.md")

if __name__ == "__main__":
    main()
