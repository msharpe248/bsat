#!/usr/bin/env python3
"""Verify C solver's solution is actually valid."""

import sys
import subprocess
from pathlib import Path
sys.path.insert(0, '../../../src')

from bsat.dimacs import read_dimacs_file, parse_dimacs_solution

def verify_c_solution(cnf_path):
    """Get C solver solution and verify it."""
    result = subprocess.run(
        ['../bin/cdcl_solver', str(cnf_path)],
        capture_output=True,
        text=True,
        timeout=10
    )

    # Parse status and solution
    status = None
    solution_line = None

    for line in result.stdout.split('\n'):
        if line.startswith('s '):
            status = line.split()[1]
        if line.startswith('v '):
            solution_line = line

    if status != 'SATISFIABLE':
        return None, None, status

    # Parse DIMACS solution
    solution = parse_dimacs_solution(solution_line)

    # Load CNF and verify
    cnf = read_dimacs_file(str(cnf_path))
    valid = cnf.evaluate(solution)

    return solution, valid, status

# Test the problematic instance
cnf_path = Path("../../../dataset/simple_tests/simple_suite/random3sat_v15_c64.cnf")
solution, valid, status = verify_c_solution(cnf_path)

print(f"Instance: {cnf_path.name}")
print(f"C Solver Status: {status}")
if solution:
    print(f"Solution Valid: {valid}")
    if not valid:
        print("❌ C SOLVER RETURNED INVALID SOLUTION!")
    else:
        print("✅ C solver solution is VALID - so the instance is SAT")
        print("\nPython solver must have a bug or timeout issue...")
