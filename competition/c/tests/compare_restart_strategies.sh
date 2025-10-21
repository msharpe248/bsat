#!/bin/bash

echo "Comparing Restart Strategies"
echo "=============================="
echo ""

INSTANCES=(
    "../../dataset/simple_tests/simple_suite/random3sat_v5_c21.cnf"
    "../../dataset/simple_tests/simple_suite/random3sat_v7_c30.cnf"
    "../../dataset/simple_tests/simple_suite/random3sat_v10_c43.cnf"
    "../../dataset/medium_tests/medium_suite/easy_3sat_v010_c0042.cnf"
    "../../dataset/medium_tests/medium_suite/easy_3sat_v012_c0050.cnf"
    "../../dataset/medium_tests/medium_suite/easy_3sat_v026_c0109.cnf"
    "../../dataset/medium_tests/medium_suite/easy_3sat_v030_c0126.cnf"
)

echo "=== GEOMETRIC (baseline) ==="
total_geom=0
for instance in "${INSTANCES[@]}"; do
    [ ! -f "$instance" ] && continue
    start=$(date +%s%N)
    ../bin/bsat "$instance" > /dev/null 2>&1
    end=$(date +%s%N)
    time=$(echo "scale=6; ($end - $start) / 1000000000" | bc)
    total_geom=$(echo "$total_geom + $time" | bc)
    printf "%-40s %8s s\n" "$(basename $instance)" "$time"
done
echo "Total: $total_geom s"
echo ""

echo "=== HYBRID GLUCOSE/GEOMETRIC ==="
total_glucose=0
for instance in "${INSTANCES[@]}"; do
    [ ! -f "$instance" ] && continue
    start=$(date +%s%N)
    ../bin/bsat --glucose-restart "$instance" > /dev/null 2>&1
    end=$(date +%s%N)
    time=$(echo "scale=6; ($end - $start) / 1000000000" | bc)
    total_glucose=$(echo "$total_glucose + $time" | bc)
    printf "%-40s %8s s\n" "$(basename $instance)" "$time"
done
echo "Total: $total_glucose s"
echo ""

speedup=$(echo "scale=2; $total_geom / $total_glucose" | bc)
echo "Speedup: ${speedup}x"
