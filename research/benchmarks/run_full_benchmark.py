#!/usr/bin/env python3
"""
Full benchmark with ALL 7 solvers (now that LA-CDCL and CGPM-SAT are fixed).
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from benchmark import SolverBenchmark, ProblemGenerator
from bsat import CNFExpression

def main():
    """Run full benchmark with all solvers."""
    # ALL 7 solvers!
    solvers = ["DPLL", "CDCL", "Schöning", "CoBD-SAT", "BB-CDCL", "LA-CDCL", "CGPM-SAT"]
    benchmark = SolverBenchmark(timeout=10.0)

    print("Running Full Benchmark (ALL 7 Solvers)")
    print("=" * 70)

    # 1. Modular problem (CoBD-SAT strength)
    cnf = ProblemGenerator.modular_problem(3, 3, 2, seed=42)
    benchmark.benchmark_problem("Modular Problem (3 modules × 3 vars)", cnf, solvers)

    # 2. Backbone problem (BB-CDCL strength)
    cnf = ProblemGenerator.backbone_problem(15, 0.5, seed=42)
    benchmark.benchmark_problem("Backbone Problem (15 vars, 50% backbone)", cnf, solvers)

    # 3. Chain problem (LA-CDCL strength)
    cnf = ProblemGenerator.chain_problem(8)
    benchmark.benchmark_problem("Chain Problem (length=8, good for LA-CDCL)", cnf, solvers)

    # 4. Circuit problem (CGPM-SAT strength)
    cnf = ProblemGenerator.circuit_problem(4, seed=42)
    benchmark.benchmark_problem("Circuit Problem (4 gates, good for CGPM-SAT)", cnf, solvers)

    # 5. Random 3-SAT (small)
    cnf = ProblemGenerator.random_3sat(10, 35, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (10 vars, 35 clauses)", cnf, solvers)

    # 6. Random 3-SAT (medium - the one that hung before)
    cnf = ProblemGenerator.random_3sat(12, 40, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (12 vars, 40 clauses - was hanging)", cnf, solvers)

    # 7. Easy problem
    cnf = CNFExpression.parse("(a | b) & (c | d) & (e)")
    benchmark.benchmark_problem("Easy Problem (shows overhead)", cnf, solvers)

    # 8. Strong backbone
    cnf = ProblemGenerator.backbone_problem(18, 0.7, seed=44)
    benchmark.benchmark_problem("Strong Backbone (18 vars, 70% backbone)", cnf, solvers)

    # Print results
    benchmark.print_summary_table()

    # Export markdown
    benchmark.export_markdown_report("benchmark_results_full.md")
    print(f"\n✅ Full results saved to: benchmark_results_full.md")

if __name__ == "__main__":
    main()
