#!/usr/bin/env python3
"""Test soundness of watched literals on multiple instances."""

import sys
import os
from pathlib import Path
sys.path.insert(0, '../../../src')
sys.path.insert(0, '..')  # For cdcl_optimized in parent directory

from bsat.dimacs import read_dimacs_file
from bsat import get_cdcl_stats
import cdcl_optimized

def test_instance(cnf_path):
    """Test a single instance for soundness."""
    name = cnf_path.stem
    cnf = read_dimacs_file(str(cnf_path))

    # Test with watched literals
    solver_watched = cdcl_optimized.CDCLSolver(cnf, use_watched_literals=True)
    result_watched = solver_watched.solve(max_conflicts=10000)

    # Test without watched literals
    solver_orig = cdcl_optimized.CDCLSolver(cnf, use_watched_literals=False)
    result_orig = solver_orig.solve(max_conflicts=10000)

    # Verify solutions
    valid_watched = cnf.evaluate(result_watched) if result_watched else None
    valid_orig = cnf.evaluate(result_orig) if result_orig else None

    # Check for discrepancies
    watched_answer = "SAT" if result_watched else "UNSAT"
    orig_answer = "SAT" if result_orig else "UNSAT"

    soundness_ok = True
    issues = []

    # Check if watched literals returned invalid SAT
    if result_watched and not valid_watched:
        soundness_ok = False
        issues.append("INVALID SAT solution")

    # Check if answers disagree
    if watched_answer != orig_answer:
        soundness_ok = False
        issues.append(f"Answer mismatch: watched={watched_answer}, orig={orig_answer}")

    # Check if original returned invalid SAT
    if result_orig and not valid_orig:
        issues.append("WARNING: Original CDCL also invalid")

    status_symbol = "✅" if soundness_ok else "❌"

    print(f"{status_symbol} {name:30s} | Watched: {watched_answer:5s} (valid={valid_watched}) | Orig: {orig_answer:5s} (valid={valid_orig})")

    if issues:
        for issue in issues:
            print(f"   {'':31s} ISSUE: {issue}")

    return soundness_ok, issues

def main():
    print("SOUNDNESS TEST: Watched Literals Implementation")
    print("=" * 100)
    print()

    # Test on simple suite
    print("Simple Test Suite:")
    print("-" * 100)

    simple_dir = Path("../../../dataset/simple_tests/simple_suite")
    simple_instances = sorted(simple_dir.glob("*.cnf"))[:5]  # First 5

    simple_pass = 0
    simple_fail = 0

    for instance in simple_instances:
        ok, issues = test_instance(instance)
        if ok:
            simple_pass += 1
        else:
            simple_fail += 1

    print()
    print(f"Simple suite: {simple_pass} PASS, {simple_fail} FAIL")
    print()

    # Test on medium suite
    print("Medium Test Suite:")
    print("-" * 100)

    medium_dir = Path("../../../dataset/medium_tests/medium_suite")
    medium_instances = sorted(medium_dir.glob("*.cnf"))[:5]  # First 5

    medium_pass = 0
    medium_fail = 0

    for instance in medium_instances:
        ok, issues = test_instance(instance)
        if ok:
            medium_pass += 1
        else:
            medium_fail += 1

    print()
    print(f"Medium suite: {medium_pass} PASS, {medium_fail} FAIL")
    print()

    # Summary
    print("=" * 100)
    total_pass = simple_pass + medium_pass
    total_fail = simple_fail + medium_fail
    total = total_pass + total_fail

    print(f"TOTAL: {total_pass}/{total} passed, {total_fail}/{total} failed")

    if total_fail == 0:
        print("✅ ALL TESTS PASSED - Watched literals implementation is SOUND")
    else:
        print(f"❌ SOUNDNESS BUG DETECTED - {total_fail} instances failed")

if __name__ == "__main__":
    main()
