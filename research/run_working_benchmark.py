#!/usr/bin/env python3
"""
Benchmark with only working solvers.
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from benchmark import SolverBenchmark, ProblemGenerator
from bsat import CNFExpression

def main():
    """Run benchmark with working solvers only."""
    # Only solvers that complete quickly
    solvers = ["DPLL", "CDCL", "Schöning", "CoBD-SAT", "BB-CDCL"]
    benchmark = SolverBenchmark(timeout=5.0)

    print("Running Benchmark (Working Solvers)")
    print("=" * 70)

    # 1. Modular problem (CoBD-SAT strength)
    cnf = ProblemGenerator.modular_problem(3, 3, 2, seed=42)
    benchmark.benchmark_problem("Modular Problem (3 modules × 3 vars)", cnf, solvers)

    # 2. Backbone problem (BB-CDCL strength)
    cnf = ProblemGenerator.backbone_problem(15, 0.5, seed=42)
    benchmark.benchmark_problem("Backbone Problem (15 vars, 50% backbone)", cnf, solvers)

    # 3. Chain problem
    cnf = ProblemGenerator.chain_problem(8)
    benchmark.benchmark_problem("Chain Problem (length=8)", cnf, solvers)

    # 4. Circuit problem
    cnf = ProblemGenerator.circuit_problem(4, seed=42)
    benchmark.benchmark_problem("Circuit Problem (4 gates)", cnf, solvers)

    # 5. Random 3-SAT (small)
    cnf = ProblemGenerator.random_3sat(10, 35, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (10 vars, 35 clauses)", cnf, solvers)

    # 6. Easy problem
    cnf = CNFExpression.parse("(a | b) & (c | d) & (e)")
    benchmark.benchmark_problem("Easy Problem", cnf, solvers)

    # 7. Strong backbone
    cnf = ProblemGenerator.backbone_problem(18, 0.7, seed=44)
    benchmark.benchmark_problem("Strong Backbone (18 vars, 70% backbone)", cnf, solvers)

    # 8. Larger modular
    cnf = ProblemGenerator.modular_problem(4, 4, 2, seed=45)
    benchmark.benchmark_problem("Larger Modular (4 modules × 4 vars)", cnf, solvers)

    # Print results
    benchmark.print_summary_table()

    # Export markdown
    benchmark.export_markdown_report("benchmark_results.md")
    print(f"\n✅ Results saved to: benchmark_results.md")

if __name__ == "__main__":
    main()
