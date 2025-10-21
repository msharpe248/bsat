#!/bin/bash

# Test script for UNSAT instances to verify solver soundness

echo "Testing BSAT C Solver on UNSAT instances"
echo "=========================================="

# Create various UNSAT test cases
mkdir -p /tmp/unsat_tests

# 1. Simple binary UNSAT (original bug case)
cat > /tmp/unsat_tests/binary_unsat.cnf << EOF
p cnf 2 4
1 2 0
-1 -2 0
1 -2 0
-1 2 0
EOF

# 2. Unit clause contradiction
cat > /tmp/unsat_tests/unit_conflict.cnf << EOF
p cnf 1 2
1 0
-1 0
EOF

# 3. Larger UNSAT with no binary clauses
cat > /tmp/unsat_tests/non_binary_unsat.cnf << EOF
p cnf 3 8
1 2 3 0
1 2 -3 0
1 -2 3 0
1 -2 -3 0
-1 2 3 0
-1 2 -3 0
-1 -2 3 0
-1 -2 -3 0
EOF

# 4. Mixed binary and ternary UNSAT
cat > /tmp/unsat_tests/mixed_unsat.cnf << EOF
p cnf 3 7
1 2 0
-1 3 0
-2 -3 0
1 -3 0
-1 2 3 0
-1 -2 0
2 3 0
EOF

# Run tests
PASSED=0
FAILED=0

for file in /tmp/unsat_tests/*.cnf; do
    echo -n "Testing $(basename $file)... "
    result=$(timeout 5 ./bin/bsat "$file" 2>&1 | grep "^s ")

    if [[ "$result" == "s UNSATISFIABLE" ]]; then
        echo "✅ PASS"
        ((PASSED++))
    else
        echo "❌ FAIL - Got: $result (expected UNSATISFIABLE)"
        ((FAILED++))
    fi
done

echo ""
echo "Results: $PASSED passed, $FAILED failed"

if [ $FAILED -eq 0 ]; then
    echo "All UNSAT tests passed! ✅"
    exit 0
else
    echo "Some tests failed! ❌"
    exit 1
fi