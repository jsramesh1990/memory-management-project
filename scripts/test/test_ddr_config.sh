#!/bin/bash
# test_ddr_config.sh - DDR Configuration Test Script
# Version: 1.0.0
# Author: Sebastian
# Description: This script tests DDR configuration and validates settings

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
WHITE='\033[1;37m'
NC='\033[0m'
BOLD='\033[1m'

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
LOG_DIR="$PROJECT_ROOT/logs"
TEST_DIR="$PROJECT_ROOT/tests"

# Default values
TEST_TYPE="all"
VERBOSE=0
TIMEOUT=60
ITERATIONS=10
BOARD="auto"
DDR_TYPE="auto"
CONFIG_FILE=""
OUTPUT_FORMAT="text"
SAVE_LOGS=1
FAIL_FAST=0

# Test results
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0
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

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_skip() {
    echo -e "${BLUE}[SKIP]${NC} $1"
}

print_step() {
    echo -e "\n${MAGENTA}▶${NC} ${BOLD}$1${NC}"
}

print_substep() {
    echo -e "  ${CYAN}›${NC} $1"
}

show_help() {
    cat << EOF
${BOLD}RK3568 DDR Configuration Test Script${NC}

${BOLD}Usage:${NC}
    $0 [OPTIONS]

${BOLD}Options:${NC}
    -t, --test TYPE      Test type (all, basic, timing, stress, stability) [default: all]
    -b, --board BOARD    Board type (edge2, rock3b, orange5, custom, auto) [default: auto]
    -c, --config FILE    Configuration file to test
    -i, --iterations N   Number of iterations [default: 10]
    --timeout N          Timeout in seconds [default: 60]
    --fail-fast          Stop on first failure
    --save-logs          Save test logs
    --no-save-logs       Don't save test logs
    -o, --output FORMAT  Output format (text, json, junit) [default: text]
    -v, --verbose        Verbose output
    -h, --help           Show this help message

${BOLD}Test Types:${NC}
    all         - Run all tests
    basic       - Basic configuration tests
    timing      - Timing parameter tests
    stress      - Stress tests
    stability   - Stability tests

${BOLD}Examples:${NC}
    $0 --test all --board edge2
    $0 --test timing --iterations 100
    $0 --test stress --timeout 300

EOF
}

# ============================================================================
# Test Results Functions
# ============================================================================

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
        SKIP)
            TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
            print_skip "$name: $message"
            ;;
    esac
}

show_summary() {
    print_header "Test Summary"
    echo ""
    echo "  Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo "  Tests Failed: ${RED}$TESTS_FAILED${NC}"
    echo "  Tests Skipped: ${BLUE}$TESTS_SKIPPED${NC}"
    echo "  Total: $((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}${BOLD}✅ All tests passed!${NC}"
    else
        echo -e "${RED}${BOLD}❌ Some tests failed!${NC}"
        exit 1
    fi
}

# ============================================================================
# Configuration Detection
# ============================================================================

detect_board() {
    if [ "$BOARD" != "auto" ]; then
        echo "$BOARD"
        return
    fi
    
    print_info "Detecting board type..."
    
    # Try to detect from DTS
    if [ -f /proc/device-tree/model ]; then
        local model=$(cat /proc/device-tree/model)
        case "$model" in
            *"Edge 2"*) echo "edge2" ;;
            *"ROCK 3B"*) echo "rock3b" ;;
            *"Orange Pi 5"*) echo "orange5" ;;
            *) echo "custom" ;;
        esac
    else
        echo "custom"
    fi
}

detect_ddr_type() {
    if [ "$DDR_TYPE" != "auto" ]; then
        echo "$DDR_TYPE"
        return
    fi
    
    # Try to detect from dmesg
    if dmesg | grep -q "LPDDR4X"; then
        echo "LPDDR4X"
    elif dmesg | grep -q "LPDDR4"; then
        echo "LPDDR4"
    elif dmesg | grep -q "DDR4"; then
        echo "DDR4"
    else
        echo "Unknown"
    fi
}

# ============================================================================
# Test Functions
# ============================================================================

