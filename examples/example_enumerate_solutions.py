"""
Examples demonstrating solution enumeration.

Shows how to find ALL satisfying assignments for a SAT formula,
not just one solution.
"""

from bsat import CNFExpression, find_all_sat_solutions, count_sat_solutions, DPLLSolver


def example1_basic_enumeration():
    """Example 1: Find all solutions to a simple formula."""
    print("=" * 70)
    print("Example 1: Basic Solution Enumeration")
    print("=" * 70)

    formula = "(x | y)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")
    print(f"\nFinding ALL solutions...")

    # The convenience function returns every distinct satisfying assignment
    solutions = find_all_sat_solutions(cnf)
    print(f"\nfind_all_sat_solutions: {len(solutions)} solution(s)")
    for i, sol in enumerate(solutions, 1):
        print(f"  {i}. {sol} → {cnf.evaluate(sol)}")

    # Plain backtracking with every optimization switched off finds the same set
    solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
    all_solutions = solver.find_all_solutions()
    print(f"\nPlain backtracking (no optimizations): {len(all_solutions)} solution(s)")
    for i, sol in enumerate(all_solutions, 1):
        print(f"  {i}. {sol} → {cnf.evaluate(sol)}")

    print("\n💡 Both agree: enumeration never uses pure literal elimination, because")
    print("   fixing a pure variable to one value would discard the solutions where")
    print("   it takes the other value.")
    print()


def example2_count_solutions():
    """Example 2: Count solutions without enumerating."""
    print("=" * 70)
    print("Example 2: Counting Solutions")
    print("=" * 70)

    formula = "(x | y | z)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")

    # Count how many solutions exist
    count = count_sat_solutions(cnf)
    print(f"\nNumber of solutions: {count}")

    # For verification, enumerate them
    solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
    solutions = solver.find_all_solutions()

    print(f"\nAll {len(solutions)} solutions:")
    for i, sol in enumerate(solutions, 1):
        x, y, z = sol['x'], sol['y'], sol['z']
        print(f"  {i}. x={x}, y={y}, z={z}")

    print(f"\n💡 Formula (x | y | z) excludes only x=F,y=F,z=F")
    print(f"   So there are 2³ - 1 = 7 solutions")
    print()


def example3_max_solutions_limit():
    """Example 3: Limit the number of solutions found."""
    print("=" * 70)
    print("Example 3: Limiting Solution Count")
    print("=" * 70)

    # Formula with many solutions
    formula = "(a | b) & (c | d)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")

    # Find only first 5 solutions
    solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
    solutions = solver.find_all_solutions(max_solutions=5)

    print(f"\nFound {len(solutions)} solutions (limited to 5):")
    for i, sol in enumerate(solutions, 1):
        print(f"  {i}. {sol}")

    # Count total (without limit)
    total = solver.count_solutions()
    print(f"\nTotal solutions: {total}")
    print(f"💡 Useful for formulas with exponentially many solutions!")
    print()


def example4_unsat_formula():
    """Example 4: UNSAT formula has no solutions."""
    print("=" * 70)
    print("Example 4: UNSAT Formula")
    print("=" * 70)

    from bsat import Clause, Literal

    # x & ~x is UNSAT
    cnf = CNFExpression([
        Clause([Literal('x', False)]),
        Clause([Literal('x', True)])
    ])

    print(f"Formula: {cnf}")

    solutions = find_all_sat_solutions(cnf)

    print(f"\nSolutions found: {len(solutions)}")
    print(f"💡 UNSAT formulas have 0 solutions")
    print()


def example5_complex_enumeration():
    """Example 5: More complex formula."""
    print("=" * 70)
    print("Example 5: Complex Formula")
    print("=" * 70)

    formula = "(a | b) & (~a | c)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")

    solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
    solutions = solver.find_all_solutions()

    print(f"\nAll {len(solutions)} solutions:")
    for i, sol in enumerate(solutions, 1):
        a = sol['a']
        b = sol['b']
        c = sol['c']
        clause1 = a or b
        clause2 = (not a) or c
        print(f"  {i}. a={a}, b={b}, c={c}  →  (a|b)={clause1}, (~a|c)={clause2}")

    print()


def example6_exponential_solutions():
    """Example 6: Demonstrate exponential growth."""
    print("=" * 70)
    print("Example 6: Exponential Solution Growth")
    print("=" * 70)

    print("Testing formulas with increasing variables...\n")

    for n in [2, 3, 4, 5]:
        # Formula: at least one variable must be true
        # Excludes only the all-false assignment
        # So has 2^n - 1 solutions

        from bsat import Clause, Literal
        literals = [Literal(f'x{i}', False) for i in range(n)]
        cnf = CNFExpression([Clause(literals)])

        solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
        count = solver.count_solutions()

        expected = 2**n - 1
        print(f"  {n} variables: {count} solutions (expected 2^{n}-1 = {expected})")

    print(f"\n💡 Solutions grow exponentially with variables!")
    print(f"   Use max_solutions to avoid combinatorial explosion")
    print()


def example7_practical_use():
    """Example 7: Practical use case - find alternative solutions."""
    print("=" * 70)
    print("Example 7: Finding Alternative Solutions")
    print("=" * 70)

    # Scheduling problem: tasks a, b, c must be scheduled
    # Constraint: if a is scheduled, b must be scheduled
    formula = "(~a | b)"
    cnf = CNFExpression.parse(formula)

    print(f"Constraint: {cnf} (if a then b)")
    print(f"\nFinding all valid schedules...")

    solver = DPLLSolver(cnf, use_unit_propagation=False, use_pure_literal=False)
    solutions = solver.find_all_solutions()

    print(f"\nAll {len(solutions)} valid schedules:")
    for i, sol in enumerate(solutions, 1):
        a = "schedule A" if sol['a'] else "skip A"
        b = "schedule B" if sol['b'] else "skip B"
        print(f"  Option {i}: {a}, {b}")

    print(f"\n💡 Client can choose from multiple valid solutions!")
    print()


def example8_model_counting():
    """Example 8: Model counting - important in AI."""
    print("=" * 70)
    print("Example 8: Model Counting (AI Application)")
    print("=" * 70)

    formula = "(x | y) & (~x | z)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")

    # Count models (satisfying assignments)
    count = count_sat_solutions(cnf)
    total_possible = 2**len(cnf.get_variables())

    print(f"\nModel counting:")
    print(f"  Total possible assignments: {total_possible}")
    print(f"  Satisfying assignments: {count}")
    print(f"  Probability formula is satisfied: {count}/{total_possible} = {count/total_possible:.2%}")

    print(f"\n💡 Used in probabilistic reasoning and weighted model counting")
    print()


if __name__ == '__main__':
    example1_basic_enumeration()
    example2_count_solutions()
    example3_max_solutions_limit()
    example4_unsat_formula()
    example5_complex_enumeration()
    example6_exponential_solutions()
    example7_practical_use()
    example8_model_counting()

    print("=" * 70)
    print("All examples completed!")
    print("=" * 70)
    print("\nKey Takeaways:")
    print("• find_all_sat_solutions() enumerates all solutions")
    print("• count_sat_solutions() counts without storing")
    print("• Solutions grow exponentially (up to 2^n)")
    print("• Use max_solutions to limit search")
    print("• Disable optimizations for ALL distinct solutions")
    print("• Useful for: alternatives, model counting, probabilistic reasoning")
