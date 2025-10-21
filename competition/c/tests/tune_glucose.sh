#!/bin/bash

# Glucose Parameter Tuning Script
#
# Tests different combinations of Glucose restart parameters to find optimal settings
# Compares against baseline (geometric restarts)

echo "Glucose Parameter Tuning Experiment"
echo "====================================="
echo ""

# Test instances (representative sample)
INSTANCES=(
    "../../../dataset/simple_tests/simple_suite/random3sat_v5_c21.cnf"
    "../../../dataset/simple_tests/simple_suite/random3sat_v7_c30.cnf"
    "../../../dataset/simple_tests/simple_suite/random3sat_v10_c43.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v010_c0042.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v012_c0050.cnf"
    "../../../dataset/medium_tests/medium_suite/easy_3sat_v026_c0109.cnf"
)

# Baseline: Geometric restarts
echo "=== BASELINE: Geometric Restarts ==="
echo ""

total_baseline=0
for instance in "${INSTANCES[@]}"; do
    if [ ! -f "$instance" ]; then
        continue
    fi

    basename=$(basename "$instance")
    start=$(date +%s%N)
    result=$(timeout 10 ../bin/bsat "$instance" 2>&1)
    end=$(date +%s%N)

    if [ $? -eq 124 ]; then
        echo "$basename: TIMEOUT"
        time=10.0
    else
        time=$(echo "scale=6; ($end - $start) / 1000000000" | bc)
        conflicts=$(echo "$result" | grep "^c Conflicts" | awk '{print $4}')
        status=$(echo "$result" | grep "^s " | awk '{print $2}')
        echo "$basename: $time s - $conflicts conflicts - $status"
    fi

    total_baseline=$(echo "$total_baseline + $time" | bc)
done

echo ""
echo "Baseline total time: $total_baseline s"
echo ""
echo "================================================"
echo ""

# Parameter combinations to test
# Format: fast_alpha slow_alpha min_conflicts postpone_threshold

CONFIGS=(
    # Original Glucose parameters
    "0.8 0.9999 100 10"

    # Faster adaptation (higher fast_alpha means slower decay = more weight on history)
    "0.85 0.9999 100 10"
    "0.75 0.9999 100 10"

    # Slower long-term tracking
    "0.8 0.999 100 10"
    "0.8 0.99999 100 10"

    # Different minimum conflicts threshold
    "0.8 0.9999 50 10"
    "0.8 0.9999 200 10"

    # Different postpone threshold
    "0.8 0.9999 100 5"
    "0.8 0.9999 100 20"
    "0.8 0.9999 100 50"

    # Combined tuning (faster adaptation + higher min conflicts)
    "0.85 0.9999 200 10"
    "0.75 0.9999 50 10"

    # Very aggressive (fast reset, low threshold)
    "0.7 0.999 50 5"

    # Very conservative (slow adaptation, high thresholds)
    "0.9 0.99999 200 50"
)

config_num=1
for config in "${CONFIGS[@]}"; do
    read -r fast_alpha slow_alpha min_conflicts postpone <<< "$config"

    echo "=== CONFIG $config_num: fast=$fast_alpha, slow=$slow_alpha, min=$min_conflicts, postpone=$postpone ==="
    echo ""

    total_time=0
    timeouts=0

    for instance in "${INSTANCES[@]}"; do
        if [ ! -f "$instance" ]; then
            continue
        fi

        basename=$(basename "$instance")
        start=$(date +%s%N)
        result=$(timeout 10 ../bin/bsat --glucose-restart \
                 --glucose-fast-alpha "$fast_alpha" \
                 --glucose-slow-alpha "$slow_alpha" \
                 --glucose-min-conflicts "$min_conflicts" \
                 "$instance" 2>&1)
        end=$(date +%s%N)

        if [ $? -eq 124 ]; then
            echo "$basename: TIMEOUT"
            time=10.0
            ((timeouts++))
        else
            time=$(echo "scale=6; ($end - $start) / 1000000000" | bc)
            conflicts=$(echo "$result" | grep "^c Conflicts" | awk '{print $4}')
            restarts=$(echo "$result" | grep "^c Restarts" | awk '{print $4}')
            status=$(echo "$result" | grep "^s " | awk '{print $2}')
            echo "$basename: $time s - $conflicts conflicts - $restarts restarts - $status"
        fi

        total_time=$(echo "$total_time + $time" | bc)
    done

    # Calculate speedup vs baseline
    if [ $(echo "$total_time > 0" | bc) -eq 1 ]; then
        speedup=$(echo "scale=2; $total_baseline / $total_time" | bc)
    else
        speedup="N/A"
    fi

    echo ""
    echo "Config $config_num total time: $total_time s (${timeouts} timeouts)"
    echo "Speedup vs baseline: ${speedup}x"
    echo ""
    echo "================================================"
    echo ""

    ((config_num++))
done

echo "Tuning complete!"
echo ""
echo "Analysis:"
echo "- Look for configs with total time < $total_baseline s"
echo "- Prefer configs with 0 timeouts"
echo "- Best config is fastest with no timeouts"
