#!/usr/bin/env python3
"""
Example usage of CQP-SAT solver.

Demonstrates clause quality prediction (Glucose's LBD approach) on SAT instances.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from cqp_sat import CQPSATSolver, solve_cqp_sat


def main():
    print("=" * 70)
    print("CQP-SAT: Clause Quality Prediction SAT Solver Example")
    print("Educational reimplementation of Glucose (Audemard & Simon 2009)")
    print("=" * 70)
    print()

    # Example 1: Simple SAT with quality tracking
    print("Example 1: Simple SAT with clause quality tracking")
    print("-" * 70)
    formula1 = "(a | b | c) & (~a | b) & (~b | c) & (a | ~c) & (~a | ~b | d)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = CQPSATSolver(cnf1, use_quality_prediction=True, glue_threshold=2)

    result1 = solver1.solve()

    if result1:
        print(f"✓ SAT: {result1}")
    else:
        print("✗ UNSAT")

    print()
    print("Statistics:")
    stats1 = solver1.get_stats()
    print(f"  Decisions: {stats1.decisions}")
    print(f"  Conflicts: {stats1.conflicts}")
    print(f"  Learned clauses (total): {stats1.clauses_learned_total}")
    print(f"  Clauses deleted: {stats1.clauses_deleted}")
    print(f"  Glue clauses (LBD≤2): {stats1.glue_clauses}")
    print(f"  Average LBD: {stats1.avg_lbd:.2f}")

    # Get detailed quality statistics
    quality_stats = solver1.get_quality_statistics()
    print()
    print("Clause Quality Analysis:")
    if 'total_clauses' in quality_stats:
        print(f"  Total learned clauses: {quality_stats['total_clauses']}")
        print(f"  Glue clauses: {quality_stats['glue_clauses']}")
        print(f"  High quality: {quality_stats['high_quality']}")
        print(f"  Medium quality: {quality_stats['medium_quality']}")
        print(f"  Low quality: {quality_stats['low_quality']}")
        if quality_stats['total_clauses'] > 0:
            print(f"  Average LBD: {quality_stats['avg_lbd']:.2f}")
            print(f"  Average quality score: {quality_stats['avg_quality']:.2f}")

    print()

    # Example 2: With vs. without quality prediction
    print("Example 2: Comparing with/without quality prediction")
    print("-" * 70)
    formula2 = "(x | y | z) & (~x | y) & (~y | z) & (x | ~z) & (~x | ~y | w) & (y | ~w)"
    print(f"Formula: {formula2}")
    print()

    cnf2 = CNFExpression.parse(formula2)

    # Without quality prediction
    print("Without quality prediction (standard CDCL):")
    solver_no_qp = CQPSATSolver(cnf2, use_quality_prediction=False)
    result_no = solver_no_qp.solve()
    stats_no = solver_no_qp.get_stats()
    print(f"  Result: {'SAT' if result_no else 'UNSAT'}")
    print(f"  Conflicts: {stats_no.conflicts}")
    print(f"  Learned clauses: {stats_no.clauses_learned_total}")
    learned_count_no = len(solver_no_qp.clauses) - solver_no_qp.num_original_clauses
    print(f"  Final database size: {learned_count_no}")

    print()

    # With quality prediction
    print("With quality prediction (Glucose approach):")
    solver_with_qp = CQPSATSolver(cnf2, use_quality_prediction=True, glue_threshold=2)
    result_with = solver_with_qp.solve()
    stats_with = solver_with_qp.get_stats()
    print(f"  Result: {'SAT' if result_with else 'UNSAT'}")
    print(f"  Conflicts: {stats_with.conflicts}")
    print(f"  Learned clauses: {stats_with.clauses_learned_total}")
    print(f"  Clauses deleted: {stats_with.clauses_deleted}")
    learned_count_with = len(solver_with_qp.clauses) - solver_with_qp.num_original_clauses
    print(f"  Final database size: {learned_count_with}")
    print(f"  Glue clauses: {stats_with.glue_clauses}")
    print(f"  Database reductions: {stats_with.database_reductions}")

    if stats_with.clauses_learned_total > 0:
        deletion_rate = (stats_with.clauses_deleted / stats_with.clauses_learned_total) * 100
        print(f"  Deletion rate: {deletion_rate:.1f}%")

    print()

    # Example 3: Using convenience function
    print("Example 3: Using solve_cqp_sat() convenience function")
    print("-" * 70)
    formula3 = "(a | b) & (~a | c) & (~b | ~c) & (c | d) & (~c | ~d)"
    print(f"Formula: {formula3}")
    print()

    cnf3 = CNFExpression.parse(formula3)
    result3 = solve_cqp_sat(cnf3, use_quality_prediction=True, glue_threshold=2)

    if result3:
        print(f"✓ SAT: {result3}")
    else:
        print("✗ UNSAT")

    print()
    print("=" * 70)
    print("CQP-SAT examples complete!")
    print("=" * 70)
    print()
    print("Key Takeaway: CQP-SAT (Glucose) manages learned clauses efficiently")
    print("by keeping high-quality 'glue' clauses (low LBD) and deleting")
    print("low-quality clauses, reducing memory usage while preserving solving power.")


if __name__ == "__main__":
    main()
