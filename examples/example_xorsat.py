"""
XOR-SAT Solver Examples

This module demonstrates the XOR-SAT solver which uses Gaussian elimination
over GF(2) to solve formulas where clauses are XORs of literals.

XOR-SAT can be solved in polynomial time O(n³), unlike general 3-SAT which is NP-complete.
"""

from bsat import CNFExpression, Clause, Literal, solve_xorsat, get_xorsat_stats


def print_header(title):
    """Print a formatted section header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_result(formula, result):
    """Print the formula and its solution."""
    print(f"\nFormula: {formula}")
    if result:
        print(f"✓ SAT")
        print(f"Solution: {result}")
        # Verify
        is_valid = all(
            sum(1 for lit in clause.literals
                if (result[lit.variable] and not lit.negated) or
                   (not result[lit.variable] and lit.negated)) % 2 == 1
            for clause in formula.clauses
        )
        print(f"Verification: {'✓ Valid' if is_valid else '✗ Invalid'}")
    else:
        print("✗ UNSAT")


def example_basic():
    """Basic XOR-SAT examples."""
    print_header("Example 1: Basic XOR Equations")

    # x ⊕ y = 1 (x and y must differ)
    print("\n1. Simple XOR: x ⊕ y = 1")
    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False)])
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)

    # x ⊕ y ⊕ z = 1 (odd parity)
    print("\n2. Three-way XOR: x ⊕ y ⊕ z = 1")
    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False), Literal('z', False)])
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)


def example_with_negations():
    """XOR formulas with negated literals."""
    print_header("Example 2: XOR with Negated Literals")

    # ¬x ⊕ y = 1
    # This means: (1 ⊕ x) ⊕ y = 1, which simplifies to x ⊕ y = 0
    # So x and y must be equal
    print("\n1. XOR with negation: ¬x ⊕ y = 1")
    cnf = CNFExpression([
        Clause([Literal('x', True), Literal('y', False)])
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)
    if result:
        print(f"Note: x={result['x']}, y={result['y']} → (not x) XOR y = {(not result['x']) ^ result['y']}")

    # x ⊕ ¬y ⊕ z = 1
    print("\n2. Mixed negations: x ⊕ ¬y ⊕ z = 1")
    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', True), Literal('z', False)])
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)


def example_system_of_equations():
    """System of XOR equations."""
    print_header("Example 3: System of XOR Equations")

    # System:
    # x ⊕ y = 1
    # y ⊕ z = 1
    # This implies: x ⊕ z = 0 (x and z must be equal)
    print("\nSystem of equations:")
    print("  x ⊕ y = 1")
    print("  y ⊕ z = 1")

    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False)]),
        Clause([Literal('y', False), Literal('z', False)])
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)

    if result:
        print(f"\nDerived: x ⊕ z = {result['x'] ^ result['z']} (should be 0, i.e., False)")


def example_unsat():
    """Unsatisfiable XOR-SAT formula."""
    print_header("Example 4: Unsatisfiable System")

    # System that leads to contradiction:
    # x ⊕ y = 1
    # y ⊕ z = 1
    # x ⊕ z = 1
    # From first two: x ⊕ z = 0, but third says x ⊕ z = 1 → Contradiction!
    print("\nContradictory system:")
    print("  x ⊕ y = 1")
    print("  y ⊕ z = 1")
    print("  x ⊕ z = 1  ← Contradiction!")
    print("\nNote: (x ⊕ y) ⊕ (y ⊕ z) = x ⊕ z = 0, but we're requiring x ⊕ z = 1")

    cnf = CNFExpression([
        Clause([Literal('x', False), Literal('y', False)]),
        Clause([Literal('y', False), Literal('z', False)]),
        Clause([Literal('x', False), Literal('z', False)])
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)


def example_parity_check():
    """Parity check code example."""
    print_header("Example 5: Error Detection with Parity Bits")

    # Simple parity check code
    # We have 3 data bits (d₁, d₂, d₃) and 1 parity bit (p)
    # Parity equation: p ⊕ d₁ ⊕ d₂ ⊕ d₃ = 0 (even parity)

    print("\nParity Check Code:")
    print("  Data bits: d₁=1, d₂=0, d₃=0")
    print("  Parity equation: p ⊕ d₁ ⊕ d₂ ⊕ d₃ = 0")
    print("  What should the parity bit p be?")

    # We need p ⊕ d₁ ⊕ d₂ ⊕ d₃ = 0
    # For even parity (sum = 0), we need ¬(p ⊕ d₁ ⊕ d₂ ⊕ d₃) = 1
    # Which is: ¬p ⊕ ¬d₁ ⊕ ¬d₂ ⊕ ¬d₃ = 1 (in XOR-SAT)
    cnf = CNFExpression([
        # p ⊕ d₁ ⊕ d₂ ⊕ d₃ = 0 is encoded as ¬p ⊕ ¬d₁ ⊕ ¬d₂ ⊕ ¬d₃ = 1
        Clause([
            Literal('p', True),
            Literal('d1', True),
            Literal('d2', True),
            Literal('d3', True)
        ]),
        Clause([Literal('d1', False)]),  # d₁ = 1
        Clause([Literal('d2', True)]),   # d₂ = 0 (¬d₂ = 1)
        Clause([Literal('d3', True)])    # d₃ = 0 (¬d₃ = 1)
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)

    if result:
        parity = result['p'] ^ result['d1'] ^ result['d2'] ^ result['d3']
        print(f"\nComputed parity: p ⊕ d₁ ⊕ d₂ ⊕ d₃ = {parity} (even parity check)")


def example_secret_sharing():
    """Secret sharing scheme using XOR."""
    print_header("Example 6: XOR-based Secret Sharing")

    # Simple (3,3) secret sharing: secret is split into 3 shares
    # All 3 shares are needed to reconstruct the secret
    # s = share₁ ⊕ share₂ ⊕ share₃

    print("\nXOR Secret Sharing:")
    print("  Secret s is split into shares: s = s₁ ⊕ s₂ ⊕ s₃")
    print("  Given: s₁=1, s₂=1, what is the secret if s₃=0?")

    # s ⊕ s₁ ⊕ s₂ ⊕ s₃ = 0 (definition)
    # We know s₁=1, s₂=1, s₃=0
    cnf = CNFExpression([
        # s ⊕ s₁ ⊕ s₂ ⊕ s₃ = 0 → ¬s ⊕ ¬s₁ ⊕ ¬s₂ ⊕ ¬s₃ = 1
        Clause([
            Literal('s', True),
            Literal('s1', True),
            Literal('s2', True),
            Literal('s3', True)
        ]),
        Clause([Literal('s1', False)]),  # s₁ = 1
        Clause([Literal('s2', False)]),  # s₂ = 1
        Clause([Literal('s3', True)])    # s₃ = 0
    ])
    result = solve_xorsat(cnf)
    print_result(cnf, result)

    if result:
        reconstructed = result['s1'] ^ result['s2'] ^ result['s3']
        print(f"\nReconstructed secret: s₁ ⊕ s₂ ⊕ s₃ = {reconstructed}")
        print(f"Matches stored secret s = {result['s']}: {reconstructed == result['s']}")


def example_statistics():
    """Show solver statistics."""
    print_header("Example 7: Solver Statistics")

    cnf = CNFExpression([
        Clause([Literal('x1', False), Literal('x2', False)]),
        Clause([Literal('x2', False), Literal('x3', False)]),
        Clause([Literal('x3', False), Literal('x4', False)]),
        Clause([Literal('x4', False), Literal('x5', False)])
    ])

    print(f"\nFormula: {cnf}")
    stats_result = get_xorsat_stats(cnf)

    print(f"\nSatisfiable: {stats_result['satisfiable']}")
    print(f"Solution: {stats_result['solution']}")
    print("\nStatistics:")
    for key, value in stats_result['stats'].items():
        print(f"  {key}: {value}")


def example_linear_system():
    """Larger system of linear equations over GF(2)."""
    print_header("Example 8: Larger Linear System")

    # 5 variables, 4 equations
    print("\nLinear system over GF(2):")
    print("  x₁ ⊕ x₂ ⊕ x₃ = 1")
    print("  x₂ ⊕ x₄ = 1")
    print("  x₃ ⊕ x₄ ⊕ x₅ = 1")
    print("  x₁ ⊕ x₅ = 1")

    cnf = CNFExpression([
        Clause([Literal('x1', False), Literal('x2', False), Literal('x3', False)]),
        Clause([Literal('x2', False), Literal('x4', False)]),
        Clause([Literal('x3', False), Literal('x4', False), Literal('x5', False)]),
        Clause([Literal('x1', False), Literal('x5', False)])
    ])

    result = solve_xorsat(cnf)
    print_result(cnf, result)

    if result:
        print("\nVerifying each equation:")
        print(f"  x₁ ⊕ x₂ ⊕ x₃ = {result['x1'] ^ result['x2'] ^ result['x3']}")
        print(f"  x₂ ⊕ x₄ = {result['x2'] ^ result['x4']}")
        print(f"  x₃ ⊕ x₄ ⊕ x₅ = {result['x3'] ^ result['x4'] ^ result['x5']}")
        print(f"  x₁ ⊕ x₅ = {result['x1'] ^ result['x5']}")


def main():
    """Run all examples."""
    print("""
╔══════════════════════════════════════════════════════════════════════╗
║                  XOR-SAT Solver Examples                             ║
║                                                                      ║
║  XOR-SAT solves formulas where each clause is an exclusive-or (⊕)  ║
║  of literals. Unlike general 3-SAT, XOR-SAT can be solved in       ║
║  polynomial time O(n³) using Gaussian elimination over GF(2).      ║
║                                                                      ║
║  Applications:                                                       ║
║  • Error detection/correction codes (parity checks)                 ║
║  • Cryptography (stream ciphers, hash functions)                    ║
║  • Secret sharing schemes                                            ║
║  • Linear algebra over finite fields                                 ║
╚══════════════════════════════════════════════════════════════════════╝
    """)

    example_basic()
    example_with_negations()
    example_system_of_equations()
    example_unsat()
    example_parity_check()
    example_secret_sharing()
    example_statistics()
    example_linear_system()

    print("\n" + "=" * 70)
    print("  All examples completed!")
    print("=" * 70 + "\n")


if __name__ == '__main__':
    main()
