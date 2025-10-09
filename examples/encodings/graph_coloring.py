"""
Graph Coloring Problem Encoding to SAT

The graph k-coloring problem asks: can we assign one of k colors to each vertex
such that no two adjacent vertices have the same color?

This is a classic NP-complete problem with applications in:
- Register allocation in compilers
- Scheduling problems
- Frequency assignment
- Map coloring
"""

from bsat import CNFExpression, Clause, Literal, solve_sat, solve_cdcl
from typing import List, Tuple, Dict, Optional


def encode_graph_coloring(edges: List[Tuple[int, int]], num_vertices: int, num_colors: int) -> CNFExpression:
    """
    Encode graph k-coloring problem as SAT.

    Variables: x_v_c means "vertex v has color c"

    Constraints:
    1. Each vertex has at least one color: (x_v_0 ∨ x_v_1 ∨ ... ∨ x_v_k)
    2. Each vertex has at most one color: (¬x_v_i ∨ ¬x_v_j) for all i ≠ j
    3. Adjacent vertices have different colors: (¬x_u_c ∨ ¬x_v_c) for edge (u,v)

    Args:
        edges: List of edges as (vertex1, vertex2) tuples
        num_vertices: Total number of vertices
        num_colors: Number of colors available (k in k-coloring)

    Returns:
        CNF formula encoding the graph coloring problem

    Example:
        >>> # Triangle graph (3 vertices, 3 edges)
        >>> edges = [(0, 1), (1, 2), (2, 0)]
        >>> cnf = encode_graph_coloring(edges, num_vertices=3, num_colors=3)
        >>> solution = solve_sat(cnf)
        >>> # Decode: vertex 0 is color 0, vertex 1 is color 1, vertex 2 is color 2
    """
    clauses = []

    def var_name(vertex: int, color: int) -> str:
        """Create variable name for vertex-color assignment."""
        return f"x_{vertex}_{color}"

    # Constraint 1: Each vertex must have at least one color
    for v in range(num_vertices):
        clause = Clause([Literal(var_name(v, c), False) for c in range(num_colors)])
        clauses.append(clause)

    # Constraint 2: Each vertex has at most one color (no two colors simultaneously)
    for v in range(num_vertices):
        for c1 in range(num_colors):
            for c2 in range(c1 + 1, num_colors):
                # ¬x_v_c1 ∨ ¬x_v_c2 (vertex v cannot be both color c1 and c2)
                clause = Clause([
                    Literal(var_name(v, c1), True),
                    Literal(var_name(v, c2), True)
                ])
                clauses.append(clause)

    # Constraint 3: Adjacent vertices must have different colors
    for u, v in edges:
        for c in range(num_colors):
            # ¬x_u_c ∨ ¬x_v_c (vertices u and v cannot both have color c)
            clause = Clause([
                Literal(var_name(u, c), True),
                Literal(var_name(v, c), True)
            ])
            clauses.append(clause)

    return CNFExpression(clauses)


def decode_coloring(solution: Dict[str, bool], num_vertices: int, num_colors: int) -> Dict[int, int]:
    """
    Decode SAT solution to vertex coloring.

    Args:
        solution: SAT solution mapping variables to True/False
        num_vertices: Number of vertices
        num_colors: Number of colors

    Returns:
        Dictionary mapping vertex -> color
    """
    coloring = {}
    for v in range(num_vertices):
        for c in range(num_colors):
            var = f"x_{v}_{c}"
            if var in solution and solution[var]:
                coloring[v] = c
                break
    return coloring


def verify_coloring(edges: List[Tuple[int, int]], coloring: Dict[int, int]) -> bool:
    """
    Verify that a coloring is valid (no adjacent vertices share colors).

    Args:
        edges: Graph edges
        coloring: Vertex to color mapping

    Returns:
        True if coloring is valid
    """
    for u, v in edges:
        if coloring.get(u) == coloring.get(v):
            return False
    return True


# Example graphs and problems

