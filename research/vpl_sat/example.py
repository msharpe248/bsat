#!/usr/bin/env python3
"""
Example usage of VPL-SAT solver.

Demonstrates variable phase learning on simple SAT instances.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from vpl_sat import VPLSATSolver, solve_vpl_sat


def main():
    print("=" * 70)
    print("VPL-SAT: Variable Phase Learning SAT Solver Example")
    print("=" * 70)
    print()

    # Example 1: Simple SAT instance
    print("Example 1: Simple SAT with phase learning")
    print("-" * 70)
    formula1 = "(a | b) & (a | c) & (b | d) & (~a | ~b | ~c)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = VPLSATSolver(cnf1, use_phase_learning=True, strategy='adaptive')

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
    print(f"  Learned phase decisions: {stats1.learned_phase_decisions}")
    print(f"  Saved phase decisions: {stats1.saved_phase_decisions}")
    print(f"  Phase flips: {stats1.phase_flips}")

    # Get detailed phase statistics
    phase_stats = solver1.get_phase_statistics()
    print()
    print("Phase Learning Analysis:")
    if 'tracker' in phase_stats:
        tracker = phase_stats['tracker']
        print(f"  Total conflicts tracked: {tracker['total_conflicts']}")
        print(f"  Total decisions tracked: {tracker['total_decisions']}")
        print(f"  Variables with learned preference: {tracker['variables_with_preference']}/{tracker['total_variables']}")
        print(f"  Phase flips recommended: {tracker['phase_flips_recommended']}")

    if 'selector' in phase_stats:
        selector = phase_stats['selector']
        print(f"  Learned decision %: {selector['learned_percentage']:.1f}%")

    print()

    # Example 2: Comparing strategies
    print("Example 2: Comparing phase selection strategies")
    print("-" * 70)
    formula2 = "(x | y | z) & (~x | y) & (~y | z) & (x | ~z)"
    print(f"Formula: {formula2}")
    print()

    cnf2 = CNFExpression.parse(formula2)

    strategies = ['adaptive', 'conflict_rate', 'hybrid']
    for strategy in strategies:
        print(f"Testing strategy: {strategy}")
        result = solve_vpl_sat(cnf2, use_phase_learning=True, strategy=strategy)
        if result:
            print(f"  ✓ SAT: {result}")
        else:
            print(f"  ✗ UNSAT")

    print()

    # Example 3: With vs. without phase learning
    print("Example 3: With vs. without phase learning")
    print("-" * 70)
    formula3 = "(a | b | c) & (~a | b) & (~b | c) & (a | ~c) & (b | ~a | ~c)"
    print(f"Formula: {formula3}")
    print()

    cnf3 = CNFExpression.parse(formula3)

    # Without phase learning (standard VSIDS phase saving)
    print("Without phase learning:")
    solver_no_learning = VPLSATSolver(cnf3, use_phase_learning=False)
    result_no = solver_no_learning.solve()
    stats_no = solver_no_learning.get_stats()
    print(f"  Result: {'SAT' if result_no else 'UNSAT'}")
    print(f"  Conflicts: {stats_no.conflicts}, Decisions: {stats_no.decisions}")

    print()

    # With phase learning
    print("With phase learning:")
    solver_learning = VPLSATSolver(cnf3, use_phase_learning=True, strategy='adaptive')
    result_learning = solver_learning.solve()
    stats_learning = solver_learning.get_stats()
    print(f"  Result: {'SAT' if result_learning else 'UNSAT'}")
    print(f"  Conflicts: {stats_learning.conflicts}, Decisions: {stats_learning.decisions}")
    print(f"  Learned phase decisions: {stats_learning.learned_phase_decisions}")
    print(f"  Phase flips: {stats_learning.phase_flips}")

    if stats_no.conflicts > 0:
        improvement = (stats_no.conflicts - stats_learning.conflicts) / stats_no.conflicts * 100
        print(f"  Conflict reduction: {improvement:.1f}%")

    print()
    print("=" * 70)
    print("VPL-SAT examples complete!")
    print("=" * 70)


if __name__ == "__main__":
    main()
