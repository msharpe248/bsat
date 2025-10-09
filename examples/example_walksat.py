"""
WalkSAT Solver Examples

This module demonstrates the WalkSAT randomized local search algorithm.

WalkSAT is an incomplete but often very fast algorithm for finding satisfying
assignments. Key characteristics:
- Incomplete: May not find solution even if one exists
- Probabilistic: Different runs may give different results
- Fast: Often much faster than complete solvers for SAT instances
- Cannot prove UNSAT: Only useful for finding solutions, not proving none exist
"""

from bsat import CNFExpression, Clause, Literal, solve_walksat, get_walksat_stats


def print_header(title):
    """Print a formatted section header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_result(formula, result, show_formula=True):
    """Print the formula and its solution."""
    if show_formula:
        print(f"\nFormula: {formula}")

    if result:
        print(f"✓ SAT (solution found)")
        print(f"Solution: {result}")
        # Verify
        is_valid = formula.evaluate(result)
        print(f"Verification: {'✓ Valid' if is_valid else '✗ Invalid'}")
    else:
        print("✗ No solution found")
        print("(Note: This doesn't prove UNSAT - WalkSAT is incomplete)")


def example_basic():
    """Basic WalkSAT examples."""
    print_header("Example 1: Basic WalkSAT Usage")

    # Simple 3-SAT formula
    print("\n1. Simple 3-SAT formula")
    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
        Clause([Literal('x', True), Literal('y', False), Literal('z', True)]),
        Clause([Literal('x', False), Literal('y', True), Literal('z', False)])
    ])

    result = solve_walksat(cnf, seed=42)
    print_result(cnf, result)


def example_noise_parameter():
    """Demonstrate effect of noise parameter."""
    print_header("Example 2: Noise Parameter Effects")

    cnf = CNFExpression([
        Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),
        Clause([Literal('a', True), Literal('b', False), Literal('c', True)]),
        Clause([Literal('a', False), Literal('b', True), Literal('c', False)]),
        Clause([Literal('a', True), Literal('b', True), Literal('c', True)])
    ])

    print(f"\nFormula: {cnf}")

    # Try different noise values
    for noise in [0.0, 0.3, 0.5, 0.7, 1.0]:
        print(f"\n--- Noise = {noise} ---")
        stats = get_walksat_stats(cnf, noise=noise, max_flips=1000, seed=42)

        print(f"Found: {stats['found']}")
        print(f"Total flips: {stats['stats']['total_flips']}")
        print(f"Total tries: {stats['stats']['total_tries']}")

        if noise == 0.0:
            print("(Pure greedy - may get stuck in local minima)")
        elif noise == 1.0:
            print("(Pure random walk - very inefficient)")
        elif 0.4 <= noise <= 0.6:
            print("(Optimal range for most problems)")


def example_random_restarts():
    """Demonstrate multiple random restarts."""
    print_header("Example 3: Multiple Random Restarts")

    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
        Clause([Literal('x', True), Literal('y', True), Literal('z', True)]),
        Clause([Literal('x', False), Literal('y', True), Literal('z', True)]),
        Clause([Literal('x', True), Literal('y', False), Literal('z', True)])
    ])

    print(f"\nFormula: {cnf}")

    # Try with different numbers of restarts
    for max_tries in [1, 5, 10]:
        print(f"\n--- Max tries = {max_tries} ---")
        stats = get_walksat_stats(cnf, max_flips=100, max_tries=max_tries, seed=42)

        print(f"Found: {stats['found']}")
        print(f"Actual tries used: {stats['stats']['total_tries']}")
        print(f"Total flips: {stats['stats']['total_flips']}")

        if stats['found']:
            print(f"Solution: {stats['solution']}")


def example_reproducibility():
    """Demonstrate reproducibility with seeds."""
    print_header("Example 4: Reproducibility with Random Seeds")

    cnf = CNFExpression([
        Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),
        Clause([Literal('a', True), Literal('b', True), Literal('c', True)])
    ])

    print(f"\nFormula: {cnf}")

    # Same seed gives same result
    print("\n--- Using seed=123 (twice) ---")
    result1 = solve_walksat(cnf, seed=123)
    result2 = solve_walksat(cnf, seed=123)

    print(f"First run:  {result1}")
    print(f"Second run: {result2}")
    print(f"Identical: {result1 == result2}")

    # Different seeds may give different results
    print("\n--- Using different seeds ---")
    result3 = solve_walksat(cnf, seed=456)
    result4 = solve_walksat(cnf, seed=789)

    print(f"Seed 456: {result3}")
    print(f"Seed 789: {result4}")
    print("(May or may not be the same - both are valid solutions)")


def example_larger_instance():
    """Solve a larger SAT instance."""
    print_header("Example 5: Larger SAT Instance")

    # Create a larger random 3-SAT instance
    cnf = CNFExpression([
        Clause([Literal('v1', False), Literal('v2', False), Literal('v3', False)]),
        Clause([Literal('v2', True), Literal('v3', False), Literal('v4', False)]),
        Clause([Literal('v1', True), Literal('v3', True), Literal('v5', False)]),
        Clause([Literal('v4', True), Literal('v5', True), Literal('v6', False)]),
        Clause([Literal('v1', False), Literal('v4', False), Literal('v6', False)]),
        Clause([Literal('v2', False), Literal('v5', True), Literal('v6', True)]),
        Clause([Literal('v3', True), Literal('v4', False), Literal('v5', False)]),
        Clause([Literal('v1', True), Literal('v2', True), Literal('v6', True)])
    ])

    print(f"\n6 variables, 8 clauses (3-SAT)")

    stats = get_walksat_stats(cnf, noise=0.5, max_flips=5000, seed=42)

    print(f"\nFound: {stats['found']}")
    print(f"Total flips: {stats['stats']['total_flips']}")
    print(f"Total tries: {stats['stats']['total_tries']}")

    if stats['found']:
        print(f"Solution: {stats['solution']}")
        print(f"Valid: {cnf.evaluate(stats['solution'])}")


def example_unsatisfiable():
    """Demonstrate behavior on UNSAT instance."""
    print_header("Example 6: Unsatisfiable Formula")

    # Classic UNSAT formula
    cnf = CNFExpression([
        Clause([Literal('x', False)]),
        Clause([Literal('x', True)])
    ])

    print(f"\nFormula: {cnf}")
    print("This formula is UNSATISFIABLE (x ∧ ¬x)")

    result = solve_walksat(cnf, max_flips=1000, max_tries=5, seed=42)

    print(f"\nResult: {result}")
    print("\nNote: WalkSAT returning None doesn't prove UNSAT!")
    print("It only means no solution was found in the given time.")
    print("For UNSAT proof, use a complete solver like DPLL.")


def example_comparison_with_dpll():
    """Compare WalkSAT with DPLL."""
    print_header("Example 7: WalkSAT vs DPLL Comparison")

    from bsat import solve_sat

    cnf = CNFExpression([
        Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),
        Clause([Literal('a', True), Literal('b', False), Literal('c', True)]),
        Clause([Literal('a', False), Literal('b', True), Literal('d', False)]),
        Clause([Literal('b', True), Literal('c', True), Literal('d', True)]),
        Clause([Literal('a', True), Literal('c', False), Literal('d', False)])
    ])

    print(f"\nFormula: {cnf}")

    # Solve with WalkSAT
    print("\n--- WalkSAT ---")
    walksat_stats = get_walksat_stats(cnf, seed=42)
    print(f"Found: {walksat_stats['found']}")
    print(f"Total flips: {walksat_stats['stats']['total_flips']}")
    if walksat_stats['solution']:
        print(f"Solution: {walksat_stats['solution']}")

    # Solve with DPLL
    print("\n--- DPLL (complete solver) ---")
    dpll_result = solve_sat(cnf)
    print(f"SAT: {dpll_result is not None}")
    if dpll_result:
        print(f"Solution: {dpll_result}")

    print("\n** Key Differences **")
    print("WalkSAT:")
    print("  + Often faster for SAT instances")
    print("  + Randomized (different runs may vary)")
    print("  - Incomplete (may not find solution)")
    print("  - Cannot prove UNSAT")
    print("\nDPLL:")
    print("  + Complete (always finds solution if exists)")
    print("  + Can prove UNSAT")
    print("  + Deterministic")
    print("  - May be slower on large SAT instances")


def example_statistics_tracking():
    """Track detailed statistics during solving."""
    print_header("Example 8: Statistics Tracking")

    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),
        Clause([Literal('x', True), Literal('y', False), Literal('z', True)]),
        Clause([Literal('x', False), Literal('y', True), Literal('z', False)]),
        Clause([Literal('x', True), Literal('y', True), Literal('z', True)])
    ])

    print(f"\nFormula: {cnf}")

    stats_result = get_walksat_stats(cnf, noise=0.5, max_flips=2000, seed=42)

    print("\n** Statistics **")
    print(f"Solution found: {stats_result['found']}")
    print(f"Number of variables: {stats_result['stats']['num_variables']}")
    print(f"Number of clauses: {stats_result['stats']['num_clauses']}")
    print(f"Total flips performed: {stats_result['stats']['total_flips']}")
    print(f"Total tries (restarts): {stats_result['stats']['total_tries']}")

    # Show unsatisfied clauses over time (first 20 flips)
    history = stats_result['stats']['unsatisfied_clauses_history'][:20]
    print(f"\nUnsatisfied clauses (first 20 flips): {history}")


def example_when_to_use_walksat():
    """Guidelines for when to use WalkSAT."""
    print_header("Example 9: When to Use WalkSAT")

    print("""
