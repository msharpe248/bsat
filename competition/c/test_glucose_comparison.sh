#!/bin/bash

# Compare Glucose EMA vs AVG on medium test suite

echo "================================================================"
echo "Glucose Restart Comparison: EMA vs AVG (Sliding Window)"
echo "================================================================"
echo ""

TIMEOUT=5
TEST_DIR="../../dataset/medium_tests/medium_suite"

# Test files
FILES=(
    "easy_3sat_v010_c0042.cnf"
    "easy_3sat_v012_c0050.cnf"
    "easy_3sat_v014_c0058.cnf"
    "hard_3sat_v108_c0461.cnf"
)

for file in "${FILES[@]}"; do
    filepath="$TEST_DIR/$file"
    if [ ! -f "$filepath" ]; then
        echo "SKIP: $file (not found)"
        continue
    fi

    echo "Testing: $file"
    echo "----------------------------------------"

    # Test with EMA mode
    echo -n "  EMA mode:  "
    result=$(timeout $TIMEOUT ./bin/bsat --glucose-restart-ema --stats "$filepath" 2>&1)
    if echo "$result" | grep -q "s SATISFIABLE"; then
        conflicts=$(echo "$result" | grep "^c Conflicts" | awk '{print $4}')
        restarts=$(echo "$result" | grep "^c Restarts" | awk '{print $4}')
        time=$(echo "$result" | grep "^c CPU time *:" | awk '{print $5}')
        echo "SAT  Conflicts=$conflicts  Restarts=$restarts  Time=${time}s"
    elif echo "$result" | grep -q "s UNSATISFIABLE"; then
        echo "UNSAT"
    else
        echo "TIMEOUT"
    fi

    # Test with AVG mode (sliding window)
    echo -n "  AVG mode:  "
    result=$(timeout $TIMEOUT ./bin/bsat --glucose-restart-avg --stats "$filepath" 2>&1)
    if echo "$result" | grep -q "s SATISFIABLE"; then
        conflicts=$(echo "$result" | grep "^c Conflicts" | awk '{print $4}')
        restarts=$(echo "$result" | grep "^c Restarts" | awk '{print $4}')
        time=$(echo "$result" | grep "^c CPU time *:" | awk '{print $5}')
        echo "SAT  Conflicts=$conflicts  Restarts=$restarts  Time=${time}s"
    elif echo "$result" | grep -q "s UNSATISFIABLE"; then
        echo "UNSAT"
    else
        echo "TIMEOUT"
    fi

    echo ""
done

echo "================================================================"
echo "Comparison complete"
echo "================================================================"
