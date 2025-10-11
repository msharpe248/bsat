"""
Davis-Putnam (1960) Algorithm Examples

Demonstrates the original SAT solver algorithm using resolution-based
variable elimination. Shows why exponential space is problematic and
how it relates to modern techniques.

HISTORICAL CONTEXT:
Davis-Putnam (1960) was the FIRST SAT solver, preceding DPLL by 2 years.
It uses resolution to eliminate variables, which can generate exponentially
many clauses. This made it impractical for large formulas.

EDUCATIONAL PURPOSE:
These examples show:
1. How Davis-Putnam works on small instances
2. Why exponential space is a problem
3. Comparison with DPLL
4. Connection to modern techniques
"""

from bsat import (
    CNFExpression, Clause, Literal,
    solve_davis_putnam, get_davis_putnam_stats,
    solve_sat, DPLLSolver
)


def example1_basic_satisfiable():
    """Example 1: Basic SAT instance."""
    print("=" * 70)
    print("Example 1: Basic Satisfiable Formula")
    print("=" * 70)

    formula = "(x | y) & (~x | z) & (y | ~z)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")
    print(f"Variables: {len(cnf.get_variables())}, Clauses: {len(cnf.clauses)}")

    # Solve with Davis-Putnam
    result, stats = get_davis_putnam_stats(cnf)

    if result:
        print(f"\nSAT!")
        print(f"Solution: {result}")
        print(f"Verification: {cnf.evaluate(result)}")
    else:
        print("\nUNSAT")

    print(f"\nStatistics:")
    print(stats)

    # Compare with DPLL
    dpll_solver = DPLLSolver(cnf)
    dpll_result = dpll_solver.solve()
    dpll_stats = dpll_solver.get_statistics()

    print(f"\nComparison with DPLL:")
    print(f"  Davis-Putnam max clauses: {stats.max_clauses}")
    print(f"  DPLL decisions: {dpll_stats['num_decisions']}")
    print(f"  Both find same answer: {(result is not None) == (dpll_result is not None)}")
    print()


def example2_unsatisfiable():
    """Example 2: UNSAT formula."""
    print("=" * 70)
    print("Example 2: Unsatisfiable Formula")
    print("=" * 70)

    # (x) ∧ (¬x) - obviously UNSAT
    cnf = CNFExpression([
        Clause([Literal('x', False)]),
        Clause([Literal('x', True)])
    ])

    print(f"Formula: {cnf}")
    print("This is obviously unsatisfiable (x and ¬x)")

    result, stats = get_davis_putnam_stats(cnf)

    if result:
        print(f"\nSAT: {result}")
    else:
        print("\nUNSAT (as expected)")

    print(f"\nStatistics:")
    print(stats)
    print()


def example3_clause_growth():
    """Example 3: Demonstrate clause growth during resolution."""
    print("=" * 70)
    print("Example 3: Clause Growth During Resolution")
    print("=" * 70)

    # Formula where resolution creates more clauses
    formula = "(a | b) & (a | c) & (~a | d) & (~a | e)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")
    print(f"Initial clauses: {len(cnf.clauses)}")
    print()

    print("When we resolve on 'a':")
    print("  Positive clauses (contain a): (a ∨ b), (a ∨ c)")
    print("  Negative clauses (contain ¬a): (¬a ∨ d), (¬a ∨ e)")
    print()
    print("  Resolution creates 2 × 2 = 4 new clauses:")
    print("    (b ∨ d) from (a ∨ b) + (¬a ∨ d)")
    print("    (b ∨ e) from (a ∨ b) + (¬a ∨ e)")
    print("    (c ∨ d) from (a ∨ c) + (¬a ∨ d)")
    print("    (c ∨ e) from (a ∨ c) + (¬a ∨ e)")
    print()

    result, stats = get_davis_putnam_stats(cnf)

    print(f"Result: {'SAT' if result else 'UNSAT'}")
    print(f"\nActual clause growth:")
    print(f"  Started with: {stats.initial_clauses} clauses")
    print(f"  Max during solving: {stats.max_clauses} clauses")
    print(f"  Resolutions performed: {stats.resolutions_performed}")
    print()

    print("💡 This is why Davis-Putnam uses exponential space!")
    print("   n positive × m negative = n×m new clauses (Cartesian product)")
    print()


