"""
Examples demonstrating Schöning's randomized SAT algorithm.

Schöning's algorithm (1999) is a beautiful randomized algorithm for k-SAT
with provably better expected runtime than brute force:
- 3SAT: O(1.334^n) expected time
- k-SAT: O((2(k-1)/k)^n) expected time

Key insights:
1. Start with random assignment
2. If unsatisfied, pick random unsatisfied clause
3. Flip random variable from that clause (random walk)
4. Repeat for 3n steps, then restart with new random assignment
5. With enough restarts, find solution with high probability

Reference: Schöning, U. (1999). "A probabilistic algorithm for k-SAT
and constraint satisfaction problems". FOCS 1999.
"""

from bsat import (
    CNFExpression,
    solve_schoening,
    get_schoening_stats,
    SchoeningSolver,
    solve_sat,
    solve_cdcl
)


def example1_basic_usage():
    """Example 1: Basic usage of Schöning's algorithm."""
    print("=" * 60)
    print("Example 1: Basic Usage")
    print("=" * 60)

    # Simple 3SAT formula
    formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")
    print(f"Variables: {cnf.get_variables()}")
    print(f"Clauses: {len(cnf.clauses)}")

    # Solve with Schöning's algorithm
    solution = solve_schoening(cnf, seed=42)

    if solution:
        print(f"\n✓ SAT")
        print(f"Solution: {solution}")
        print(f"Valid: {cnf.evaluate(solution)}")
    else:
        print("\n✗ No solution found (but may exist - algorithm is incomplete)")
    print()


def example2_statistics():
    """Example 2: Track algorithm statistics."""
    print("=" * 60)
    print("Example 2: Statistics Tracking")
    print("=" * 60)

    cnf = CNFExpression.parse("(a | b | c) & (~a | b | ~c) & (a | ~b | c) & (~a | ~b | ~c)")

    print(f"Formula: {cnf}")
    print(f"Variables: {len(cnf.get_variables())}")

    # Solve and get detailed statistics
    solution, stats = get_schoening_stats(cnf, max_tries=100, seed=42)

    print(f"\nResult: {'SAT' if solution else 'UNSAT (or not found)'}")
    if solution:
        print(f"Solution: {solution}")

    print(f"\nStatistics:")
    print(f"  Tries: {stats.tries}")
    print(f"  Total flips: {stats.total_flips}")
    if stats.flips_per_try:
        avg_flips = sum(stats.flips_per_try) / len(stats.flips_per_try)
        print(f"  Average flips per try: {avg_flips:.1f}")
        print(f"  Min flips in a try: {min(stats.flips_per_try)}")
        print(f"  Max flips in a try: {max(stats.flips_per_try)}")
    print()


def example3_reproducibility():
    """Example 3: Reproducible results with seed."""
    print("=" * 60)
    print("Example 3: Reproducibility with Seed")
    print("=" * 60)

    cnf = CNFExpression.parse("(x | y | z) & (~x | y) & (x | ~z)")

    # Same seed gives same results
    sol1 = solve_schoening(cnf, seed=123)
    sol2 = solve_schoening(cnf, seed=123)

    print(f"Solution 1 (seed=123): {sol1}")
    print(f"Solution 2 (seed=123): {sol2}")
    print(f"Identical: {sol1 == sol2}")

    # Different seed may give different results
    sol3 = solve_schoening(cnf, seed=456)
    print(f"\nSolution 3 (seed=456): {sol3}")
    print(f"Same as solution 1: {sol1 == sol3}")

    # All solutions should be valid
    if sol1:
        print(f"\nSolution 1 valid: {cnf.evaluate(sol1)}")
    if sol3:
        print(f"Solution 3 valid: {cnf.evaluate(sol3)}")
    print()


