#!/usr/bin/env python3
"""
Example usage of SSTA-SAT solver.

Demonstrates solution space topology analysis on simple SAT instances.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from ssta_sat import SSTASATSolver, solve_ssta_sat


def main():
    print("=" * 70)
    print("SSTA-SAT: Solution Space Topology Analysis SAT Solver Example")
    print("=" * 70)
    print()

    # Example 1: Simple SAT instance with multiple solutions
    print("Example 1: Simple SAT with multiple solutions")
    print("-" * 70)
    formula1 = "(a | b) & (a | c) & (b | d)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = SSTASATSolver(cnf1, num_samples=20, distance_threshold=3)

    result1 = solver1.solve()

    if result1:
        print(f"✓ SAT: {result1}")
    else:
        print("✗ UNSAT")

    print()
    print("Statistics:")
    stats1 = solver1.get_stats()
    print(f"  Decisions: {stats1.decisions}")
    print(f"  Conflicts: {stats1.conflicts}")
    print(f"  Solutions sampled: {stats1.solutions_sampled}")
    print(f"  Clusters detected: {stats1.clusters_detected}")
    print(f"  Topology-guided decisions: {stats1.topology_guided_decisions}")

    # Get detailed topology stats
    topo_stats = solver1.get_topology_statistics()
    print()
    print("Topology Analysis:")
    if 'sampling' in topo_stats:
        print(f"  Samples requested: {topo_stats['sampling']['num_samples_requested']}")
        print(f"  Solutions found: {topo_stats['sampling']['num_solutions']}")
        if 'avg_hamming_distance' in topo_stats['sampling']:
            print(f"  Avg Hamming distance: {topo_stats['sampling']['avg_hamming_distance']:.1f}")

    if 'topology' in topo_stats and topo_stats['topology']:
        print(f"  Number of clusters: {topo_stats['topology'].get('num_clusters', 0)}")
        print(f"  Avg clustering coeff: {topo_stats['topology'].get('avg_clustering', 0):.2f}")

    print()

    # Example 2: Using convenience function
    print("Example 2: Using solve_ssta_sat() convenience function")
    print("-" * 70)
    formula2 = "(x | y | z) & (~x | y) & (~y | z)"
    print(f"Formula: {formula2}")

    cnf2 = CNFExpression.parse(formula2)
    result2 = solve_ssta_sat(cnf2, num_samples=15)

    if result2:
        print(f"✓ SAT: {result2}")
    else:
        print("✗ UNSAT")

    print()
    print("=" * 70)
    print("SSTA-SAT examples complete!")
    print("=" * 70)


if __name__ == "__main__":
    main()