def example4_exponential_blowup():
    """Example 4: Show exponential blowup on slightly larger instance."""
    print("=" * 70)
    print("Example 4: Exponential Clause Blowup")
    print("=" * 70)

    # Carefully crafted formula that causes clause growth
    # but still solvable (10 variables)
    variables = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j']
    clauses = []

    # Create formula: each pair of variables appears together
    for i in range(len(variables) - 1):
        for j in range(i + 1, min(i + 3, len(variables))):
            clauses.append(Clause([
                Literal(variables[i], False),
                Literal(variables[j], False)
            ]))

    cnf = CNFExpression(clauses)

    print(f"Formula: {len(variables)} variables, {len(clauses)} initial clauses")
    print("(pairs of variables)")
    print()

    result, stats = get_davis_putnam_stats(cnf)

    print(f"Result: {'SAT' if result else 'UNSAT'}")
    print(f"\nClause growth:")
    print(f"  Initial clauses: {stats.initial_clauses}")
    print(f"  Maximum clauses: {stats.max_clauses}")
    print(f"  Growth factor: {stats.max_clauses / stats.initial_clauses:.2f}x")
    print(f"  Resolutions: {stats.resolutions_performed}")
    print(f"  Variables eliminated: {stats.variables_eliminated}")
    print()

    # Compare with DPLL
    dpll_solver = DPLLSolver(cnf)
    dpll_result = dpll_solver.solve()
    dpll_stats = dpll_solver.get_statistics()

    print(f"DPLL comparison:")
    print(f"  DPLL decisions: {dpll_stats['num_decisions']}")
    print(f"  DPLL never stores more than original {len(clauses)} clauses!")
    print()

    print("💡 DPLL (1962) solved the space problem using backtracking")
    print("   instead of storing all resolved clauses.")
    print()


def example5_vs_dpll_performance():
    """Example 5: Performance comparison with DPLL."""
    print("=" * 70)
    print("Example 5: Davis-Putnam vs DPLL Performance")
    print("=" * 70)

    import time

    # Small 3SAT formula
    formula = "(a | b | c) & (~a | b | ~c) & (a | ~b | c) & (~a | ~b | ~c)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")
    print(f"{len(cnf.get_variables())} variables, {len(cnf.clauses)} clauses")
    print()

    # Davis-Putnam
    start = time.time()
    dp_result, dp_stats = get_davis_putnam_stats(cnf)
    dp_time = time.time() - start

    # DPLL
    start = time.time()
    dpll_result = solve_sat(cnf)
    dpll_time = time.time() - start

    print(f"Davis-Putnam (1960):")
    print(f"  Time: {dp_time*1000:.3f} ms")
    print(f"  Max clauses: {dp_stats.max_clauses}")
    print(f"  Resolutions: {dp_stats.resolutions_performed}")
    print()

    print(f"DPLL (1962):")
    print(f"  Time: {dpll_time*1000:.3f} ms")
    print(f"  Never stores more than {len(cnf.clauses)} clauses")
    print()

    if dp_time > dpll_time:
        print(f"⚡ DPLL is {dp_time/dpll_time:.1f}x faster")
    else:
        print(f"⚡ Similar performance on this small instance")

    print()
    print("💡 For small instances (< 20 vars), both are fast")
    print("   For large instances, DPLL scales much better")
    print()


def example6_pure_literal_elimination():
    """Example 6: Show one-literal and pure literal rules."""
    print("=" * 70)
    print("Example 6: One-Literal and Pure Literal Rules")
    print("=" * 70)

    # Formula with unit clause and pure literal
    formula = "x & (x | y) & (y | z)"
    cnf = CNFExpression.parse(formula)

    print(f"Formula: {cnf}")
    print()

    print("Analysis:")
    print("  'x' appears as unit clause → one-literal rule applies")
    print("  'z' appears only positively → pure literal (affirmative-negative rule)")
    print()

    result, stats = get_davis_putnam_stats(cnf)

    print(f"Result: SAT")
    print(f"Solution: {result}")
    print()

    print(f"Statistics:")
    print(f"  One-literal eliminations: {stats.one_literal_eliminations}")
    print(f"  Pure literal eliminations: {stats.pure_literal_eliminations}")
    print(f"  Resolutions: {stats.resolutions_performed}")
    print()

    print("💡 These rules avoid resolution when possible")
    print("   Same rules used in modern DPLL/CDCL solvers!")
    print()


