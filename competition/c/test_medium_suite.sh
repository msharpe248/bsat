#!/bin/bash

# Test all medium instances with improved solver

DATASET_DIR="/Users/msharpe/python/bsat/dataset/medium_tests/medium_suite"
SOLVER="./bin/bsat"
TIMEOUT=10

passed=0
failed=0
timeout_count=0

echo "Testing Medium Suite with Random Phase Enabled by Default"
echo "=========================================================="
echo ""

for cnf_file in "$DATASET_DIR"/*.cnf; do
    if [ ! -f "$cnf_file" ]; then
        continue
    fi

    basename=$(basename "$cnf_file")

    # Run solver
    output=$(timeout $TIMEOUT $SOLVER "$cnf_file" 2>&1)
    result=$(echo "$output" | grep "^s " | awk '{print $2}')
    conflicts=$(echo "$output" | grep "^c Conflicts " | grep -v "Conflicts/sec" | awk -F':' '{print $2}' | tr -d ' ')

    if [ -z "$result" ]; then
        echo "❌ TIMEOUT: $basename"
        ((timeout_count++))
    else
        echo "✅ PASS: $basename ($result in $conflicts conflicts)"
        ((passed++))
    fi
done

echo ""
echo "=========================================================="
echo "Results:"
echo "  Passed:  $passed"
echo "  Timeout: $timeout_count"
echo "  Total:   $((passed + timeout_count))"
