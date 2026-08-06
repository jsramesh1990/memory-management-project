#!/bin/bash
# test_memory_stress.sh - Memory Stress Test Script
# Version: 1.0.0
# Author: Sebastian
# Description: This script performs comprehensive memory stress testing

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
REPORT_DIR="$PROJECT_ROOT/reports"

# Test parameters
DURATION=3600  # 1 hour default
ITERATIONS=100
MEMORY_SIZE_MB=256
TEST_PATTERN="all"
VERBOSE=0
FAIL_FAST=0
SAVE_LOGS=1
MONITOR_INTERVAL=5

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
        SKIP)
            print_warning "$name: $message"
            ;;
    esac
}

show_help() {
    cat << EOF
${BOLD}RK3568 Memory Stress Test${NC}

${BOLD}Usage:${NC}
    $0 [OPTIONS]

${BOLD}Options:${NC}
    -d, --duration SECS   Test duration in seconds [default: 3600]
    -i, --iterations N    Number of iterations [default: 100]
    -m, --memory MB       Memory size in MB [default: 256]
    -p, --pattern TYPE    Test pattern (all, sequential, random, alternating) [default: all]
    --fail-fast           Stop on first failure
    --no-save-logs        Don't save test logs
    -v, --verbose         Verbose output
    -h, --help            Show this help message

${BOLD}Examples:${NC}
    $0 --duration 1800 --memory 512
    $0 --pattern random --iterations 1000

EOF
}

# ============================================================================
# Memory Stress Functions
# ============================================================================

stress_sequential() {
    local size_mb="$1"
    local iterations="$2"
    
    print_substep "Sequential memory stress (${size_mb}MB, ${iterations} iterations)"
    
    if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
        $BUILD_DIR/bin/memory_benchmark --stress --sequential \
            --size $((size_mb * 1024 * 1024)) \
            --iterations $iterations 2>&1
    elif [ -f "/usr/bin/stress" ]; then
        stress --vm 1 --vm-bytes ${size_mb}M --vm-keep \
               --timeout $((iterations * 10)) 2>&1
    else
        # Simple memory test using dd
        dd if=/dev/zero of=/tmp/memtest bs=1M count=$size_mb 2>&1
        dd if=/tmp/memtest of=/dev/null bs=1M 2>&1
        rm -f /tmp/memtest
    fi
}

stress_random() {
    local size_mb="$1"
    local iterations="$2"
    
    print_substep "Random memory stress (${size_mb}MB, ${iterations} iterations)"
    
    if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
        $BUILD_DIR/bin/memory_benchmark --stress --random \
            --size $((size_mb * 1024 * 1024)) \
            --iterations $iterations 2>&1
    else
        # Simple random test using dd with random data
        dd if=/dev/urandom of=/tmp/randtest bs=1M count=$size_mb 2>&1
        dd if=/tmp/randtest of=/dev/null bs=1M 2>&1
        rm -f /tmp/randtest
    fi
}

stress_alternating() {
    local size_mb="$1"
    local iterations="$2"
    
    print_substep "Alternating memory stress (${size_mb}MB, ${iterations} iterations)"
    
    for i in $(seq 1 $iterations); do
        if [ $((i % 2)) -eq 0 ]; then
            dd if=/dev/zero of=/tmp/alt$i bs=1M count=$size_mb 2>/dev/null
        else
            dd if=/dev/urandom of=/tmp/alt$i bs=1M count=$size_mb 2>/dev/null
        fi
        rm -f /tmp/alt$i
        if [ $VERBOSE -eq 1 ] && [ $((i % 10)) -eq 0 ]; then
            echo -n "."
        fi
    done
    if [ $VERBOSE -eq 1 ]; then
        echo ""
    fi
}

stress_fragmentation() {
    print_substep "Memory fragmentation test"
    
    # Create fragmentation by allocating and freeing different sized blocks
    for i in $(seq 1 100); do
        local size=$(( (RANDOM % 64 + 1) * 1024 ))
        dd if=/dev/zero of=/tmp/frag$i bs=1K count=$size 2>/dev/null
        if [ $((RANDOM % 2)) -eq 0 ]; then
            rm -f /tmp/frag$i
        fi
    done
    rm -f /tmp/frag* 2>/dev/null
}

