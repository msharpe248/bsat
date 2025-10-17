#!/usr/bin/env python3
"""
Example usage of TPM-SAT solver.

Demonstrates temporal pattern mining on a simple SAT instance.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from tpm_sat import TPMSATSolver, solve_tpm_sat


def main():
    print("=" * 70)
    print("TPM-SAT: Temporal Pattern Mining SAT Solver Example")
    print("=" * 70)
    print()

    # Example 1: Simple satisfiable formula
    print("Example 1: Simple SAT instance")
    print("-" * 70)
    formula1 = "(a | b) & (~a | c) & (~b | ~c) & (c | d)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = TPMSATSolver(cnf1, window_size=5, conflict_threshold=0.8)

    result1 = solver1.solve()

    if result1:
        print(f"✓ SAT: {result1}")
    else:
        print("✗ UNSAT")

    print()
    print("Statistics:")
    stats1 = solver1.get_stats()
    print(f"  Decisions: {stats1.decisions}")
    print(f"  Propagations: {stats1.propagations}")
    print(f"  Conflicts: {stats1.conflicts}")
    print(f"  Patterns learned: {stats1.patterns_learned}")
    print(f"  Bad patterns avoided: {stats1.bad_patterns_avoided}")
    print(f"  Phase flips from patterns: {stats1.phase_flips_from_patterns}")
    print()

    # Example 2: Moderately complex formula
    print("Example 2: More complex instance")
    print("-" * 70)
    formula2 = "(x1 | x2 | x3) & (~x1 | x4) & (~x2 | x4) & (~x3 | x4) & (~x4 | x5) & (~x5 | x6) & (~x6)"
    print(f"Formula: {formula2}")
    print()

    cnf2 = CNFExpression.parse(formula2)
    solver2 = TPMSATSolver(cnf2, window_size=4, conflict_threshold=0.75)

    result2 = solver2.solve()

    if result2:
        print(f"✓ SAT: {result2}")
    else:
        print("✗ UNSAT")

    print()
    print("Statistics:")
    stats2 = solver2.get_stats()
    print(f"  Decisions: {stats2.decisions}")
    print(f"  Conflicts: {stats2.conflicts}")
    print(f"  Patterns learned: {stats2.patterns_learned}")

    # Get pattern statistics
    pattern_stats = solver2.get_pattern_statistics()
    print()
    print("Pattern Mining Statistics:")
    print(f"  Total patterns: {pattern_stats['miner_stats']['num_patterns']}")
    print(f"  Avg conflict rate: {pattern_stats['miner_stats']['avg_conflict_rate']:.2f}")
    if 'patterns_with_high_conflict_rate' in pattern_stats['miner_stats']:
        print(f"  Bad patterns (>70% conflict rate): {pattern_stats['miner_stats']['patterns_with_high_conflict_rate']}")

    if pattern_stats['top_bad_patterns']:
        print()
        print("Top 3 worst patterns:")
        for i, entry in enumerate(pattern_stats['top_bad_patterns'][:3], 1):
            pattern = entry['pattern']
            rate = entry['conflict_rate']
            count = entry['occurrences']
            print(f"  {i}. {pattern}")
            print(f"     Conflict rate: {rate:.1%} ({count} occurrences)")

    print()

    # Example 3: Using convenience function
    print("Example 3: Using solve_tpm_sat() convenience function")
    print("-" * 70)
    formula3 = "(p | q) & (~p | r) & (~q | ~r)"
    print(f"Formula: {formula3}")

    cnf3 = CNFExpression.parse(formula3)
    result3 = solve_tpm_sat(cnf3, window_size=5)

    if result3:
        print(f"✓ SAT: {result3}")
    else:
        print("✗ UNSAT")

    print()
    print("=" * 70)
    print("TPM-SAT examples complete!")
    print("=" * 70)


if __name__ == "__main__":
    main()
