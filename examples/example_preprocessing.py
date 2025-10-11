"""
Examples demonstrating SAT preprocessing and simplification techniques.

Preprocessing can dramatically reduce problem complexity before solving:
- Connected component decomposition: Split independent subproblems
- Unit propagation: Propagate forced assignments
- Pure literal elimination: Remove variables with single polarity
- Subsumption: Remove redundant clauses
- Self-subsumption: Simplify clauses

These techniques can reduce solve time from hours to seconds!
"""

from bsat import CNFExpression, solve_sat
from bsat.preprocessing import (
    decompose_into_components,
    preprocess_cnf,
    decompose_and_preprocess,
    SATPreprocessor
)


def example1_independent_components():
    """Example 1: Decompose independent subproblems."""
    print("=" * 70)
    print("Example 1: Connected Component Decomposition")
    print("=" * 70)

    # Two completely independent subproblems
    formula = "(a | b) & (c | d)"
    cnf = CNFExpression.parse(formula)

    print(f"Original formula: {cnf}")
    print(f"Variables: {cnf.get_variables()}")
    print(f"Clauses: {len(cnf.clauses)}")

    # Decompose into components
    components = decompose_into_components(cnf)

    print(f"\n✓ Found {len(components)} independent components:")
    for i, comp in enumerate(components, 1):
        print(f"  Component {i}: {comp}")
        print(f"    Variables: {comp.get_variables()}")

    print("\n💡 Key insight: These can be solved independently and in parallel!")
    print()


def example2_unit_propagation():
    """Example 2: Unit propagation simplification."""
    print("=" * 70)
    print("Example 2: Unit Propagation")
    print("=" * 70)

    # Formula with unit clause
    formula = "a & (a | b | c) & (~a | d)"
    cnf = CNFExpression.parse(formula)

    print(f"Original formula: {cnf}")
    print(f"Clauses: {len(cnf.clauses)}")

    # Preprocess
    result = preprocess_cnf(cnf)

    print(f"\nSimplified formula: {result.simplified}")
    print(f"Clauses after: {len(result.simplified.clauses)}")
    print(f"\nForced assignments: {result.assignments}")
    print(f"  a = {result.assignments['a']} (unit clause)")
    print(f"  d = {result.assignments.get('d', 'unassigned')} (propagated from ~a | d)")

    print(f"\nStatistics:")
    print(f"  Unit propagations: {result.stats.unit_propagations}")
    print(f"  Clauses removed: {result.stats.original_clauses - result.stats.final_clauses}")
    print()


def example3_pure_literals():
    """Example 3: Pure literal elimination."""
    print("=" * 70)
    print("Example 3: Pure Literal Elimination")
    print("=" * 70)

    # Variable 'a' appears only positive
    formula = "(a | b) & (a | c) & (~b | ~c)"
    cnf = CNFExpression.parse(formula)

    print(f"Original formula: {cnf}")

    # Check polarities
    print(f"\nVariable polarities:")
    print(f"  a: only positive (pure!)")
    print(f"  b: both polarities")
    print(f"  c: both polarities")

    result = preprocess_cnf(cnf)

    print(f"\nSimplified formula: {result.simplified}")
    print(f"Forced assignments: {result.assignments}")
    print(f"\n💡 Pure literal 'a' set to True, satisfying first two clauses!")
    print()


def example4_subsumption():
    """Example 4: Clause subsumption."""
    print("=" * 70)
    print("Example 4: Clause Subsumption")
    print("=" * 70)

    # (a) subsumes (a | b) and (a | b | c)
    from bsat import Clause, Literal

    cnf = CNFExpression([
        Clause([Literal('a', False)]),  # a
        Clause([Literal('a', False), Literal('b', False)]),  # a | b
        Clause([Literal('a', False), Literal('b', False), Literal('c', False)])  # a | b | c
    ])

    print(f"Original formula: {cnf}")
    print(f"Clauses: {len(cnf.clauses)}")

    print(f"\nSubsumption analysis:")
    print(f"  (a) ⊆ (a ∨ b) → (a) subsumes (a ∨ b)")
    print(f"  (a) ⊆ (a ∨ b ∨ c) → (a) subsumes (a ∨ b ∨ c)")

    result = preprocess_cnf(cnf)

    print(f"\nSimplified: {result.simplified}")
    print(f"Clauses after: {len(result.simplified.clauses)}")
    print(f"Subsumed clauses: {result.stats.subsumed_clauses}")
    print()


