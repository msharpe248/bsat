#!/bin/bash

DATASET_DIR="../../dataset/medium_tests/medium_suite"
TIMEOUT=30

passed=0
failed=0

echo "Testing C CDCL with Luby + No Random Phase (Python Config)"
echo "==========================================================="
echo ""

for cnf_file in "$DATASET_DIR"/*.cnf; do
    if [ ! -f "$cnf_file" ]; then
        continue
    fi

    basename=$(basename "$cnf_file")
    
    # Run with Luby + no random phase
    output=$(timeout $TIMEOUT ./bin/bsat --luby-restart --luby-unit 100 --no-random-phase "$cnf_file" 2>&1)
    
    result=$(echo "$output" | grep "^s " | awk '{print $2}')
    conflicts=$(echo "$output" | grep "^c Conflicts" | awk '{print $4}')
    
    if [ -z "$result" ]; then
        echo "❌ TIMEOUT: $basename"
        ((failed++))
    else
        echo "✅ PASS: $basename ($result in $conflicts conflicts)"
        ((passed++))
    fi
done

echo ""
echo "==========================================================="
echo "Results:"
echo "  Passed:  $passed"
echo "  Failed:  $failed"
echo "  Total:   $((passed + failed))"
echo "  Success: $(echo "scale=1; $passed * 100 / ($passed + $failed)" | bc)%"
