#!/bin/bash

DATASET_DIR="../../dataset/medium_tests/medium_suite"
TIMEOUT=30

echo "Testing Glucose Restarts + Random Phase Variations"
echo "===================================================="
echo ""

for random_prob in 0.05 0.10; do
    echo "Configuration: Glucose + ${random_prob} random phase"
    echo "Command: ./bin/bsat --no-luby-restart --random-prob $random_prob <file>"
    echo "==========================================================="

    passed=0
    failed=0
    failed_files=""

    for cnf_file in "$DATASET_DIR"/*.cnf; do
        if [ ! -f "$cnf_file" ]; then
            continue
        fi

        basename=$(basename "$cnf_file")
        output=$(timeout $TIMEOUT ./bin/bsat --no-luby-restart --random-prob $random_prob "$cnf_file" 2>&1)
        result=$(echo "$output" | grep "^s " | awk '{print $2}')
        conflicts=$(echo "$output" | grep "^c Conflicts" | awk '{print $4}')

        if [ -z "$result" ]; then
            echo "❌ TIMEOUT: $basename"
            failed_files="$failed_files $basename"
            ((failed++))
        else
            echo "✅ PASS: $basename ($result in $conflicts conflicts)"
            ((passed++))
        fi
    done

    echo ""
    echo "==========================================================="
    echo "Results for ${random_prob} random phase:"
    echo "  Passed:  $passed"
    echo "  Failed:  $failed"
    echo "  Total:   $((passed + failed))"
    echo "  Success: $(echo "scale=1; $passed * 100 / ($passed + $failed)" | bc)%"

    if [ -n "$failed_files" ]; then
        echo "  Failed files:$failed_files"
    fi

    echo ""
    echo ""
done