test_basic_config() {
    print_step "Testing Basic Configuration"
    
    # Test 1: DDR Init
    print_substep "Testing DDR initialization..."
    if [ -f "$BUILD_DIR/bin/ddr_info_tool" ]; then
        if $BUILD_DIR/bin/ddr_info_tool --init 2>/dev/null; then
            record_result "DDR Init" "PASS" "DDR initialized successfully"
        else
            record_result "DDR Init" "FAIL" "DDR initialization failed"
        fi
    else
        record_result "DDR Init" "SKIP" "ddr_info_tool not found"
    fi
    
    # Test 2: DDR Info
    print_substep "Testing DDR info retrieval..."
    if [ -f "$BUILD_DIR/bin/ddr_info_tool" ]; then
        local info=$($BUILD_DIR/bin/ddr_info_tool --info 2>/dev/null)
        if [ -n "$info" ]; then
            record_result "DDR Info" "PASS" "DDR info retrieved successfully"
            if [ $VERBOSE -eq 1 ]; then
                echo "$info"
            fi
        else
            record_result "DDR Info" "FAIL" "Failed to retrieve DDR info"
        fi
    else
        record_result "DDR Info" "SKIP" "ddr_info_tool not found"
    fi
    
    # Test 3: Memory Size
    print_substep "Testing memory size detection..."
    local mem_size=$(free -m | awk '/^Mem:/{print $2}')
    if [ $mem_size -gt 1024 ]; then
        record_result "Memory Size" "PASS" "Memory size: ${mem_size}MB"
    else
        record_result "Memory Size" "FAIL" "Unexpected memory size: ${mem_size}MB"
    fi
}

test_timing_params() {
    print_step "Testing Timing Parameters"
    
    local board=$(detect_board)
    
    # Test 1: CAS Latency
    print_substep "Testing CAS Latency..."
    local tCL=0
    if [ -f "$BUILD_DIR/bin/ddr_info_tool" ]; then
        tCL=$($BUILD_DIR/bin/ddr_info_tool --timing tCL 2>/dev/null || echo "0")
    fi
    if [ $tCL -gt 0 ]; then
        record_result "CAS Latency" "PASS" "tCL = $tCL"
    else
        record_result "CAS Latency" "FAIL" "Failed to get CAS latency"
    fi
    
    # Test 2: RAS-to-CAS Delay
    print_substep "Testing RAS-to-CAS Delay..."
    local tRCD=0
    if [ -f "$BUILD_DIR/bin/ddr_info_tool" ]; then
        tRCD=$($BUILD_DIR/bin/ddr_info_tool --timing tRCD 2>/dev/null || echo "0")
    fi
    if [ $tRCD -gt 0 ]; then
        record_result "RAS-to-CAS Delay" "PASS" "tRCD = $tRCD"
    else
        record_result "RAS-to-CAS Delay" "FAIL" "Failed to get RAS-to-CAS delay"
    fi
    
    # Test 3: RAS Precharge
    print_substep "Testing RAS Precharge..."
    local tRP=0
    if [ -f "$BUILD_DIR/bin/ddr_info_tool" ]; then
        tRP=$($BUILD_DIR/bin/ddr_info_tool --timing tRP 2>/dev/null || echo "0")
    fi
    if [ $tRP -gt 0 ]; then
        record_result "RAS Precharge" "PASS" "tRP = $tRP"
    else
        record_result "RAS Precharge" "FAIL" "Failed to get RAS precharge"
    fi
    
    # Test 4: Active to Precharge
    print_substep "Testing Active to Precharge..."
    local tRAS=0
    if [ -f "$BUILD_DIR/bin/ddr_info_tool" ]; then
        tRAS=$($BUILD_DIR/bin/ddr_info_tool --timing tRAS 2>/dev/null || echo "0")
    fi
    if [ $tRAS -gt 0 ]; then
        record_result "Active to Precharge" "PASS" "tRAS = $tRAS"
    else
        record_result "Active to Precharge" "FAIL" "Failed to get active to precharge"
    fi
}

