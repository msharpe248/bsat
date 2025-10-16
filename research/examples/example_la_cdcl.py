#!/usr/bin/env python3
"""
LA-CDCL Example Demonstrations

This example demonstrates the Lookahead-Enhanced CDCL solver on various types
of formulas, showing when lookahead helps and when it adds overhead.
"""

import sys
import os
from time import time

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.cdcl import CDCLSolver
from la_cdcl import LACDCLSolver


def print_header(title):
    """Print a formatted header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_stats(stats):
    """Print solver statistics in a formatted way."""
    print("\nStatistics:")
    print(f"  Decisions Made: {stats['decisions_made']}")
    print(f"  Lookahead Used: {stats['lookahead_used']} ({stats['lookahead_percentage']:.1f}%)")
    print(f"  Lookahead Skipped: {stats['lookahead_skipped']}")
    print(f"  Total Conflicts: {stats['conflicts_total']}")
    print(f"  Lookahead Time: {stats['lookahead_time']:.4f}s")
    print(f"  CDCL Time: {stats['cdcl_time']:.4f}s")
    print(f"  Total Time: {stats['total_time']:.4f}s")
    print(f"  Lookahead Overhead: {stats['lookahead_overhead_percentage']:.1f}%")

    # Lookahead engine stats
    la_stats = stats['lookahead_engine_stats']
    print(f"\n  Lookahead Engine:")
    print(f"    Total Lookaheads: {la_stats['total_lookaheads']}")
    print(f"    Cache Hits: {la_stats['cache_hits']}")
    print(f"    Cache Misses: {la_stats['cache_misses']}")
    print(f"    Cache Hit Rate: {la_stats['cache_hit_rate']:.1f}%")
    print(f"    Avg Propagations/Lookahead: {la_stats['avg_propagations_per_lookahead']:.2f}")


def example_1_hard_sat():
    """
    Example 1: Hard Random SAT

    Hard instances benefit most from lookahead.
    Expected: Significant speedup vs pure CDCL.
    """
    print_header("Example 1: Hard Random SAT")

    # Hard 3-SAT instance (near phase transition)
    formula_str = "(a | b | c) & (~a | b | d) & (a | ~c | ~d) & (~b | c | d) & (a | ~b | ~c) & (~a | ~b | d) & (a | c | ~d) & (~a | c | ~d)"
    print(f"\nFormula: {formula_str}")
    print("Expected: Lookahead helps make better decisions")

    cnf = CNFExpression.parse(formula_str)

    # Solve with LA-CDCL
    print("\nSolving with LA-CDCL (depth=2, candidates=5)...")
    la_solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)
    start = time()
    result = la_solver.solve()
    la_time = time() - start

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")
    print(f"Time: {la_time:.4f}s")
    print_stats(la_solver.get_statistics())

    # Compare with pure CDCL
    print("\nSolving with pure CDCL for comparison...")
    cdcl_solver = CDCLSolver(cnf)
    start = time()
    result_cdcl = cdcl_solver.solve()
    cdcl_time = time() - start

    print(f"Result: {'SAT' if result_cdcl else 'UNSAT'}")
    print(f"Time: {cdcl_time:.4f}s")

    if cdcl_time > 0:
        print(f"\nSpeedup: {cdcl_time / la_time:.2f}x")


def example_2_easy_sat():
    """
    Example 2: Easy SAT Instance

    Easy instances show lookahead overhead.
    Expected: Slower than pure CDCL (overhead > benefit).
    """
    print_header("Example 2: Easy SAT Instance")

    formula_str = "(a | b) & (c | d) & (e | f)"
    print(f"\nFormula: {formula_str}")
    print("Expected: Lookahead adds overhead (no benefit)")

    cnf = CNFExpression.parse(formula_str)

    # Solve with LA-CDCL
    print("\nSolving with LA-CDCL...")
    la_solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)
    start = time()
    result = la_solver.solve()
    la_time = time() - start

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")
    print(f"Time: {la_time:.4f}s")
    print_stats(la_solver.get_statistics())

    # Compare with pure CDCL
    print("\nSolving with pure CDCL for comparison...")
    cdcl_solver = CDCLSolver(cnf)
    start = time()
    result_cdcl = cdcl_solver.solve()
    cdcl_time = time() - start

    print(f"Result: {'SAT' if result_cdcl else 'UNSAT'}")
    print(f"Time: {cdcl_time:.4f}s")

    if cdcl_time > 0:
        print(f"\nSpeedup: {cdcl_time / la_time:.2f}x (negative = overhead)")


def example_3_propagation_heavy():
    """
    Example 3: Formula with Long Propagation Chains

    Lookahead excels when propagations reveal good/bad choices.
    Expected: High lookahead benefit.
    """
    print_header("Example 3: Long Propagation Chains")

    # Chain formula: a forces b, b forces c, etc.
    formula_str = "(a) & (~a | b) & (~b | c) & (~c | d) & (~d | e) & (e | f | g) & (f | ~g)"
    print(f"\nFormula: {formula_str}")
    print("Expected: Lookahead reveals long propagation chains")

    cnf = CNFExpression.parse(formula_str)

    # Solve with LA-CDCL
    la_solver = LACDCLSolver(cnf, lookahead_depth=3, num_candidates=5)
    result = la_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(la_solver.get_statistics())


def example_4_depth_comparison():
    """
    Example 4: Comparing Different Lookahead Depths

    Shows impact of lookahead depth on performance.
    Expected: d=2-3 best, d=1 weaker, d=4+ overhead.
    """
    print_header("Example 4: Lookahead Depth Comparison")

    formula_str = "(a | b | c) & (~a | d | e) & (~b | ~d | f) & (~c | ~e | ~f) & (a | ~b | d) & (~a | c | ~e)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    depths = [1, 2, 3]

    for depth in depths:
        print(f"\n--- Lookahead Depth: {depth} ---")

        solver = LACDCLSolver(cnf, lookahead_depth=depth, num_candidates=5)
        start = time()
        result = solver.solve()
        solve_time = time() - start

        stats = solver.get_statistics()
        print(f"  Result: {'SAT' if result else 'UNSAT'}")
        print(f"  Time: {solve_time:.4f}s")
        print(f"  Decisions: {stats['decisions_made']}")
        print(f"  Conflicts: {stats['conflicts_total']}")
        print(f"  Avg Propagations: {stats['lookahead_engine_stats']['avg_propagations_per_lookahead']:.2f}")
        print(f"  Overhead: {stats['lookahead_overhead_percentage']:.1f}%")


def example_5_candidate_comparison():
    """
    Example 5: Comparing Number of Candidates

    Shows impact of evaluating more/fewer candidates.
    Expected: k=5 best, k=1 poor, k=10+ overhead.
    """
    print_header("Example 5: Number of Candidates Comparison")

    formula_str = "(a | b | c) & (~a | d) & (~b | e) & (~c | f) & (d | e | f) & (~d | ~e) & (~e | ~f)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    candidates_list = [1, 3, 5, 10]

    for num_candidates in candidates_list:
        print(f"\n--- Number of Candidates: {num_candidates} ---")

        solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=num_candidates)
        start = time()
        result = solver.solve()
        solve_time = time() - start

        stats = solver.get_statistics()
        print(f"  Result: {'SAT' if result else 'UNSAT'}")
        print(f"  Time: {solve_time:.4f}s")
        print(f"  Total Lookaheads: {stats['lookahead_engine_stats']['total_lookaheads']}")
        print(f"  Overhead: {stats['lookahead_overhead_percentage']:.1f}%")


def example_6_adaptive_frequency():
    """
    Example 6: Adaptive Lookahead Frequency

    Shows using lookahead less frequently to reduce overhead.
    Expected: Lower frequency = less overhead but less benefit.
    """
    print_header("Example 6: Adaptive Lookahead Frequency")

    formula_str = "(a | b | c | d) & (~a | ~b | e) & (~c | ~d | f) & (e | f | g) & (~e | ~f | ~g) & (a | ~c | ~e)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    frequencies = [1, 3, 5]

    for freq in frequencies:
        print(f"\n--- Lookahead Frequency: Every {freq} decision(s) ---")

        solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5, lookahead_frequency=freq)
        start = time()
        result = solver.solve()
        solve_time = time() - start

        stats = solver.get_statistics()
        print(f"  Result: {'SAT' if result else 'UNSAT'}")
        print(f"  Time: {solve_time:.4f}s")
        print(f"  Lookahead Used: {stats['lookahead_used']} / {stats['decisions_made']}")
        print(f"  Overhead: {stats['lookahead_overhead_percentage']:.1f}%")


def example_7_disable_lookahead():
    """
    Example 7: Disable Lookahead (Pure CDCL Baseline)

    Shows LA-CDCL with lookahead disabled = pure CDCL.
    Expected: Same as CDCL, no lookahead overhead.
    """
    print_header("Example 7: Lookahead Disabled (Pure CDCL Baseline)")

    formula_str = "(a | b) & (~a | c) & (~b | d) & (~c | ~d) & (a | d)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # LA-CDCL with lookahead disabled
    print("\nSolving with LA-CDCL (lookahead disabled)...")
    solver = LACDCLSolver(cnf, use_lookahead=False)
    result = solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(solver.get_statistics())


def example_8_visualization_data():
    """
    Example 8: Exporting Visualization Data

    Shows detailed lookahead data for visualization.
    """
    print_header("Example 8: Visualization Data Export")

    formula_str = "(a | b | c) & (~a | d) & (~b | e) & (~c | ~d | ~e)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with LA-CDCL
    solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)
    result = solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")

    # Get visualization data
    viz_data = solver.get_visualization_data()

    print("\nVisualization Data:")
    print(f"  Lookahead Depth: {viz_data['lookahead_depth']}")
    print(f"  Num Candidates: {viz_data['num_candidates']}")
    print(f"  Use Lookahead: {viz_data['use_lookahead']}")

    print("\n  Statistics:")
    stats = viz_data['statistics']
    print(f"    Total Decisions: {stats['decisions_made']}")
    print(f"    Lookahead Used: {stats['lookahead_used']}")
    print(f"    Cache Hit Rate: {stats['lookahead_engine_stats']['cache_hit_rate']:.1f}%")


def main():
    """Run all examples."""
    print("\n" + "█" * 70)
    print("█" + " " * 68 + "█")
    print("█" + "  LA-CDCL: Lookahead-Enhanced CDCL Solver".center(68) + "█")
    print("█" + "  Example Demonstrations".center(68) + "█")
    print("█" + " " * 68 + "█")
    print("█" * 70)

    try:
        example_1_hard_sat()
        example_2_easy_sat()
        example_3_propagation_heavy()
        example_4_depth_comparison()
        example_5_candidate_comparison()
        example_6_adaptive_frequency()
        example_7_disable_lookahead()
        example_8_visualization_data()

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
