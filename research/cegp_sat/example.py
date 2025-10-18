#!/usr/bin/env python3
"""
Example usage of CEGP-SAT solver.

Demonstrates Clause Evolution with Genetic Programming for SAT solving.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from cegp_sat import CEGPSATSolver, solve_cegp_sat


def main():
    print("=" * 70)
    print("CEGP-SAT: Clause Evolution with Genetic Programming Example")
    print("Educational/Experimental: Evolves learned clauses using GP")
    print("=" * 70)
    print()

    # Example 1: Simple SAT with clause evolution
    print("Example 1: Simple SAT with clause evolution")
    print("-" * 70)
    formula1 = "(a | b | c) & (~a | b) & (~b | c) & (a | ~c) & (b | ~a | d)"
    print(f"Formula: {formula1}")
    print()

    cnf1 = CNFExpression.parse(formula1)
    solver1 = CEGPSATSolver(
        cnf1,
        use_evolution=True,
        evolution_frequency=100  # Evolve every 100 conflicts
    )

    result1 = solver1.solve(max_conflicts=10000)

    if result1:
        print(f"✓ SAT: {result1}")
    else:
        print("✗ UNSAT")

    print()
    print("Statistics:")
    stats1 = solver1.get_stats()
    print(f"  Decisions: {stats1.decisions}")
    print(f"  Conflicts: {stats1.conflicts}")
    print(f"  Evolutions: {stats1.evolutions}")
    print(f"  Evolved clauses: {stats1.evolved_clauses}")
    print(f"  Crossovers: {stats1.crossovers}")
    print(f"  Mutations: {stats1.mutations}")

    # Get detailed evolution statistics
    evo_stats = solver1.get_evolution_statistics()
    print()
    print("Evolution Analysis:")
    print(f"  Enabled: {evo_stats['enabled']}")
    if evo_stats['enabled'] and 'fitness' in evo_stats:
        fitness = evo_stats['fitness']
        print(f"  Total clauses tracked: {fitness['total_clauses']}")
        print(f"  Avg propagations: {fitness['avg_propagations']:.2f}")
        print(f"  Avg conflict participation: {fitness['avg_conflicts']:.2f}")

    print()

    # Example 2: With vs. without evolution
    print("Example 2: Comparing with/without clause evolution")
    print("-" * 70)
    formula2 = "(x | y | z) & (~x | y) & (~y | z) & (x | ~z) & (y | ~w) & (w | ~x)"
    print(f"Formula: {formula2}")
    print()

    cnf2 = CNFExpression.parse(formula2)

    # Without evolution (standard CDCL)
    print("Without evolution (standard CDCL):")
    solver_no_evo = CEGPSATSolver(cnf2, use_evolution=False)
    result_no = solver_no_evo.solve()
    stats_no = solver_no_evo.get_stats()
    print(f"  Result: {'SAT' if result_no else 'UNSAT'}")
    print(f"  Decisions: {stats_no.decisions}")
    print(f"  Conflicts: {stats_no.conflicts}")

    print()

    # With evolution (CEGP-SAT)
    print("With evolution (CEGP-SAT):")
    solver_with_evo = CEGPSATSolver(
        cnf2,
        use_evolution=True,
        evolution_frequency=50
    )
    result_with = solver_with_evo.solve()
    stats_with = solver_with_evo.get_stats()
    print(f"  Result: {'SAT' if result_with else 'UNSAT'}")
    print(f"  Decisions: {stats_with.decisions}")
    print(f"  Conflicts: {stats_with.conflicts}")
    print(f"  Evolutions: {stats_with.evolutions}")
    print(f"  Evolved clauses: {stats_with.evolved_clauses}")

    print()

    # Example 3: Using convenience function
    print("Example 3: Using solve_cegp_sat() convenience function")
    print("-" * 70)
    formula3 = "(a | b) & (~a | c) & (~b | ~c) & (c | d)"
    print(f"Formula: {formula3}")
    print()

    cnf3 = CNFExpression.parse(formula3)
    result3 = solve_cegp_sat(cnf3, use_evolution=True, evolution_frequency=100)

    if result3:
        print(f"✓ SAT: {result3}")
    else:
        print("✗ UNSAT")

    print()

    # Example 4: More complex instance with aggressive evolution
    print("Example 4: Complex instance with aggressive evolution")
    print("-" * 70)
    formula4 = """
    (a | b | c) & (~a | d) & (~b | e) & (~c | f) &
    (d | e | f) & (~d | g) & (~e | h) & (~f | i) &
    (g | h) & (~g | ~h) & (i | j) & (~i | k) &
    (j | ~k) & (~j | k)
    """
    print(f"Formula: {formula4.strip()}")
    print()

    cnf4 = CNFExpression.parse(formula4)
    solver4 = CEGPSATSolver(
        cnf4,
        use_evolution=True,
        evolution_frequency=50,   # Evolve more frequently
        population_size=30,        # Larger population
        crossover_rate=0.8,        # Higher crossover
        mutation_rate=0.15         # Higher mutation
    )

    result4 = solver4.solve(max_conflicts=100000)

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
    print(f"  Evolutions: {solver4.stats.evolutions}")
    print(f"  Evolved clauses: {solver4.stats.evolved_clauses}")
    print(f"  Crossovers: {solver4.stats.crossovers}")
    print(f"  Mutations: {solver4.stats.mutations}")

    # Detailed evolution analysis
    evo4 = solver4.get_evolution_statistics()
    if evo4['enabled'] and 'fitness' in evo4:
        print()
        print("Detailed Evolution Analysis:")
        fitness4 = evo4['fitness']
        print(f"  Clauses tracked: {fitness4['total_clauses']}")
        print(f"  Avg propagations/clause: {fitness4['avg_propagations']:.2f}")
        print(f"  Avg conflict participation: {fitness4['avg_conflicts']:.2f}")

        if fitness4['avg_propagations'] > 1.0:
            print()
            print("  ✓ Evolved clauses are propagating effectively!")
        if fitness4['avg_conflicts'] > 0.5:
            print("  ✓ Evolved clauses participating in conflict analysis!")

    print()
    print("=" * 70)
    print("CEGP-SAT examples complete!")
    print("=" * 70)
    print()
    print("Key Insight: CEGP-SAT evolves learned clauses using genetic")
    print("programming operators (crossover, mutation) to discover high-quality")
    print("clauses that propagate effectively. Fitness is based on propagation")
    print("count and conflict participation.")
    print()
    print("Educational/Experimental: This demonstrates genetic programming")
    print("applied to SAT solving. Effectiveness varies by problem structure.")
    print("Traditional CDCL is generally more reliable for production use.")


if __name__ == "__main__":
    main()
