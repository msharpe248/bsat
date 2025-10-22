#!/bin/bash

# BSAT C Unit Tests - Runner Script
# Runs all compiled test binaries and reports results

set -e

echo "========================================"
echo "BSAT C Solver - Unit Test Suite"
echo "========================================"
echo ""

PASSED=0
FAILED=0
TOTAL=0

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Find all test binaries (exclude .dSYM directories)
TEST_BINARIES=$(find ../bin -maxdepth 1 -type f -name "test_*" 2>/dev/null || true)

if [ -z "$TEST_BINARIES" ]; then
    echo "❌ No test binaries found. Run 'make tests' first."
    exit 1
fi

# Run each test (from parent directory so paths work)
cd ..
for test in bin/test_*; do
    # Skip if directory
    [ -f "$test" ] || continue
    ((TOTAL++))
    test_name=$(basename "$test")

    echo "Running $test_name..."
    echo "----------------------------------------"

    if $test; then
        echo -e "${GREEN}✅ $test_name PASSED${NC}"
        ((PASSED++))
    else
        echo -e "${RED}❌ $test_name FAILED${NC}"
        ((FAILED++))
    fi

    echo ""
done

# Summary
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Total tests: $TOTAL"
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
else
    echo "Failed: $FAILED"
fi
echo "========================================"

if [ $FAILED -eq 0 ]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "❌ Some tests failed!"
    exit 1
fi