def example4_comparison_with_dpll():
    """Example 4: Compare Schöning with DPLL."""
    print("=" * 60)
    print("Example 4: Comparison with DPLL")
    print("=" * 60)

    # Medium-sized formula
    cnf = CNFExpression.parse(
        "(a | b | c) & (~a | d | e) & (~b | ~d | f) & "
        "(~c | ~e | ~f) & (a | ~b | e) & (~a | b | ~e)"
    )

    print(f"Formula with {len(cnf.get_variables())} variables, {len(cnf.clauses)} clauses")

    # Solve with both algorithms
    dpll_solution = solve_sat(cnf)
    schoening_solution, stats = get_schoening_stats(cnf, seed=42)

    print(f"\nDPLL: {'SAT' if dpll_solution else 'UNSAT'}")
    if dpll_solution:
        print(f"  Solution: {dpll_solution}")

    print(f"\nSchöning: {'SAT' if schoening_solution else 'UNSAT (or not found)'}")
    if schoening_solution:
        print(f"  Solution: {schoening_solution}")
        print(f"  Tries: {stats.tries}")
        print(f"  Total flips: {stats.total_flips}")

    # Both should find valid solutions (if SAT)
    if dpll_solution and schoening_solution:
        print(f"\nBoth solutions valid:")
        print(f"  DPLL valid: {cnf.evaluate(dpll_solution)}")
        print(f"  Schöning valid: {cnf.evaluate(schoening_solution)}")
    print()


def example5_parameter_tuning():
    """Example 5: Tuning max_tries and max_flips."""
    print("=" * 60)
    print("Example 5: Parameter Tuning")
    print("=" * 60)

    cnf = CNFExpression.parse("(x | y | z) & (~x | y | w) & (~y | z | ~w) & (x | ~z | w)")
    n = len(cnf.get_variables())

    print(f"Formula: {cnf}")
    print(f"Variables: n = {n}")

    # Default: max_flips = 3*n (optimal for 3SAT)
    print(f"\nDefault parameters (max_flips = 3*n = {3*n}):")
    sol1, stats1 = get_schoening_stats(cnf, max_tries=100, seed=42)
    print(f"  Result: {'SAT' if sol1 else 'not found'}")
    print(f"  Tries: {stats1.tries}, Total flips: {stats1.total_flips}")

    # More flips per try (may find solution faster)
    print(f"\nMore flips per try (max_flips = 10*n = {10*n}):")
    sol2, stats2 = get_schoening_stats(cnf, max_tries=100, max_flips=10*n, seed=42)
    print(f"  Result: {'SAT' if sol2 else 'not found'}")
    print(f"  Tries: {stats2.tries}, Total flips: {stats2.total_flips}")

    # Fewer flips but more tries
    print(f"\nFewer flips (max_flips = n = {n}), more tries (200):")
    sol3, stats3 = get_schoening_stats(cnf, max_tries=200, max_flips=n, seed=42)
    print(f"  Result: {'SAT' if sol3 else 'not found'}")
    print(f"  Tries: {stats3.tries}, Total flips: {stats3.total_flips}")
    print()


def example6_incomplete_behavior():
    """Example 6: Demonstrating incomplete behavior."""
    print("=" * 60)
    print("Example 6: Incomplete Behavior")
    print("=" * 60)

    # UNSAT formula
    unsat = CNFExpression.parse(
        "(x | y | z) & (~x | y | z) & (x | ~y | z) & "
        "(x | y | ~z) & (~x | ~y | ~z)"
    )

    print("Testing on UNSAT formula:")
    print(f"Formula: {unsat}")

    # Schöning's algorithm is incomplete - won't prove UNSAT
    solution = solve_schoening(unsat, max_tries=100, seed=42)

    if solution is None:
        print("\n✓ No solution found (expected for UNSAT)")
        print("Note: Algorithm cannot prove UNSAT - just didn't find solution")
    else:
        # This shouldn't happen for UNSAT, but algorithm is randomized
        print(f"\nUnexpected: Found solution {solution}")
        print(f"Valid: {unsat.evaluate(solution)}")

    # Compare with complete solver
    print("\nCompare with DPLL (complete solver):")
    dpll_solution = solve_sat(unsat)
    print(f"DPLL: {'SAT' if dpll_solution else 'UNSAT'}")
    print()


