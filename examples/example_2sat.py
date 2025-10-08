#!/usr/bin/env python3
"""Examples of using the 2SAT solver."""

from bsat import CNFExpression, TwoSATSolver, solve_2sat, is_2sat_satisfiable


def example_basic_usage():
    """Basic usage of the 2SAT solver."""
    print("=" * 60)
    print("Example 1: Basic 2SAT Solving")
    print("=" * 60)

    # Create a simple 2SAT instance
    expr = CNFExpression.parse("(x | y) & (~x | z) & (~y | z)")
    print(f"Formula: {expr}\n")

    # Check if satisfiable
    is_sat = is_2sat_satisfiable(expr)
    print(f"Is satisfiable: {is_sat}")

    # Get a solution
    solution = solve_2sat(expr)
    if solution:
        print(f"Solution found: {solution}")
        print(f"Verification: {expr.evaluate(solution)}")
    else:
        print("No solution exists (unsatisfiable)")

    print()


def example_unsatisfiable():
    """Example of an unsatisfiable 2SAT instance."""
    print("=" * 60)
    print("Example 2: Unsatisfiable Formula")
    print("=" * 60)

    # This formula is unsatisfiable
    # (x ∨ y) ∧ (¬x ∨ y) ∧ (x ∨ ¬y) ∧ (¬x ∨ ¬y)
    # Forces both y and ¬y, which is impossible
    expr = CNFExpression.parse("(x | y) & (~x | y) & (x | ~y) & (~x | ~y)")
    print(f"Formula: {expr}\n")

    solution = solve_2sat(expr)
    if solution:
        print(f"Solution found: {solution}")
    else:
        print("No solution exists (unsatisfiable)")
        print("\nWhy? This formula forces:")
        print("  - From (x ∨ y) and (¬x ∨ y): y must be true")
        print("  - From (x ∨ ¬y) and (¬x ∨ ¬y): y must be false")
        print("  - This is a contradiction!")

    print()


def example_implication():
    """Example using implications."""
    print("=" * 60)
    print("Example 3: Logical Implications")
    print("=" * 60)

    # Encode: If it's raining, then the ground is wet
    #         If the ground is wet, then it's slippery
    # Using implications: rain → wet, wet → slippery
    # In CNF: (¬rain ∨ wet) ∧ (¬wet ∨ slippery)

    expr = CNFExpression.parse("(~rain | wet) & (~wet | slippery)")
    print(f"Formula: {expr}")
    print("Encodes: rain → wet → slippery\n")

    solution = solve_2sat(expr)
    print(f"Solution: {solution}\n")

    # Test specific scenarios
    print("Testing scenarios:")

    scenario1 = {"rain": True, "wet": solution["wet"], "slippery": solution["slippery"]}
    print(f"  If rain=True: wet={scenario1['wet']}, slippery={scenario1['slippery']}")
    print(f"  Formula satisfied: {expr.evaluate(scenario1)}")

    scenario2 = {"rain": False, "wet": False, "slippery": False}
    print(f"  If rain=False: Can have wet=False, slippery=False")
    print(f"  Formula satisfied: {expr.evaluate(scenario2)}")

    print()


def example_scheduling():
    """Example: Simple scheduling problem."""
    print("=" * 60)
    print("Example 4: Scheduling Problem")
    print("=" * 60)

    print("Problem: Schedule tasks A and B")
    print("  - At least one task must be scheduled in the morning")
    print("  - If A is in morning, B must be in afternoon")
    print("  - If B is in afternoon, task C must be in morning\n")

    # Variables: am = A in morning, bm = B in morning, cm = C in morning
    # Constraints:
    # 1. At least one of A or B in morning: (am ∨ bm)
    # 2. A in morning → B in afternoon: (¬am ∨ ¬bm)
    # 3. B in afternoon → C in morning: (bm ∨ cm) [if B not morning, C in morning]

    expr = CNFExpression.parse("(am | bm) & (~am | ~bm) & (bm | cm)")
    print(f"Formula: {expr}\n")

    solution = solve_2sat(expr)
    if solution:
        print("Schedule found:")
        print(f"  Task A in morning: {solution['am']}")
        print(f"  Task B in morning: {solution['bm']}")
        print(f"  Task C in morning: {solution['cm']}")
        print(f"\nInterpretation:")
        print(f"  Task A: {'Morning' if solution['am'] else 'Afternoon'}")
        print(f"  Task B: {'Morning' if solution['bm'] else 'Afternoon'}")
        print(f"  Task C: {'Morning' if solution['cm'] else 'Afternoon'}")
    else:
        print("No valid schedule exists")

    print()


def example_graph_coloring():
    """Example: 2-coloring problem."""
    print("=" * 60)
    print("Example 5: Graph 2-Coloring")
    print("=" * 60)

    print("Problem: Color a triangle graph with 2 colors")
    print("  Vertices: 1, 2, 3")
    print("  Edges: (1,2), (2,3), (1,3)")
    print("  Variables: vi means vertex i is colored with color 1\n")

    # For each edge (u,v), we need: u and v have different colors
    # Different colors: (u ∨ v) ∧ (¬u ∨ ¬v)
    # This means: at least one has color 1, and at least one has color 0

    expr = CNFExpression.parse(
        "(v1 | v2) & (~v1 | ~v2) & "  # Edge (1,2)
        "(v2 | v3) & (~v2 | ~v3) & "  # Edge (2,3)
        "(v1 | v3) & (~v1 | ~v3)"     # Edge (1,3)
    )
    print(f"Formula: {expr}\n")

    solution = solve_2sat(expr)
    if solution:
        print("2-coloring found:")
        for vertex in ['v1', 'v2', 'v3']:
            color = "Color 1" if solution[vertex] else "Color 0"
            print(f"  Vertex {vertex[-1]}: {color}")
    else:
        print("No 2-coloring exists (graph is not bipartite)")
        print("\nWhy? A triangle (3-cycle) requires 3 colors minimum.")

    print()


def example_solver_class():
    """Example using TwoSATSolver class directly."""
    print("=" * 60)
    print("Example 6: Using TwoSATSolver Class")
    print("=" * 60)

    expr = CNFExpression.parse("(p | q) & (~p | r) & (~q | r)")
    print(f"Formula: {expr}\n")

    # Create solver instance
    solver = TwoSATSolver(expr)

    # Check satisfiability
    print(f"Is satisfiable: {solver.is_satisfiable()}")

    # Get solution
    solution = solver.solve()
    print(f"Solution: {solution}")

    # Verify
    if solution:
        print(f"Verification: {expr.evaluate(solution)}")

    # Show implication graph structure
    print(f"\nImplication graph has {len(solver.implication_graph)} nodes")
    print("Sample implications:")
    for literal, implications in list(solver.implication_graph.items())[:5]:
        if implications:
            print(f"  {literal} → {implications}")

    print()


def main():
    """Run all examples."""
    print("\n" + "=" * 60)
    print("2SAT Solver - Practical Examples")
    print("=" * 60)
    print()

    example_basic_usage()
    example_unsatisfiable()
    example_implication()
    example_scheduling()
    example_graph_coloring()
    example_solver_class()

    print("=" * 60)
    print("Examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