test_stress() {
    print_step "Running Stress Tests"
    
    local duration=${TIMEOUT:-60}
    local iterations=${ITERATIONS:-10}
    
    # Test 1: Memory Allocation Stress
    print_substep "Testing memory allocation stress..."
    if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
        if $BUILD_DIR/bin/memory_benchmark --stress --duration $duration --iterations $iterations 2>/dev/null; then
            record_result "Memory Stress" "PASS" "Allocation stress test passed"
        else
            record_result "Memory Stress" "FAIL" "Allocation stress test failed"
        fi
    else
        record_result "Memory Stress" "SKIP" "memory_benchmark not found"
    fi
    
    # Test 2: Memory Access Stress
    print_substep "Testing memory access stress..."
    if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
        if $BUILD_DIR/bin/memory_benchmark --access --duration $duration 2>/dev/null; then
            record_result "Memory Access" "PASS" "Access stress test passed"
        else
            record_result "Memory Access" "FAIL" "Access stress test failed"
        fi
    else
        record_result "Memory Access" "SKIP" "memory_benchmark not found"
    fi
    
    # Test 3: Bandwidth Stress
    print_substep "Testing bandwidth stress..."
    if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
        if $BUILD_DIR/bin/memory_benchmark --bandwidth --duration $duration 2>/dev/null; then
            record_result "Bandwidth Stress" "PASS" "Bandwidth stress test passed"
        else
            record_result "Bandwidth Stress" "FAIL" "Bandwidth stress test failed"
        fi
    else
        record_result "Bandwidth Stress" "SKIP" "memory_benchmark not found"
    fi
}

test_stability() {
    print_step "Running Stability Tests"
    
    local duration=${TIMEOUT:-300}
    
    # Test 1: Long Running Test
    print_substep "Testing long-running stability..."
    if [ -f "$BUILD_DIR/bin/ddr_monitor" ]; then
        if timeout $duration $BUILD_DIR/bin/ddr_monitor --daemon --interval 1 2>/dev/null; then
            record_result "Long Running" "PASS" "Stability test passed"
        else
            record_result "Long Running" "FAIL" "Stability test failed"
        fi
    else
        record_result "Long Running" "SKIP" "ddr_monitor not found"
    fi
    
    # Test 2: Temperature Stability
    print_substep "Testing temperature stability..."
    if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
        local temp=$(cat /sys/class/thermal/thermal_zone0/temp)
        temp=$((temp / 1000))
        if [ $temp -lt 85 ]; then
            record_result "Temperature" "PASS" "Temperature: ${temp}°C"
        else
            record_result "Temperature" "FAIL" "Temperature too high: ${temp}°C"
        fi
    else
        record_result "Temperature" "SKIP" "Temperature sensor not found"
    fi
    
    # Test 3: Error Rate
    print_substep "Testing error rate..."
    if [ -f "$BUILD_DIR/bin/ddr_info_tool" ]; then
        local errors=$($BUILD_DIR/bin/ddr_info_tool --errors 2>/dev/null || echo "0")
        if [ $errors -eq 0 ]; then
            record_result "Error Rate" "PASS" "No errors detected"
        else
            record_result "Error Rate" "FAIL" "Errors detected: $errors"
        fi
    else
        record_result "Error Rate" "SKIP" "ddr_info_tool not found"
    fi
}

# ============================================================================
# Output Functions
# ============================================================================

output_text() {
    local output_file="$1"
    
    if [ -n "$output_file" ]; then
        exec > >(tee -a "$output_file")
    fi
    
    show_summary
}

output_json() {
    local output_file="$1"
    
    echo "{" > "$output_file"
    echo "  \"timestamp\": \"$(date -Iseconds)\"," >> "$output_file"
    echo "  \"board\": \"$(detect_board)\"," >> "$output_file"
    echo "  \"ddr_type\": \"$(detect_ddr_type)\"," >> "$output_file"
    echo "  \"results\": [" >> "$output_file"
    
    local first=1
    for result in "${TEST_RESULTS[@]}"; do
        local name="${result%:*}"
        local rest="${result#*:}"
        local status="${rest%:*}"
        local message="${rest#*:}"
        
        if [ $first -eq 1 ]; then
            first=0
        else
            echo "," >> "$output_file"
        fi
        
        echo "    {" >> "$output_file"
        echo "      \"name\": \"$name\"," >> "$output_file"
        echo "      \"status\": \"$status\"," >> "$output_file"
        echo "      \"message\": \"$message\"" >> "$output_file"
        echo "    }" >> "$output_file"
    done
    
    echo "  ]," >> "$output_file"
    echo "  \"summary\": {" >> "$output_file"
    echo "    \"passed\": $TESTS_PASSED," >> "$output_file"
    echo "    \"failed\": $TESTS_FAILED," >> "$output_file"
    echo "    \"skipped\": $TESTS_SKIPPED" >> "$output_file"
    echo "  }" >> "$output_file"
    echo "}" >> "$output_file"
}

