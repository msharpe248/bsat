#!/bin/bash

# BSAT Solver Benchmark Script
# Compares C solver performance against optimized Python competition solver
# (competition/python/competition_solver.py) and tracks key metrics

set -e

TIMEOUT=30  # seconds per instance
RESULTS_DIR="benchmark_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/benchmark_$TIMESTAMP.txt"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

mkdir -p "$RESULTS_DIR"

echo "========================================" | tee "$RESULT_FILE"
echo "BSAT Solver Benchmark" | tee -a "$RESULT_FILE"
echo "Date: $(date)" | tee -a "$RESULT_FILE"
echo "Timeout: ${TIMEOUT}s per instance" | tee -a "$RESULT_FILE"
echo "========================================" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# Counters
total_instances=0
c_solved=0
python_solved=0
c_faster=0
python_faster=0
both_solved=0
c_only=0
python_only=0
neither_solved=0
total_c_time=0
total_python_time=0

# Function to run solver with timeout
run_solver() {
    local solver=$1
    local instance=$2
    local timeout=$3

    local start=$(date +%s.%N)
    local result
    local status

    timeout "$timeout" $solver "$instance" > /tmp/solver_output_$$.txt 2>&1
    local exit_code=$?

    local end=$(date +%s.%N)
    local elapsed=$(echo "$end - $start" | bc)

    # Check exit code (SAT competition standard: 10=SAT, 20=UNSAT, 0=UNKNOWN)
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 10 ] || [ $exit_code -eq 20 ]; then
        status="ok"
        # Get result from output
        result=$(grep "^s " /tmp/solver_output_$$.txt | awk '{print $2}')
        if [ -z "$result" ]; then
            result="UNKNOWN"
            status="error"
        fi
    elif [ $exit_code -eq 124 ]; then
        status="timeout"
        result="TIMEOUT"
    else
        status="error"
        result="ERROR"
    fi

    rm -f /tmp/solver_output_$$.txt

    echo "$status|$result|$elapsed"
}

# Function to process a test suite
process_suite() {
    local suite_name=$1
    local pattern=$2

    echo -e "${BLUE}=== Testing: $suite_name ===${NC}" | tee -a "$RESULT_FILE"
    echo "" | tee -a "$RESULT_FILE"

    printf "%-45s %-15s %-15s %-10s\n" "Instance" "C Solver" "Python Solver" "Speedup" | tee -a "$RESULT_FILE"
    printf "%-45s %-15s %-15s %-10s\n" "--------" "--------" "-------------" "-------" | tee -a "$RESULT_FILE"

    for instance in $pattern; do
        [ ! -f "$instance" ] && continue

        basename=$(basename "$instance")
        total_instances=$((total_instances + 1))

        # Run C solver
        c_result=$(run_solver "./bin/bsat" "$instance" "$TIMEOUT")
        c_status=$(echo "$c_result" | cut -d'|' -f1)
        c_answer=$(echo "$c_result" | cut -d'|' -f2)
        c_time=$(echo "$c_result" | cut -d'|' -f3)

        # Run Python solver
        python_result=$(run_solver "python ../python/competition_solver.py" "$instance" "$TIMEOUT")
        python_status=$(echo "$python_result" | cut -d'|' -f1)
        python_answer=$(echo "$python_result" | cut -d'|' -f2)
        python_time=$(echo "$python_result" | cut -d'|' -f3)

        # Format times
        local c_display python_display color speedup_display
        if [ "$c_status" = "ok" ]; then
            c_display=$(printf "%.3fs (%s)" "$c_time" "$c_answer")
            c_solved=$((c_solved + 1))
            total_c_time=$(echo "$total_c_time + $c_time" | bc)
        else
            c_display=$(echo "$c_status" | tr 'a-z' 'A-Z')
        fi

        if [ "$python_status" = "ok" ]; then
            python_display=$(printf "%.3fs (%s)" "$python_time" "$python_answer")
            python_solved=$((python_solved + 1))
            total_python_time=$(echo "$total_python_time + $python_time" | bc)
        else
            python_display=$(echo "$python_status" | tr 'a-z' 'A-Z')
        fi

        # Calculate speedup
        if [ "$c_status" = "ok" ] && [ "$python_status" = "ok" ]; then
            both_solved=$((both_solved + 1))
            speedup=$(echo "scale=2; $python_time / $c_time" | bc)
            speedup_display="${speedup}x"

            # Determine which is faster
            result=$(echo "$speedup > 1.0" | bc)
            if [ "$result" -eq 1 ]; then
                c_faster=$((c_faster + 1))
                color=$GREEN
            else
                python_faster=$((python_faster + 1))
                color=$RED
            fi

            # Check answer agreement
            if [ "$c_answer" != "$python_answer" ]; then
                color=$RED
                speedup_display="MISMATCH!"
            fi
        elif [ "$c_status" = "ok" ]; then
            c_only=$((c_only + 1))
            speedup_display="C only"
            color=$GREEN
        elif [ "$python_status" = "ok" ]; then
            python_only=$((python_only + 1))
            speedup_display="Py only"
            color=$YELLOW
        else
            neither_solved=$((neither_solved + 1))
            speedup_display="-"
            color=$NC
        fi

        echo -e "${color}$(printf "%-45s %-15s %-15s %-10s" "$basename" "$c_display" "$python_display" "$speedup_display")${NC}" | tee -a "$RESULT_FILE"
    done

    echo "" | tee -a "$RESULT_FILE"
}

# Process each test suite
process_suite "Simple" "../../dataset/simple_tests/simple_suite/*.cnf"
process_suite "Medium" "../../dataset/medium_tests/medium_suite/*.cnf"

# Summary statistics
echo "========================================" | tee -a "$RESULT_FILE"
echo "SUMMARY" | tee -a "$RESULT_FILE"
echo "========================================" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

echo "Total instances tested: $total_instances" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

echo "Instances solved:" | tee -a "$RESULT_FILE"
echo "  C solver:        $c_solved / $total_instances ($(echo "scale=1; 100*$c_solved/$total_instances" | bc)%)" | tee -a "$RESULT_FILE"
echo "  Python solver:   $python_solved / $total_instances ($(echo "scale=1; 100*$python_solved/$total_instances" | bc)%)" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

echo "Comparison (both solved same instances):" | tee -a "$RESULT_FILE"
echo "  Both solved:     $both_solved" | tee -a "$RESULT_FILE"
echo "  C faster:        $c_faster" | tee -a "$RESULT_FILE"
echo "  Python faster:   $python_faster" | tee -a "$RESULT_FILE"
echo "  C only solved:   $c_only" | tee -a "$RESULT_FILE"
echo "  Py only solved:  $python_only" | tee -a "$RESULT_FILE"
echo "  Neither solved:  $neither_solved" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

if [ $both_solved -gt 0 ]; then
    avg_speedup=$(echo "scale=2; $total_python_time / $total_c_time" | bc)
    echo "Average speedup (C vs Python): ${avg_speedup}x" | tee -a "$RESULT_FILE"
    echo "  Total C time:      $(printf "%.3f" $total_c_time)s" | tee -a "$RESULT_FILE"
    echo "  Total Python time: $(printf "%.3f" $total_python_time)s" | tee -a "$RESULT_FILE"
fi

echo "" | tee -a "$RESULT_FILE"
echo "Results saved to: $RESULT_FILE" | tee -a "$RESULT_FILE"