def example7_large_instance():
    """Example 7: Performance on larger instances."""
    print("=" * 60)
    print("Example 7: Larger Instance")
    print("=" * 60)

    # Build larger random-ish 3SAT formula
    from bsat import Clause, Literal

    clauses = []
    for i in range(15):
        clauses.append(Clause([
            Literal(f'x{i}', False),
            Literal(f'x{i+1}', False),
            Literal(f'x{i+2}', False)
        ]))
        clauses.append(Clause([
            Literal(f'x{i}', True),
            Literal(f'x{i+1}', False),
            Literal(f'x{i+2}', True)
        ]))

    cnf = CNFExpression(clauses)
    n = len(cnf.get_variables())

    print(f"Formula: {n} variables, {len(cnf.clauses)} clauses")
    print(f"Clause-to-variable ratio: {len(cnf.clauses) / n:.2f}")

    # Try Schöning with more attempts
    solution, stats = get_schoening_stats(cnf, max_tries=1000, seed=42)

    print(f"\nSchöning's algorithm:")
    print(f"  Result: {'SAT' if solution else 'not found'}")
    print(f"  Tries: {stats.tries}")
    print(f"  Total flips: {stats.total_flips}")

    if solution:
        print(f"  Valid: {cnf.evaluate(solution)}")
        if stats.flips_per_try:
            avg = sum(stats.flips_per_try) / len(stats.flips_per_try)
            print(f"  Average flips per try: {avg:.1f}")
    print()


def example8_theoretical_behavior():
    """Example 8: Observing theoretical properties."""
    print("=" * 60)
    print("Example 8: Theoretical Properties")
    print("=" * 60)

    # For 3SAT, optimal is 3n flips per try
    cnf = CNFExpression.parse(
        "(a | b | c) & (~a | b | ~c) & (a | ~b | c) & "
        "(~a | ~b | ~c) & (a | b | ~c)"
    )
    n = len(cnf.get_variables())

    print(f"3SAT formula with n = {n} variables")
    print(f"Theory: Optimal max_flips = 3n = {3*n}")
    print(f"Expected runtime: O(1.334^n) ≈ O({1.334**n:.1f})")

    # Run multiple trials to see distribution
    print(f"\nRunning 10 trials with seed variation:")
    all_tries = []

    for i in range(10):
        solution, stats = get_schoening_stats(cnf, max_tries=100, seed=i)
        all_tries.append(stats.tries)
        status = "✓" if solution else "✗"
        print(f"  Trial {i+1}: {status} Tries: {stats.tries:3d}, Total flips: {stats.total_flips:4d}")

    if all_tries:
        print(f"\nStatistics across trials:")
        print(f"  Average tries: {sum(all_tries) / len(all_tries):.1f}")
        print(f"  Min tries: {min(all_tries)}")
        print(f"  Max tries: {max(all_tries)}")
    print()


if __name__ == '__main__':
    example1_basic_usage()
    example2_statistics()
    example3_reproducibility()
    example4_comparison_with_dpll()
    example5_parameter_tuning()
    example6_incomplete_behavior()
    example7_large_instance()
    example8_theoretical_behavior()

    print("=" * 60)
    print("All examples completed!")
    print("=" * 60)
    print("\nKey Takeaways:")
    print("• Schöning's algorithm is simple and elegant")
    print("• Expected O(1.334^n) for 3SAT - provably better than O(2^n)")
    print("• Incomplete - may not find solution even if exists")
    print("• Randomized - different seeds give different behavior")
    print("• Use seed parameter for reproducibility")
    print("• Optimal: 3n flips per try for 3SAT")
    print("• Good for finding solutions quickly on satisfiable instances")
    print("• Cannot prove UNSAT (use DPLL or CDCL for that)")
