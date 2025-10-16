#!/usr/bin/env python3
"""
CoBD-SAT Example Demonstrations

This example demonstrates the Community-Based Decomposition SAT solver on
various types of formulas, showing when it excels and when it falls back.
"""

import sys
import os
from time import time

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.dpll import DPLLSolver
from cobd_sat import CoBDSATSolver


def print_header(title):
    """Print a formatted header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_stats(stats):
    """Print solver statistics in a formatted way."""
    print("\nStatistics:")
    print(f"  Communities: {stats['num_communities']}")
    print(f"  Modularity Q: {stats['modularity']:.3f}")
    print(f"  Interface Variables: {stats['num_interface_vars']} ({stats['interface_percentage']:.1f}%)")
    print(f"  Used Decomposition: {stats['used_decomposition']}")
    if not stats['used_decomposition'] and stats['fallback_reason']:
        print(f"  Fallback Reason: {stats['fallback_reason']}")
    if stats['used_decomposition']:
        print(f"  Interface Assignments Tried: {stats['interface_assignments_tried']}")
        print(f"  Community Solve Attempts: {stats['community_solve_attempts']}")


def example_1_modular_formula():
    """
    Example 1: Modular Formula (Two Independent Communities)

    Formula has two groups of variables that don't interact much:
    Group 1: (a | b) & (a | c) & (b | c)
    Group 2: (d | e) & (d | f) & (e | f)
    Interface: Single clause (c | d) connecting the groups

    Expected: High modularity, effective decomposition
    """
    print_header("Example 1: Modular Formula (Two Communities)")

    formula_str = "(a | b) & (a | c) & (b | c) & (c | d) & (d | e) & (d | f) & (e | f)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CoBD-SAT
    print("\nSolving with CoBD-SAT...")
    cobd_solver = CoBDSATSolver(cnf)
    start = time()
    result = cobd_solver.solve()
    cobd_time = time() - start

    print(f"Result: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")
    print(f"Time: {cobd_time:.4f}s")
    print_stats(cobd_solver.get_statistics())

    # Compare with standard DPLL
    print("\nSolving with standard DPLL for comparison...")
    dpll_solver = DPLLSolver(cnf)
    start = time()
    result_dpll = dpll_solver.solve()
    dpll_time = time() - start

    print(f"Result: {'SAT' if result_dpll else 'UNSAT'}")
    print(f"Time: {dpll_time:.4f}s")

    if cobd_time > 0:
        print(f"\nSpeedup: {dpll_time / cobd_time:.2f}x")


def example_2_chain_formula():
    """
    Example 2: Chain Formula (Linear Structure)

    Variables form a chain: a → b → c → d → e → f
    Each variable only interacts with neighbors.

    Expected: Multiple small communities, high modularity
    """
    print_header("Example 2: Chain Formula (Linear Communities)")

    formula_str = "(a) & (~a | b) & (~b | c) & (~c | d) & (~d | e) & (~e | f)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CoBD-SAT
    cobd_solver = CoBDSATSolver(cnf, max_communities=6)
    result = cobd_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(cobd_solver.get_statistics())


def example_3_random_formula():
    """
    Example 3: Random-like Formula (No Structure)

    Highly interconnected formula with no clear communities.
    Every variable appears with many others.

    Expected: Low modularity, fallback to DPLL
    """
    print_header("Example 3: Random Formula (No Structure)")

    # Dense formula where variables are highly mixed
    formula_str = "(a | b | c) & (a | d | e) & (b | d | f) & (c | e | f) & (a | b | f) & (c | d | e) & (~a | ~b | d) & (~c | ~d | f) & (~e | ~f | b)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CoBD-SAT
    cobd_solver = CoBDSATSolver(cnf)
    result = cobd_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(cobd_solver.get_statistics())


def example_4_circuit_like():
    """
    Example 4: Circuit-like Formula (Multiple Modules)

    Simulates a circuit with multiple independent blocks connected by signals.

    Module 1: Input processing (in1, in2, sig1)
    Module 2: Logic block 1 (sig1, sig2, out1)
    Module 3: Logic block 2 (sig1, sig3, out2)
    Module 4: Output combining (out1, out2, final)

    Expected: High modularity with sig1 as key interface variable
    """
    print_header("Example 4: Circuit-like Formula (Module Structure)")

    # Module 1: Input processing
    m1 = "(in1 | in2) & (~in1 | sig1) & (~in2 | sig1)"

    # Module 2: Logic block 1
    m2 = "(sig1 | sig2) & (~sig1 | out1) & (~sig2 | out1)"

    # Module 3: Logic block 2
    m3 = "(sig1 | sig3) & (~sig1 | out2) & (~sig3 | out2)"

    # Module 4: Output combining
    m4 = "(out1 | out2) & (~out1 | final) & (~out2 | final)"

    formula_str = f"{m1} & {m2} & {m3} & {m4}"
    print(f"\nFormula (4 modules):")
    print(f"  Module 1 (Input):  {m1}")
    print(f"  Module 2 (Logic1): {m2}")
    print(f"  Module 3 (Logic2): {m3}")
    print(f"  Module 4 (Output): {m4}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CoBD-SAT
    cobd_solver = CoBDSATSolver(cnf, max_communities=4)
    result = cobd_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    stats = cobd_solver.get_statistics()
    print_stats(stats)

    # Show interface variables
    viz_data = cobd_solver.get_visualization_data()
    print(f"\nInterface Variables: {viz_data['interface_variables']}")


def example_5_planning_like():
    """
    Example 5: Planning-like Formula (Temporal Structure)

    Simulates a planning problem with time steps.
    Each time step is a community connected by state transition variables.

    Time 0: Initial state (s0, a0)
    Time 1: State after action (s1, a1)
    Time 2: State after action (s2, a2)
    Time 3: Goal state (s3)

    Expected: Temporal communities with state variables as interfaces
    """
    print_header("Example 5: Planning Formula (Temporal Communities)")

    # Time 0: Initial state constraints
    t0 = "(s0) & (~s0 | a0)"

    # Time 0→1 transition
    trans01 = "(~a0 | s1) & (~s0 | s1)"

    # Time 1: Action selection
    t1 = "(s1 | a1)"

    # Time 1→2 transition
    trans12 = "(~a1 | s2) & (~s1 | s2)"

    # Time 2: Action selection
    t2 = "(s2 | a2)"

    # Time 2→3 transition
    trans23 = "(~a2 | s3) & (~s2 | s3)"

    # Time 3: Goal
    t3 = "(s3)"

    formula_str = f"{t0} & {trans01} & {t1} & {trans12} & {t2} & {trans23} & {t3}"
    print(f"\nFormula (4 time steps):")
    print(f"  T0: {t0}")
    print(f"  T0→T1: {trans01}")
    print(f"  T1: {t1}")
    print(f"  T1→T2: {trans12}")
    print(f"  T2: {t2}")
    print(f"  T2→T3: {trans23}")
    print(f"  T3: {t3}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CoBD-SAT
    cobd_solver = CoBDSATSolver(cnf, max_communities=4)
    result = cobd_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(cobd_solver.get_statistics())


def example_6_visualization_data():
    """
    Example 6: Exporting Visualization Data

    Shows how to export data for visualization in the web interface.
    """
    print_header("Example 6: Visualization Data Export")

    formula_str = "(a | b) & (b | c) & (c | d) & (d | e) & (e | f)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)
    solver = CoBDSATSolver(cnf)
    result = solver.solve()

    # Get visualization data
    viz_data = solver.get_visualization_data()

    print(f"\nVisualization Data Structure:")
    print(f"  Nodes: {len(viz_data['nodes'])} (variables + clauses)")
    print(f"  Edges: {len(viz_data['edges'])}")
    print(f"\nNode types:")

    var_nodes = [n for n in viz_data['nodes'] if n['type'] == 'variable']
    clause_nodes = [n for n in viz_data['nodes'] if n['type'] == 'clause']

    print(f"  Variables: {len(var_nodes)}")
    print(f"  Clauses: {len(clause_nodes)}")

    print(f"\nSample variable nodes:")
    for node in var_nodes[:3]:
        print(f"    {node}")

    print(f"\nSample clause nodes:")
    for node in clause_nodes[:3]:
        print(f"    {node}")

    print(f"\nCommunity distribution:")
    for comm_id, members in viz_data['statistics']['vars_per_community'].items():
        print(f"  Community {comm_id}: {members} variables")


def main():
    """Run all examples."""
    print("\n" + "█" * 70)
    print("█" + " " * 68 + "█")
    print("█" + "  CoBD-SAT: Community-Based Decomposition SAT Solver".center(68) + "█")
    print("█" + "  Example Demonstrations".center(68) + "█")
    print("█" + " " * 68 + "█")
    print("█" * 70)

    try:
        example_1_modular_formula()
        example_2_chain_formula()
        example_3_random_formula()
        example_4_circuit_like()
        example_5_planning_like()
        example_6_visualization_data()

        print("\n" + "=" * 70)
        print("  All examples completed successfully!")
        print("=" * 70 + "\n")

    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
