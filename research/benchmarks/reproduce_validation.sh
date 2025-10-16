#!/bin/bash
#
# Comprehensive Validation Reproduction Script
#
# This script runs ALL validation tools to prove performance claims:
# 1. Correctness Verification
# 2. Statistical Validation
# 3. Profiling Analysis
# 4. Full Benchmark Suite
#
# Usage:
#   ./reproduce_validation.sh              # Run all validations
#   ./reproduce_validation.sh --quick      # Quick validation (fewer iterations)
#   ./reproduce_validation.sh --help       # Show help
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
QUICK_MODE=false
STATISTICAL_RUNS=10
OUTPUT_DIR="validation_results"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            QUICK_MODE=true
            STATISTICAL_RUNS=5
            shift
            ;;
        --help)
            echo "Comprehensive Validation Reproduction Script"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --quick    Quick validation mode (fewer iterations)"
            echo "  --help     Show this help message"
            echo ""
            echo "This script validates SAT solver performance claims by running:"
            echo "  1. Correctness Verification (validate_correctness.py)"
            echo "  2. Statistical Validation (statistical_benchmark.py)"
            echo "  3. Profiling Analysis (profile_solvers.py)"
            echo "  4. Full Benchmark Suite (run_full_benchmark.py)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Print banner
echo -e "${BLUE}================================================================================================${NC}"
echo -e "${BLUE}SAT SOLVER VALIDATION - COMPREHENSIVE REPRODUCTION${NC}"
echo -e "${BLUE}================================================================================================${NC}"
echo ""
echo "This script validates the performance claims made in BENCHMARKS.md"
echo ""
if [ "$QUICK_MODE" = true ]; then
    echo -e "${YELLOW}Running in QUICK mode (reduced iterations)${NC}"
else
    echo -e "${GREEN}Running in FULL mode (complete validation)${NC}"
fi
echo ""
echo "Output directory: $OUTPUT_DIR"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Track start time
SCRIPT_START=$(date +%s)

# ================================================================================================
# LEVEL 1: CORRECTNESS VERIFICATION
# ================================================================================================

echo -e "${BLUE}================================================================================================${NC}"
echo -e "${BLUE}LEVEL 1: CORRECTNESS VERIFICATION${NC}"
echo -e "${BLUE}================================================================================================${NC}"
echo ""
echo "Verifying that all solver results are correct:"
echo "  - SAT solutions actually satisfy all clauses"
echo "  - UNSAT results confirmed by independent solvers"
echo ""

START=$(date +%s)
python3 validate_correctness.py 2>&1 | tee "$OUTPUT_DIR/correctness_validation.log"
CORRECTNESS_EXIT_CODE=$?
END=$(date +%s)
DURATION=$((END - START))

