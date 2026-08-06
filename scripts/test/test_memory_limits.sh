#!/bin/bash
# test_memory_limits.sh - Memory Limits Test Script
# Version: 1.0.0
# Author: Sebastian
# Description: This script tests memory limits and boundaries

set -e

# ============================================================================
# Colors and Formatting
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
LOG_DIR="$PROJECT_ROOT/logs"

# Test parameters
TEST_SIZE_MIN=64
TEST_SIZE_MAX=134217728  # 128MB
TEST_STEP=2
ITERATIONS=5
TIMEOUT=30
VERBOSE=0
FAIL_FAST=0

# Test results
TESTS_PASSED=0
TESTS_FAILED=0
TEST_RESULTS=()

# ============================================================================
# Helper Functions
# ============================================================================

print_header() {
    echo -e "\n${BLUE}${BOLD}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}${BOLD}║  $1${NC}"
    echo -e "${BLUE}${BOLD}╚══════════════════════════════════════════════════════════════╝${NC}"
}

print_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_step() {
    echo -e "\n${MAGENTA}▶${NC} ${BOLD}$1${NC}"
}

print_substep() {
    echo -e "  ${CYAN}›${NC} $1"
}

record_result() {
    local name="$1"
    local status="$2"
    local message="$3"
    
    TEST_RESULTS+=("$name:$status:$message")
    
    case "$status" in
        PASS)
            TESTS_PASSED=$((TESTS_PASSED + 1))
            print_success "$name: $message"
            ;;
        FAIL)
            TESTS_FAILED=$((TESTS_FAILED + 1))
            print_error "$name: $message"
            if [ $FAIL_FAST -eq 1 ]; then
                exit 1
            fi
            ;;
    esac
}

# ============================================================================
# Memory Limit Tests
# ============================================================================

test_min_allocation() {
    print_step "Testing Minimum Allocation"
    
    local size=$TEST_SIZE_MIN
    local test_name="Min Allocation (${size}B)"
    
    print_substep "Allocating minimum size: ${size}B"
    
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        if $BUILD_DIR/bin/memory_test --alloc $size --timeout $TIMEOUT 2>/dev/null; then
            record_result "$test_name" "PASS" "Minimum allocation successful"
        else
            record_result "$test_name" "FAIL" "Minimum allocation failed"
        fi
    else
        record_result "$test_name" "FAIL" "memory_test not found"
    fi
}

test_max_allocation() {
    print_step "Testing Maximum Allocation"
    
    # Determine max allocation size
    local total_mem=$(free -b | awk '/^Mem:/{print $2}')
    local max_size=$((total_mem / 4))
    if [ $max_size -gt $TEST_SIZE_MAX ]; then
        max_size=$TEST_SIZE_MAX
    fi
    
    local test_name="Max Allocation (${max_size}B)"
    
    print_substep "Allocating maximum size: ${max_size}B"
    
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        if $BUILD_DIR/bin/memory_test --alloc $max_size --timeout $TIMEOUT 2>/dev/null; then
            record_result "$test_name" "PASS" "Maximum allocation successful"
        else
            record_result "$test_name" "FAIL" "Maximum allocation failed"
        fi
    else
        record_result "$test_name" "FAIL" "memory_test not found"
    fi
}

test_allocation_sizes() {
    print_step "Testing Various Allocation Sizes"
    
    local sizes=(1024 4096 16384 65536 262144 1048576 4194304 16777216)
    
    for size in "${sizes[@]}"; do
        local test_name="Alloc ${size}B"
        print_substep "Testing allocation: ${size}B"
        
        if [ -f "$BUILD_DIR/bin/memory_test" ]; then
            if $BUILD_DIR/bin/memory_test --alloc $size --iterations $ITERATIONS 2>/dev/null; then
                record_result "$test_name" "PASS" "Allocation successful"
            else
                record_result "$test_name" "FAIL" "Allocation failed"
            fi
        else
            record_result "$test_name" "FAIL" "memory_test not found"
            break
        fi
    done
}

test_alignment() {
    print_step "Testing Memory Alignment"
    
    local alignments=(1 2 4 8 16 32 64 128 256 512 1024 2048 4096)
    local size=16384
    
    for align in "${alignments[@]}"; do
        local test_name="Alignment ${align}B"
        print_substep "Testing alignment: ${align}B"
        
        if [ -f "$BUILD_DIR/bin/memory_test" ]; then
            if $BUILD_DIR/bin/memory_test --alloc $size --align $align 2>/dev/null; then
                record_result "$test_name" "PASS" "Alignment successful"
            else
                record_result "$test_name" "FAIL" "Alignment failed"
            fi
        else
            record_result "$test_name" "FAIL" "memory_test not found"
            break
        fi
    done
}