def example1_triangle():
    """Example 1: Triangle graph - needs 3 colors."""
    print("=" * 60)
    print("Example 1: Triangle Graph (3-Coloring)")
    print("=" * 60)

    # Triangle: 0-1-2-0
    edges = [(0, 1), (1, 2), (2, 0)]
    num_vertices = 3

    print(f"\nGraph: Triangle with {num_vertices} vertices")
    print(f"Edges: {edges}")

    # Try 2-coloring (should fail)
    print("\n--- Trying 2-coloring ---")
    cnf_2 = encode_graph_coloring(edges, num_vertices, num_colors=2)
    solution_2 = solve_sat(cnf_2)
    print(f"2-colorable? {solution_2 is not None}")

    # Try 3-coloring (should succeed)
    print("\n--- Trying 3-coloring ---")
    cnf_3 = encode_graph_coloring(edges, num_vertices, num_colors=3)
    solution_3 = solve_sat(cnf_3)

    if solution_3:
        coloring = decode_coloring(solution_3, num_vertices, num_colors=3)
        print(f"3-colorable? Yes")
        print(f"Coloring: {coloring}")
        print(f"Valid? {verify_coloring(edges, coloring)}")
    else:
        print("3-colorable? No")


def example2_complete_graph():
    """Example 2: Complete graph K4 - needs 4 colors."""
    print("\n" + "=" * 60)
    print("Example 2: Complete Graph K4")
    print("=" * 60)

    # K4: every vertex connected to every other vertex
    num_vertices = 4
    edges = [(i, j) for i in range(num_vertices) for j in range(i + 1, num_vertices)]

    print(f"\nGraph: Complete graph K{num_vertices}")
    print(f"Edges: {edges}")
    print(f"Note: K{num_vertices} requires exactly {num_vertices} colors")

    # Try (n-1) colors (should fail)
    print(f"\n--- Trying {num_vertices - 1}-coloring ---")
    cnf = encode_graph_coloring(edges, num_vertices, num_colors=num_vertices - 1)
    solution = solve_sat(cnf)
    print(f"{num_vertices - 1}-colorable? {solution is not None}")

    # Try n colors (should succeed)
    print(f"\n--- Trying {num_vertices}-coloring ---")
    cnf = encode_graph_coloring(edges, num_vertices, num_colors=num_vertices)
    solution = solve_sat(cnf)

    if solution:
        coloring = decode_coloring(solution, num_vertices, num_colors=num_vertices)
        print(f"{num_vertices}-colorable? Yes")
        print(f"Coloring: {coloring}")
        print(f"Valid? {verify_coloring(edges, coloring)}")


def example3_bipartite_graph():
    """Example 3: Bipartite graph - needs 2 colors."""
    print("\n" + "=" * 60)
    print("Example 3: Bipartite Graph")
    print("=" * 60)

    # Bipartite graph: vertices {0,1} connect to {2,3}
    edges = [(0, 2), (0, 3), (1, 2), (1, 3)]
    num_vertices = 4

    print(f"\nGraph: Bipartite with sets {{0,1}} and {{2,3}}")
    print(f"Edges: {edges}")
    print("Note: Bipartite graphs are always 2-colorable")

    cnf = encode_graph_coloring(edges, num_vertices, num_colors=2)
    solution = solve_sat(cnf)

    if solution:
        coloring = decode_coloring(solution, num_vertices, num_colors=2)
        print(f"\n2-colorable? Yes")
        print(f"Coloring: {coloring}")
        print(f"Valid? {verify_coloring(edges, coloring)}")
        print(f"Set 1 (color {coloring[0]}): {[v for v in range(num_vertices) if coloring[v] == coloring[0]]}")
        print(f"Set 2 (color {1-coloring[0]}): {[v for v in range(num_vertices) if coloring[v] != coloring[0]]}")


