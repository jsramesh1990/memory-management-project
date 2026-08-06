#!/bin/bash
# test_full_boot.sh - Full Boot Integration Test
# Version: 1.0.0
# Author: Sebastian
# Description: This script performs comprehensive boot testing for RK3568

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
LOG_DIR="$PROJECT_ROOT/logs"
TEST_DIR="$PROJECT_ROOT/tests"
REPORT_DIR="$PROJECT_ROOT/reports"

# Test parameters
TIMEOUT=300
BOOT_ATTEMPTS=3
SERIAL_DEVICE="/dev/ttyUSB0"
BAUDRATE=1500000
EXPECTED_BOOT_MSG="Linux version"
VERBOSE=0
SAVE_LOGS=1
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

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
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

show_help() {
    cat << EOF
${BOLD}RK3568 Full Boot Integration Test${NC}

${BOLD}Usage:${NC}
    $0 [OPTIONS]

${BOLD}Options:${NC}
    --serial DEVICE      Serial device [default: /dev/ttyUSB0]
    --baudrate SPEED     Baud rate [default: 1500000]
    --timeout SECONDS    Timeout in seconds [default: 300]
    --attempts N         Number of boot attempts [default: 3]
    --expected MSG       Expected boot message [default: "Linux version"]
    --fail-fast          Stop on first failure
    --no-save-logs       Don't save test logs
    -v, --verbose        Verbose output
    -h, --help           Show this help message

${BOLD}Examples:${NC}
    $0 --serial /dev/ttyUSB1 --timeout 60
    $0 --expected "Kernel panic" --fail-fast

EOF
}

# ============================================================================
# Serial Communication Functions
# ============================================================================

serial_init() {
    print_substep "Initializing serial device: $SERIAL_DEVICE"
    
    if [ ! -c "$SERIAL_DEVICE" ]; then
        print_error "Serial device not found: $SERIAL_DEVICE"
        return 1
    fi
    
    # Configure serial port
    stty -F $SERIAL_DEVICE $BAUDRATE cs8 -cstopb -parenb -crtscts
    stty -F $SERIAL_DEVICE -echo -echoe -echok
    
    print_success "Serial initialized"
    return 0
}

serial_read() {
    local timeout=$1
    local output=""
    
    # Read from serial with timeout
    output=$(timeout $timeout cat $SERIAL_DEVICE 2>/dev/null || echo "")
    echo "$output"
}

serial_write() {
    local cmd="$1"
    echo -e "$cmd" > $SERIAL_DEVICE
}

serial_wait_for() {
    local pattern="$1"
    local timeout="$2"
    local output=""
    local count=0
    
    print_substep "Waiting for: $pattern"
    
    while [ $count -lt $timeout ]; do
        output=$(serial_read 1)
        if echo "$output" | grep -q "$pattern"; then
            echo "$output"
            return 0
        fi
        count=$((count + 1))
        if [ $VERBOSE -eq 1 ]; then
            echo -n "."
        fi
    done
    
    echo "$output"
    return 1
}

serial_clear() {
    cat $SERIAL_DEVICE > /dev/null 2>&1 &
    local pid=$!
    sleep 1
    kill $pid 2>/dev/null || true
}

# ============================================================================
# Boot Test Functions
# ============================================================================

test_power_on() {
    local attempt="$1"
    print_substep "Powering on board (Attempt $attempt/$BOOT_ATTEMPTS)"
    
    # Simulate power-on (platform specific)
    # This could be GPIO control or USB power toggle
    # For now, we'll just clear the serial buffer
    
    serial_clear
    sleep 2
    
    # Trigger reset (platform specific)
    # echo 1 > /sys/class/gpio/gpioX/value 2>/dev/null || true
    # sleep 1
    # echo 0 > /sys/class/gpio/gpioX/value 2>/dev/null || true
    
    print_success "Power on signal sent"
}

test_bootloader() {
    print_substep "Testing bootloader output"
    
    local output=$(serial_wait_for "U-Boot" 10)
    if [ $? -eq 0 ]; then
        record_result "Bootloader Output" "PASS" "U-Boot detected"
        if [ $VERBOSE -eq 1 ]; then
            echo "$output" | head -10
        fi
        return 0
    else
        record_result "Bootloader Output" "FAIL" "U-Boot not detected"
        return 1
    fi
}

