#!/bin/bash
# Test C solver on simple instances

SOLVER="../bin/cdcl_solver"
DATASET="../../../dataset/simple_tests/simple_suite"

echo "Testing C CDCL Solver on Simple Suite"
echo "======================================"

count=0
for cnf in "$DATASET"/*.cnf; do
    filename=$(basename "$cnf")
    result=$("$SOLVER" "$cnf" 2>&1 | grep "^s " | awk '{print $2}')
    echo "$filename: $result"

    count=$((count + 1))
    if [ $count -ge 10 ]; then
        break
    fi
done
