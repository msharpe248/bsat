#!/bin/bash

# Test impact of --random-phase on all medium instances

DATASET_DIR="/Users/msharpe/python/bsat/dataset/medium_tests/medium_suite"
C_SOLVER="./bin/bsat"
TIMEOUT=5

echo "Impact of --random-phase on Medium Instances"
echo "============================================="
echo ""
printf "%-40s %12s %12s %10s\n" "Instance" "Default" "Random Phase" "Speedup"
printf "%-40s %12s %12s %10s\n" "--------" "-------" "------------" "-------"

for cnf_file in "$DATASET_DIR"/easy_3sat_*.cnf "$DATASET_DIR"/medium_3sat_v040*.cnf "$DATASET_DIR"/medium_3sat_v042*.cnf; do
    if [ ! -f "$cnf_file" ]; then
        continue
    fi

    basename=$(basename "$cnf_file")

    # Test default (no random phase)
    default_output=$(timeout $TIMEOUT $C_SOLVER "$cnf_file" 2>&1)
    default_conflicts=$(echo "$default_output" | grep "^c Conflicts " | grep -v "Conflicts/sec" | awk -F':' '{print $2}' | tr -d ' ')
    default_result=$(echo "$default_output" | grep "^s " | awk '{print $2}')

    if [ -z "$default_result" ]; then
        default_conflicts="TIMEOUT"
    fi

    # Test with --random-phase
    random_output=$(timeout $TIMEOUT $C_SOLVER --random-phase "$cnf_file" 2>&1)
    random_conflicts=$(echo "$random_output" | grep "^c Conflicts " | grep -v "Conflicts/sec" | awk -F':' '{print $2}' | tr -d ' ')
    random_result=$(echo "$random_output" | grep "^s " | awk '{print $2}')

    if [ -z "$random_result" ]; then
        random_conflicts="TIMEOUT"
    fi

    # Calculate speedup
    if [ "$default_conflicts" != "TIMEOUT" ] && [ "$random_conflicts" != "TIMEOUT" ] && [ "$random_conflicts" != "0" ]; then
        speedup=$(echo "scale=2; $default_conflicts / $random_conflicts" | bc)
    elif [ "$default_conflicts" = "TIMEOUT" ] && [ "$random_conflicts" != "TIMEOUT" ]; then
        speedup="FIXED!"
    else
        speedup="-"
    fi

    printf "%-40s %12s %12s %10s\n" \
        "$basename" "$default_conflicts" "$random_conflicts" "$speedup"
done
