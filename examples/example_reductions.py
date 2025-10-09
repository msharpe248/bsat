"""
Examples demonstrating k-SAT to 3-SAT reduction.

The reduction allows solving larger clauses by converting them to 3-SAT formulas
using auxiliary variables while preserving satisfiability.
"""

from bsat import (
    CNFExpression, Clause, Literal,
    reduce_to_3sat, extract_original_solution, solve_with_reduction,
    is_3sat, get_max_clause_size, solve_sat
)


def example1_basic_4sat_reduction():
    """Example 1: Reduce a simple 4-SAT clause to 3-SAT."""
    print("=" * 60)
    print("Example 1: Basic 4-SAT to 3-SAT Reduction")
    print("=" * 60)

    # Original 4-SAT formula: (a ∨ b ∨ c ∨ d)
    cnf = CNFExpression([
        Clause([
            Literal('a', False),
            Literal('b', False),
            Literal('c', False),
            Literal('d', False)
        ])
    ])

    print(f"\nOriginal formula: {cnf}")
    print(f"Is 3-SAT? {is_3sat(cnf)}")
    print(f"Max clause size: {get_max_clause_size(cnf)}")

    # Reduce to 3-SAT
    reduced, aux_map, stats = reduce_to_3sat(cnf)

    print(f"\nReduced formula: {reduced}")
    print(f"Is 3-SAT? {is_3sat(reduced)}")
    print(f"Max clause size: {get_max_clause_size(reduced)}")
    print(f"\nAuxiliary variables: {list(aux_map.keys())}")
    print(f"\n{stats}")


def example2_5sat_reduction():
    """Example 2: Reduce a 5-SAT clause."""
    print("\n" + "=" * 60)
    print("Example 2: 5-SAT to 3-SAT Reduction")
    print("=" * 60)

    # Original 5-SAT formula: (a ∨ b ∨ c ∨ d ∨ e)
    cnf = CNFExpression([
        Clause([
            Literal('a', False),
            Literal('b', False),
            Literal('c', False),
            Literal('d', False),
            Literal('e', False)
        ])
    ])

    print(f"\nOriginal formula: {cnf}")

    reduced, aux_map, stats = reduce_to_3sat(cnf)

    print(f"\nReduced formula: {reduced}")
    print(f"Number of clauses: {len(reduced.clauses)}")
    print(f"Auxiliary variables created: {stats.auxiliary_variables}")

    # Show the individual reduced clauses
    print("\nReduced clauses:")
    for i, clause in enumerate(reduced.clauses):
        print(f"  {i+1}. {clause}")


def example3_mixed_formula():
    """Example 3: Reduce a formula with mixed clause sizes."""
    print("\n" + "=" * 60)
    print("Example 3: Mixed Formula Reduction")
    print("=" * 60)

    # Formula with 2-SAT, 3-SAT, and 5-SAT clauses
    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False)]),  # 2-SAT (unchanged)
        Clause([Literal('a', False), Literal('b', False), Literal('c', False)]),  # 3-SAT (unchanged)
        Clause([  # 5-SAT (reduced)
            Literal('p', False),
            Literal('q', False),
            Literal('r', False),
            Literal('s', False),
            Literal('t', False)
        ])
    ])

    print(f"\nOriginal formula: {cnf}")
    print(f"Original clauses: {len(cnf.clauses)}")

    reduced, aux_map, stats = reduce_to_3sat(cnf)

    print(f"\nReduced formula: {reduced}")
    print(f"Reduced clauses: {len(reduced.clauses)}")
    print(f"Clauses expanded: {stats.clauses_expanded}")
    print(f"Auxiliary variables: {stats.auxiliary_variables}")


def example4_solve_with_reduction():
    """Example 4: Solve a 4-SAT formula using reduction."""
    print("\n" + "=" * 60)
    print("Example 4: Solving with Reduction")
    print("=" * 60)

    # 4-SAT formula
    cnf = CNFExpression([
        Clause([
            Literal('a', False),
            Literal('b', False),
            Literal('c', False),
            Literal('d', False)
        ])
    ])

    print(f"\nOriginal formula: {cnf}")

    # Solve using reduction
    solution, stats = solve_with_reduction(cnf)

    if solution:
        print(f"\nSolution found: {solution}")
        print(f"Only original variables (no auxiliary): {list(solution.keys())}")
        print(f"\nVerifying solution satisfies original formula: {cnf.evaluate(solution)}")
    else:
        print("\nNo solution found (UNSAT)")

    print(f"\n{stats}")


def example5_equivalence_preservation():
    """Example 5: Show that reduction preserves satisfiability."""
    print("\n" + "=" * 60)
    print("Example 5: Equivalence Preservation")
    print("=" * 60)

    # Create a SAT formula
    sat_formula = CNFExpression([
        Clause([Literal('a', False), Literal('b', False), Literal('c', False), Literal('d', False)])
    ])

    # Create an UNSAT formula
    unsat_formula = CNFExpression([
        Clause([Literal('a', False), Literal('b', False), Literal('c', False), Literal('d', False)]),
        Clause([Literal('a', True)]),
        Clause([Literal('b', True)]),
        Clause([Literal('c', True)]),
        Clause([Literal('d', True)])
    ])

    print("\nTesting SAT formula:")
    print(f"Original: {sat_formula}")
    reduced_sat, aux_map_sat, _ = reduce_to_3sat(sat_formula)
    print(f"Reduced: {reduced_sat}")

    orig_sol = solve_sat(sat_formula)
    red_sol = solve_sat(reduced_sat)
    extracted_sol = extract_original_solution(red_sol, aux_map_sat)

    print(f"\nOriginal solvable: {orig_sol is not None}")
    print(f"Reduced solvable: {red_sol is not None}")
    print(f"Extracted solution: {extracted_sol}")
    if extracted_sol:
        print(f"Solution satisfies original: {sat_formula.evaluate(extracted_sol)}")

    print("\n" + "-" * 60)
    print("\nTesting UNSAT formula:")
    print(f"Original: {unsat_formula}")
    reduced_unsat, _, _ = reduce_to_3sat(unsat_formula)

    orig_sol = solve_sat(unsat_formula)
    red_sol = solve_sat(reduced_unsat)

    print(f"\nOriginal solvable: {orig_sol is not None}")
    print(f"Reduced solvable: {red_sol is not None}")


