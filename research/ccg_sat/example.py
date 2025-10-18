#!/usr/bin/env python3
"""
Example usage of CCG-SAT solver.

Demonstrates Conflict Causality Graph analysis for intelligent restart decisions.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from ccg_sat import CCGSATSolver, solve_ccg_sat


def main():
    print("=" * 70)
    print("CCG-SAT: Conflict Causality Graph SAT Solver Example")
    print("Partially novel: Uses causality analysis ONLINE during solving")
    print("Related work: CausalSAT (Yang 2023) - post-hoc explanation")
    print("=" * 70)
    print()

    # Example 1: Simple SAT with causality tracking
    print("Example 1: Simple SAT with causality tracking")
    print("-" * 70)
    formula1 = "(a | b | c) & (~a | b) & (~b | c) & (a | ~c) & (b | ~a | d)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = CCGSATSolver(cnf1, use_causality=True, old_age_threshold=5000)

    result1 = solver1.solve()

    if result1:
        print(f"✓ SAT: {result1}")
    else:
        print("✗ UNSAT")

    print()
    print("Statistics:")
    print(f"  Decisions: {solver1.stats.decisions}")
    print(f"  Conflicts: {solver1.stats.conflicts}")
    print(f"  Causality restarts: {solver1.stats.causality_restarts}")
    print(f"  Root causes detected: {solver1.stats.root_causes_detected}")
    print(f"  Causality nodes: {solver1.stats.causality_nodes}")

    # Get detailed causality statistics
    ccg_stats = solver1.get_causality_statistics()
    print()
    print("Causality Analysis:")
    print(f"  Enabled: {ccg_stats['enabled']}")
    if ccg_stats['enabled'] and 'graph' in ccg_stats:
        graph = ccg_stats['graph']
        print(f"  Total nodes: {graph['total_nodes']}")
        print(f"  Conflict nodes: {graph['conflict_nodes']}")
        print(f"  Learned nodes: {graph['learned_nodes']}")
        print(f"  Total edges: {graph['total_edges']}")
        print(f"  Avg out-degree: {graph['avg_out_degree']:.2f}")
        print(f"  Max out-degree: {graph['max_out_degree']}")

    if ccg_stats['enabled'] and 'root_cause_analysis' in ccg_stats:
        rca = ccg_stats['root_cause_analysis']
        print()
        print("Root Cause Analysis:")
        print(f"  Root causes found: {rca['root_causes_found']}")
        print(f"  Top root cause degree: {rca['top_root_cause_degree']}")

    print()

    # Example 2: With vs. without causality
    print("Example 2: Comparing with/without causality analysis")
    print("-" * 70)
    formula2 = "(x | y | z) & (~x | y) & (~y | z) & (x | ~z) & (y | ~w) & (w | ~x)"
    print(f"Formula: {formula2}")
    print()

    cnf2 = CNFExpression.parse(formula2)

    # Without causality (standard CDCL)
    print("Without causality (standard CDCL):")
    solver_no_ccg = CCGSATSolver(cnf2, use_causality=False)
    result_no = solver_no_ccg.solve()
    stats_no = solver_no_ccg.get_stats()
    print(f"  Result: {'SAT' if result_no else 'UNSAT'}")
    print(f"  Decisions: {stats_no.decisions}")
    print(f"  Conflicts: {stats_no.conflicts}")
    print(f"  Restarts: {stats_no.restarts}")

    print()

    # With causality (CCG-SAT)
    print("With causality (CCG-SAT):")
    solver_with_ccg = CCGSATSolver(cnf2, use_causality=True, old_age_threshold=5000)
    result_with = solver_with_ccg.solve()
    stats_with = solver_with_ccg.get_stats()
    print(f"  Result: {'SAT' if result_with else 'UNSAT'}")
    print(f"  Decisions: {stats_with.decisions}")
    print(f"  Conflicts: {stats_with.conflicts}")
    print(f"  Restarts: {stats_with.restarts}")
    print(f"  Causality restarts: {stats_with.causality_restarts}")
    print(f"  Root causes detected: {stats_with.root_causes_detected}")

    print()

    # Example 3: Using convenience function
    print("Example 3: Using solve_ccg_sat() convenience function")
    print("-" * 70)
    formula3 = "(a | b) & (~a | c) & (~b | ~c) & (c | d)"
    print(f"Formula: {formula3}")
    print()

    cnf3 = CNFExpression.parse(formula3)
    result3 = solve_ccg_sat(cnf3, use_causality=True, old_age_threshold=5000)

    if result3:
        print(f"✓ SAT: {result3}")
    else:
        print("✗ UNSAT")

    print()

    # Example 4: Harder instance with more causality tracking
    print("Example 4: More complex instance with causality tracking")
    print("-" * 70)
    formula4 = """
    (a | b | c) & (~a | d) & (~b | e) & (~c | f) &
    (d | e | f) & (~d | g) & (~e | h) & (~f | i) &
    (g | h) & (~g | ~h) & (i | j) & (~i | k) &
    (j | ~k) & (~j | k) & (a | ~d | g)
    """
    print(f"Formula: {formula4.strip()}")
    print()

    cnf4 = CNFExpression.parse(formula4)
    solver4 = CCGSATSolver(
        cnf4,
        use_causality=True,
        old_age_threshold=3000,  # Lower threshold for earlier restart
        max_causality_nodes=5000
    )

    result4 = solver4.solve(max_conflicts=10000)

    if result4:
        print(f"✓ SAT: {result4}")
    else:
        print("✗ UNSAT or timeout")

    print()
    print("Extended Statistics:")
    print(f"  Decisions: {solver4.stats.decisions}")
    print(f"  Conflicts: {solver4.stats.conflicts}")
    print(f"  Propagations: {solver4.stats.propagations}")
    print(f"  Restarts: {solver4.stats.restarts}")
    print(f"  Causality restarts: {solver4.stats.causality_restarts}")
    print(f"  Root causes detected: {solver4.stats.root_causes_detected}")

    # Detailed causality graph analysis
    ccg4 = solver4.get_causality_statistics()
    if ccg4['enabled'] and 'graph' in ccg4:
        graph4 = ccg4['graph']
        print()
        print("Detailed Causality Graph:")
        print(f"  Total causal nodes: {graph4['total_nodes']}")
        print(f"  Conflicts tracked: {graph4['conflict_nodes']}")
        print(f"  Learned clauses tracked: {graph4['learned_nodes']}")
        print(f"  Causal edges: {graph4['total_edges']}")
        print(f"  Average causal out-degree: {graph4['avg_out_degree']:.2f}")
        print(f"  Max causal out-degree: {graph4['max_out_degree']}")

        if graph4['avg_out_degree'] > 2.0:
            print()
            print("  ⚠️  High avg out-degree detected!")
            print("      Many cascading conflicts → root causes driving search")

    print()
    print("=" * 70)
    print("CCG-SAT examples complete!")
    print("=" * 70)
    print()
    print("Key Insight: CCG-SAT tracks causal relationships between conflicts")
    print("and learned clauses, using root cause analysis (high out-degree)")
    print("to detect when search is stuck in bad region (old root causes).")
    print()
    print("Partially Novel: Uses causality ONLINE during solving to guide")
    print("restarts, unlike CausalSAT (Yang 2023) which does post-hoc")
    print("explanation after solving completes.")


if __name__ == "__main__":
    main()
