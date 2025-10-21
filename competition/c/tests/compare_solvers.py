#!/usr/bin/env python3
"""Compare C and Python CDCL solvers for correctness."""

import sys
import subprocess
from pathlib import Path
sys.path.insert(0, '../../../src')

from bsat.dimacs import read_dimacs_file
from bsat import solve_cdcl

def test_c_solver(cnf_path):
    """Run C solver and return (SAT/UNSAT, solution)."""
    result = subprocess.run(
        ['../bin/cdcl_solver', str(cnf_path)],
        capture_output=True,
        text=True,
        timeout=10
    )

    # Extract status
    for line in result.stdout.split('\n'):
        if line.startswith('s '):
            status = line.split()[1]
            return status, result.stdout

    return 'UNKNOWN', result.stdout

def test_python_solver(cnf_path):
    """Run Python solver and return SAT/UNSAT."""
    cnf = read_dimacs_file(str(cnf_path))
    result = solve_cdcl(cnf)

    if result:
        # Verify solution
        valid = cnf.evaluate(result)
        return 'SATISFIABLE' if valid else 'INVALID', result
    else:
        return 'UNSATISFIABLE', None

def main():
    print("Comparing C and Python CDCL Solvers")
    print("=" * 70)

    # Test simple suite
    simple_dir = Path("../../../dataset/simple_tests/simple_suite")
    simple_instances = sorted(simple_dir.glob("*.cnf"))[:10]

    print("\nSimple Test Suite (10 instances):")
    print("-" * 70)

    pass_count = 0
    fail_count = 0

    for instance in simple_instances:
        c_status, _ = test_c_solver(instance)
        py_status, _ = test_python_solver(instance)

        match = "✅" if c_status == py_status else "❌"

        print(f"{match} {instance.stem:30s} | C: {c_status:13s} | Py: {py_status:13s}")

        if c_status == py_status:
            pass_count += 1
        else:
            fail_count += 1

    print()
    print("-" * 70)
    print(f"Results: {pass_count} PASS, {fail_count} FAIL")

    if fail_count == 0:
        print("✅ ALL TESTS PASSED - C solver matches Python solver")
    else:
        print(f"❌ {fail_count} MISMATCHES DETECTED")

if __name__ == '__main__':
    main()