def example6_large_clause():
    """Example 6: Reduce a very large clause."""
    print("\n" + "=" * 60)
    print("Example 6: Large Clause Reduction")
    print("=" * 60)

    # Create a 10-literal clause
    literals = [Literal(f'x{i}', False) for i in range(10)]
    cnf = CNFExpression([Clause(literals)])

    print(f"\nOriginal clause size: {len(literals)}")
    print(f"Formula: {cnf}")

    reduced, aux_map, stats = reduce_to_3sat(cnf)

    print(f"\nReduced to {len(reduced.clauses)} clauses")
    print(f"Auxiliary variables created: {stats.auxiliary_variables}")
    print(f"All clauses are 3-SAT: {all(len(c.literals) == 3 for c in reduced.clauses)}")

    # Formula to calculate auxiliary variables needed
    k = len(literals)
    expected_aux = k - 3
    print(f"\nExpected auxiliary variables (k-3): {expected_aux}")
    print(f"Actual auxiliary variables: {stats.auxiliary_variables}")


def example7_negations():
    """Example 7: Reduction preserves negations correctly."""
    print("\n" + "=" * 60)
    print("Example 7: Preserving Negations")
    print("=" * 60)

    # Formula with mixed positive and negative literals
    cnf = CNFExpression([
        Clause([
            Literal('a', True),   # ¬a
            Literal('b', False),  # b
            Literal('c', True),   # ¬c
            Literal('d', False)   # d
        ])
    ])

    print(f"\nOriginal formula: {cnf}")
    print("Original clause literals:")
    for lit in cnf.clauses[0].literals:
        print(f"  {lit} (negated={lit.negated})")

    reduced, aux_map, stats = reduce_to_3sat(cnf)

    print(f"\nReduced formula: {reduced}")
    print("\nReduced clauses and their literals:")
    for i, clause in enumerate(reduced.clauses):
        print(f"Clause {i+1}: {clause}")
        for lit in clause.literals:
            print(f"  {lit} (negated={lit.negated})")


def example8_custom_prefix():
    """Example 8: Use custom prefix for auxiliary variables."""
    print("\n" + "=" * 60)
    print("Example 8: Custom Auxiliary Variable Prefix")
    print("=" * 60)

    cnf = CNFExpression([
        Clause([Literal('a', False), Literal('b', False), Literal('c', False), Literal('d', False)])
    ])

    # Default prefix
    reduced_default, aux_map_default, _ = reduce_to_3sat(cnf)
    print(f"\nDefault prefix auxiliary variables: {list(aux_map_default.keys())}")

    # Custom prefix
    reduced_custom, aux_map_custom, _ = reduce_to_3sat(cnf, var_prefix="helper")
    print(f"Custom prefix auxiliary variables: {list(aux_map_custom.keys())}")


def example9_statistics():
    """Example 9: Detailed reduction statistics."""
    print("\n" + "=" * 60)
    print("Example 9: Reduction Statistics")
    print("=" * 60)

    # Create a complex formula
    cnf = CNFExpression([
        Clause([Literal('a', False), Literal('b', False)]),  # 2-SAT
        Clause([Literal('x', False), Literal('y', False), Literal('z', False)]),  # 3-SAT
        Clause([  # 5-SAT
            Literal('p', False),
            Literal('q', False),
            Literal('r', False),
            Literal('s', False),
            Literal('t', False)
        ]),
        Clause([  # 6-SAT
            Literal('u', False),
            Literal('v', False),
            Literal('w', False),
            Literal('m', False),
            Literal('n', False),
            Literal('o', False)
        ])
    ])

    print(f"\nOriginal formula: {cnf}")

    reduced, aux_map, stats = reduce_to_3sat(cnf)

    print(f"\nReduced formula: {reduced}")
    print(f"\nDetailed statistics:")
    print(f"  Original clauses: {stats.original_clauses}")
    print(f"  Original variables: {stats.original_variables}")
    print(f"  Original max clause size: {stats.max_clause_size_original}")
    print(f"  Reduced clauses: {stats.reduced_clauses}")
    print(f"  Reduced variables: {stats.reduced_variables}")
    print(f"  Reduced max clause size: {stats.max_clause_size_reduced}")
    print(f"  Clauses expanded: {stats.clauses_expanded}")
    print(f"  Auxiliary variables added: {stats.auxiliary_variables}")


if __name__ == '__main__':
    example1_basic_4sat_reduction()
    example2_5sat_reduction()
    example3_mixed_formula()
    example4_solve_with_reduction()
    example5_equivalence_preservation()
    example6_large_clause()
    example7_negations()
    example8_custom_prefix()
    example9_statistics()

    print("\n" + "=" * 60)
    print("All k-SAT reduction examples completed!")
    print("=" * 60)