stress_alloc_free() {
    local iterations="$1"
    
    print_substep "Allocation/free stress (${iterations} iterations)"
    
    for i in $(seq 1 $iterations); do
        local size=$(( (RANDOM % 1024 + 1) * 1024 ))
        dd if=/dev/zero of=/tmp/stress$i bs=1K count=$size 2>/dev/null
        rm -f /tmp/stress$i
        if [ $VERBOSE -eq 1 ] && [ $((i % 100)) -eq 0 ]; then
            echo -n "."
        fi
    done
    if [ $VERBOSE -eq 1 ]; then
        echo ""
    fi
}

stress_concurrent() {
    local size_mb="$1"
    local duration="$2"
    
    print_substep "Concurrent memory stress (${size_mb}MB, ${duration}s)"
    
    # Run multiple stress processes
    local pids=()
    local num_procs=4
    
    for p in $(seq 1 $num_procs); do
        if [ -f "/usr/bin/stress" ]; then
            stress --vm 1 --vm-bytes ${size_mb}M --vm-keep \
                   --timeout $duration &
            pids+=($!)
        else
            # Simple concurrent test
            (
                local end=$((SECONDS + duration))
                while [ $SECONDS -lt $end ]; do
                    dd if=/dev/zero of=/tmp/concurrent_$$ bs=1M count=$size_mb 2>/dev/null
                    rm -f /tmp/concurrent_$$ 2>/dev/null
                    sleep 1
                done
            ) &
            pids+=($!)
        fi
    done
    
    # Wait for all processes
    for pid in "${pids[@]}"; do
        wait $pid 2>/dev/null || true
    done
}

# ============================================================================
# Monitoring Functions
# ============================================================================

monitor_memory() {
    local duration="$1"
    local interval="$2"
    
    print_step "Monitoring memory usage (${duration}s, interval ${interval}s)"
    
    local start_time=$(date +%s)
    local end_time=$((start_time + duration))
    
    echo "Time,Total,Used,Free,Cached,Swap" > "$LOG_DIR/memory_monitor.csv"
    
    while [ $(date +%s) -lt $end_time ]; do
        local timestamp=$(date +%Y-%m-%d_%H:%M:%S)
        local mem_info=$(free -m | awk '/^Mem:/{print $2","$3","$4","$6}')
        local swap_info=$(free -m | awk '/^Swap:/{print $3}')
        
        echo "$timestamp,$mem_info,$swap_info" >> "$LOG_DIR/memory_monitor.csv"
        
        if [ $VERBOSE -eq 1 ]; then
            echo -e "\rMemory: $(free -m | awk '/^Mem:/{printf "%d%%", $3*100/$2}')   " 
        fi
        
        sleep $interval
    done
    
    if [ $VERBOSE -eq 1 ]; then
        echo ""
    fi
}

monitor_cpu() {
    local duration="$1"
    local interval="$2"
    
    print_substep "Monitoring CPU usage"
    
    local start_time=$(date +%s)
    local end_time=$((start_time + duration))
    
    echo "Time,CPU,Load1,Load5,Load15" > "$LOG_DIR/cpu_monitor.csv"
    
    while [ $(date +%s) -lt $end_time ]; do
        local timestamp=$(date +%Y-%m-%d_%H:%M:%S)
        local cpu=$(mpstat 1 1 | awk '/Average:/{print $NF}')
        local load=$(cat /proc/loadavg | awk '{print $1","$2","$3}')
        
        echo "$timestamp,$cpu,$load" >> "$LOG_DIR/cpu_monitor.csv"
        sleep $interval
    done
}

monitor_errors() {
    local duration="$1"
    
    print_substep "Monitoring system errors"
    
    local start_time=$(date +%s)
    local end_time=$((start_time + duration))
    
    while [ $(date +%s) -lt $end_time ]; do
        if dmesg | tail -20 | grep -i -E "error|fail|panic" > /dev/null; then
            local errors=$(dmesg | tail -20 | grep -i -E "error|fail|panic")
            print_warning "Errors detected:\n$errors"
        fi
        sleep 10
    done
}

# ============================================================================
# Main Test Functions
# ============================================================================

run_stress_test() {
    local pattern="$1"
    local size_mb="$2"
    local iterations="$3"
    local duration="$4"
    
    print_header "Memory Stress Test"
    print_info "Pattern: $pattern"
    print_info "Memory: ${size_mb}MB"
    print_info "Iterations: $iterations"
    print_info "Duration: ${duration}s"
    
    case "$pattern" in
        all)
            stress_sequential $size_mb $iterations
            stress_random $size_mb $iterations
            stress_alternating $size_mb $iterations
            stress_fragmentation
            stress_alloc_free $iterations
            stress_concurrent $size_mb $duration
            ;;
        sequential)
            stress_sequential $size_mb $iterations
            ;;
        random)
            stress_random $size_mb $iterations
            ;;
        alternating)
            stress_alternating $size_mb $iterations
            ;;
        fragmentation)
            stress_fragmentation
            ;;
        alloc_free)
            stress_alloc_free $iterations
            ;;
        concurrent)
            stress_concurrent $size_mb $duration
            ;;
        *)
            print_error "Unknown pattern: $pattern"
            return 1
            ;;
    esac
}