def example5_combined_preprocessing():
    """Example 5: All techniques combined."""
    print("=" * 70)
    print("Example 5: Combined Preprocessing")
    print("=" * 70)

    # Complex formula benefiting from multiple techniques
    formula = "a & (a | b | c) & (~a | d) & (d | e) & (f | g) & (f | h)"
    cnf = CNFExpression.parse(formula)

    print(f"Original formula: {cnf}")
    print(f"Variables: {len(cnf.get_variables())}")
    print(f"Clauses: {len(cnf.clauses)}")

    result = preprocess_cnf(cnf)

    print(f"\nSimplified formula: {result.simplified}")
    print(f"Variables after: {len(result.simplified.get_variables())}")
    print(f"Clauses after: {len(result.simplified.clauses)}")

    print(f"\nForced assignments: {result.assignments}")

    print(f"\nSimplification steps:")
    print(f"  1. Unit propagation: a=True")
    print(f"  2. This satisfies (a | b | c), removes it")
    print(f"  3. Simplifies (~a | d) to (d), making d=True")
    print(f"  4. This satisfies (d | e), removes it")
    print(f"  5. Pure literal f=True satisfies remaining clauses")

    print(f"\n{result.stats}")
    print()


def example6_decompose_and_preprocess():
    """Example 6: Decompose then preprocess - best workflow."""
    print("=" * 70)
    print("Example 6: Decompose + Preprocess (Recommended Workflow)")
    print("=" * 70)

    # Mix of independent components with simplification opportunities
    formula = "(a | b) & a & (c | d) & c & (e | f)"
    cnf = CNFExpression.parse(formula)

    print(f"Original formula: {cnf}")
    print(f"Variables: {len(cnf.get_variables())}")
    print(f"Clauses: {len(cnf.clauses)}")

    # Decompose and preprocess in one step
    components, assignments, stats = decompose_and_preprocess(cnf)

    print(f"\nResults:")
    print(f"  Components: {stats.components}")
    print(f"  Forced assignments: {assignments}")
    print(f"  Remaining components: {len(components)}")

    if components:
        print(f"\nComponents to solve:")
        for i, comp in enumerate(components, 1):
            print(f"  {i}. {comp} (vars: {comp.get_variables()})")
    else:
        print(f"\n✓ Completely solved by preprocessing!")

    print(f"\n{stats}")
    print()


def example7_solving_with_preprocessing():
    """Example 7: Solve using preprocessing first."""
    print("=" * 70)
    print("Example 7: Preprocessing Before Solving")
    print("=" * 70)

    # Large formula that benefits from preprocessing
    from bsat import Clause, Literal

    clauses = []
    # Build chain: x1 & (~x1 | x2) & (~x2 | x3) & ... & (~x4 | x5)
    clauses.append(Clause([Literal('x1', False)]))
    for i in range(1, 5):
        clauses.append(Clause([
            Literal(f'x{i}', True),
            Literal(f'x{i+1}', False)
        ]))
    # Add some more complex clauses
    clauses.append(Clause([Literal('x5', False), Literal('y', False)]))
    clauses.append(Clause([Literal('y', True), Literal('z', False)]))

    cnf = CNFExpression(clauses)

    print(f"Original formula:")
    print(f"  Variables: {len(cnf.get_variables())}")
    print(f"  Clauses: {len(cnf.clauses)}")

    # Preprocess first
    result = preprocess_cnf(cnf)

    print(f"\nAfter preprocessing:")
    print(f"  Variables: {len(result.simplified.get_variables())}")
    print(f"  Clauses: {len(result.simplified.clauses)}")
    print(f"  Forced: {result.assignments}")

    # Now solve the simplified problem
    if result.is_sat == True:
        print(f"\n✓ Trivially SAT from preprocessing alone!")
        solution = result.assignments
    elif result.is_sat == False:
        print(f"\n✗ Trivially UNSAT from preprocessing alone!")
        solution = None
    else:
        print(f"\nSolving simplified formula...")
        solution = solve_sat(result.simplified)
        if solution:
            # Merge with forced assignments
            solution.update(result.assignments)

    if solution:
        print(f"\nFinal solution: {solution}")
        print(f"Verification: {cnf.evaluate(solution)}")

    print()


