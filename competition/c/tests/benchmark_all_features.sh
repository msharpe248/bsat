#!/bin/bash

echo "Comprehensive Feature Benchmark"
echo "================================"
echo ""
echo "Comparing:"
echo "  1. Baseline (geometric restarts only)"
echo "  2. Current (all optimizations enabled)"
echo ""

INSTANCES=(
    "../../../dataset/simple_tests/simple_suite/random3sat_v5_c21.cnf"
    "../../../dataset/simple_tests/simple_suite/random3sat_v7_c30.cnf"
    "../../../dataset/simple_tests/simple_suite/random3sat_v10_c43.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v010_c0042.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v012_c0050.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v014_c0058.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v026_c0109.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v030_c0126.cnf"
    "../../../dataset/medium_tests/medium_suite/medium_3sat_v040_c0170.cnf"
)

echo "=== BASELINE (Geometric restarts, no optimizations) ==="
echo ""
total_baseline=0
for instance in "${INSTANCES[@]}"; do
    [ ! -f "$instance" ] && continue
    basename=$(basename "$instance")
    start=$(date +%s%N)
    result=$(../bin/bsat --no-phase-saving "$instance" 2>&1)
    end=$(date +%s%N)
    time=$(echo "scale=6; ($end - $start) / 1000000000" | bc)
    total_baseline=$(echo "$total_baseline + $time" | bc)

    conflicts=$(echo "$result" | grep "^c Conflicts" | awk '{print $4}')
    status=$(echo "$result" | grep "^s " | awk '{print $2}')
    printf "%-40s %8s s - %6s conflicts - %s\n" "$basename" "$time" "$conflicts" "$status"
done
echo ""
echo "Baseline total: $total_baseline s"
echo ""
echo "================================================"
echo ""

echo "=== CURRENT (All optimizations enabled) ==="
echo ""
total_current=0
declare -a details
for instance in "${INSTANCES[@]}"; do
    [ ! -f "$instance" ] && continue
    basename=$(basename "$instance")
    start=$(date +%s%N)
    result=$(../bin/bsat "$instance" 2>&1)
    end=$(date +%s%N)
    time=$(echo "scale=6; ($end - $start) / 1000000000" | bc)
    total_current=$(echo "$total_current + $time" | bc)

    conflicts=$(echo "$result" | grep "^c Conflicts" | awk '{print $4}')
    subsumed=$(echo "$result" | grep "^c Subsumed" | awk '{print $4}')
    minimized=$(echo "$result" | grep "^c Minimized" | awk '{print $4}')
    status=$(echo "$result" | grep "^s " | awk '{print $2}')

    printf "%-40s %8s s - %6s conflicts - %s\n" "$basename" "$time" "$conflicts" "$status"
    details+=("$basename: $subsumed subsumed, $minimized minimized")
done
echo ""
echo "Current total: $total_current s"
echo ""

# Calculate speedup
if [ $(echo "$total_current > 0" | bc) -eq 1 ]; then
    speedup=$(echo "scale=2; $total_baseline / $total_current" | bc)
else
    speedup="N/A"
fi

echo "================================================"
echo ""
echo "RESULTS:"
echo "  Baseline time: $total_baseline s"
echo "  Current time:  $total_current s"
echo "  Speedup:       ${speedup}x"
echo ""
echo "Optimization impact:"
for detail in "${details[@]}"; do
    echo "  $detail"
done
