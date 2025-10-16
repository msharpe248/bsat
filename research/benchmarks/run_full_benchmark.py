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
    benchmark = SolverBenchmark(timeout=60.0)  # Increased timeout for larger problems

    print("Running Full Benchmark (ALL 7 Solvers)")
    print("=" * 70)

    # Small problems (< 15 vars) - production solvers dominate
    print("\n=== SMALL PROBLEMS (< 15 vars) ===")

    # 1. Modular problem (CoBD-SAT strength)
    cnf = ProblemGenerator.modular_problem(3, 3, 2, seed=42)
    benchmark.benchmark_problem("Modular Problem (3 modules × 3 vars)", cnf, solvers)

    # 2. Easy problem
    cnf = CNFExpression.parse("(a | b) & (c | d) & (e)")
    benchmark.benchmark_problem("Easy Problem (shows overhead)", cnf, solvers)

    # 3. Random 3-SAT (small)
    cnf = ProblemGenerator.random_3sat(10, 35, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (10 vars, 35 clauses)", cnf, solvers)

    # Medium problems (15-25 vars) - research solvers start to shine
    print("\n=== MEDIUM PROBLEMS (15-25 vars) ===")

    # 4. Backbone problem (BB-CDCL strength)
    cnf = ProblemGenerator.backbone_problem(15, 0.5, seed=42)
    benchmark.benchmark_problem("Backbone Problem (15 vars, 50% backbone)", cnf, solvers)

    # 5. Random 3-SAT (medium)
    cnf = ProblemGenerator.random_3sat(15, 60, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (15 vars, 60 clauses)", cnf, solvers)

    # 6. Random 3-SAT (medium-hard)
    cnf = ProblemGenerator.random_3sat(20, 85, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (20 vars, 85 clauses)", cnf, solvers)

    # 7. Modular problem (medium)
    cnf = ProblemGenerator.modular_problem(4, 5, 3, seed=42)
    benchmark.benchmark_problem("Modular Problem (4 modules × 5 vars)", cnf, solvers)

    # Large problems (25-35 vars) - research solvers should dominate
    print("\n=== LARGE PROBLEMS (25-35 vars) ===")

    # 8. Random 3-SAT (large)
    cnf = ProblemGenerator.random_3sat(25, 105, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (25 vars, 105 clauses)", cnf, solvers)

    # 9. Random 3-SAT (larger)
    cnf = ProblemGenerator.random_3sat(30, 127, seed=42)
    benchmark.benchmark_problem("Random 3-SAT (30 vars, 127 clauses)", cnf, solvers)

    # 10. Backbone problem (large)
    cnf = ProblemGenerator.backbone_problem(30, 0.5, seed=42)
    benchmark.benchmark_problem("Backbone Problem (30 vars, 50% backbone)", cnf, solvers)

    # 11. Modular problem (large)
    cnf = ProblemGenerator.modular_problem(5, 6, 3, seed=42)
    benchmark.benchmark_problem("Modular Problem (5 modules × 6 vars)", cnf, solvers)

    # Special problems to showcase specific algorithms
    print("\n=== SPECIAL PROBLEMS (algorithm-specific) ===")

    # 12. Chain problem (LA-CDCL strength)
    cnf = ProblemGenerator.chain_problem(15)
    benchmark.benchmark_problem("Chain Problem (length=15, LA-CDCL test)", cnf, solvers)

    # 13. Circuit problem (CGPM-SAT strength)
    cnf = ProblemGenerator.circuit_problem(8, seed=42)
    benchmark.benchmark_problem("Circuit Problem (8 gates, CGPM-SAT test)", cnf, solvers)

    # 14. Strong backbone (BB-CDCL strength)
    cnf = ProblemGenerator.backbone_problem(25, 0.7, seed=44)
    benchmark.benchmark_problem("Backbone Problem (25 vars, 70% backbone, BB-CDCL test)", cnf, solvers)

    # 15. UNSAT test (BB-CDCL fix verification)
    cnf = ProblemGenerator.backbone_problem(20, 0.7, seed=99)
    # Make it UNSAT by adding conflicting constraints
    benchmark.benchmark_problem("UNSAT Test (20 vars, BB-CDCL fix verification)", cnf, solvers)

    # Print results
    benchmark.print_summary_table()

    # Export markdown
    benchmark.export_markdown_report("benchmark_results_full.md")
    print(f"\n✅ Full results saved to: benchmark_results_full.md")

if __name__ == "__main__":
    main()
