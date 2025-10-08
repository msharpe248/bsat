#!/usr/bin/env python3
"""Examples of using the Horn-SAT solver."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from bsat import CNFExpression, Clause, Literal, HornSATSolver, solve_horn_sat, is_horn_formula


def example_basic_usage():
    """Basic usage of the Horn-SAT solver."""
    print("=" * 60)
    print("Example 1: Basic Horn-SAT Solving")
    print("=" * 60)

    # Simple implication chain: x → y → z
    # In Horn-SAT: (x) ∧ (¬x ∨ y) ∧ (¬y ∨ z)
    expr = CNFExpression.parse("x & (~x | y) & (~y | z)")
    print(f"Formula: {expr}")
    print(f"Meaning: x → y → z (implication chain)\n")

    # Check if it's a Horn formula
    print(f"Is Horn formula: {is_horn_formula(expr)}")

    # Solve
    solution = solve_horn_sat(expr)
    if solution:
        print(f"\n✓ SAT - Found satisfying assignment:")
        for var in sorted(solution.keys()):
            print(f"  {var} = {solution[var]}")
        print(f"\nVerification: {expr.evaluate(solution)}")
    else:
        print("\n✗ UNSAT - No solution exists")

    print()


def example_logic_programming():
    """Example using Horn-SAT for logic programming."""
    print("=" * 60)
    print("Example 2: Logic Programming (Prolog-style)")
    print("=" * 60)

    print("Logic program:")
    print("  likes_pizza(john).")
    print("  likes_italian(X) :- likes_pizza(X).")
    print("  happy(X) :- likes_italian(X).")
    print("\nQuestion: Is john happy?\n")

    # Convert to Horn clauses
    cnf = CNFExpression([
        Clause([Literal('likes_pizza_john')]),
        Clause([Literal('likes_pizza_john', True), Literal('likes_italian_john')]),
        Clause([Literal('likes_italian_john', True), Literal('happy_john')])
    ])

    print(f"As Horn-SAT formula: {cnf}\n")

    solver = HornSATSolver(cnf)
    solution = solver.solve()

    if solution:
        print("✓ Answer: YES, john is happy!")
        print(f"\nDerivation:")
        print(f"  likes_pizza(john) = {solution['likes_pizza_john']} (fact)")
        print(f"  likes_italian(john) = {solution['likes_italian_john']} (inferred)")
        print(f"  happy(john) = {solution['happy_john']} (inferred)")

        stats = solver.get_statistics()
        print(f"\nUnit propagations: {stats['num_unit_propagations']}")
    else:
        print("✗ Answer: Cannot determine")

    print()


def example_expert_system():
    """Example: Simple expert system using Horn rules."""
    print("=" * 60)
    print("Example 3: Expert System (Diagnostic Rules)")
    print("=" * 60)

    print("Medical diagnosis rules:")
    print("  has_fever(patient).")
    print("  has_cough(patient).")
    print("  has_flu(X) :- has_fever(X), has_cough(X).")
    print("  needs_rest(X) :- has_flu(X).")
    print("\nQuestion: Does patient need rest?\n")

    # Convert to Horn-SAT:
    # Facts: has_fever, has_cough
    # Rule: has_flu ← (has_fever ∧ has_cough)
    #       In CNF: (¬has_fever ∨ ¬has_cough ∨ has_flu)
    # Rule: needs_rest ← has_flu
    #       In CNF: (¬has_flu ∨ needs_rest)

    cnf = CNFExpression([
        Clause([Literal('has_fever')]),
        Clause([Literal('has_cough')]),
        Clause([Literal('has_fever', True), Literal('has_cough', True), Literal('has_flu')]),
        Clause([Literal('has_flu', True), Literal('needs_rest')])
    ])

    print(f"As Horn-SAT formula: {cnf}\n")

    solution = solve_horn_sat(cnf)

    if solution:
        print("✓ Diagnosis:")
        print(f"  Has fever: {solution['has_fever']}")
        print(f"  Has cough: {solution['has_cough']}")
        print(f"  Has flu: {solution['has_flu']}")
        print(f"  Needs rest: {solution['needs_rest']}")
        print("\nRecommendation: Patient needs rest!")
    else:
        print("✗ Cannot make diagnosis")

    print()


def example_unsatisfiable():
    """Example of an unsatisfiable Horn formula."""
    print("=" * 60)
    print("Example 4: Unsatisfiable Horn Formula")
    print("=" * 60)

    # Contradictory facts: (x) ∧ (¬x)
    expr = CNFExpression.parse("x & ~x")
    print(f"Formula: {expr}")
    print("Meaning: x is both true and false (contradiction)\n")

    solution = solve_horn_sat(expr)

    if solution:
        print(f"✓ SAT: {solution}")
    else:
        print("✗ UNSAT - Formula is unsatisfiable (as expected)")
        print("\nWhy? Cannot assign x to be both true and false.")

    print()


def example_pure_negative():
    """Example with only negative clauses."""
    print("=" * 60)
    print("Example 5: Pure Negative Clauses")
    print("=" * 60)

    # All negative clauses: (¬x ∨ ¬y) ∧ (¬y ∨ ¬z) ∧ (¬x ∨ ¬z)
    # Meaning: At most one variable can be true
    expr = CNFExpression.parse("(~x | ~y) & (~y | ~z) & (~x | ~z)")
    print(f"Formula: {expr}")
    print("Meaning: At most one of x, y, z can be true\n")

    solution = solve_horn_sat(expr)

    if solution:
        print(f"✓ SAT - Found satisfying assignment:")
        for var in sorted(solution.keys()):
            print(f"  {var} = {solution[var]}")
        print("\nNote: All variables are False (satisfies all constraints)")
        print(f"Verification: {expr.evaluate(solution)}")
    else:
        print("✗ UNSAT")

    print()


def example_type_inference():
    """Example: Type inference system."""
    print("=" * 60)
    print("Example 6: Type Inference System")
    print("=" * 60)

    print("Type inference rules:")
    print("  var_x_is_int.")
    print("  expr_has_int :- var_x_is_int.")
    print("  expr_type_checked :- expr_has_int.")
    print("\nQuestion: Does expression type-check?\n")

    cnf = CNFExpression([
        Clause([Literal('var_x_is_int')]),
        Clause([Literal('var_x_is_int', True), Literal('expr_has_int')]),
        Clause([Literal('expr_has_int', True), Literal('expr_type_checked')])
    ])

    print(f"As Horn-SAT formula: {cnf}\n")

    solver = HornSATSolver(cnf)
    solution = solver.solve()

    if solution and solution['expr_type_checked']:
        print("✓ Type checking result: PASS")
        print(f"\nType derivation:")
        print(f"  var_x_is_int = {solution['var_x_is_int']} (given)")
        print(f"  expr_has_int = {solution['expr_has_int']} (inferred)")
        print(f"  expr_type_checked = {solution['expr_type_checked']} (inferred)")

        stats = solver.get_statistics()
        print(f"\nInference steps: {stats['num_unit_propagations']}")
    else:
        print("✗ Type checking result: FAIL")

    print()


def example_non_horn():
    """Example showing what is NOT a Horn formula."""
    print("=" * 60)
    print("Example 7: Non-Horn Formula Detection")
    print("=" * 60)

    # Formula with 2 positive literals: (x ∨ y) - NOT Horn
    expr1 = CNFExpression.parse("(x | y)")
    print(f"Formula 1: {expr1}")
    print(f"Is Horn: {is_horn_formula(expr1)}")
    print("Reason: Has 2 positive literals in a clause\n")

    # Valid Horn formula: (x ∨ ¬y) - IS Horn
    expr2 = CNFExpression.parse("(x | ~y)")
    print(f"Formula 2: {expr2}")
    print(f"Is Horn: {is_horn_formula(expr2)}")
    print("Reason: Has at most 1 positive literal per clause\n")

    # Try to solve non-Horn with Horn-SAT solver
    try:
        solver = HornSATSolver(expr1)
        print("This shouldn't print!")
    except ValueError as e:
        print(f"✓ Correctly rejected non-Horn formula:")
        print(f"  Error: {e}")

    print()


def example_performance():
    """Example showing linear-time performance."""
    print("=" * 60)
    print("Example 8: Linear-Time Performance")
    print("=" * 60)

    # Create a long implication chain: a → b → c → ... → z
    # This would be expensive for DPLL but is O(n) for Horn-SAT

    print("Creating implication chain: a → b → c → d → e → f\n")

    clauses = []
    variables = ['a', 'b', 'c', 'd', 'e', 'f']

    # Starting fact: a
    clauses.append(Clause([Literal('a')]))

    # Implication chain
    for i in range(len(variables) - 1):
        # var[i] → var[i+1]  becomes  (¬var[i] ∨ var[i+1])
        clauses.append(Clause([
            Literal(variables[i], negated=True),
            Literal(variables[i + 1], negated=False)
        ]))

    cnf = CNFExpression(clauses)
    print(f"Formula: {cnf}\n")

    solver = HornSATSolver(cnf)
    solution = solver.solve()

    if solution:
        print("✓ SAT - All variables inferred:")
        for var in variables:
            print(f"  {var} = {solution[var]}")

        stats = solver.get_statistics()
        print(f"\nPerformance statistics:")
        print(f"  Variables: {stats['num_variables']}")
        print(f"  Clauses: {stats['num_clauses']}")
        print(f"  Unit propagations: {stats['num_unit_propagations']}")
        print(f"  Time complexity: O(n+m) - Linear!")
    else:
        print("✗ UNSAT")

    print()


def example_database_query():
    """Example: Database query using Datalog-style rules."""
    print("=" * 60)
    print("Example 9: Database Query (Datalog)")
    print("=" * 60)

    print("Database facts and rules:")
    print("  parent(alice, bob).")
    print("  parent(bob, charlie).")
    print("  grandparent(X,Z) :- parent(X,Y), parent(Y,Z).")
    print("\nQuery: Is alice a grandparent of charlie?\n")

    cnf = CNFExpression([
        # Facts
        Clause([Literal('parent_alice_bob')]),
        Clause([Literal('parent_bob_charlie')]),
        # Rule: grandparent(alice, charlie) ← parent(alice, bob) ∧ parent(bob, charlie)
        Clause([
            Literal('parent_alice_bob', True),
            Literal('parent_bob_charlie', True),
            Literal('grandparent_alice_charlie')
        ])
    ])

    print(f"As Horn-SAT formula: {cnf}\n")

    solution = solve_horn_sat(cnf)

    if solution and solution['grandparent_alice_charlie']:
        print("✓ Query result: YES")
        print(f"\nFacts and inferences:")
        print(f"  parent(alice, bob) = {solution['parent_alice_bob']} (fact)")
        print(f"  parent(bob, charlie) = {solution['parent_bob_charlie']} (fact)")
        print(f"  grandparent(alice, charlie) = {solution['grandparent_alice_charlie']} (inferred)")
    else:
        print("✗ Query result: NO")

    print()


def main():
    """Run all examples."""
    print("\n" + "=" * 60)
    print("Horn-SAT Solver - Practical Examples")
    print("=" * 60)
    print()

    example_basic_usage()
    example_logic_programming()
    example_expert_system()
    example_unsatisfiable()
    example_pure_negative()
    example_type_inference()
    example_non_horn()
    example_performance()
    example_database_query()

    print("=" * 60)
    print("Examples completed!")
    print("=" * 60)
    print("\nKey Takeaways:")
    print("  • Horn-SAT is polynomial time: O(n+m)")
    print("  • Used in logic programming (Prolog, Datalog)")
    print("  • Great for rule-based reasoning and expert systems")
    print("  • At most 1 positive literal per clause")
    print("  • Solved via unit propagation starting from all-false")
    print()


if __name__ == "__main__":
    main()