test_ddr_init() {
    print_substep "Testing DDR initialization"
    
    local output=$(serial_wait_for "DDR" 15)
    if [ $? -eq 0 ]; then
        if echo "$output" | grep -q "DDR: Initialized"; then
            record_result "DDR Init" "PASS" "DDR initialized successfully"
            return 0
        else
            record_result "DDR Init" "FAIL" "DDR initialization incomplete"
            return 1
        fi
    else
        record_result "DDR Init" "FAIL" "DDR init messages not detected"
        return 1
    fi
}

test_kernel_boot() {
    print_substep "Testing kernel boot"
    
    local output=$(serial_wait_for "Starting kernel" 10)
    if [ $? -eq 0 ]; then
        record_result "Kernel Start" "PASS" "Kernel started"
        return 0
    else
        record_result "Kernel Start" "FAIL" "Kernel not started"
        return 1
    fi
}

test_kernel_log() {
    print_substep "Checking kernel log"
    
    local output=$(serial_wait_for "$EXPECTED_BOOT_MSG" 30)
    if [ $? -eq 0 ]; then
        record_result "Kernel Log" "PASS" "Kernel boot message found"
        if [ $VERBOSE -eq 1 ]; then
            echo "$output" | head -20
        fi
        return 0
    else
        record_result "Kernel Log" "FAIL" "Kernel boot message not found"
        return 1
    fi
}

test_rootfs_mount() {
    print_substep "Testing root filesystem mount"
    
    local output=$(serial_wait_for "rootfs" 30)
    if [ $? -eq 0 ]; then
        if echo "$output" | grep -q "mounted"; then
            record_result "RootFS Mount" "PASS" "RootFS mounted successfully"
            return 0
        else
            record_result "RootFS Mount" "FAIL" "RootFS mount issue"
            return 1
        fi
    else
        record_result "RootFS Mount" "FAIL" "RootFS mount messages not detected"
        return 1
    fi
}

test_init_start() {
    print_substep "Testing init process start"
    
    local output=$(serial_wait_for "init:" 20)
    if [ $? -eq 0 ]; then
        record_result "Init Start" "PASS" "Init process started"
        return 0
    else
        record_result "Init Start" "FAIL" "Init process not started"
        return 1
    fi
}

test_console_ready() {
    print_substep "Testing console ready"
    
    local output=$(serial_wait_for "login:" 20)
    if [ $? -eq 0 ]; then
        record_result "Console Ready" "PASS" "Console ready"
        return 0
    else
        # Try sending a command to check if shell is responsive
        serial_write "echo test\n"
        sleep 2
        output=$(serial_read 2)
        if echo "$output" | grep -q "test"; then
            record_result "Console Ready" "PASS" "Console responsive"
            return 0
        else
            record_result "Console Ready" "FAIL" "Console not responsive"
            return 1
        fi
    fi
}

test_memory_info() {
    print_substep "Testing memory information"
    
    serial_write "cat /proc/meminfo\n"
    sleep 2
    local output=$(serial_read 2)
    
    if echo "$output" | grep -q "MemTotal"; then
        record_result "Memory Info" "PASS" "Memory info available"
        return 0
    else
        record_result "Memory Info" "FAIL" "Memory info not available"
        return 1
    fi
}

test_cpu_info() {
    print_substep "Testing CPU information"
    
    serial_write "cat /proc/cpuinfo\n"
    sleep 2
    local output=$(serial_read 2)
    
    if echo "$output" | grep -q "Processor"; then
        record_result "CPU Info" "PASS" "CPU info available"
        return 0
    else
        record_result "CPU Info" "FAIL" "CPU info not available"
        return 1
    fi
}

test_ddr_tool() {
    print_substep "Testing DDR tool"
    
    if [ -f "$PROJECT_ROOT/build/bin/ddr_info_tool" ]; then
        serial_write "ddr_info_tool --info\n"
        sleep 2
        local output=$(serial_read 2)
        
        if echo "$output" | grep -q "Total Memory"; then
            record_result "DDR Tool" "PASS" "DDR tool working"
            return 0
        else
            record_result "DDR Tool" "FAIL" "DDR tool not working"
            return 1
        fi
    else
        record_result "DDR Tool" "SKIP" "DDR tool not built"
        return 0
    fi
}

# ============================================================================
# Complete Boot Test
# ============================================================================

run_boot_test() {
    local attempt="$1"
    local passed=0
    
    print_header "Boot Test Attempt $attempt/$BOOT_ATTEMPTS"
    
    # Power on
    if ! test_power_on $attempt; then
        return 1
    fi
    
    # Run boot tests
    test_bootloader
    test_ddr_init
    test_kernel_boot
    test_kernel_log
    test_rootfs_mount
    test_init_start
    test_console_ready
    test_memory_info
    test_cpu_info
    test_ddr_tool
    
    # Count passes
    for result in "${TEST_RESULTS[@]}"; do
        if echo "$result" | grep -q ":PASS:"; then
            passed=$((passed + 1))
        fi
    done
    
    if [ $passed -ge 8 ]; then
        print_success "Boot test passed (${passed}/10 tests passed)"
        return 0
    else
        print_error "Boot test failed (${passed}/10 tests passed)"
        return 1
    fi
}