def example4_petersen_graph():
    """Example 4: Petersen graph - famous 3-chromatic graph."""
    print("\n" + "=" * 60)
    print("Example 4: Petersen Graph")
    print("=" * 60)

    # Petersen graph: 10 vertices, 15 edges
    # Outer pentagon: 0-1-2-3-4-0
    # Inner pentagram: 5-7-9-6-8-5
    # Connections: 0-5, 1-6, 2-7, 3-8, 4-9
    edges = [
        # Outer pentagon
        (0, 1), (1, 2), (2, 3), (3, 4), (4, 0),
        # Inner pentagram
        (5, 7), (7, 9), (9, 6), (6, 8), (8, 5),
        # Spokes
        (0, 5), (1, 6), (2, 7), (3, 8), (4, 9)
    ]
    num_vertices = 10

    print(f"\nGraph: Petersen graph ({num_vertices} vertices, {len(edges)} edges)")
    print("Famous example: 3-regular, non-planar, 3-chromatic")

    # Try 2-coloring
    print("\n--- Trying 2-coloring ---")
    cnf_2 = encode_graph_coloring(edges, num_vertices, num_colors=2)
    solution_2 = solve_sat(cnf_2)
    print(f"2-colorable? {solution_2 is not None}")

    # Try 3-coloring
    print("\n--- Trying 3-coloring ---")
    cnf_3 = encode_graph_coloring(edges, num_vertices, num_colors=3)
    solution_3 = solve_sat(cnf_3)

    if solution_3:
        coloring = decode_coloring(solution_3, num_vertices, num_colors=3)
        print(f"3-colorable? Yes")
        print(f"Coloring: {coloring}")
        print(f"Valid? {verify_coloring(edges, coloring)}")


def example5_chromatic_number():
    """Example 5: Finding chromatic number by binary search."""
    print("\n" + "=" * 60)
    print("Example 5: Finding Chromatic Number")
    print("=" * 60)

    # Wheel graph W5: center connected to 5-cycle
    edges = [
        (1, 2), (2, 3), (3, 4), (4, 5), (5, 1),  # Outer cycle
        (0, 1), (0, 2), (0, 3), (0, 4), (0, 5)   # Center to all
    ]
    num_vertices = 6

    print(f"\nGraph: Wheel graph W5")
    print(f"Edges: {edges}")

    # Find minimum number of colors needed
    print("\nFinding chromatic number...")
    for k in range(1, num_vertices + 1):
        cnf = encode_graph_coloring(edges, num_vertices, num_colors=k)
        solution = solve_sat(cnf)

        if solution:
            coloring = decode_coloring(solution, num_vertices, num_colors=k)
            print(f"\nChromatic number: {k}")
            print(f"Coloring: {coloring}")
            print(f"Valid? {verify_coloring(edges, coloring)}")
            break
        else:
            print(f"{k}-coloring: UNSAT")


def example6_cdcl_comparison():
    """Example 6: Compare DPLL vs CDCL on larger graph."""
    print("\n" + "=" * 60)
    print("Example 6: DPLL vs CDCL Performance")
    print("=" * 60)

    # Larger graph: 10-vertex cycle
    num_vertices = 10
    edges = [(i, (i + 1) % num_vertices) for i in range(num_vertices)]

    print(f"\nGraph: {num_vertices}-vertex cycle")
    print(f"Edges: {edges}")
    print(f"Note: Cycles are 2-colorable if even length, 3-colorable if odd")

    cnf = encode_graph_coloring(edges, num_vertices, num_colors=2)

    print("\n--- Using DPLL ---")
    from bsat import solve_sat
    solution_dpll = solve_sat(cnf)
    if solution_dpll:
        coloring = decode_coloring(solution_dpll, num_vertices, num_colors=2)
        print(f"2-colorable? Yes")
        print(f"Coloring: {coloring}")

    print("\n--- Using CDCL ---")
    from bsat import get_cdcl_stats
    solution_cdcl, stats = get_cdcl_stats(cnf)
    if solution_cdcl:
        coloring = decode_coloring(solution_cdcl, num_vertices, num_colors=2)
        print(f"2-colorable? Yes")
        print(f"Coloring: {coloring}")
        print(f"\nCDCL Stats:")
        print(f"  Decisions: {stats.decisions}")
        print(f"  Propagations: {stats.propagations}")
        print(f"  Conflicts: {stats.conflicts}")


if __name__ == '__main__':
    example1_triangle()
    example2_complete_graph()
    example3_bipartite_graph()
    example4_petersen_graph()
    example5_chromatic_number()
    example6_cdcl_comparison()

    print("\n" + "=" * 60)
    print("All graph coloring examples completed!")
    print("=" * 60)