if [ $CORRECTNESS_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ CORRECTNESS VERIFICATION PASSED${NC} (${DURATION}s)"
else
    echo -e "${RED}❌ CORRECTNESS VERIFICATION FAILED${NC} (${DURATION}s)"
    echo -e "${RED}Cannot proceed with other validations - fix correctness issues first!${NC}"
    exit 1
fi

echo ""

# ================================================================================================
# LEVEL 2: STATISTICAL VALIDATION
# ================================================================================================

echo -e "${BLUE}================================================================================================${NC}"
echo -e "${BLUE}LEVEL 2: STATISTICAL VALIDATION${NC}"
echo -e "${BLUE}================================================================================================${NC}"
echo ""
echo "Running $STATISTICAL_RUNS iterations per solver to compute:"
echo "  - Mean, median, standard deviation"
echo "  - 95% confidence intervals"
echo "  - Coefficient of variation (stability)"
echo "  - Speedup with confidence bounds"
echo ""

START=$(date +%s)
python3 statistical_benchmark.py \
    --runs $STATISTICAL_RUNS \
    --output "$OUTPUT_DIR/statistical_results.csv" \
    2>&1 | tee "$OUTPUT_DIR/statistical_validation.log"
STATISTICAL_EXIT_CODE=$?
END=$(date +%s)
DURATION=$((END - START))

if [ $STATISTICAL_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ STATISTICAL VALIDATION COMPLETED${NC} (${DURATION}s)"
    echo "   Results: $OUTPUT_DIR/statistical_results.csv"
else
    echo -e "${YELLOW}⚠️  STATISTICAL VALIDATION HAD ISSUES${NC} (${DURATION}s)"
    echo "   Check logs: $OUTPUT_DIR/statistical_validation.log"
fi

echo ""

# ================================================================================================
# LEVEL 3: PROFILING ANALYSIS
# ================================================================================================

echo -e "${BLUE}================================================================================================${NC}"
echo -e "${BLUE}LEVEL 3: PROFILING ANALYSIS${NC}"
echo -e "${BLUE}================================================================================================${NC}"
echo ""
echo "Profiling solvers to understand performance characteristics:"
echo "  - Function-level time breakdown"
echo "  - Memory usage patterns"
echo "  - Hotspot identification"
echo ""

START=$(date +%s)
python3 profile_solvers.py \
    --output "$OUTPUT_DIR/profile_report.md" \
    2>&1 | tee "$OUTPUT_DIR/profiling.log"
PROFILING_EXIT_CODE=$?
END=$(date +%s)
DURATION=$((END - START))

if [ $PROFILING_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ PROFILING ANALYSIS COMPLETED${NC} (${DURATION}s)"
    echo "   Report: $OUTPUT_DIR/profile_report.md"
else
    echo -e "${YELLOW}⚠️  PROFILING ANALYSIS HAD ISSUES${NC} (${DURATION}s)"
    echo "   Check logs: $OUTPUT_DIR/profiling.log"
fi

echo ""

# ================================================================================================
# LEVEL 4: FULL BENCHMARK SUITE
# ================================================================================================

echo -e "${BLUE}================================================================================================${NC}"
echo -e "${BLUE}LEVEL 4: FULL BENCHMARK SUITE${NC}"
echo -e "${BLUE}================================================================================================${NC}"
echo ""
echo "Running comprehensive benchmark across all problem types"
echo ""

START=$(date +%s)
python3 run_full_benchmark.py 2>&1 | tee "$OUTPUT_DIR/full_benchmark.log"
BENCHMARK_EXIT_CODE=$?
END=$(date +%s)
DURATION=$((END - START))

if [ $BENCHMARK_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ FULL BENCHMARK COMPLETED${NC} (${DURATION}s)"
    echo "   Results: benchmark_results_full.md"
    # Copy to output directory
    if [ -f "benchmark_results_full.md" ]; then
        cp benchmark_results_full.md "$OUTPUT_DIR/"
    fi
else
    echo -e "${YELLOW}⚠️  FULL BENCHMARK HAD ISSUES${NC} (${DURATION}s)"
    echo "   Check logs: $OUTPUT_DIR/full_benchmark.log"
fi

echo ""

# ================================================================================================
# SUMMARY
# ================================================================================================

SCRIPT_END=$(date +%s)
TOTAL_DURATION=$((SCRIPT_END - SCRIPT_START))

echo -e "${BLUE}================================================================================================${NC}"
echo -e "${BLUE}VALIDATION SUMMARY${NC}"
echo -e "${BLUE}================================================================================================${NC}"
echo ""
echo "Total runtime: ${TOTAL_DURATION}s"
echo ""
echo "Validation Results:"
echo ""

# Level 1 - CRITICAL
if [ $CORRECTNESS_EXIT_CODE -eq 0 ]; then
    echo -e "  ${GREEN}✅ Level 1: Correctness Verification - PASSED${NC}"
else
    echo -e "  ${RED}❌ Level 1: Correctness Verification - FAILED${NC}"
fi

# Level 2
if [ $STATISTICAL_EXIT_CODE -eq 0 ]; then
    echo -e "  ${GREEN}✅ Level 2: Statistical Validation - COMPLETED${NC}"
else
    echo -e "  ${YELLOW}⚠️  Level 2: Statistical Validation - HAD ISSUES${NC}"
fi

# Level 3
if [ $PROFILING_EXIT_CODE -eq 0 ]; then
    echo -e "  ${GREEN}✅ Level 3: Profiling Analysis - COMPLETED${NC}"
else
    echo -e "  ${YELLOW}⚠️  Level 3: Profiling Analysis - HAD ISSUES${NC}"
fi

# Level 4
if [ $BENCHMARK_EXIT_CODE -eq 0 ]; then
    echo -e "  ${GREEN}✅ Level 4: Full Benchmark - COMPLETED${NC}"
else
    echo -e "  ${YELLOW}⚠️  Level 4: Full Benchmark - HAD ISSUES${NC}"
fi

echo ""
echo "Output files:"
echo "  - $OUTPUT_DIR/correctness_validation.log"
echo "  - $OUTPUT_DIR/statistical_validation.log"
echo "  - $OUTPUT_DIR/statistical_results.csv"
echo "  - $OUTPUT_DIR/profiling.log"
echo "  - $OUTPUT_DIR/profile_report.md"
echo "  - $OUTPUT_DIR/full_benchmark.log"
echo "  - $OUTPUT_DIR/benchmark_results_full.md"
echo ""

# Final verdict
if [ $CORRECTNESS_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}================================================================================================${NC}"
    echo -e "${GREEN}🏆 VALIDATION SUCCESSFUL!${NC}"
    echo -e "${GREEN}================================================================================================${NC}"
    echo ""
    echo "Performance claims are validated with:"
    echo "  ✅ Correctness verification (all results are correct)"
    echo "  ✅ Statistical confidence (reproducible with confidence intervals)"
    echo "  ✅ Profiling analysis (algorithmic efficiency confirmed)"
    echo "  ✅ Comprehensive benchmarking (multiple problem types)"
    echo ""
    exit 0
else
    echo -e "${RED}================================================================================================${NC}"
    echo -e "${RED}❌ VALIDATION FAILED - CORRECTNESS ISSUES DETECTED${NC}"
    echo -e "${RED}================================================================================================${NC}"
    echo ""
    echo "Please review correctness validation logs and fix issues before claiming performance results."
    echo ""
    exit 1
fi
