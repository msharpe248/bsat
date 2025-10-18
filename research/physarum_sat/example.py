#!/usr/bin/env python3
"""
Example usage of PHYSARUM-SAT solver.

Demonstrates slime mold network flow SAT solving on simple problems.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from physarum_sat import PHYSARUMSATSolver


def example_simple():
    """Simple SAT problem."""
    print("=" * 60)
    print("Example 1: Simple SAT Formula")
    print("=" * 60)

    # (a ∨ b) ∧ (~a ∨ c)
    cnf = CNFExpression.parse("(a | b) & (~a | c)")

    print(f"Formula: {cnf}")
    print()

    # Solve with PHYSARUM-SAT
    solver = PHYSARUMSATSolver(cnf, max_iterations=1000)
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()
        print("Network Statistics:")
        stats = solver.get_network_statistics()
        for key, value in stats.items():
            print(f"  {key}: {value}")
    else:
        print("✗ UNSAT")

    print()


def example_unit_clauses():
    """Example with unit clauses (high flow priority)."""
    print("=" * 60)
    print("Example 2: Unit Clauses (Forced Flow)")
    print("=" * 60)

    # x1 ∧ (x1 ∨ x2) ∧ (~x1 ∨ x3)
    cnf = CNFExpression.parse("x1 & (x1 | x2) & (~x1 | x3)")

    print(f"Formula: {cnf}")
    print("Note: x1 is a unit clause - must receive flow")
    print()

    solver = PHYSARUMSATSolver(cnf, max_iterations=1000)
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
    """UNSAT example (no flow equilibrium exists)."""
    print("=" * 60)
    print("Example 3: UNSAT Formula")
    print("=" * 60)

    # x ∧ ~x (contradiction)
    cnf = CNFExpression.parse("x & ~x")

    print(f"Formula: {cnf}")
    print("This is a contradiction - no solution exists")
    print()

    solver = PHYSARUMSATSolver(cnf, max_iterations=500)
    result = solver.solve()

    if result:
        print(f"✓ SAT! (unexpected)")
        print(f"Solution: {result}")
    else:
        print("✗ UNSAT (as expected)")
        print()
        print("Network Interpretation:")
        print("  - Both clauses compete for flow from x")
        print("  - One wants x_T path, other wants x_F path")
        print("  - No flow pattern satisfies both")

    print()


def example_larger():
    """Larger problem to show network dynamics."""
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

    solver = PHYSARUMSATSolver(cnf, max_iterations=2000)
    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()

        # Count how many are True
        true_count = sum(1 for v in result.values() if v)
        print(f"Variables set to True: {true_count}")
        print()

        print("Network Statistics:")
        stats = solver.get_network_statistics()
        print(f"  Nodes: {stats.get('nodes', 0)}")
        print(f"  Edges: {stats.get('edges', 0)}")
        print(f"  Flow iterations: {stats.get('flow_iterations', 0)}")
        print(f"  Satisfied clauses: {stats.get('satisfied_clauses', 0)}")
    else:
        print("✗ UNSAT")

    print()


def example_compare_to_cdcl():
    """Compare PHYSARUM-SAT to CDCL."""
    print("=" * 60)
    print("Example 5: PHYSARUM-SAT vs CDCL")
    print("=" * 60)

    from bsat.cdcl import CDCLSolver

    cnf = CNFExpression.parse("(a | b) & (~a | c) & (b | ~c) & (~b | d)")

    print(f"Formula: {cnf}")
    print()

    # Solve with PHYSARUM-SAT
    print("Solving with PHYSARUM-SAT...")
    physarum_solver = PHYSARUMSATSolver(cnf, max_iterations=1000)
    physarum_result = physarum_solver.solve()
    physarum_stats = physarum_solver.get_network_statistics()

    print(f"  Result: {'SAT' if physarum_result else 'UNSAT'}")
    print(f"  Flow iterations: {physarum_stats.get('flow_iterations', 0)}")
    print()

    # Solve with CDCL
    print("Solving with CDCL...")
    cdcl_solver = CDCLSolver(cnf)
    cdcl_result = cdcl_solver.solve()

    print(f"  Result: {'SAT' if cdcl_result else 'UNSAT'}")
    print(f"  Conflicts: {cdcl_solver.stats.conflicts}")
    print()

    # Compare
    if physarum_result == cdcl_result:
        print("✓ Both solvers agree!")
    else:
        print("✗ Solvers disagree (potential bug)")

    print()


def example_flow_visualization():
    """Show flow dynamics details."""
    print("=" * 60)
    print("Example 6: Flow Dynamics Visualization")
    print("=" * 60)

    cnf = CNFExpression.parse("(a | b) & (~a | b)")

    print(f"Formula: {cnf}")
    print()

    solver = PHYSARUMSATSolver(cnf, max_iterations=100)

    print("Network structure:")
    print(f"  Variable nodes: {len(solver.network.get_variable_nodes())}")
    print(f"  Clause nodes: {len(solver.network.get_clause_nodes())}")
    print(f"  Total edges: {len(solver.network.edges)}")
    print()

    print("Variables and their paths:")
    for var in cnf.get_variables():
        true_edge, false_edge = solver.network.get_path_edges(var)
        if true_edge and false_edge:
            print(f"  {var}: T-path={true_edge.edge_id}, F-path={false_edge.edge_id}")
    print()

    result = solver.solve()

    if result:
        print(f"✓ SAT!")
        print(f"Solution: {result}")
        print()
        print("Flow pattern (stronger flow → assignment):")
        for var in cnf.get_variables():
            true_edge, false_edge = solver.network.get_path_edges(var)
            if true_edge and false_edge:
                true_flow = abs(true_edge.flow_rate)
                false_flow = abs(false_edge.flow_rate)
                assignment = "True" if result[var] else "False"
                print(f"  {var}: T-flow={true_flow:.3f}, F-flow={false_flow:.3f} → {assignment}")
    else:
        print("✗ UNSAT")

    print()


def main():
    """Run all examples."""
    print()
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 8 + "PHYSARUM-SAT: Slime Mold Network SAT Solver" + " " * 7 + "║")
    print("╚" + "=" * 58 + "╝")
    print()

    example_simple()
    example_unit_clauses()
    example_unsat()
    example_larger()
    example_compare_to_cdcl()
    example_flow_visualization()

    print("=" * 60)
    print("All examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