# ============================================================================
# Boot Analysis Functions
# ============================================================================

analyze_boot_time() {
    print_step "Analyzing boot time"
    
    local output=$(serial_read 60)
    local boot_time=0
    
    # Look for boot time messages
    if echo "$output" | grep -q "start_kernel"; then
        boot_time=$(echo "$output" | grep "start_kernel" | tail -1 | awk '{print $NF}')
        print_info "Kernel start: ${boot_time}s"
    fi
    
    if echo "$output" | grep -q "init:"; then
        local init_time=$(echo "$output" | grep "init:" | tail -1 | awk '{print $NF}')
        print_info "Init start: ${init_time}s"
    fi
    
    if echo "$output" | grep -q "login:"; then
        local login_time=$(echo "$output" | grep "login:" | tail -1 | awk '{print $NF}')
        print_info "Console ready: ${login_time}s"
    fi
}

analyze_ddr_messages() {
    print_step "Analyzing DDR messages"
    
    local output=$(serial_read 60)
    
    echo "$output" | grep -i "DDR" | while read line; do
        print_info "$line"
    done
}

analyze_kernel_panic() {
    print_step "Checking for kernel panic"
    
    local output=$(serial_read 60)
    
    if echo "$output" | grep -i "panic" > /dev/null; then
        print_error "Kernel panic detected!"
        echo "$output" | grep -i "panic" -A 10
        return 1
    fi
    
    print_success "No kernel panic detected"
    return 0
}

analyze_memory_errors() {
    print_step "Checking memory errors"
    
    local output=$(serial_read 60)
    
    if echo "$output" | grep -i "memory error" > /dev/null; then
        print_error "Memory errors detected!"
        echo "$output" | grep -i "memory error" -A 5
        return 1
    fi
    
    print_success "No memory errors detected"
    return 0
}

# ============================================================================
# Show Results
# ============================================================================

show_summary() {
    print_header "Boot Test Summary"
    echo ""
    echo "  Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo "  Tests Failed: ${RED}$TESTS_FAILED${NC}"
    echo "  Total: $((TESTS_PASSED + TESTS_FAILED))"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}${BOLD}✅ All boot tests passed!${NC}"
        return 0
    else
        echo -e "${RED}${BOLD}❌ Some boot tests failed!${NC}"
        return 1
    fi
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 Full Boot Integration Test"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --serial)
                SERIAL_DEVICE="$2"
                shift 2
                ;;
            --baudrate)
                BAUDRATE="$2"
                shift 2
                ;;
            --timeout)
                TIMEOUT="$2"
                shift 2
                ;;
            --attempts)
                BOOT_ATTEMPTS="$2"
                shift 2
                ;;
            --expected)
                EXPECTED_BOOT_MSG="$2"
                shift 2
                ;;
            --fail-fast)
                FAIL_FAST=1
                shift
                ;;
            --no-save-logs)
                SAVE_LOGS=0
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
                show_help
                exit 1
                ;;
        esac
    done
    
    # Create log directory
    if [ $SAVE_LOGS -eq 1 ]; then
        mkdir -p "$LOG_DIR"
        local log_file="$LOG_DIR/boot_test_$(date +%Y%m%d_%H%M%S).log"
        print_info "Log file: $log_file"
        exec > >(tee -a "$log_file")
        exec 2>&1
    fi
    
    # Initialize serial
    if ! serial_init; then
        print_error "Serial initialization failed"
        exit 1
    fi
    
    # Run boot tests
    local boot_passed=0
    for attempt in $(seq 1 $BOOT_ATTEMPTS); do
        if run_boot_test $attempt; then
            boot_passed=1
            break
        fi
        if [ $attempt -lt $BOOT_ATTEMPTS ]; then
            print_warning "Retrying boot test..."
            sleep 10
        fi
    done
    
    if [ $boot_passed -eq 0 ]; then
        print_error "All boot attempts failed"
        exit 1
    fi
    
    # Analyze boot
    analyze_boot_time
    analyze_ddr_messages
    analyze_kernel_panic
    analyze_memory_errors
    
    # Show summary
    show_summary
}

# Run main function
main "$@"