def example8_comparison():
    """Example 8: Compare solve time with/without preprocessing."""
    print("=" * 70)
    print("Example 8: Performance Comparison")
    print("=" * 70)

    import time
    from bsat import Clause, Literal

    # Build medium-sized formula with structure
    clauses = []

    # Add unit clause chain
    clauses.append(Clause([Literal('a', False)]))
    for i in range(ord('a'), ord('e')):
        var1 = chr(i)
        var2 = chr(i + 1)
        clauses.append(Clause([Literal(var1, True), Literal(var2, False)]))

    # Add some redundant clauses (will be subsumed)
    clauses.append(Clause([Literal('a', False), Literal('b', False)]))
    clauses.append(Clause([Literal('a', False), Literal('c', False)]))

    # Add independent component
    clauses.append(Clause([Literal('x', False), Literal('y', False)]))
    clauses.append(Clause([Literal('x', True), Literal('z', False)]))

    cnf = CNFExpression(clauses)

    print(f"Formula: {len(cnf.clauses)} clauses, {len(cnf.get_variables())} variables")

    # Solve without preprocessing
    start = time.time()
    sol1 = solve_sat(cnf)
    time_no_preproc = time.time() - start

    # Solve with preprocessing
    start = time.time()
    result = preprocess_cnf(cnf)
    sol2 = solve_sat(result.simplified) if result.is_sat is None else result.assignments
    if sol2:
        sol2.update(result.assignments)
    time_with_preproc = time.time() - start

    print(f"\nWithout preprocessing:")
    print(f"  Time: {time_no_preproc*1000:.2f}ms")
    print(f"  Clauses solved: {len(cnf.clauses)}")

    print(f"\nWith preprocessing:")
    print(f"  Time: {time_with_preproc*1000:.2f}ms")
    print(f"  Clauses after preprocessing: {len(result.simplified.clauses)}")
    print(f"  Reduction: {((len(cnf.clauses) - len(result.simplified.clauses)) / len(cnf.clauses) * 100):.1f}%")

    if time_no_preproc > 0:
        speedup = time_no_preproc / time_with_preproc if time_with_preproc > 0 else float('inf')
        print(f"  Speedup: {speedup:.1f}x")

    print()


def example9_real_world_pattern():
    """Example 9: Real-world preprocessing pattern."""
    print("=" * 70)
    print("Example 9: Real-World Workflow")
    print("=" * 70)

    # Simulate a real problem
    formula = "(a | b | c) & (a | d) & (~a | e | f) & (g | h) & g & (i | j) & (~g | k)"
    cnf = CNFExpression.parse(formula)

    print(f"Problem: {cnf}")
    print(f"Size: {len(cnf.clauses)} clauses, {len(cnf.get_variables())} variables")

    print(f"\nStep 1: Decompose into independent components")
    components = decompose_into_components(cnf)
    print(f"  Found {len(components)} component(s)")

    print(f"\nStep 2: Preprocess each component")
    all_solutions = []
    all_assignments = {}

    for i, comp in enumerate(components, 1):
        print(f"\n  Component {i}: {comp}")
        result = preprocess_cnf(comp)

        print(f"    After preprocessing: {result.simplified}")
        print(f"    Forced: {result.assignments}")

        if result.is_sat == True:
            print(f"    Status: ✓ Trivially SAT")
            all_assignments.update(result.assignments)
        elif result.is_sat == False:
            print(f"    Status: ✗ UNSAT - entire formula is UNSAT!")
            break
        else:
            print(f"    Status: Needs solving")
            sol = solve_sat(result.simplified)
            if sol:
                sol.update(result.assignments)
                all_assignments.update(sol)
                all_solutions.append(sol)
            else:
                print(f"    Status: ✗ UNSAT - entire formula is UNSAT!")
                break

    if all_solutions or all_assignments:
        print(f"\n✓ Final solution: {all_assignments}")
        print(f"  Verification: {cnf.evaluate(all_assignments)}")

    print()


if __name__ == '__main__':
    example1_independent_components()
    example2_unit_propagation()
    example3_pure_literals()
    example4_subsumption()
    example5_combined_preprocessing()
    example6_decompose_and_preprocess()
    example7_solving_with_preprocessing()
    example8_comparison()
    example9_real_world_pattern()

    print("=" * 70)
    print("All examples completed!")
    print("=" * 70)
    print("\nKey Takeaways:")
    print("• Decompose first: Split independent subproblems")
    print("• Preprocess next: Simplify before solving")
    print("• Unit propagation: Handle forced assignments")
    print("• Pure literals: Eliminate variables with single polarity")
    print("• Subsumption: Remove redundant clauses")
    print("• Can reduce problem size by 50-90% in practice!")
    print("• Preprocessing time << solving time for large problems")
    print("\nBest Practice: Always decompose_and_preprocess() before solving!")