output_junit() {
    local output_file="$1"
    
    cat > "$output_file" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="DDR Configuration Tests" tests="$((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))" 
           failures="$TESTS_FAILED" skipped="$TESTS_SKIPPED" time="0">
EOF
    
    for result in "${TEST_RESULTS[@]}"; do
        local name="${result%:*}"
        local rest="${result#*:}"
        local status="${rest%:*}"
        local message="${rest#*:}"
        
        case "$status" in
            PASS)
                echo "  <testcase name=\"$name\"/>" >> "$output_file"
                ;;
            FAIL)
                echo "  <testcase name=\"$name\">" >> "$output_file"
                echo "    <failure message=\"$message\"/>" >> "$output_file"
                echo "  </testcase>" >> "$output_file"
                ;;
            SKIP)
                echo "  <testcase name=\"$name\">" >> "$output_file"
                echo "    <skipped message=\"$message\"/>" >> "$output_file"
                echo "  </testcase>" >> "$output_file"
                ;;
        esac
    done
    
    echo "</testsuite>" >> "$output_file"
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 DDR Configuration Test"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -t|--test)
                TEST_TYPE="$2"
                shift 2
                ;;
            -b|--board)
                BOARD="$2"
                shift 2
                ;;
            -c|--config)
                CONFIG_FILE="$2"
                shift 2
                ;;
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
            --save-logs)
                SAVE_LOGS=1
                shift
                ;;
            --no-save-logs)
                SAVE_LOGS=0
                shift
                ;;
            -o|--output)
                OUTPUT_FORMAT="$2"
                shift 2
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
                show_help
                exit 1
                ;;
        esac
    done
    
    # Validate test type
    case "$TEST_TYPE" in
        all|basic|timing|stress|stability)
            print_info "Test type: $TEST_TYPE"
            ;;
        *)
            print_error "Invalid test type: $TEST_TYPE"
            show_help
            exit 1
            ;;
    esac
    
    # Validate output format
    case "$OUTPUT_FORMAT" in
        text|json|junit)
            print_info "Output format: $OUTPUT_FORMAT"
            ;;
        *)
            print_error "Invalid output format: $OUTPUT_FORMAT"
            exit 1
            ;;
    esac
    
    # Create log directory
    if [ $SAVE_LOGS -eq 1 ]; then
        mkdir -p "$LOG_DIR"
        local log_file="$LOG_DIR/ddr_test_$(date +%Y%m%d_%H%M%S).log"
        print_info "Log file: $log_file"
    fi
    
    # Detect board
    BOARD=$(detect_board)
    print_info "Board: $BOARD"
    
    DDR_TYPE=$(detect_ddr_type)
    print_info "DDR Type: $DDR_TYPE"
    
    # Run tests
    case "$TEST_TYPE" in
        all|basic)
            test_basic_config
            ;;
    esac
    
    case "$TEST_TYPE" in
        all|timing)
            test_timing_params
            ;;
    esac
    
    case "$TEST_TYPE" in
        all|stress)
            test_stress
            ;;
    esac
    
    case "$TEST_TYPE" in
        all|stability)
            test_stability
            ;;
    esac
    
    # Output results
    case "$OUTPUT_FORMAT" in
        text)
            output_text "${SAVE_LOGS:+$log_file}"
            ;;
        json)
            output_json "${SAVE_LOGS:-ddr_test.json}"
            ;;
        junit)
            output_junit "${SAVE_LOGS:-ddr_test.xml}"
            ;;
    esac
    
    # Show summary
    show_summary
}

# Run main function
main "$@"