test_fragmentation() {
    print_step "Testing Memory Fragmentation"
    
    # Test 1: Allocate and free in random order
    print_substep "Testing random allocation/free pattern..."
    
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        if $BUILD_DIR/bin/memory_test --fragmentation --iterations $ITERATIONS 2>/dev/null; then
            record_result "Fragmentation Random" "PASS" "Fragmentation test passed"
        else
            record_result "Fragmentation Random" "FAIL" "Fragmentation test failed"
        fi
    else
        record_result "Fragmentation Random" "FAIL" "memory_test not found"
    fi
    
    # Test 2: Allocate and free in specific pattern
    print_substep "Testing specific allocation pattern..."
    
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        if $BUILD_DIR/bin/memory_test --pattern "alternating" --iterations $ITERATIONS 2>/dev/null; then
            record_result "Fragmentation Pattern" "PASS" "Pattern test passed"
        else
            record_result "Fragmentation Pattern" "FAIL" "Pattern test failed"
        fi
    else
        record_result "Fragmentation Pattern" "FAIL" "memory_test not found"
    fi
}

test_edge_cases() {
    print_step "Testing Edge Cases"
    
    # Test 1: Zero allocation
    print_substep "Testing zero allocation..."
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        if $BUILD_DIR/bin/memory_test --alloc 0 2>/dev/null; then
            record_result "Edge Zero" "PASS" "Zero allocation handled"
        else
            record_result "Edge Zero" "FAIL" "Zero allocation failed"
        fi
    else
        record_result "Edge Zero" "FAIL" "memory_test not found"
    fi
    
    # Test 2: Very large allocation
    print_substep "Testing very large allocation..."
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        local mem_size=$(free -b | awk '/^Mem:/{print $2}')
        local huge_size=$((mem_size * 2))
        if $BUILD_DIR/bin/memory_test --alloc $huge_size --expect-fail 2>/dev/null; then
            record_result "Edge Huge" "PASS" "Huge allocation properly rejected"
        else
            record_result "Edge Huge" "FAIL" "Huge allocation test failed"
        fi
    else
        record_result "Edge Huge" "FAIL" "memory_test not found"
    fi
    
    # Test 3: Negative allocation
    print_substep "Testing negative allocation..."
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        if $BUILD_DIR/bin/memory_test --alloc -1 --expect-fail 2>/dev/null; then
            record_result "Edge Negative" "PASS" "Negative allocation properly rejected"
        else
            record_result "Edge Negative" "FAIL" "Negative allocation test failed"
        fi
    else
        record_result "Edge Negative" "FAIL" "memory_test not found"
    fi
}

test_concurrent() {
    print_step "Testing Concurrent Allocations"
    
    local num_threads=4
    local size=1048576  # 1MB
    
    print_substep "Testing concurrent allocations (${num_threads} threads)"
    
    if [ -f "$BUILD_DIR/bin/memory_test" ]; then
        if $BUILD_DIR/bin/memory_test --concurrent --threads $num_threads --alloc $size --iterations $ITERATIONS 2>/dev/null; then
            record_result "Concurrent Alloc" "PASS" "Concurrent allocation passed"
        else
            record_result "Concurrent Alloc" "FAIL" "Concurrent allocation failed"
        fi
    else
        record_result "Concurrent Alloc" "FAIL" "memory_test not found"
    fi
}

# ============================================================================
# Show Results
# ============================================================================

show_summary() {
    print_header "Memory Limits Test Summary"
    echo ""
    echo "  Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo "  Tests Failed: ${RED}$TESTS_FAILED${NC}"
    echo "  Total: $((TESTS_PASSED + TESTS_FAILED))"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}${BOLD}✅ All memory limit tests passed!${NC}"
    else
        echo -e "${RED}${BOLD}❌ Some memory limit tests failed!${NC}"
        exit 1
    fi
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 Memory Limits Test"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -i|--iterations)
                ITERATIONS="$2"
                shift 2
                ;;
            --timeout)
                TIMEOUT="$2"
                shift 2
                ;;
            --fail-fast)
                FAIL_FAST=1
                shift
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    # Run tests
    test_min_allocation
    test_max_allocation
    test_allocation_sizes
    test_alignment
    test_fragmentation
    test_edge_cases
    test_concurrent
    
    # Show summary
    show_summary
}

main "$@"
