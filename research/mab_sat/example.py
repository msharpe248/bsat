#!/usr/bin/env python3
"""
Example usage of MAB-SAT solver.

Demonstrates Multi-Armed Bandit (UCB1) variable selection on SAT instances.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from mab_sat import MABSATSolver, solve_mab_sat


def main():
    print("=" * 70)
    print("MAB-SAT: Multi-Armed Bandit SAT Solver Example")
    print("Educational reimplementation of MapleSAT/Kissat UCB1 approach")
    print("=" * 70)
    print()

    # Example 1: Simple SAT with UCB1
    print("Example 1: Simple SAT with UCB1 variable selection")
    print("-" * 70)
    formula1 = "(a | b | c) & (~a | b) & (~b | c) & (a | ~c) & (b | ~a | d)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = MABSATSolver(cnf1, use_mab=True, exploration_constant=1.4)

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
    print(f"  UCB1 decisions: {stats1.ucb1_decisions}")
    print(f"  Exploration: {stats1.exploration_decisions}")
    print(f"  Exploitation: {stats1.exploitation_decisions}")
    print(f"  Average reward: {stats1.avg_reward:.2f}")

    # Get detailed MAB statistics
    mab_stats = solver1.get_mab_statistics()
    print()
    print("MAB Analysis:")
    if 'bandit' in mab_stats:
        bandit = mab_stats['bandit']
        print(f"  Total selections: {bandit['total_selections']}")
        print(f"  Variables selected: {bandit['variables_selected']}/{bandit['total_variables']}")
        print(f"  Exploration rate: {bandit['exploration_rate']:.1f}%")

    if 'rewards' in mab_stats:
        rewards = mab_stats['rewards']
        print(f"  Positive rewards: {rewards['positive_rewards']}")
        print(f"  Negative rewards: {rewards['negative_rewards']}")
        print(f"  Avg reward: {rewards['avg_reward']:.2f}")

    print()

    # Example 2: With vs. without MAB
    print("Example 2: Comparing with/without MAB (UCB1)")
    print("-" * 70)
    formula2 = "(x | y | z) & (~x | y) & (~y | z) & (x | ~z) & (y | ~w) & (w | ~x)"
    print(f"Formula: {formula2}")
    print()

    cnf2 = CNFExpression.parse(formula2)

    # Without MAB (standard VSIDS)
    print("Without MAB (standard VSIDS):")
    solver_no_mab = MABSATSolver(cnf2, use_mab=False)
    result_no = solver_no_mab.solve()
    stats_no = solver_no_mab.get_stats()
    print(f"  Result: {'SAT' if result_no else 'UNSAT'}")
    print(f"  Decisions: {stats_no.decisions}")
    print(f"  Conflicts: {stats_no.conflicts}")

    print()

    # With MAB (UCB1)
    print("With MAB (UCB1):")
    solver_with_mab = MABSATSolver(cnf2, use_mab=True, exploration_constant=1.4)
    result_with = solver_with_mab.solve()
    stats_with = solver_with_mab.get_stats()
    print(f"  Result: {'SAT' if result_with else 'UNSAT'}")
    print(f"  Decisions: {stats_with.decisions}")
    print(f"  Conflicts: {stats_with.conflicts}")
    print(f"  Exploration: {stats_with.exploration_decisions}")
    print(f"  Exploitation: {stats_with.exploitation_decisions}")

    print()

    # Example 3: Using convenience function
    print("Example 3: Using solve_mab_sat() convenience function")
    print("-" * 70)
    formula3 = "(a | b) & (~a | c) & (~b | ~c) & (c | d)"
    print(f"Formula: {formula3}")
    print()

    cnf3 = CNFExpression.parse(formula3)
    result3 = solve_mab_sat(cnf3, use_mab=True, exploration_constant=1.4)

    if result3:
        print(f"✓ SAT: {result3}")
    else:
        print("✗ UNSAT")

    print()
    print("=" * 70)
    print("MAB-SAT examples complete!")
    print("=" * 70)
    print()
    print("Key Takeaway: MAB-SAT uses UCB1 to balance exploration")
    print("(trying new variables) with exploitation (choosing historically")
    print("good variables), adapting variable selection based on rewards.")


if __name__ == "__main__":
    main()
