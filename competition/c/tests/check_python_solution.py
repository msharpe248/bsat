#!/usr/bin/env python3
"""Check if Python's solution for random3sat_v7_c30 is valid."""

import sys
from pathlib import Path
sys.path.insert(0, '../../../src')

from bsat.dimacs import read_dimacs_file
from bsat import solve_cdcl

cnf_path = Path("../../../dataset/simple_tests/simple_suite/random3sat_v7_c30.cnf")
cnf = read_dimacs_file(str(cnf_path))

print(f"Testing: {cnf_path.name}")
print("=" * 70)

result = solve_cdcl(cnf)

if result:
    print("Python solver: SATISFIABLE")
    print(f"Solution: {result}")

    valid = cnf.evaluate(result)
    print(f"Solution valid: {valid}")

    if valid:
        print("✅ Python solution is VALID - instance is SAT")
        print("❌ C solver is WRONG (reported UNSAT for SAT instance)")
    else:
        print("❌ Python solution is INVALID")
else:
    print("Python solver: UNSATISFIABLE")
