#!/usr/bin/env python3
"""
Example usage of MARKET-SAT solver.

Demonstrates auction-based SAT solving on simple problems.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from market_sat import MARKETSATSolver


def example_simple():
    """Simple SAT problem."""
    print("=" * 60)
    print("Example 1: Simple SAT Formula")
    print("=" * 60)

    # (a ∨ b) ∧ (~a ∨ c)
    cnf = CNFExpression.parse("(a | b) & (~a | c)")

    print(f"Formula: {cnf}")
    print()

    # Solve with MARKET-SAT
    solver = MARKETSATSolver(cnf)
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()
        print("Market Statistics:")
        stats = solver.get_market_statistics()
        for key, value in stats.items():
            print(f"  {key}: {value}")
    else:
        print("✗ UNSAT")

    print()


def example_unit_clauses():
    """Example with unit clauses (high priority)."""
    print("=" * 60)
    print("Example 2: Unit Clauses (Forced Assignments)")
    print("=" * 60)

    # x1 ∧ (x1 ∨ x2) ∧ (~x1 ∨ x3)
    cnf = CNFExpression.parse("x1 & (x1 | x2) & (~x1 | x3)")

    print(f"Formula: {cnf}")
    print("Note: x1 is a unit clause - must be True")
    print()

    solver = MARKETSATSolver(cnf)
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()
        print("Notice: x1=True (unit clause forces this)")
        print("        x3=True (follows from ~x1 ∨ x3 with x1=True)")
    else:
        print("✗ UNSAT")

    print()


def example_unsat():
    """UNSAT example (no equilibrium exists)."""
    print("=" * 60)
    print("Example 3: UNSAT Formula")
    print("=" * 60)

    # x ∧ ~x (contradiction)
    cnf = CNFExpression.parse("x & ~x")

    print(f"Formula: {cnf}")
    print("This is a contradiction - no solution exists")
    print()

    solver = MARKETSATSolver(cnf)
    result = solver.solve()

    if result:
        print(f"✓ SAT! (unexpected)")
        print(f"Solution: {result}")
    else:
        print("✗ UNSAT (as expected)")
        print()
        print("Market Interpretation:")
        print("  - Both clauses compete for variable x")
        print("  - One wants x=True, other wants x=False")
        print("  - No price equilibrium exists")

    print()


def example_larger():
    """Larger problem to show market dynamics."""
    print("=" * 60)
    print("Example 4: Larger Formula")
    print("=" * 60)

    # (a ∨ b ∨ c) ∧ (~a ∨ ~b) ∧ (~b ∨ ~c) ∧ (~a ∨ ~c)
    cnf = CNFExpression.parse(
        "(a | b | c) & (~a | ~b) & (~b | ~c) & (~a | ~c)"
    )

    print(f"Formula: {cnf}")
    print("Complex constraint: at most one of {a, b, c} can be True")
    print()

    solver = MARKETSATSolver(cnf)
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()

        # Count how many are True
        true_count = sum(1 for v in result.values() if v)
        print(f"Variables set to True: {true_count}")
        print()

        print("Market Statistics:")
        stats = solver.get_market_statistics()
        print(f"  Auction rounds: {stats.get('auction_rounds', 0)}")
        print(f"  Clauses satisfied: {stats.get('clauses_satisfied', 0)}")
        print(f"  Equilibrium reached: {stats.get('equilibrium', False)}")
    else:
        print("✗ UNSAT")

    print()


def example_compare_to_cdcl():
    """Compare MARKET-SAT to CDCL."""
    print("=" * 60)
    print("Example 5: MARKET-SAT vs CDCL")
    print("=" * 60)

    from bsat.cdcl import CDCLSolver

    cnf = CNFExpression.parse("(a | b) & (~a | c) & (b | ~c) & (~b | d)")

    print(f"Formula: {cnf}")
    print()

    # Solve with MARKET-SAT
    print("Solving with MARKET-SAT...")
    market_solver = MARKETSATSolver(cnf)
    market_result = market_solver.solve()
    market_stats = market_solver.get_market_statistics()

    print(f"  Result: {'SAT' if market_result else 'UNSAT'}")
    print(f"  Auction rounds: {market_stats.get('auction_rounds', 0)}")
    print()

    # Solve with CDCL
    print("Solving with CDCL...")
    cdcl_solver = CDCLSolver(cnf)
    cdcl_result = cdcl_solver.solve()

    print(f"  Result: {'SAT' if cdcl_result else 'UNSAT'}")
    print(f"  Conflicts: {cdcl_solver.stats.conflicts}")
    print()

    # Compare
    if market_result == cdcl_result:
        print("✓ Both solvers agree!")
    else:
        print("✗ Solvers disagree (potential bug)")

    print()


def main():
    """Run all examples."""
    print()
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 10 + "MARKET-SAT: Auction-Based SAT Solver" + " " * 12 + "║")
    print("╚" + "=" * 58 + "╝")
    print()

    example_simple()
    example_unit_clauses()
    example_unsat()
    example_larger()
    example_compare_to_cdcl()

    print("=" * 60)
    print("All examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
