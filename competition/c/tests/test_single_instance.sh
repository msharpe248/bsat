#!/bin/bash
# Run C solver with verbose output on problematic instance

SOLVER="../bin/cdcl_solver"
INSTANCE="../../../dataset/simple_tests/simple_suite/random3sat_v7_c30.cnf"

echo "Testing: random3sat_v7_c30.cnf"
echo "======================================"
echo

echo "C Solver Output:"
$SOLVER "$INSTANCE"

echo
echo "======================================"
echo

# Check the actual CNF to see if there are any unit clauses
echo "CNF File Content:"
head -20 "$INSTANCE"
