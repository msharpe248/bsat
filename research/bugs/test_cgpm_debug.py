#!/usr/bin/env python3
"""
Debug script for CGPM-SAT - test the fix.
"""

import sys
import os
import signal

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from cgpm_sat import CGPMSolver

def timeout_handler(signum, frame):
    raise TimeoutError("Solver timed out!")

signal.signal(signal.SIGALRM, timeout_handler)

def test_case(name, formula_str):
    """Test a single formula with timeout."""
    print(f"\nTesting: {name}")
    print(f"Formula: {formula_str}")

    try:
        cnf = CNFExpression.parse(formula_str)
        print(f"Variables: {len(cnf.get_variables())}, Clauses: {len(cnf.clauses)}")

        solver = CGPMSolver(cnf, graph_weight=0.5)

        # Set 5 second alarm
        signal.alarm(5)

        result = solver.solve()

        # Cancel alarm
        signal.alarm(0)

        print(f"✅ Result: {'SAT' if result else 'UNSAT'}")
        if result:
            print(f"   Solution: {result}")

        stats = solver.get_statistics()
        print(f"   Decisions: {stats['decisions_made']}")
        print(f"   Graph influence: {stats['graph_influence_rate']:.0f}%")

    except TimeoutError:
        signal.alarm(0)
        print(f"❌ TIMEOUT - Solver hung!")
        return False
    except Exception as e:
        signal.alarm(0)
        print(f"❌ ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False

    return True

if __name__ == "__main__":
    print("=" * 70)
    print("CGPM-SAT Debug Tests")
    print("=" * 70)

    # Test simple cases
    test_case("Simple SAT", "(a | b) & (c)")
    test_case("Easy problem", "(a | b) & (c | d) & (e)")
    test_case("Chain", "(a) & (~a | b) & (~b | c) & (c | d)")
    test_case("Small Random 3-SAT", "(a | b | c) & (~a | d | e) & (b | ~d | f)")
    test_case("Unit clause", "(a)")
    test_case("UNSAT", "(a) & (~a)")

    print("\n" + "=" * 70)
    print("Debug tests complete")
    print("=" * 70)
