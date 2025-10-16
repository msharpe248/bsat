#!/usr/bin/env python3
"""
BB-CDCL Example Demonstrations

This example demonstrates the Backbone-Based CDCL solver on various types
of formulas, showing when backbone detection helps and when it doesn't.
"""

import sys
import os
from time import time

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.dpll import DPLLSolver
from bb_cdcl import BBCDCLSolver


def print_header(title):
    """Print a formatted header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_stats(stats):
    """Print solver statistics in a formatted way."""
    print("\nStatistics:")
    print(f"  Samples Collected: {stats['num_samples']}")
    print(f"  Backbone Detected: {stats['num_backbone_detected']} vars ({stats['backbone_percentage']:.1f}%)")
    print(f"  Backbone Fixed: {stats['num_backbone_fixed']} vars")
    print(f"  Used Backbone: {stats['used_backbone']}")
    if stats['used_backbone']:
        print(f"  Search Space Reduction: 2^n → 2^n/{stats['search_space_reduction']}")
        print(f"  Backbone Conflicts: {stats['backbone_conflicts']}")
        print(f"  Backbone Backtracks: {stats['backbone_backtracks']}")
    print(f"  Backbone Detection Time: {stats['backbone_detection_time']:.4f}s")
    print(f"  Systematic Search Time: {stats['systematic_search_time']:.4f}s")
    print(f"  Total Time: {stats['total_time']:.4f}s")


def example_1_strong_backbone():
    """
    Example 1: Formula with Strong Backbone

    Formula: (a) & (~a | b) & (~b | c) & (~c | d) & (d | e | f)
    Backbone: a=True, b=True, c=True, d=True (4 out of 6 variables = 67%)

    Expected: High backbone detection, significant speedup
    """
    print_header("Example 1: Strong Backbone Formula")

    formula_str = "(a) & (~a | b) & (~b | c) & (~c | d) & (d | e | f)"
    print(f"\nFormula: {formula_str}")
    print("Expected backbone: a=T, b=T, c=T, d=T (67% of variables)")

    cnf = CNFExpression.parse(formula_str)

    # Solve with BB-CDCL
    print("\nSolving with BB-CDCL...")
    bb_solver = BBCDCLSolver(cnf, num_samples=50)
    start = time()
    result = bb_solver.solve()
    bb_time = time() - start

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")
    print(f"Time: {bb_time:.4f}s")
    print_stats(bb_solver.get_statistics())

    # Compare with DPLL
    print("\nSolving with standard DPLL for comparison...")
    dpll_solver = DPLLSolver(cnf)
    start = time()
    result_dpll = dpll_solver.solve()
    dpll_time = time() - start

    print(f"Result: {'SAT' if result_dpll else 'UNSAT'}")
    print(f"Time: {dpll_time:.4f}s")

    if bb_time > 0:
        print(f"\nSpeedup: {dpll_time / bb_time:.2f}x")


def example_2_partial_backbone():
    """
    Example 2: Formula with Partial Backbone

    Some variables forced, others free.

    Expected: Moderate backbone detection, modest speedup
    """
    print_header("Example 2: Partial Backbone Formula")

    # Backbone: a=T, b=T; Free: c, d, e, f
    formula_str = "(a) & (~a | b) & (c | d) & (d | e) & (e | f) & (c | ~f)"
    print(f"\nFormula: {formula_str}")
    print("Expected backbone: a=T, b=T (33% of variables)")

    cnf = CNFExpression.parse(formula_str)

    # Solve with BB-CDCL
    bb_solver = BBCDCLSolver(cnf, num_samples=50)
    result = bb_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(bb_solver.get_statistics())


def example_3_no_backbone():
    """
    Example 3: Formula with No Backbone

    All variables can vary across solutions.

    Expected: No backbone detected, no speedup (overhead only)
    """
    print_header("Example 3: No Backbone Formula")

    formula_str = "(a | b) & (c | d) & (e | f)"
    print(f"\nFormula: {formula_str}")
    print("Expected backbone: None (many solutions, no forced variables)")

    cnf = CNFExpression.parse(formula_str)

    # Solve with BB-CDCL
    bb_solver = BBCDCLSolver(cnf, num_samples=50)
    result = bb_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(bb_solver.get_statistics())


def example_4_planning_backbone():
    """
    Example 4: Planning-like Formula with Goal-Induced Backbone

    Initial state and goal state force many variables.

    Expected: Strong backbone from constraints
    """
    print_header("Example 4: Planning Formula with Goal Backbone")

    # Initial: s0=T, a0 forced
    # Goal: s3=T forces backwards
    initial = "(s0) & (~s0 | a0)"
    trans01 = "(~a0 | s1) & (~s0 | s1)"
    trans12 = "(~a1 | s2) & (~s1 | s2)"
    trans23 = "(~a2 | s3) & (~s2 | s3)"
    goal = "(s3)"

    formula_str = f"{initial} & {trans01} & (s1 | a1) & {trans12} & (s2 | a2) & {trans23} & {goal}"
    print(f"\nFormula (planning with fixed initial and goal):")
    print(f"  Initial: s0=T")
    print(f"  Goal: s3=T")
    print(f"  Expected backbone: All state variables s0, s1, s2, s3 = True")

    cnf = CNFExpression.parse(formula_str)

    # Solve with BB-CDCL
    bb_solver = BBCDCLSolver(cnf, num_samples=50)
    result = bb_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(bb_solver.get_statistics())


def example_5_confidence_thresholds():
    """
    Example 5: Effect of Confidence Threshold

    Shows how different thresholds affect backbone detection.
    """
    print_header("Example 5: Confidence Threshold Comparison")

    formula_str = "(a) & (~a | b) & (~b | c) & (c | d | e)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    thresholds = [0.85, 0.90, 0.95, 0.99]

    for threshold in thresholds:
        print(f"\n--- Confidence Threshold: {threshold:.2f} ({threshold*100:.0f}%) ---")

        bb_solver = BBCDCLSolver(cnf, num_samples=50, confidence_threshold=threshold)
        result = bb_solver.solve()

        stats = bb_solver.get_statistics()
        print(f"  Backbone Detected: {stats['num_backbone_detected']} vars")
        print(f"  Backbone Conflicts: {stats['backbone_conflicts']}")
        print(f"  Result: {'SAT' if result else 'UNSAT'}")


def example_6_visualization_data():
    """
    Example 6: Exporting Confidence Visualization Data

    Shows detailed confidence scores for each variable.
    """
    print_header("Example 6: Confidence Visualization Data")

    formula_str = "(a) & (~a | b) & (~b | c) & (c | d | e) & (d | ~e)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with BB-CDCL
    bb_solver = BBCDCLSolver(cnf, num_samples=50)
    result = bb_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")

    # Get visualization data
    viz_data = bb_solver.get_visualization_data()

    print("\nPer-Variable Confidence Scores:")
    print(f"{'Variable':<10} {'Conf(T)':<10} {'Conf(F)':<10} {'Max':<10} {'Backbone?':<12} {'Value':<10}")
    print("-" * 70)

    for var_data in viz_data['confidence_data'][:10]:  # Show top 10
        var = var_data['variable']
        conf_t = var_data['confidence_true']
        conf_f = var_data['confidence_false']
        max_conf = var_data['max_confidence']
        is_bb = '✓ Yes' if var_data['is_backbone'] else '  No'
        bb_val = str(var_data['backbone_value']) if var_data['backbone_value'] is not None else '-'

        print(f"{var:<10} {conf_t:<10.2f} {conf_f:<10.2f} {max_conf:<10.2f} {is_bb:<12} {bb_val:<10}")

    print(f"\nTotal Variables: {len(viz_data['confidence_data'])}")
    print(f"Backbone Variables: {len(viz_data['backbone_variables'])}")


def example_7_unsat_instance():
    """
    Example 7: UNSAT Instance

    Shows how BB-CDCL handles unsatisfiable formulas.
    Note: Sampling may be misleading on UNSAT instances!
    """
    print_header("Example 7: UNSAT Instance")

    # Classic UNSAT formula
    formula_str = "(a | b) & (~a | b) & (a | ~b) & (~a | ~b)"
    print(f"\nFormula: {formula_str}")
    print("This formula is UNSAT (all combinations blocked)")

    cnf = CNFExpression.parse(formula_str)

    # Solve with BB-CDCL
    print("\nSolving with BB-CDCL...")
    bb_solver = BBCDCLSolver(cnf, num_samples=50)
    result = bb_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")

    stats = bb_solver.get_statistics()
    print(f"\nNote: Collected {stats['num_samples']} samples")
    if stats['num_samples'] == 0:
        print("  → WalkSAT found no solutions (good sign for UNSAT)")
    else:
        print("  → WalkSAT may have found false solutions (incomplete solver)")

    print_stats(bb_solver.get_statistics())


def main():
    """Run all examples."""
    print("\n" + "█" * 70)
    print("█" + " " * 68 + "█")
    print("█" + "  BB-CDCL: Backbone-Based CDCL Solver".center(68) + "█")
    print("█" + "  Example Demonstrations".center(68) + "█")
    print("█" + " " * 68 + "█")
    print("█" * 70)

    try:
        example_1_strong_backbone()
        example_2_partial_backbone()
        example_3_no_backbone()
        example_4_planning_backbone()
        example_5_confidence_thresholds()
        example_6_visualization_data()
        example_7_unsat_instance()

        print("\n" + "=" * 70)
        print("  All examples completed successfully!")
        print("=" * 70 + "\n")

    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
