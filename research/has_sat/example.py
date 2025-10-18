#!/usr/bin/env python3
"""
Example usage of HAS-SAT solver.

Demonstrates Hierarchical Abstraction with variable clustering
and abstraction-refinement solving.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from has_sat import HASSATSolver, solve_has_sat


def main():
    print("=" * 70)
    print("HAS-SAT: Hierarchical Abstraction SAT Solver Example")
    print("Educational: Abstraction-refinement for SAT solving")
    print("=" * 70)
    print()

    # Example 1: Simple SAT with abstraction
    print("Example 1: Simple SAT with hierarchical abstraction")
    print("-" * 70)
    formula1 = "(a | b | c) & (~a | b) & (~b | c) & (a | ~c) & (b | ~a | d)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = HASSATSolver(cnf1, use_abstraction=True, num_levels=2)

    result1 = solver1.solve()

    if result1:
        print(f"✓ SAT: {result1}")
    else:
        print("✗ UNSAT")

    print()
    print("Statistics:")
    stats1 = solver1.get_stats()
    print(f"  Conflicts: {stats1.conflicts}")
    print(f"  Abstraction levels: {stats1.abstraction_levels}")
    print(f"  Levels solved: {stats1.levels_solved}")
    print(f"  Refinements: {stats1.refinements}")
    print(f"  Abstract conflicts: {stats1.abstract_conflicts}")

    # Get detailed abstraction statistics
    abs_stats = solver1.get_abstraction_statistics()
    print()
    print("Abstraction Analysis:")
    print(f"  Enabled: {abs_stats['enabled']}")
    if abs_stats['enabled'] and 'hierarchy' in abs_stats:
        hierarchy = abs_stats['hierarchy']
        print(f"  Total levels: {hierarchy['total_levels']}")
        for level_info in hierarchy['levels']:
            print(f"    Level {level_info['level']}: {level_info['clusters']} clusters, {level_info['abstract_vars']} abstract vars")

    print()

    # Example 2: With vs. without abstraction
    print("Example 2: Comparing with/without abstraction")
    print("-" * 70)
    formula2 = "(x | y | z) & (~x | y) & (~y | z) & (x | ~z) & (y | ~w) & (w | ~x)"
    print(f"Formula: {formula2}")
    print()

    cnf2 = CNFExpression.parse(formula2)

    # Without abstraction (standard CDCL)
    print("Without abstraction (standard CDCL):")
    solver_no_abs = HASSATSolver(cnf2, use_abstraction=False)
    result_no = solver_no_abs.solve()
    stats_no = solver_no_abs.get_stats()
    print(f"  Result: {'SAT' if result_no else 'UNSAT'}")
    print(f"  Conflicts: {stats_no.conflicts}")

    print()

    # With abstraction (HAS-SAT)
    print("With abstraction (HAS-SAT):")
    solver_with_abs = HASSATSolver(cnf2, use_abstraction=True, num_levels=2)
    result_with = solver_with_abs.solve()
    stats_with = solver_with_abs.get_stats()
    print(f"  Result: {'SAT' if result_with else 'UNSAT'}")
    print(f"  Conflicts: {stats_with.conflicts}")
    print(f"  Levels solved: {stats_with.levels_solved}")
    print(f"  Refinements: {stats_with.refinements}")

    print()

    # Example 3: Using convenience function
    print("Example 3: Using solve_has_sat() convenience function")
    print("-" * 70)
    formula3 = "(a | b) & (~a | c) & (~b | ~c) & (c | d)"
    print(f"Formula: {formula3}")
    print()

    cnf3 = CNFExpression.parse(formula3)
    result3 = solve_has_sat(cnf3, use_abstraction=True, num_levels=2)

    if result3:
        print(f"✓ SAT: {result3}")
    else:
        print("✗ UNSAT")

    print()

    # Example 4: More complex instance with multiple abstraction levels
    print("Example 4: Complex instance with hierarchical abstraction")
    print("-" * 70)
    formula4 = """
    (a | b | c) & (~a | d) & (~b | e) & (~c | f) &
    (d | e | f) & (~d | g) & (~e | h) & (~f | i) &
    (g | h) & (~g | ~h) & (i | j) & (~i | k) &
    (j | ~k) & (~j | k) & (a | ~d | g)
    """
    print(f"Formula: {formula4.strip()}")
    print()

    cnf4 = CNFExpression.parse(formula4)
    solver4 = HASSATSolver(
        cnf4,
        use_abstraction=True,
        num_levels=3,  # More abstraction levels
        max_conflicts_per_level=10000
    )

    result4 = solver4.solve(max_conflicts=100000)

    if result4:
        print(f"✓ SAT: {result4}")
    else:
        print("✗ UNSAT or timeout")

    print()
    print("Extended Statistics:")
    print(f"  Conflicts: {solver4.stats.conflicts}")
    print(f"  Abstraction levels: {solver4.stats.abstraction_levels}")
    print(f"  Levels solved: {solver4.stats.levels_solved}")
    print(f"  Refinements: {solver4.stats.refinements}")
    print(f"  Abstract conflicts: {solver4.stats.abstract_conflicts}")

    # Detailed abstraction analysis
    abs4 = solver4.get_abstraction_statistics()
    if abs4['enabled'] and 'hierarchy' in abs4:
        print()
        print("Detailed Abstraction Hierarchy:")
        hierarchy4 = abs4['hierarchy']
        print(f"  Total levels: {hierarchy4['total_levels']}")
        for level_info in hierarchy4['levels']:
            print(f"    Level {level_info['level']}:")
            print(f"      Clusters: {level_info['clusters']}")
            print(f"      Abstract variables: {level_info['abstract_vars']}")

        if 'refinement' in abs4:
            ref = abs4['refinement']
            print()
            print("Refinement Statistics:")
            print(f"  Levels solved: {ref['levels_solved']}")
            print(f"  Total conflicts: {ref['total_conflicts']}")
            print(f"  Refinements: {ref['refinements']}")
            print(f"  Avg conflicts/level: {ref['avg_conflicts_per_level']:.1f}")

    print()
    print("=" * 70)
    print("HAS-SAT examples complete!")
    print("=" * 70)
    print()
    print("Key Insight: HAS-SAT builds abstraction hierarchy by clustering")
    print("related variables, then solves at high level (clusters) first.")
    print("If SAT, refines to concrete level. If UNSAT at abstract level,")
    print("original is UNSAT. This can reduce search space for structured")
    print("problems with variable locality.")
    print()
    print("Educational: Demonstrates abstraction-refinement concept from")
    print("planning and model checking applied to SAT solving.")


if __name__ == "__main__":
    main()
