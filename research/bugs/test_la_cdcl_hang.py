#!/usr/bin/env python3
"""
Test LA-CDCL with the exact problem that hung in benchmark.
"""

import sys
import os
import signal

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../src'))
sys.path.insert(0, os.path.dirname(__file__))

from bsat import CNFExpression
from la_cdcl import LACDCLSolver
from benchmark import ProblemGenerator

def timeout_handler(signum, frame):
    raise TimeoutError("Solver timed out!")

signal.signal(signal.SIGALRM, timeout_handler)

print("Testing LA-CDCL with Random 3-SAT that hung in benchmark...")

# Generate the exact same problem from benchmark
cnf = ProblemGenerator.random_3sat(12, 40, seed=42)

print(f"Variables: {len(cnf.get_variables())}")
print(f"Clauses: {len(cnf.clauses)}")

try:
    solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)

    # Set 10 second alarm
    signal.alarm(10)

    print("Solving...")
    result = solver.solve()

    signal.alarm(0)

    print(f"✅ Result: {'SAT' if result else 'UNSAT'}")
    stats = solver.get_statistics()
    print(f"Decisions: {stats['decisions_made']}")
    print(f"Conflicts: {stats['conflicts_total']}")

except TimeoutError:
    signal.alarm(0)
    print("❌ TIMEOUT - Solver hung on this exact problem!")
    print("\nThis is the problem that hung in the benchmark.")
    print("Let me analyze the solving loop...")

except Exception as e:
    signal.alarm(0)
    print(f"❌ ERROR: {e}")
    import traceback
    traceback.print_exc()
