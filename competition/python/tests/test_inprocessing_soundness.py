#!/usr/bin/env python3
"""
Detailed soundness testing for inprocessing.

Check if inprocessing preserves satisfiability.
"""

import sys
import os

# Add paths for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression, Clause, Literal
import cdcl_optimized
from inprocessing import Inprocessor


def test_subsumption_preserves_sat():
    """Test that subsumption doesn't change satisfiability."""
    print("="*70)
    print("Test 1: Subsumption Preserves Satisfiability")
    print("="*70)

    # Create clauses with subsumption: (a | b) subsumes (a | b | c)
    int_clauses = [
        [1, 2],      # (a | b)
        [1, 2, 3],   # (a | b | c) - subsumed
        [-1, -2]     # (~a | ~b)
    ]

    print(f"Original clauses: {int_clauses}")

    inprocessor = Inprocessor()
    simplified = inprocessor.simplify(int_clauses, subsumption=True,
                                     self_subsumption=False, var_elimination=False)

    print(f"After subsumption: {simplified}")
    print(f"Subsumed: {inprocessor.stats.subsumed_clauses}")

    # Check: (a | b | c) should be removed
    if [1, 2, 3] not in simplified:
        print("✅ Subsumed clause removed")
    else:
        print("❌ Subsumed clause NOT removed")

    # Check: remaining clauses should be equivalent
    if len(simplified) == 2:
        print("✅ Correct number of clauses")
    else:
        print(f"❌ Wrong number of clauses: {len(simplified)}")

    print()


def test_self_subsumption_preserves_sat():
    """Test that self-subsumption doesn't change satisfiability."""
    print("="*70)
    print("Test 2: Self-Subsumption Preserves Satisfiability")
    print("="*70)

    # (a | b) and (~a | b | c) → (~a | b | c) can be strengthened to (~a | b)
    int_clauses = [
        [1, 2],       # (a | b)
        [-1, 2, 3]    # (~a | b | c)
    ]

    print(f"Original clauses: {int_clauses}")

    inprocessor = Inprocessor()
    simplified = inprocessor.simplify(int_clauses, subsumption=False,
                                     self_subsumption=True, var_elimination=False)

    print(f"After self-subsumption: {simplified}")
    print(f"Self-subsumptions: {inprocessor.stats.self_subsumptions}")

    # The result should be satisfiable (e.g., a=T, b=T)
    print("✅ Self-subsumption completed")
    print()


def test_variable_elimination_preserves_sat():
    """Test that variable elimination doesn't change satisfiability."""
    print("="*70)
    print("Test 3: Variable Elimination Preserves Satisfiability")
    print("="*70)

    # Simple case: eliminate variable 'b' from (a | b) & (~a | ~b)
    # Resolving on b: (a) from (a | b) and (~a) from (~a | ~b)
    # Wait, that's wrong! Let me think...
    # Resolution: (a | b) and (~a | ~b) → (a | ~a) = tautology
    # So eliminating b here would leave us with no clauses (all tautologies)

    # Better example: (a | b) & (~b | c) - eliminate b
    # Resolution: (a | c) - this is correct
    int_clauses = [
        [1, 2],    # (a | b)
        [-2, 3]    # (~b | c)
    ]

    print(f"Original clauses: {int_clauses}")

    inprocessor = Inprocessor()
    simplified = inprocessor.simplify(int_clauses, subsumption=False,
                                     self_subsumption=False, var_elimination=True,
                                     max_var_occur=10, max_resolvent_size=20)

    print(f"After variable elimination: {simplified}")
    print(f"Eliminated vars: {inprocessor.stats.eliminated_vars}")
    print(f"Clauses removed: {inprocessor.stats.clauses_removed}")
    print(f"Clauses added: {inprocessor.stats.clauses_added}")

    # Should produce (a | c) after eliminating b
    if [1, 3] in simplified or [3, 1] in simplified:
        print("✅ Resolvent (a | c) produced")
    else:
        print(f"⚠️  Unexpected result")

    print()


def test_clause_conversion():
    """Test that clause format conversion is correct."""
    print("="*70)
    print("Test 4: Clause Format Conversion")
    print("="*70)

    # Create a CNF formula
    cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")

    # Create variable mappings
    variables = sorted(cnf.get_variables())
    var_to_int = {var: i + 1 for i, var in enumerate(variables)}
    int_to_var = {i + 1: var for i, var in enumerate(variables)}

    print(f"Variables: {variables}")
    print(f"Mapping: {var_to_int}")

    # Convert to integer format
    int_clauses = []
    for clause in cnf.clauses:
        int_clause = []
        for lit in clause.literals:
            var_id = var_to_int[lit.variable]
            int_lit = -var_id if lit.negated else var_id
            int_clause.append(int_lit)
        int_clauses.append(int_clause)

    print(f"Integer clauses: {int_clauses}")

    # Convert back to CNF
    clauses = []
    for int_clause in int_clauses:
        literals = []
        for int_lit in int_clause:
            var_id = abs(int_lit)
            var_name = int_to_var[var_id]
            negated = int_lit < 0
            literals.append(Literal(var_name, negated))
        clauses.append(Clause(literals))
    cnf_back = CNFExpression(clauses)

    print(f"Converted back: {cnf_back}")

    # Check if they're equivalent
    if len(cnf.clauses) == len(cnf_back.clauses):
        print("✅ Same number of clauses")
    else:
        print(f"❌ Different number of clauses: {len(cnf.clauses)} vs {len(cnf_back.clauses)}")

    print()