WalkSAT is best used when:

✓ GOOD for:
  • Finding ANY solution quickly (don't care which one)
  • Large satisfiable instances (thousands of variables)
  • When you know the formula is likely SAT
  • Real-time applications where speed matters
  • Optimization problems (can be adapted for MaxSAT)
  • When you can run multiple times with different seeds

✗ NOT GOOD for:
  • Proving unsatisfiability (use DPLL or CDCL)
  • Small instances (DPLL is fine and complete)
  • When you need guaranteed solutions
  • When correctness is more important than speed
  • Finding ALL solutions (WalkSAT only finds one)

** Example Use Cases **

1. Hardware Testing:
   - Quickly find test vectors to expose bugs
   - Don't need to prove absence of bugs

2. Planning Problems:
   - Find any valid plan quickly
   - Can try multiple times if needed

3. Configuration:
   - Find valid system configurations
   - Speed is important for user experience

4. Puzzle Solving:
   - Sudoku, N-Queens, etc.
   - Just need one solution
    """)


def main():
    """Run all examples."""
    print("""
╔══════════════════════════════════════════════════════════════════════╗
║                  WalkSAT Solver Examples                             ║
║                                                                      ║
║  WalkSAT is a randomized local search algorithm for SAT.            ║
║  Unlike complete solvers (DPLL, CDCL), it's incomplete but often   ║
║  much faster for finding solutions to satisfiable instances.        ║
║                                                                      ║
║  Key Properties:                                                     ║
║  • Incomplete: May not find solution even if one exists             ║
║  • Probabilistic: Different runs give different results             ║
║  • Fast: Often much faster than complete solvers                    ║
║  • Cannot prove UNSAT: Only useful for finding solutions            ║
╚══════════════════════════════════════════════════════════════════════╝
    """)

    example_basic()
    example_noise_parameter()
    example_random_restarts()
    example_reproducibility()
    example_larger_instance()
    example_unsatisfiable()
    example_comparison_with_dpll()
    example_statistics_tracking()
    example_when_to_use_walksat()

    print("\n" + "=" * 70)
    print("  All examples completed!")
    print("=" * 70 + "\n")


if __name__ == '__main__':
    main()
