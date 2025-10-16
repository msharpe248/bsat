#!/usr/bin/env python3
"""
Debug script for LA-CDCL - isolate the hanging issue.
"""

import sys
import os
import signal

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from la_cdcl import LACDCLSolver

def timeout_handler(signum, frame):
    raise TimeoutError("Solver timed out!")

# Set 5 second timeout
signal.signal(signal.SIGALRM, timeout_handler)

def test_case(name, formula_str):
    """Test a single formula with timeout."""
    print(f"\nTesting: {name}")
    print(f"Formula: {formula_str}")

    try:
        cnf = CNFExpression.parse(formula_str)
        print(f"Variables: {len(cnf.get_variables())}, Clauses: {len(cnf.clauses)}")

        solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)

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
        print(f"   Conflicts: {stats['conflicts_total']}")

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
    print("LA-CDCL Debug Tests")
    print("=" * 70)

    # Test 1: Very simple (should work)
    test_case("Simple SAT", "(a | b) & (c)")

    # Test 2: Easy problem
    test_case("Easy problem", "(a | b) & (c | d) & (e)")

    # Test 3: Chain (this was tested in benchmark)
    test_case("Chain", "(a) & (~a | b) & (~b | c) & (c | d)")

    # Test 4: Random 3-SAT (this hung in benchmark)
    test_case("Small Random 3-SAT", "(a | b | c) & (~a | d | e) & (b | ~d | f)")

    # Test 5: Unit clause
    test_case("Unit clause", "(a)")

    # Test 6: UNSAT
    test_case("UNSAT", "(a) & (~a)")

    print("\n" + "=" * 70)
    print("Debug tests complete")
    print("=" * 70)
