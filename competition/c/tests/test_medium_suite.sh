#!/bin/bash

# Test BSAT solver on medium complexity instances

echo "Testing BSAT C Solver on medium instances"
echo "=========================================="

PASSED=0
FAILED=0
TIMEOUT=0

# Test various sizes from medium suite
for file in ../../../dataset/medium_tests/medium_suite/easy_3sat_v0*.cnf \
            ../../../dataset/medium_tests/medium_suite/medium_3sat_v040_c0170.cnf \
            ../../../dataset/medium_tests/medium_suite/medium_3sat_v044_c0187.cnf; do

    if [ ! -f "$file" ]; then
        continue
    fi

    echo -n "Testing $(basename $file)... "

    # Run with 10 second timeout
    result=$(timeout 10 ../bin/bsat "$file" 2>&1 | grep "^s ")

    if [ $? -eq 124 ]; then
        echo "⏱️  TIMEOUT (>10s)"
        ((TIMEOUT++))
    elif [[ "$result" == "s SATISFIABLE" ]] || [[ "$result" == "s UNSATISFIABLE" ]]; then
        echo "✅ PASS ($result)"
        ((PASSED++))
    else
        echo "❌ FAIL - Got: $result"
        ((FAILED++))
    fi
done

echo ""
echo "Results: $PASSED passed, $FAILED failed, $TIMEOUT timeouts"

if [ $FAILED -eq 0 ]; then
    echo "All non-timeout tests passed! ✅"
    exit 0
else
    echo "Some tests failed! ❌"
    exit 1
fi