verify_stability() {
    print_step "Verifying system stability"
    
    # Check memory usage
    local mem_usage=$(free | awk '/^Mem:/{printf "%.0f", $3/$2 * 100}')
    if [ $mem_usage -gt 95 ]; then
        record_result "Memory Usage" "FAIL" "Memory usage too high: ${mem_usage}%"
        return 1
    else
        record_result "Memory Usage" "PASS" "Memory usage: ${mem_usage}%"
    fi
    
    # Check CPU usage
    local cpu_usage=$(mpstat 1 1 | awk '/Average:/{printf "%.0f", 100 - $NF}')
    if [ $cpu_usage -gt 90 ]; then
        record_result "CPU Usage" "FAIL" "CPU usage too high: ${cpu_usage}%"
    else
        record_result "CPU Usage" "PASS" "CPU usage: ${cpu_usage}%"
    fi
    
    # Check for errors in dmesg
    local errors=$(dmesg | grep -i "error" | wc -l)
    if [ $errors -gt 0 ]; then
        record_result "System Errors" "FAIL" "${errors} errors detected"
        return 1
    else
        record_result "System Errors" "PASS" "No errors detected"
    fi
    
    return 0
}

# ============================================================================
# Show Results
# ============================================================================

show_summary() {
    print_header "Memory Stress Test Summary"
    echo ""
    echo "  Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo "  Tests Failed: ${RED}$TESTS_FAILED${NC}"
    echo "  Total: $((TESTS_PASSED + TESTS_FAILED))"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}${BOLD}✅ All memory stress tests passed!${NC}"
        return 0
    else
        echo -e "${RED}${BOLD}❌ Some memory stress tests failed!${NC}"
        return 1
    fi
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 Memory Stress Test"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -d|--duration)
                DURATION="$2"
                shift 2
                ;;
            -i|--iterations)
                ITERATIONS="$2"
                shift 2
                ;;
            -m|--memory)
                MEMORY_SIZE_MB="$2"
                shift 2
                ;;
            -p|--pattern)
                TEST_PATTERN="$2"
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
        mkdir -p "$REPORT_DIR"
        local log_file="$LOG_DIR/memory_stress_$(date +%Y%m%d_%H%M%S).log"
        print_info "Log file: $log_file"
        exec > >(tee -a "$log_file")
        exec 2>&1
    fi
    
    # Start monitoring
    monitor_memory $DURATION $MONITOR_INTERVAL &
    local monitor_pid=$!
    
    monitor_cpu $DURATION $MONITOR_INTERVAL &
    local cpu_pid=$!
    
    monitor_errors $DURATION &
    local error_pid=$!
    
    # Run stress test
    local start_time=$(date +%s)
    run_stress_test "$TEST_PATTERN" "$MEMORY_SIZE_MB" "$ITERATIONS" "$DURATION"
    local end_time=$(date +%s)
    
    # Stop monitoring
    kill $monitor_pid 2>/dev/null || true
    kill $cpu_pid 2>/dev/null || true
    kill $error_pid 2>/dev/null || true
    
    # Verify stability
    verify_stability
    
    # Generate report
    local report_file="$REPORT_DIR/memory_stress_$(date +%Y%m%d_%H%M%S).txt"
    cat > "$report_file" << EOF
Memory Stress Test Report
=========================
Date: $(date)
Duration: $((end_time - start_time))s
Memory Size: ${MEMORY_SIZE_MB}MB
Pattern: $TEST_PATTERN
Iterations: $ITERATIONS

Results:
- Tests Passed: $TESTS_PASSED
- Tests Failed: $TESTS_FAILED
- Total: $((TESTS_PASSED + TESTS_FAILED))

Detailed Results:
EOF
    
    for result in "${TEST_RESULTS[@]}"; do
        echo "$result" >> "$report_file"
    done
    
    print_info "Report saved: $report_file"
    
    # Show summary
    show_summary
}

# Run main function
main "$@"
