#!/usr/bin/env python3
"""Example usage of the bsat package."""

from bsat import CNFExpression, Clause, Literal


def main():
    print("=" * 60)
    print("Boolean Satisfiability (SAT) Package - Examples")
    print("=" * 60)

    # Example 1: Creating expressions programmatically
    print("\n1. Creating CNF expressions programmatically:")
    print("-" * 60)

    # Create (x ∨ y) ∧ (¬x ∨ z)
    clause1 = Clause([Literal("x"), Literal("y")])
    clause2 = Clause([Literal("x", negated=True), Literal("z")])
    expr1 = CNFExpression([clause1, clause2])

    print(f"Expression 1: {expr1}")
    print(f"Variables: {expr1.get_variables()}")

    # Example 2: Parsing expressions from strings
    print("\n2. Parsing CNF expressions from strings:")
    print("-" * 60)

    expr2 = CNFExpression.parse("(a ∨ b) ∧ (¬a ∨ c)")
    print(f"Parsed expression: {expr2}")

    expr3 = CNFExpression.parse("(p | q) & (~p | r)")
    print(f"Alternative notation: {expr3}")

    expr4 = CNFExpression.parse("(x OR y) AND (NOT x OR z)")
    print(f"Text notation: {expr4}")

    # Example 3: Truth tables
    print("\n3. Generating and printing truth tables:")
    print("-" * 60)

    simple_expr = CNFExpression.parse("(x ∨ y) ∧ ¬x")
    print(f"Expression: {simple_expr}")
    print("\nTruth table:")
    simple_expr.print_truth_table()

    # Example 4: Checking logical equivalence
    print("\n4. Checking logical equivalence using truth tables:")
    print("-" * 60)

    expr_a = CNFExpression.parse("(x ∨ y)")
    expr_b = CNFExpression.parse("(y ∨ x)")
    print(f"Expression A: {expr_a}")
    print(f"Expression B: {expr_b}")
    print(f"Are they equivalent? {expr_a.is_equivalent(expr_b)}")

    expr_c = CNFExpression.parse("(x ∨ y)")
    expr_d = CNFExpression.parse("(x ∨ z)")
    print(f"\nExpression C: {expr_c}")
    print(f"Expression D: {expr_d}")
    print(f"Are they equivalent? {expr_c.is_equivalent(expr_d)}")

    # Example 5: JSON serialization/deserialization
    print("\n5. JSON serialization and deserialization:")
    print("-" * 60)

    original = CNFExpression.parse("(a ∨ ¬b) ∧ (b ∨ c)")
    print(f"Original expression: {original}")

    json_str = original.to_json()
    print(f"\nJSON representation:\n{json_str}")

    restored = CNFExpression.from_json(json_str)
    print(f"\nRestored expression: {restored}")
    print(f"Are they equivalent? {original.is_equivalent(restored)}")

    # Example 6: Complex expression with truth table
    print("\n6. Complex expression example:")
    print("-" * 60)

    complex_expr = CNFExpression.parse("(p ∨ q ∨ r) ∧ (¬p ∨ ¬q) ∧ (¬p ∨ ¬r)")
    print(f"Expression: {complex_expr}")
    print("\nTruth table:")
    complex_expr.print_truth_table()

    # Example 7: Evaluating with specific assignments
    print("\n7. Evaluating with specific variable assignments:")
    print("-" * 60)

    expr = CNFExpression.parse("(x ∨ y) ∧ (¬x ∨ z)")
    print(f"Expression: {expr}")

    assignment1 = {"x": True, "y": False, "z": True}
    print(f"\nAssignment {assignment1}: {expr.evaluate(assignment1)}")

    assignment2 = {"x": True, "y": False, "z": False}
    print(f"Assignment {assignment2}: {expr.evaluate(assignment2)}")

    assignment3 = {"x": False, "y": True, "z": False}
    print(f"Assignment {assignment3}: {expr.evaluate(assignment3)}")

    print("\n" + "=" * 60)
    print("Examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
