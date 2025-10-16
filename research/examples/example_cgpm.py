#!/usr/bin/env python3
"""
CGPM-SAT Example Demonstrations

This example demonstrates the Conflict Graph Pattern Mining SAT solver,
showing how graph metrics guide variable selection and when this helps.
"""

import sys
import os
from time import time

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.cdcl import CDCLSolver
from cgpm_sat import CGPMSolver


def print_header(title):
    """Print a formatted header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_stats(stats):
    """Print solver statistics in a formatted way."""
    print("\nStatistics:")
    print(f"  Decisions Made: {stats['decisions_made']}")
    print(f"  Graph Influenced Decisions: {stats['graph_influenced_decisions']} ({stats['graph_influence_rate']:.1f}%)")
    print(f"  Graph Updates: {stats['graph_updates']}")
    print(f"  Graph Queries: {stats['graph_queries']}")
    print(f"  Graph Construction Time: {stats['graph_construction_time']:.4f}s")
    print(f"  Total Time: {stats['total_time']:.4f}s")
    print(f"  Graph Overhead: {stats['graph_overhead_percentage']:.1f}%")

    # Graph statistics
    graph_stats = stats['graph_statistics']
    print(f"\n  Conflict Graph:")
    print(f"    Variables: {graph_stats['num_variables']}")
    print(f"    Edges: {graph_stats['num_edges']}")
    print(f"    Avg Degree: {graph_stats['avg_degree']:.2f}")
    print(f"    Max Degree: {graph_stats['max_degree']}")
    print(f"    Density: {graph_stats['graph_density']:.4f}")


def example_1_structured_conflicts():
    """
    Example 1: Formula with Structured Conflicts

    Some variables appear in many clauses together, creating clear graph structure.
    Expected: Graph metrics identify important variables.
    """
    print_header("Example 1: Structured Conflicts")

    # Formula where 'a' is central (appears with many others)
    formula_str = "(a | b) & (a | c) & (a | d) & (b | c) & (c | d) & (~a | ~b | e) & (~c | ~d | f)"
    print(f"\nFormula: {formula_str}")
    print("Expected: 'a' has high PageRank (central to conflicts)")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CGPM-SAT
    print("\nSolving with CGPM-SAT...")
    cgpm_solver = CGPMSolver(cnf, graph_weight=0.5, update_frequency=10)
    start = time()
    result = cgpm_solver.solve()
    cgpm_time = time() - start

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")
    print(f"Time: {cgpm_time:.4f}s")
    print_stats(cgpm_solver.get_statistics())

    # Show top variables by PageRank
    top_vars = cgpm_solver.get_top_variables_by_graph(k=3)
    print(f"\nTop 3 variables by PageRank: {top_vars}")

    # Compare with pure CDCL
    print("\nSolving with pure CDCL for comparison...")
    cdcl_solver = CDCLSolver(cnf)
    start = time()
    result_cdcl = cdcl_solver.solve()
    cdcl_time = time() - start

    print(f"Result: {'SAT' if result_cdcl else 'UNSAT'}")
    print(f"Time: {cdcl_time:.4f}s")

    if cdcl_time > 0:
        print(f"\nSpeedup: {cdcl_time / cgpm_time:.2f}x")


def example_2_no_structure():
    """
    Example 2: Formula with No Clear Structure

    Variables don't have clear interaction patterns.
    Expected: Graph overhead without benefit.
    """
    print_header("Example 2: No Clear Structure")

    formula_str = "(a | b) & (c | d) & (e | f) & (g | h)"
    print(f"\nFormula: {formula_str}")
    print("Expected: Graph shows little structure (low density)")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CGPM-SAT
    cgpm_solver = CGPMSolver(cnf, graph_weight=0.5)
    result = cgpm_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(cgpm_solver.get_statistics())


def example_3_circuit_like():
    """
    Example 3: Circuit-like Formula

    Mimics circuit structure with gates and connections.
    Expected: High graph structure, clear PageRank leaders.
    """
    print_header("Example 3: Circuit-like Structure")

    # Circuit: AND gates and connections
    gate1 = "(~a | ~b | g1)"  # g1 = a AND b
    gate2 = "(~c | ~d | g2)"  # g2 = c AND d
    output = "(~g1 | ~g2 | out)"  # out = g1 AND g2
    inputs = "(a) & (b) & (c)"  # Fixed inputs

    formula_str = f"{gate1} & {gate2} & {output} & {inputs}"
    print(f"\nFormula (circuit with 2 AND gates):")
    print(f"  g1 = a AND b")
    print(f"  g2 = c AND d")
    print(f"  out = g1 AND g2")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CGPM-SAT
    cgpm_solver = CGPMSolver(cnf, graph_weight=0.7, update_frequency=5)
    result = cgpm_solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(cgpm_solver.get_statistics())

    # Show top variables
    top_vars = cgpm_solver.get_top_variables_by_graph(k=5)
    print(f"\nTop 5 variables by PageRank: {top_vars}")


def example_4_graph_weight_comparison():
    """
    Example 4: Comparing Different Graph Weights

    Shows impact of graph_weight parameter (balance with VSIDS).
    """
    print_header("Example 4: Graph Weight Comparison")

    formula_str = "(a | b | c) & (~a | d) & (~b | e) & (~c | f) & (d | e | f) & (~d | ~e) & (~e | ~f)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    weights = [0.0, 0.3, 0.5, 0.7, 1.0]

    for weight in weights:
        print(f"\n--- Graph Weight: {weight:.1f} ({weight*100:.0f}% graph, {(1-weight)*100:.0f}% VSIDS) ---")

        solver = CGPMSolver(cnf, graph_weight=weight, update_frequency=10)
        start = time()
        result = solver.solve()
        solve_time = time() - start

        stats = solver.get_statistics()
        print(f"  Result: {'SAT' if result else 'UNSAT'}")
        print(f"  Time: {solve_time:.4f}s")
        print(f"  Decisions: {stats['decisions_made']}")
        print(f"  Graph Influence: {stats['graph_influence_rate']:.1f}%")


def example_5_update_frequency():
    """
    Example 5: Update Frequency Impact

    Shows how often to update graph with learned clauses.
    """
    print_header("Example 5: Update Frequency Comparison")

    formula_str = "(a | b | c | d) & (~a | ~b | e) & (~c | ~d | f) & (e | f | g) & (~e | ~g) & (a | ~c)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    frequencies = [1, 5, 10, 50]

    for freq in frequencies:
        print(f"\n--- Update Every {freq} Learned Clause(s) ---")

        solver = CGPMSolver(cnf, graph_weight=0.5, update_frequency=freq)
        start = time()
        result = solver.solve()
        solve_time = time() - start

        stats = solver.get_statistics()
        print(f"  Result: {'SAT' if result else 'UNSAT'}")
        print(f"  Time: {solve_time:.4f}s")
        print(f"  Graph Updates: {stats['graph_updates']}")
        print(f"  Overhead: {stats['graph_overhead_percentage']:.1f}%")


def example_6_graph_metrics():
    """
    Example 6: Detailed Graph Metrics

    Shows various graph metrics for each variable.
    """
    print_header("Example 6: Detailed Graph Metrics")

    formula_str = "(a | b) & (a | c) & (b | c) & (~a | d) & (~b | e) & (~c | f) & (d | e | f)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CGPM-SAT
    solver = CGPMSolver(cnf)
    result = solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")

    # Get visualization data
    viz_data = solver.get_visualization_data()

    print("\nPer-Variable Graph Metrics:")
    print(f"{'Variable':<10} {'PageRank':<12} {'Degree':<8} {'Clustering':<12}")
    print("-" * 50)

    # Sort by PageRank
    nodes = sorted(viz_data['conflict_graph']['nodes'],
                  key=lambda n: n['pagerank'], reverse=True)

    for node in nodes:
        var = node['id']
        pr = node['pagerank']
        deg = node['degree']
        clust = node['clustering']

        print(f"{var:<10} {pr:<12.4f} {deg:<8} {clust:<12.4f}")


def example_7_graph_visualization_export():
    """
    Example 7: Export Graph for Visualization

    Shows how to export conflict graph structure.
    """
    print_header("Example 7: Graph Visualization Export")

    formula_str = "(a | b | c) & (~a | d) & (~b | e) & (~c | ~d | ~e) & (d | e)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # Solve with CGPM-SAT
    solver = CGPMSolver(cnf, graph_weight=0.5)
    result = solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")

    # Export visualization data
    viz_data = solver.get_visualization_data()

    print("\nConflict Graph Structure:")
    print(f"  Nodes: {len(viz_data['conflict_graph']['nodes'])}")
    print(f"  Edges: {len(viz_data['conflict_graph']['edges'])}")

    print("\nSample Edges (variable co-occurrences in clauses):")
    for edge in viz_data['conflict_graph']['edges'][:5]:
        print(f"  {edge['source']} -- {edge['target']}: weight={edge['weight']}")

    print("\nGraph Statistics:")
    graph_stats = viz_data['conflict_graph']['statistics']
    print(f"  Density: {graph_stats['graph_density']:.4f}")
    print(f"  Avg Degree: {graph_stats['avg_degree']:.2f}")


def example_8_disable_graph():
    """
    Example 8: Disable Graph Heuristic

    Shows CGPM-SAT with graph disabled = pure CDCL baseline.
    """
    print_header("Example 8: Graph Heuristic Disabled (Pure CDCL)")

    formula_str = "(a | b) & (~a | c) & (~b | d) & (~c | ~d)"
    print(f"\nFormula: {formula_str}")

    cnf = CNFExpression.parse(formula_str)

    # CGPM-SAT with graph disabled
    print("\nSolving with CGPM-SAT (graph disabled)...")
    solver = CGPMSolver(cnf, use_graph_heuristic=False)
    result = solver.solve()

    print(f"\nResult: {'SAT' if result else 'UNSAT'}")
    if result:
        print(f"Solution: {result}")

    print_stats(solver.get_statistics())


def main():
    """Run all examples."""
    print("\n" + "█" * 70)
    print("█" + " " * 68 + "█")
    print("█" + "  CGPM-SAT: Conflict Graph Pattern Mining SAT".center(68) + "█")
    print("█" + "  Example Demonstrations".center(68) + "█")
    print("█" + " " * 68 + "█")
    print("█" * 70)

    try:
        example_1_structured_conflicts()
        example_2_no_structure()
        example_3_circuit_like()
        example_4_graph_weight_comparison()
        example_5_update_frequency()
        example_6_graph_metrics()
        example_7_graph_visualization_export()
        example_8_disable_graph()

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