def example7_when_not_to_use():
    """Example 7: Demonstrate when NOT to use Davis-Putnam."""
    print("=" * 70)
    print("Example 7: When NOT to Use Davis-Putnam")
    print("=" * 70)

    # Medium-sized formula (30 variables)
    variables = [f'x{i}' for i in range(30)]
    clauses = []

    # Random 3SAT-like formula
    import random
    random.seed(42)

    for _ in range(90):  # 3:1 clause-to-variable ratio
        lits = random.sample(variables, 3)
        clause = Clause([
            Literal(lit, random.choice([True, False]))
            for lit in lits
        ])
        clauses.append(clause)

    cnf = CNFExpression(clauses)

    print(f"Formula: {len(variables)} variables, {len(clauses)} clauses")
    print("(Random 3SAT-like instance)")
    print()

    print("⚠️  WARNING: Davis-Putnam may use excessive memory!")
    print("    This instance is too large for educational purposes.")
    print()

    print("For this size, you should use:")
    print("  ✅ solve_sat() - DPLL with O(n) space")
    print("  ✅ solve_cdcl() - Modern solver with clause learning")
    print("  ❌ solve_davis_putnam() - Will use too much memory!")
    print()

    # Don't actually solve - would take too long/memory
    print("💡 Davis-Putnam (1960) is EDUCATIONAL ONLY")
    print("   Use DPLL (1962) or CDCL (1996+) for real problems")
    print()


def example8_connection_to_modern():
    """Example 8: Connection to modern techniques."""
    print("=" * 70)
    print("Example 8: Connection to Modern SAT Solving")
    print("=" * 70)

    print("Davis-Putnam (1960) lives on in modern SAT solving:")
    print()

    print("1. COMPONENT DECOMPOSITION")
    print("   When formula splits into independent parts:")
    print("   (a ∨ b) ∧ (c ∨ d) ← two separate components")
    print("   Modern solvers detect and solve independently")
    print("   (similar to Davis-Putnam's variable elimination idea)")
    print()

    print("2. CONFLICT ANALYSIS (CDCL)")
    print("   Modern CDCL solvers use resolution to analyze conflicts")
    print("   They resolve clauses to find the 'real reason' for conflict")
    print("   (resolution from Davis-Putnam, but selective)")
    print()

    print("3. MODEL COUNTING (#SAT)")
    print("   sharpSAT and other model counters use:")
    print("   - Component caching (like variable elimination)")
    print("   - Resolution for simplification")
    print("   (Davis-Putnam ideas adapted for counting)")
    print()

    print("4. PREPROCESSING")
    print("   Modern preprocessors use:")
    print("   - Bounded Variable Addition (BVA)")
    print("   - Variable elimination (when safe)")
    print("   - Resolution (selectively)")
    print("   (Controlled use of Davis-Putnam techniques)")
    print()

    # Demonstrate with component decomposition
    from bsat import decompose_into_components

    formula = "(a | b) & (c | d)"
    cnf = CNFExpression.parse(formula)

    print(f"Example: {cnf}")

    components = decompose_into_components(cnf)
    print(f"Components: {len(components)}")
    for i, comp in enumerate(components):
        print(f"  Component {i+1}: {comp}")

    print()
    print("💡 Modern SAT solving builds on Davis-Putnam's ideas")
    print("   while avoiding the exponential space problem!")
    print()


if __name__ == '__main__':
    example1_basic_satisfiable()
    example2_unsatisfiable()
    example3_clause_growth()
    example4_exponential_blowup()
    example5_vs_dpll_performance()
    example6_pure_literal_elimination()
    example7_when_not_to_use()
    example8_connection_to_modern()

    print("=" * 70)
    print("All Davis-Putnam examples completed!")
    print("=" * 70)
    print()
    print("Key Takeaways:")
    print("• Davis-Putnam (1960) was the FIRST SAT solver")
    print("• Uses resolution to eliminate variables")
    print("• Problem: Exponential space (n×m clause blowup)")
    print("• Solution: DPLL (1962) uses backtracking instead")
    print("• Legacy: Ideas live on in CDCL, #SAT, preprocessing")
    print("• Use: Educational purposes only (< 20 variables)")
    print("• Modern: Use DPLL or CDCL for real problems")
    print()
