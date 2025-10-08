"""Example usage of the DPLL SAT solver."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from bsat.cnf import CNFExpression
from bsat.dpll import DPLLSolver, solve_sat


def example_satisfiable_3sat():
    """Example of solving a satisfiable 3SAT problem."""
    print("Example 1: Satisfiable 3SAT")
    print("-" * 50)

    # Parse a 3SAT formula: (x ∨ y ∨ z) ∧ (¬x ∨ y ∨ ¬z) ∧ (x ∨ ¬y ∨ z)
    formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")
    print(f"Variables: {sorted(cnf.get_variables())}")
    print(f"Number of clauses: {len(cnf.clauses)}")

    # Solve using DPLL
    solver = DPLLSolver(cnf)
    result = solver.solve()

    if result:
        print(f"\n✓ SAT - Found satisfying assignment:")
        for var in sorted(result.keys()):
            print(f"  {var} = {result[var]}")

        # Verify the solution
        print(f"\nVerification: {cnf.evaluate(result)}")

        stats = solver.get_statistics()
        print(f"\nStatistics:")
        print(f"  Decisions made: {stats['num_decisions']}")
    else:
        print("\n✗ UNSAT - No satisfying assignment exists")

    print()


def example_unsatisfiable():
    """Example of an unsatisfiable formula."""
    print("Example 2: Unsatisfiable Formula")
    print("-" * 50)

    # (x) ∧ (¬x) ∧ (y ∨ z) ∧ (¬y) ∧ (¬z)
    formula = "x & ~x & (y | z) & ~y & ~z"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")

    result = solve_sat(cnf)

    if result:
        print(f"\n✓ SAT: {result}")
    else:
        print("\n✗ UNSAT - Formula is unsatisfiable (as expected)")

    print()


def example_large_3sat():
    """Example with a larger 3SAT instance."""
    print("Example 3: Larger 3SAT Instance")
    print("-" * 50)

    # Create a 3SAT with 10 variables and 20 clauses
    formula = """
    (a | b | c) & (~a | d | e) & (b | ~c | f) &
    (~d | e | g) & (a | ~f | h) & (c | ~g | i) &
    (d | ~h | j) & (~b | f | ~i) & (e | g | ~j) &
    (~a | ~e | h) & (b | ~d | i) & (~c | f | ~j) &
    (a | ~g | j) & (~b | c | ~h) & (d | ~f | i) &
    (~e | g | ~a) & (f | ~h | b) & (~i | j | c) &
    (g | ~j | ~d) & (h | i | ~e)
    """

    cnf = CNFExpression.parse(formula)

    print(f"Variables: {len(cnf.get_variables())}")
    print(f"Clauses: {len(cnf.clauses)}")

    solver = DPLLSolver(cnf)
    result = solver.solve()

    if result:
        print(f"\n✓ SAT - Found satisfying assignment:")
        # Show first few variables
        for var in sorted(list(result.keys())[:5]):
            print(f"  {var} = {result[var]}")
        print(f"  ... and {len(result) - 5} more variables")

        stats = solver.get_statistics()
        print(f"\nStatistics:")
        print(f"  Decisions made: {stats['num_decisions']}")
        print(f"  Verification: {cnf.evaluate(result)}")
    else:
        print("\n✗ UNSAT")

    print()


if __name__ == '__main__':
    print("=" * 50)
    print("DPLL SAT Solver Examples")
    print("=" * 50)
    print()

    example_satisfiable_3sat()
    example_unsatisfiable()
    example_large_3sat()

    print("=" * 50)
    print("Examples complete!")
