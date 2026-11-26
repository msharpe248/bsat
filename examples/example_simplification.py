"""
Examples demonstrating formula simplification.

Modern SAT solvers spend a significant part of their time rewriting the
formula into a smaller, easier one. This file shows the four techniques in
`bsat.simplification` one at a time, and how a solution of the simplified
formula is turned back into a solution of the original.
"""

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "tests"))

from bsat import (
    CNFExpression,
    Simplifier, simplify_cnf, solve_with_simplification,
    solve_modern_cdcl,
)
from benchmarks import graph_coloring_hard, random_3sat


def example1_failed_literal_probing():
    """Example 1: Failed literal probing."""
    print("=" * 60)
    print("Example 1: Failed Literal Probing")
    print("=" * 60)

    cnf = CNFExpression.parse("(~a | b) & (~a | ~b) & (a | c | d) & (~c | e)")
    print(f"\nFormula: {cnf}")
    print("\nAssume a: then b (first clause) and ¬b (second clause). Conflict!")
    print("So ¬a is a consequence of the formula.")

    simplifier = Simplifier(cnf)
    fixed = simplifier.probe_failed_literals()
    result = simplifier.result()
    print(f"\nVariables fixed: {fixed}")
    print(f"Simplified: {result.cnf}")


def example2_blocked_clauses():
    """Example 2: Blocked clause elimination."""
    print("\n" + "=" * 60)
    print("Example 2: Blocked Clause Elimination")
    print("=" * 60)

    cnf = CNFExpression.parse("(a | b) & (~a | ~b | c) & (b | c) & (~b | ~c)")
    print(f"\nFormula: {cnf}")
    print("\n(a ∨ b) can only be resolved on a with (¬a ∨ ¬b ∨ c).")
    print("The resolvent (b ∨ ¬b ∨ c) is a tautology, so (a ∨ b) is blocked on a.")

    simplifier = Simplifier(cnf)
    removed = simplifier.eliminate_blocked_clauses()
    result = simplifier.result()
    print(f"\nClauses removed: {removed}")
    print("(Removing one blocked clause leaves fewer resolution partners, which")
    print("makes other clauses blocked. Here the whole formula unravels.)")
    print(f"Simplified: {result.cnf}")

    model = solve_modern_cdcl(result.cnf)
    full = result.extend_model(model)
    print(f"\nSolution of simplified formula: {model}")
    print(f"Extended to the original:       {full}")
    print(f"Satisfies the original: {cnf.evaluate(full)}")


def example3_variable_elimination():
    """Example 3: Bounded variable elimination."""
    print("\n" + "=" * 60)
    print("Example 3: Bounded Variable Elimination")
    print("=" * 60)

    cnf = CNFExpression.parse("(a | x) & (b | x) & (~x | c) & (~x | d)")
    print(f"\nFormula: {cnf}")
    print("\nResolving every clause with x against every clause with ¬x gives")
    print("(a ∨ c), (a ∨ d), (b ∨ c), (b ∨ d): four clauses replace four, and x is gone.")

    simplifier = Simplifier(cnf)
    eliminated = simplifier.eliminate_variables()
    result = simplifier.result()
    print(f"\nVariables eliminated in total: {eliminated} "
          f"(the others then become easy to eliminate too)")
    print(f"Simplified: {result.cnf}")

    model = solve_modern_cdcl(result.cnf)
    full = result.extend_model(model)
    print(f"\nExtended solution: {full}")
    print(f"Satisfies the original: {cnf.evaluate(full)}")


def example4_vivification():
    """Example 4: Vivification."""
    print("\n" + "=" * 60)
    print("Example 4: Vivification")
    print("=" * 60)

    cnf = CNFExpression.parse("(a | ~b) & (a | b | c | d) & (~c | e) & (~e | a)")
    print(f"\nFormula: {cnf}")
    print("\nTake (a ∨ b ∨ c ∨ d) and assume ¬a. Propagation through the other")
    print("clauses then forces ¬b (first clause), ¬e (last clause) and ¬c (third")
    print("clause). So b and c are already false: both are redundant and can be")
    print("dropped, leaving (a ∨ d).")

    simplifier = Simplifier(cnf)
    shortened = simplifier.vivify()
    result = simplifier.result()
    print(f"\nClauses shortened: {shortened}, literals removed: {result.stats.vivified_literals}")
    print(f"Simplified: {result.cnf}")


def example5_reconstruction_stack():
    """Example 5: The reconstruction stack."""
    print("\n" + "=" * 60)
    print("Example 5: Reconstructing a Solution")
    print("=" * 60)

    cnf = CNFExpression.parse(
        "(a | b) & (~a | c) & (~b | c) & (~c | d | e) & (~d | f) & (~e | f) & (~f | ~a | g)"
    )
    print(f"\nFormula: {cnf}")

    simplifier = Simplifier(cnf)
    simplifier.probe_failed_literals()
    simplifier.eliminate_variables(max_occurrences=1)
    simplifier.eliminate_blocked_clauses()
    result = simplifier.result()

    print(f"\nSimplified: {result.cnf if result.cnf.clauses else '⊤ (no clauses left)'}")
    print("\nReconstruction stack (replayed last to first):")
    for step in simplifier.reconstruction:
        kind = step[0]
        if kind == 'unit':
            lit = step[1]
            print(f"  forced      {'¬' if lit < 0 else ''}{simplifier.names[abs(lit) - 1]}")
        elif kind == 'blocked':
            print(f"  blocked     clause removed, blocked on "
                  f"{'¬' if step[1] < 0 else ''}{simplifier.names[abs(step[1]) - 1]}")
        else:
            print(f"  eliminated  {simplifier.names[step[1] - 1]} "
                  f"({len(step[2])} clauses saved)")

    model = solve_modern_cdcl(result.cnf)
    full = result.extend_model(model)
    print(f"\nSolution of simplified formula: {model}")
    print(f"Extended to the original:       {full}")
    print(f"Satisfies the original: {cnf.evaluate(full)}")


def example6_structured_problem():
    """Example 6: Effect on structured and random instances."""
    print("\n" + "=" * 60)
    print("Example 6: Structured vs Random Instances")
    print("=" * 60)

    instances = [
        ("Graph coloring (8 nodes, 4 colors)", graph_coloring_hard(8, 4, density=0.3, seed=49)),
        ("Random 3-SAT (30 vars, 100 clauses)", random_3sat(30, 100, seed=45)),
    ]
    for name, cnf in instances:
        start = time.perf_counter()
        result = simplify_cnf(cnf)
        elapsed = time.perf_counter() - start
        stats = result.stats
        print(f"\n{name}")
        print(f"  Variables: {stats.original_vars} → {stats.final_vars}")
        print(f"  Clauses:   {stats.original_clauses} → {stats.final_clauses}")
        print(f"  Failed literals {stats.failed_literals}, blocked clauses {stats.blocked_clauses}, "
              f"eliminated variables {stats.eliminated_variables}, "
              f"vivified clauses {stats.vivified_clauses}")
        print(f"  Time: {elapsed:.3f}s")

        solution = solve_with_simplification(cnf)
        if solution is None:
            print("  UNSAT")
        else:
            print(f"  SAT, solution verifies: {cnf.evaluate(solution)}")


if __name__ == '__main__':
    example1_failed_literal_probing()
    example2_blocked_clauses()
    example3_variable_elimination()
    example4_vivification()
    example5_reconstruction_stack()
    example6_structured_problem()
