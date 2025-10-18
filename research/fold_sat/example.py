#!/usr/bin/env python3
"""
Example usage of FOLD-SAT solver.

Demonstrates protein folding energy minimization SAT solving on simple problems.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from fold_sat import FOLDSATSolver


def example_simple():
    """Simple SAT problem."""
    print("=" * 60)
    print("Example 1: Simple SAT Formula")
    print("=" * 60)

    # (a ∨ b) ∧ (~a ∨ c)
    cnf = CNFExpression.parse("(a | b) & (~a | c)")

    print(f"Formula: {cnf}")
    print()

    # Solve with FOLD-SAT
    solver = FOLDSATSolver(cnf, max_iterations=10000, T_initial=10.0)
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()
        print("Energy Statistics:")
        stats = solver.get_energy_statistics()
        for key, value in stats.items():
            print(f"  {key}: {value}")
    else:
        print("✗ UNSAT")

    print()


def example_unit_clauses():
    """Example with unit clauses (high energy penalty)."""
    print("=" * 60)
    print("Example 2: Unit Clauses (High Priority)")
    print("=" * 60)

    # x1 ∧ (x1 ∨ x2) ∧ (~x1 ∨ x3)
    cnf = CNFExpression.parse("x1 & (x1 | x2) & (~x1 | x3)")

    print(f"Formula: {cnf}")
    print("Note: x1 is a unit clause - high energy penalty if unsatisfied")
    print()

    solver = FOLDSATSolver(cnf, max_iterations=10000, T_initial=10.0)
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
    """UNSAT example (no low-energy state exists)."""
    print("=" * 60)
    print("Example 3: UNSAT Formula")
    print("=" * 60)

    # x ∧ ~x (contradiction)
    cnf = CNFExpression.parse("x & ~x")

    print(f"Formula: {cnf}")
    print("This is a contradiction - no ground state exists")
    print()

    solver = FOLDSATSolver(cnf, max_iterations=5000, T_initial=10.0)
    result = solver.solve()

    if result:
        print(f"✓ SAT! (unexpected)")
        print(f"Solution: {result}")
    else:
        print("✗ UNSAT (as expected)")
        print()
        print("Energy Interpretation:")
        print("  - Both clauses contribute positive energy")
        print("  - No assignment achieves ground state")
        print("  - System cannot fold to stable state")

    print()


def example_larger():
    """Larger problem to show annealing dynamics."""
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

    solver = FOLDSATSolver(cnf, max_iterations=20000, T_initial=50.0)
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()

        # Count how many are True
        true_count = sum(1 for v in result.values() if v)
        print(f"Variables set to True: {true_count}")
        print()

        print("Energy Statistics:")
        stats = solver.get_energy_statistics()
        print(f"  Annealing iterations: {stats.get('annealing_iterations', 0)}")
        print(f"  Acceptance rate: {stats.get('acceptance_rate', 0):.2%}")
        print(f"  Final energy: {stats.get('final_energy', 0):.2f}")
        print(f"  Ground state: {stats.get('ground_state_energy', 0):.2f}")
        print(f"  Reached ground state: {stats.get('reached_ground_state', False)}")
    else:
        print("✗ UNSAT")

    print()


def example_compare_to_cdcl():
    """Compare FOLD-SAT to CDCL."""
    print("=" * 60)
    print("Example 5: FOLD-SAT vs CDCL")
    print("=" * 60)

    from bsat.cdcl import CDCLSolver

    cnf = CNFExpression.parse("(a | b) & (~a | c) & (b | ~c) & (~b | d)")

    print(f"Formula: {cnf}")
    print()

    # Solve with FOLD-SAT
    print("Solving with FOLD-SAT...")
    fold_solver = FOLDSATSolver(cnf, max_iterations=10000, T_initial=20.0)
    fold_result = fold_solver.solve()
    fold_stats = fold_solver.get_energy_statistics()

    print(f"  Result: {'SAT' if fold_result else 'UNSAT'}")
    print(f"  Annealing iterations: {fold_stats.get('annealing_iterations', 0)}")
    print(f"  Acceptance rate: {fold_stats.get('acceptance_rate', 0):.2%}")
    print()

    # Solve with CDCL
    print("Solving with CDCL...")
    cdcl_solver = CDCLSolver(cnf)
    cdcl_result = cdcl_solver.solve()

    print(f"  Result: {'SAT' if cdcl_result else 'UNSAT'}")
    print(f"  Conflicts: {cdcl_solver.stats.conflicts}")
    print()

    # Compare
    if (fold_result is None) == (cdcl_result is None):
        print("✓ Both solvers agree on SAT/UNSAT!")
    else:
        print("✗ Solvers disagree (potential bug)")

    print()


def example_parallel_tempering():
    """Demonstrate parallel tempering mode."""
    print("=" * 60)
    print("Example 6: Parallel Tempering (Replica Exchange)")
    print("=" * 60)

    cnf = CNFExpression.parse(
        "(a | b) & (~a | c) & (b | d) & (~c | ~d) & (a | ~b | c)"
    )

    print(f"Formula: {cnf}")
    print()

    print("Using parallel tempering (8 replicas)...")
    solver = FOLDSATSolver(
        cnf,
        max_iterations=20000,
        mode='parallel_tempering'
    )
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()
        stats = solver.get_energy_statistics()
        print(f"  Annealing iterations: {stats.get('annealing_iterations', 0)}")
        print(f"  Final energy: {stats.get('final_energy', 0):.2f}")
    else:
        print("✗ UNSAT")

    print()


def example_energy_landscape():
    """Visualize energy landscape."""
    print("=" * 60)
    print("Example 7: Energy Landscape Analysis")
    print("=" * 60)

    from fold_sat import EnergyLandscape

    cnf = CNFExpression.parse("(a | b) & (~a | b) & (a | ~b)")

    print(f"Formula: {cnf}")
    print()

    landscape = EnergyLandscape(cnf)

    print("Landscape properties:")
    print(f"  Variables: {len(cnf.get_variables())}")
    print(f"  Clauses: {len(cnf.clauses)}")
    print(f"  Ground state energy: {landscape.get_ground_state_energy():.2f}")
    print()

    print("Testing all possible assignments:")
    variables = list(cnf.get_variables())

    # Enumerate all assignments (only works for small problems!)
    for i in range(2 ** len(variables)):
        assignment = {}
        for j, var in enumerate(variables):
            assignment[var] = bool((i >> j) & 1)

        energy = landscape.compute_energy(assignment)
        num_unsat = landscape.get_num_unsatisfied(assignment)

        print(f"  {assignment} → E={energy:+6.2f}, unsat={num_unsat}")

    print()


def main():
    """Run all examples."""
    print()
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 6 + "FOLD-SAT: Protein Folding Energy Minimization" + " " * 5 + "║")
    print("╚" + "=" * 58 + "╝")
    print()

    example_simple()
    example_unit_clauses()
    example_unsat()
    example_larger()
    example_compare_to_cdcl()
    example_parallel_tempering()
    example_energy_landscape()

    print("=" * 60)
    print("All examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
