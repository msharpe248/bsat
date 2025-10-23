#!/bin/bash

# Compare C vs Python CDCL solvers on medium test instances
# Shows conflicts/decisions to identify algorithmic differences

DATASET_DIR="/Users/msharpe/python/bsat/dataset/medium_tests/medium_suite"
C_SOLVER="./bin/bsat"
PYTHON_SOLVER="python"
TIMEOUT=5

echo "C vs Python CDCL Comparison on Medium Instances"
echo "================================================"
echo ""
printf "%-40s %10s %10s %10s %10s %10s\n" "Instance" "C_Result" "C_Confl" "Py_Result" "Py_Confl" "C/Py Ratio"
printf "%-40s %10s %10s %10s %10s %10s\n" "--------" "--------" "--------" "---------" "---------" "----------"

for cnf_file in "$DATASET_DIR"/easy_3sat_*.cnf "$DATASET_DIR"/medium_3sat_v040*.cnf; do
    if [ ! -f "$cnf_file" ]; then
        continue
    fi

    basename=$(basename "$cnf_file")

    # Test C solver
    c_output=$(timeout $TIMEOUT $C_SOLVER "$cnf_file" 2>&1)
    c_result=$(echo "$c_output" | grep "^s " | awk '{print $2}')
    c_conflicts=$(echo "$c_output" | grep "^c Conflicts " | grep -v "Conflicts/sec" | awk -F':' '{print $2}' | tr -d ' ')

    if [ -z "$c_result" ]; then
        c_result="TIMEOUT"
        c_conflicts="-"
    fi

    # Test Python solver
    py_output=$(timeout $TIMEOUT python -c "
import sys
sys.path.insert(0, '../../src')
from bsat.dimacs import read_dimacs_file
from bsat import get_cdcl_stats

cnf = read_dimacs_file('$cnf_file')
result, stats = get_cdcl_stats(cnf, max_conflicts=100000)
print('SAT' if result else 'UNSAT')
print(stats.conflicts)
" 2>&1)

    py_result=$(echo "$py_output" | head -1)
    py_conflicts=$(echo "$py_output" | tail -1)

    if [ -z "$py_result" ]; then
        py_result="TIMEOUT"
        py_conflicts="-"
    fi

    # Calculate ratio
    if [ "$c_conflicts" != "-" ] && [ "$py_conflicts" != "-" ] && [ "$py_conflicts" != "0" ]; then
        ratio=$(echo "scale=2; $c_conflicts / $py_conflicts" | bc)
    else
        ratio="-"
    fi

    printf "%-40s %10s %10s %10s %10s %10s\n" \
        "$basename" "$c_result" "$c_conflicts" "$py_result" "$py_conflicts" "$ratio"
done
