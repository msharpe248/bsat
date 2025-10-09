"""
Examples demonstrating CDCL (Conflict-Driven Clause Learning) SAT solver.

CDCL is a modern SAT solving algorithm that learns from conflicts to avoid
repeating the same mistakes. This implementation includes unit propagation,
conflict analysis, clause learning, VSIDS heuristic, and restart strategies.
"""

from bsat import (
    CNFExpression, Clause, Literal,
    CDCLSolver, solve_cdcl, get_cdcl_stats
)


def example1_basic_usage():
    """Example 1: Basic CDCL usage."""
    print("=" * 60)
    print("Example 1: Basic CDCL Usage")
    print("=" * 60)

    # Simple 3-SAT formula
    formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z)"
    cnf = CNFExpression.parse(formula)

    print(f"\nFormula: {formula}")
    print(f"CNF: {cnf}")

    # Solve using CDCL
    result = solve_cdcl(cnf)

    if result:
        print(f"\nSAT: {result}")
        print(f"Verifies: {cnf.evaluate(result)}")
    else:
        print("\nUNSAT")


def example2_statistics():
    """Example 2: Collecting statistics."""
    print("\n" + "=" * 60)
    print("Example 2: CDCL Statistics")
    print("=" * 60)

    formula = "(a | b) & (~a | c) & (~b | ~c) & (c | d) & (~d)"
    cnf = CNFExpression.parse(formula)

    print(f"\nFormula: {formula}")

    # Solve and get statistics
    result, stats = get_cdcl_stats(cnf)

    if result:
        print(f"\nSolution: {result}")
    else:
        print("\nNo solution (UNSAT)")

    print(f"\nStatistics:")
    print(stats)


def example3_vsids_heuristic():
    """Example 3: VSIDS heuristic with different decay factors."""
    print("\n" + "=" * 60)
    print("Example 3: VSIDS Heuristic")
    print("=" * 60)

    formula = "(p | q | r) & (~p | q) & (~q | r) & (~r | p)"
    cnf = CNFExpression.parse(formula)

    print(f"\nFormula: {formula}")

    # Try different VSIDS decay factors
    for decay in [0.90, 0.95, 0.99]:
        result = solve_cdcl(cnf, vsids_decay=decay)
        print(f"\nVSIDS decay={decay}: {result if result else 'UNSAT'}")


def example4_unit_propagation():
    """Example 4: Demonstrating unit propagation."""
    print("\n" + "=" * 60)
    print("Example 4: Unit Propagation")
    print("=" * 60)

    # Formula with chain of implications
    # x → y → z → w (encoded as CNF)
    cnf = CNFExpression([
        Clause([Literal('x', False)]),  # x must be true
        Clause([Literal('x', True), Literal('y', False)]),  # x → y
        Clause([Literal('y', True), Literal('z', False)]),  # y → z
        Clause([Literal('z', True), Literal('w', False)])   # z → w
    ])

    print(f"\nFormula (chain of implications): {cnf}")
    print("Expected: x=T, y=T, z=T, w=T (via unit propagation)")

    result, stats = get_cdcl_stats(cnf)

    print(f"\nSolution: {result}")
    print(f"Unit propagations performed: {stats.propagations}")
    print(f"Decisions made: {stats.decisions}")


def example5_clause_learning():
    """Example 5: Clause learning from conflicts."""
    print("\n" + "=" * 60)
    print("Example 5: Clause Learning")
    print("=" * 60)

    # Formula that causes conflicts
    cnf = CNFExpression([
        Clause([Literal('a', False), Literal('b', False)]),
        Clause([Literal('a', True), Literal('c', False)]),
        Clause([Literal('b', True), Literal('d', False)]),
        Clause([Literal('c', True), Literal('d', True)])
    ])

    print(f"\nFormula: {cnf}")

    result, stats = get_cdcl_stats(cnf)

    if result:
        print(f"\nSolution: {result}")
    else:
        print("\nUNSAT")

    print(f"\nConflicts encountered: {stats.conflicts}")
    print(f"Clauses learned: {stats.learned_clauses}")
    print(f"Restarts: {stats.restarts}")


def example6_comparison_with_dpll():
    """Example 6: Comparing CDCL with DPLL."""
    print("\n" + "=" * 60)
    print("Example 6: CDCL vs DPLL")
    print("=" * 60)

    from bsat import solve_sat  # DPLL solver

    formula = "(x | y | z) & (~x | y) & (~y | z) & (~z | x)"
    cnf = CNFExpression.parse(formula)

    print(f"\nFormula: {formula}")

    # Solve with DPLL
    result_dpll = solve_sat(cnf)
    print(f"\nDPLL result: {result_dpll}")

    # Solve with CDCL
    result_cdcl, stats_cdcl = get_cdcl_stats(cnf)
    print(f"CDCL result: {result_cdcl}")
    print(f"\nCDCL stats:")
    print(f"  Decisions: {stats_cdcl.decisions}")
    print(f"  Propagations: {stats_cdcl.propagations}")
    print(f"  Conflicts: {stats_cdcl.conflicts}")


def example7_solver_class():
    """Example 7: Using CDCLSolver class directly."""
    print("\n" + "=" * 60)
    print("Example 7: Using CDCLSolver Class")
    print("=" * 60)

    cnf = CNFExpression([
        Clause([Literal('p', False), Literal('q', False)]),
        Clause([Literal('p', True), Literal('r', False)]),
        Clause([Literal('q', True), Literal('r', True)])
    ])

    print(f"\nFormula: {cnf}")

    # Create solver with custom parameters
    solver = CDCLSolver(
        cnf,
        vsids_decay=0.95,
        restart_base=50,
        learned_clause_limit=1000
    )

    # Solve
    result = solver.solve(max_conflicts=500)

    print(f"\nSolution: {result}")
    print(f"\nStatistics:")
    print(solver.get_stats())


def example8_unsat_detection():
    """Example 8: UNSAT detection."""
    print("\n" + "=" * 60)
    print("Example 8: UNSAT Detection")
    print("=" * 60)

    # Unsatisfiable formula
    cnf = CNFExpression([
        Clause([Literal('x', False)]),
        Clause([Literal('x', True)])
    ])

    print(f"\nFormula: {cnf}")
    print("This formula is clearly UNSAT (x and ~x)")

    result, stats = get_cdcl_stats(cnf)

    print(f"\nResult: {result}")
    print(f"Conflicts: {stats.conflicts}")


if __name__ == '__main__':
    example1_basic_usage()
    example2_statistics()
    example3_vsids_heuristic()
    example4_unit_propagation()
    example5_clause_learning()
    example6_comparison_with_dpll()
    example7_solver_class()
    example8_unsat_detection()

    print("\n" + "=" * 60)
    print("All CDCL examples completed!")
    print("=" * 60)
