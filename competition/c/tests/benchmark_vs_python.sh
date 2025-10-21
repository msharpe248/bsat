#!/bin/bash

# Benchmark C solver vs Python solver

echo "Benchmarking C CDCL vs Python CDCL"
echo "===================================="
echo ""

# Test instances
INSTANCES=(
    "../../../dataset/simple_tests/simple_suite/random3sat_v5_c21.cnf"
    "../../../dataset/simple_tests/simple_suite/random3sat_v7_c30.cnf"
    "../../../dataset/simple_tests/simple_suite/random3sat_v10_c43.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v026_c0109.cnf"
    "../../../dataset/medium_tests/medium_suite/medium_3sat_v040_c0170.cnf"
)

for instance in "${INSTANCES[@]}"; do
    if [ ! -f "$instance" ]; then
        continue
    fi

    basename=$(basename "$instance")
    echo "Testing $basename"
    echo "---"

    # C solver
    echo -n "C solver:      "
    c_start=$(date +%s%N)
    ../bin/bsat "$instance" > /tmp/c_output.txt 2>&1
    c_end=$(date +%s%N)
    c_time=$(echo "scale=6; ($c_end - $c_start) / 1000000000" | bc)
    c_result=$(grep "^s " /tmp/c_output.txt)
    c_conflicts=$(grep "^c Conflicts" /tmp/c_output.txt | awk '{print $4}')
    echo "$c_time s - $c_result - $c_conflicts conflicts"

    # Python solver
    echo -n "Python solver: "
    python_start=$(date +%s%N)
    python3 << EOF > /tmp/py_output.txt 2>&1
import sys
import time
sys.path.insert(0, '../../../src')
from bsat.dimacs import read_dimacs_file
from bsat import get_cdcl_stats

cnf = read_dimacs_file('$instance')
result, stats = get_cdcl_stats(cnf)
print(f"s {'SATISFIABLE' if result else 'UNSATISFIABLE'}")
print(f"Conflicts: {stats.conflicts}")
EOF
    python_end=$(date +%s%N)
    python_time=$(echo "scale=6; ($python_end - $python_start) / 1000000000" | bc)
    py_result=$(grep "^s " /tmp/py_output.txt)
    py_conflicts=$(grep "Conflicts:" /tmp/py_output.txt | awk '{print $2}')
    echo "$python_time s - $py_result - $py_conflicts conflicts"

    # Speedup
    speedup=$(echo "scale=2; $python_time / $c_time" | bc)
    echo "Speedup: ${speedup}x"
    echo ""
done

echo "Summary: C solver should be faster in wall-clock time"
echo "Note: Conflict counts may differ due to different heuristics"