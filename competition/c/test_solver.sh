#!/bin/bash

# Simple test script for the C CDCL solver

echo "Testing BSAT C Solver on simple instances..."
echo "============================================"

# Counter for passed tests
PASSED=0
FAILED=0

# Test function
test_instance() {
    local file=$1
    local expected=$2

    echo -n "Testing $(basename $file)... "

    # Run solver quietly
    result=$(./bin/bsat "$file" 2>/dev/null | grep "^s " | cut -d' ' -f2)

    if [ "$result" = "$expected" ]; then
        echo "✅ PASS ($result)"
        PASSED=$((PASSED + 1))
    else
        echo "❌ FAIL (got $result, expected $expected)"
        FAILED=$((FAILED + 1))
    fi
}

# Test SAT instances
test_instance "../../dataset/simple_tests/simple_suite/random3sat_v5_c21.cnf" "SATISFIABLE"
test_instance "../../dataset/simple_tests/simple_suite/random3sat_v7_c30.cnf" "SATISFIABLE"
test_instance "../../dataset/simple_tests/simple_suite/random3sat_v10_c43.cnf" "SATISFIABLE"

# Create a simple UNSAT instance
cat > /tmp/unsat.cnf << EOF
p cnf 2 4
1 2 0
-1 -2 0
1 -2 0
-1 2 0
EOF

test_instance "/tmp/unsat.cnf" "UNSATISFIABLE"

echo ""
echo "Results: $PASSED passed, $FAILED failed"

if [ $FAILED -eq 0 ]; then
    echo "All tests passed! ✅"
    exit 0
else
    echo "Some tests failed! ❌"
    exit 1
fi