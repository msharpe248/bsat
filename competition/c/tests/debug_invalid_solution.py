#!/usr/bin/env python3
"""Debug why C solver solution is invalid."""

import sys
from pathlib import Path
sys.path.insert(0, '../../../src')

from bsat.dimacs import read_dimacs_file

# The C solver's solution
solution = {
    'x1': True, 'x2': True, 'x3': True, 'x4': True, 'x5': True,
    'x6': True, 'x7': True, 'x8': True, 'x9': True, 'x10': False,
    'x11': False, 'x12': True, 'x13': True, 'x14': True, 'x15': True
}

# Load CNF
cnf_path = Path("../../../dataset/simple_tests/simple_suite/random3sat_v15_c64.cnf")
cnf = read_dimacs_file(str(cnf_path))

print(f"Total clauses: {len(cnf.clauses)}")
print("\nChecking which clauses are falsified...")
print("=" * 70)

falsified_count = 0
for i, clause in enumerate(cnf.clauses):
    satisfied = clause.evaluate(solution)
    if not satisfied:
        falsified_count += 1
        print(f"\nClause {i+1} FALSIFIED: {clause}")
        print(f"  Literals:")
        for lit in clause.literals:
            var_value = solution.get(lit.variable, None)
            lit_value = (not lit.negated and var_value) or (lit.negated and not var_value)
            symbol = "✓" if lit_value else "✗"
            print(f"    {symbol} {lit} (var={lit.variable}, negated={lit.negated}, value={var_value})")

print(f"\nTotal falsified clauses: {falsified_count}")
print(f"Valid solution: {cnf.evaluate(solution)}")