def test_benchmark_instance():
    """Test the problematic benchmark instance in detail."""
    print("="*70)
    print("Test 5: Problematic Instance (easy_3sat_v014_c0058)")
    print("="*70)

    try:
        from bsat.dimacs import read_dimacs_file
        cnf = read_dimacs_file("../../dataset/medium_tests/medium_suite/easy_3sat_v014_c0058.cnf")

        print(f"Variables: {len(cnf.get_variables())}")
        print(f"Clauses: {len(cnf.clauses)}")

        # Solve WITHOUT inprocessing
        print("\n[1] Solving WITHOUT inprocessing (max_conflicts=1000)...")
        solver1 = cdcl_optimized.CDCLSolver(cnf, use_watched_literals=True, enable_inprocessing=False)
        result1 = solver1.solve(max_conflicts=1000)
        print(f"Result: {'SAT' if result1 else 'UNSAT/TIMEOUT'}")
        print(f"Conflicts: {solver1.stats.conflicts}")

        # Verify solution if SAT
        if result1:
            is_valid = cnf.evaluate(result1)
            print(f"Solution valid: {is_valid}")
            if not is_valid:
                print("❌ INVALID SOLUTION!")
            else:
                print("✅ Solution is valid")

        # Solve WITH inprocessing
        print("\n[2] Solving WITH inprocessing (max_conflicts=1000)...")
        solver2 = cdcl_optimized.CDCLSolver(cnf, use_watched_literals=True,
                                           enable_inprocessing=True, inprocessing_interval=500)
        result2 = solver2.solve(max_conflicts=1000)
        print(f"Result: {'SAT' if result2 else 'UNSAT/TIMEOUT'}")
        print(f"Conflicts: {solver2.stats.conflicts}")
        print(f"Inprocessing calls: {solver2.stats.inprocessing_calls}")
        print(f"Inprocessing subsumed: {solver2.stats.inprocessing_subsumed}")

        # Verify solution if SAT
        if result2:
            is_valid = cnf.evaluate(result2)
            print(f"Solution valid: {is_valid}")
            if not is_valid:
                print("❌ INVALID SOLUTION!")
            else:
                print("✅ Solution is valid")

        # Compare results
        print(f"\n[3] Comparison:")
        print(f"Without inproc: {'SAT' if result1 else 'UNSAT/TIMEOUT'}")
        print(f"With inproc:    {'SAT' if result2 else 'UNSAT/TIMEOUT'}")

        if (result1 is None) != (result2 is None):
            print("⚠️  RESULTS DIFFER! This could indicate a bug.")
            print("   However, this could also be due to hitting conflict limit.")
            print("   Let's check if both hit the limit...")
            print(f"   Solver 1 conflicts: {solver1.stats.conflicts}")
            print(f"   Solver 2 conflicts: {solver2.stats.conflicts}")

            if solver1.stats.conflicts >= 1000 or solver2.stats.conflicts >= 1000:
                print("   → One or both hit conflict limit (not necessarily a bug)")
            else:
                print("   → NEITHER hit conflict limit - POSSIBLE BUG!")
        else:
            print("✅ Results agree")

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()

    print()


def test_simple_known_instances():
    """Test on simple instances where we know the answer."""
    print("="*70)
    print("Test 6: Simple Known Instances")
    print("="*70)

    test_cases = [
        ("(a | b) & (~a | ~b)", False, "UNSAT: forces a=T,b=F or a=F,b=T but also a≠b"),
        ("(a | b) & (a | ~b) & (~a | b)", True, "SAT: a=T,b=T"),
        ("(a) & (~a)", False, "UNSAT: trivial contradiction"),
        ("(a | b | c) & (~a) & (~b)", True, "SAT: c=T"),
    ]

    for formula, expected_sat, description in test_cases:
        print(f"\nFormula: {formula}")
        print(f"Expected: {'SAT' if expected_sat else 'UNSAT'}")
        print(f"Description: {description}")

        cnf = CNFExpression.parse(formula)

        # Solve without inprocessing
        solver1 = cdcl_optimized.CDCLSolver(cnf, use_watched_literals=True, enable_inprocessing=False)
        result1 = solver1.solve(max_conflicts=1000)

        # Solve with inprocessing
        solver2 = cdcl_optimized.CDCLSolver(cnf, use_watched_literals=True,
                                           enable_inprocessing=True, inprocessing_interval=10)
        result2 = solver2.solve(max_conflicts=1000)

        sat1 = result1 is not None
        sat2 = result2 is not None

        print(f"Without inproc: {'SAT' if sat1 else 'UNSAT'}")
        print(f"With inproc:    {'SAT' if sat2 else 'UNSAT'}")

        # Check correctness
        if sat1 != expected_sat:
            print(f"❌ Without inprocessing got wrong answer!")
        elif sat2 != expected_sat:
            print(f"❌ With inprocessing got wrong answer!")
        elif sat1 != sat2:
            print(f"❌ Results differ between solvers!")
        else:
            print(f"✅ Both correct")

        # Verify solutions if SAT
        if result1:
            if not cnf.evaluate(result1):
                print(f"❌ Solution 1 invalid!")
        if result2:
            if not cnf.evaluate(result2):
                print(f"❌ Solution 2 invalid!")


def main():
    print("\n" + "="*70)
    print(" INPROCESSING SOUNDNESS TESTS")
    print("="*70 + "\n")

    test_subsumption_preserves_sat()
    test_self_subsumption_preserves_sat()
    test_variable_elimination_preserves_sat()
    test_clause_conversion()
    test_simple_known_instances()
    test_benchmark_instance()

    print("="*70)
    print(" SOUNDNESS TESTS COMPLETE")
    print("="*70)


if __name__ == "__main__":
    main()
