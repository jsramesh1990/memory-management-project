#!/bin/bash
# run_benchmarks.sh - Benchmark Runner Script
# Version: 1.0.0
# Author: Sebastian
# Description: This script runs performance benchmarks for DDR memory

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
REPORT_DIR="$PROJECT_ROOT/reports"

# Benchmark parameters
BENCHMARK_TYPE="all"
ITERATIONS=10
WARMUP=5
DURATION=10
SIZE_FACTOR="normal"
OUTPUT_FORMAT="text"
COMPARE_WITH=""
VERBOSE=0
SAVE_RESULTS=1

# Benchmark results
declare -A BENCH_RESULTS
BENCH_NAMES=()
BENCH_VALUES=()

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
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "\n${MAGENTA}▶${NC} ${BOLD}$1${NC}"
}

print_substep() {
    echo -e "  ${CYAN}›${NC} $1"
}

show_help() {
    cat << EOF
${BOLD}RK3568 DDR Benchmark Runner${NC}

${BOLD}Usage:${NC}
    $0 [OPTIONS]

${BOLD}Options:${NC}
    -t, --type TYPE      Benchmark type (all, bandwidth, latency, speed) [default: all]
    -i, --iterations N   Number of iterations [default: 10]
    --warmup N           Warmup iterations [default: 5]
    --duration N         Duration in seconds [default: 10]
    --size SIZE          Size factor (small, normal, large) [default: normal]
    -o, --output FORMAT  Output format (text, json, csv, html) [default: text]
    --compare FILE       Compare with previous results
    --save-results       Save benchmark results
    --no-save-results    Don't save benchmark results
    -v, --verbose        Verbose output
    -h, --help           Show this help message

${BOLD}Size Factors:${NC}
    small    - Small size (64KB - 1MB)
    normal   - Normal size (1MB - 64MB)
    large    - Large size (64MB - 1GB)

${BOLD}Examples:${NC}
    $0 --type all --iterations 50
    $0 --type bandwidth --size large
    $0 --type latency --output json --save-results

EOF
}

# ============================================================================
# Benchmark Functions
# ============================================================================

run_bandwidth_benchmark() {
    print_step "Bandwidth Benchmark"
    
    local sizes=()
    case "$SIZE_FACTOR" in
        small)
            sizes=(64 256 1024 4096 16384 65536 262144 1048576)
            ;;
        normal)
            sizes=(1048576 4194304 16777216 67108864 268435456)
            ;;
        large)
            sizes=(67108864 268435456 536870912 1073741824)
            ;;
    esac
    
    print_info "Testing bandwidth with size factor: $SIZE_FACTOR"
    
    for size in "${sizes[@]}"; do
        local size_mb=$((size / 1024 / 1024))
        print_substep "Testing size: ${size_mb}MB"
        
        if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
            local result=$($BUILD_DIR/bin/memory_benchmark --bandwidth --size $size --iterations $ITERATIONS --warmup $WARMUP 2>/dev/null)
            if [ -n "$result" ]; then
                local bandwidth=$(echo "$result" | grep -o "[0-9.]*" | head -1)
                BENCH_NAMES+=("Bandwidth_${size_mb}MB")
                BENCH_VALUES+=("$bandwidth")
                BENCH_RESULTS["Bandwidth_${size_mb}MB"]="$bandwidth MB/s"
                print_success "Bandwidth: ${bandwidth} MB/s"
            else
                print_error "Failed to run bandwidth benchmark"
            fi
        else
            print_error "memory_benchmark not found"
            return 1
        fi
    done
}

run_latency_benchmark() {
    print_step "Latency Benchmark"
    
    local sizes=(1024 4096 16384 65536 262144 1048576)
    
    print_info "Testing latency with various sizes"
    
    for size in "${sizes[@]}"; do
        local size_kb=$((size / 1024))
        print_substep "Testing size: ${size_kb}KB"
        
        if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
            local result=$($BUILD_DIR/bin/memory_benchmark --latency --size $size --iterations $ITERATIONS 2>/dev/null)
            if [ -n "$result" ]; then
                local latency=$(echo "$result" | grep -o "[0-9.]*" | head -1)
                BENCH_NAMES+=("Latency_${size_kb}KB")
                BENCH_VALUES+=("$latency")
                BENCH_RESULTS["Latency_${size_kb}KB"]="${latency} ns"
                print_success "Latency: ${latency} ns"
            else
                print_error "Failed to run latency benchmark"
            fi
        else
            print_error "memory_benchmark not found"
            return 1
        fi
    done
}

run_speed_benchmark() {
    print_step "Speed Benchmark"
    
    local operations=("read" "write" "copy" "fill")
    
    for op in "${operations[@]}"; do
        print_substep "Testing ${op} speed"
        
        if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
            local result=$($BUILD_DIR/bin/memory_benchmark --speed --operation $op --iterations $ITERATIONS 2>/dev/null)
            if [ -n "$result" ]; then
                local speed=$(echo "$result" | grep -o "[0-9.]*" | head -1)
                BENCH_NAMES+=("Speed_${op}")
                BENCH_VALUES+=("$speed")
                BENCH_RESULTS["Speed_${op}"]="${speed} MB/s"
                print_success "${op} speed: ${speed} MB/s"
            else
                print_error "Failed to run speed benchmark"
            fi
        else
            print_error "memory_benchmark not found"
            return 1
        fi
    done
}

run_memory_benchmark() {
    print_step "Memory Benchmark"
    
    # Test different memory operations
    local operations=("malloc" "calloc" "realloc" "free")
    
    for op in "${operations[@]}"; do
        print_substep "Testing ${op} performance"
        
        if [ -f "$BUILD_DIR/bin/memory_benchmark" ]; then
            local result=$($BUILD_DIR/bin/memory_benchmark --mem --operation $op --iterations $ITERATIONS 2>/dev/null)
            if [ -n "$result" ]; then
                local time=$(echo "$result" | grep -o "[0-9.]*" | head -1)
                BENCH_NAMES+=("Mem_${op}")
                BENCH_VALUES+=("$time")
                BENCH_RESULTS["Mem_${op}"]="${time} us"
                print_success "${op}: ${time} us"
            else
                print_error "Failed to run memory benchmark"
            fi
        else
            print_error "memory_benchmark not found"
            return 1
        fi
    done
}

run_npu_benchmark() {
    print_step "NPU Benchmark"
    
    if [ ! -f "$BUILD_DIR/bin/npu_benchmark" ]; then
        print_warning "npu_benchmark not found, skipping"
        return 0
    fi
    
    # Test inference speed
    print_substep "Testing NPU inference speed"
    
    local result=$($BUILD_DIR/bin/npu_benchmark --inference --iterations $ITERATIONS 2>/dev/null)
    if [ -n "$result" ]; then
        local fps=$(echo "$result" | grep -o "[0-9.]*" | head -1)
        BENCH_NAMES+=("NPU_Inference")
        BENCH_VALUES+=("$fps")
        BENCH_RESULTS["NPU_Inference"]="${fps} FPS"
        print_success "NPU Inference: ${fps} FPS"
    else
        print_error "Failed to run NPU benchmark"
    fi
    
    # Test memory bandwidth
    print_substep "Testing NPU memory bandwidth"
    
    result=$($BUILD_DIR/bin/npu_benchmark --memory --iterations $ITERATIONS 2>/dev/null)
    if [ -n "$result" ]; then
        local bandwidth=$(echo "$result" | grep -o "[0-9.]*" | head -1)
        BENCH_NAMES+=("NPU_Bandwidth")
        BENCH_VALUES+=("$bandwidth")
        BENCH_RESULTS["NPU_Bandwidth"]="${bandwidth} MB/s"
        print_success "NPU Bandwidth: ${bandwidth} MB/s"
    else
        print_error "Failed to run NPU memory benchmark"
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
    
    print_header "Benchmark Results"
    echo ""
    printf "%-30s %20s\n" "Benchmark" "Result"
    printf "%-30s %20s\n" "---------" "------"
    
    for i in "${!BENCH_NAMES[@]}"; do
        printf "%-30s %20s\n" "${BENCH_NAMES[$i]}" "${BENCH_RESULTS[${BENCH_NAMES[$i]}]}"
    done
}

output_csv() {
    local output_file="$1"
    
    echo "Benchmark,Result" > "$output_file"
    for i in "${!BENCH_NAMES[@]}"; do
        echo "${BENCH_NAMES[$i]},${BENCH_RESULTS[${BENCH_NAMES[$i]}]}" >> "$output_file"
    done
    print_info "Results saved to: $output_file"
}

output_json() {
    local output_file="$1"
    
    echo "{" > "$output_file"
    echo "  \"timestamp\": \"$(date -Iseconds)\"," >> "$output_file"
    echo "  \"benchmarks\": {" >> "$output_file"
    
    local first=1
    for i in "${!BENCH_NAMES[@]}"; do
        if [ $first -eq 1 ]; then
            first=0
        else
            echo "," >> "$output_file"
        fi
        echo "    \"${BENCH_NAMES[$i]}\": \"${BENCH_RESULTS[${BENCH_NAMES[$i]}]}\"" >> "$output_file"
    done
    
    echo "  }" >> "$output_file"
    echo "}" >> "$output_file"
    print_info "Results saved to: $output_file"
}

output_html() {
    local output_file="$1"
    
    cat > "$output_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>DDR Benchmark Results</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        table { border-collapse: collapse; width: 100%; max-width: 600px; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background-color: #4CAF50; color: white; }
        tr:hover { background-color: #f5f5f5; }
        .header { background: #2c3e50; color: white; padding: 20px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>DDR Benchmark Results</h1>
        <p>Generated: $(date)</p>
    </div>
    <table>
        <tr>
            <th>Benchmark</th>
            <th>Result</th>
        </tr>
EOF
    
    for i in "${!BENCH_NAMES[@]}"; do
        echo "        <tr>" >> "$output_file"
        echo "            <td>${BENCH_NAMES[$i]}</td>" >> "$output_file"
        echo "            <td>${BENCH_RESULTS[${BENCH_NAMES[$i]}]}</td>" >> "$output_file"
        echo "        </tr>" >> "$output_file"
    done
    
    cat >> "$output_file" << EOF
    </table>
</body>
</html>
EOF
    
    print_info "Results saved to: $output_file"
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 DDR Memory Benchmark"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -t|--type)
                BENCHMARK_TYPE="$2"
                shift 2
                ;;
            -i|--iterations)
                ITERATIONS="$2"
                shift 2
                ;;
            --warmup)
                WARMUP="$2"
                shift 2
                ;;
            --duration)
                DURATION="$2"
                shift 2
                ;;
            --size)
                SIZE_FACTOR="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_FORMAT="$2"
                shift 2
                ;;
            --compare)
                COMPARE_WITH="$2"
                shift 2
                ;;
            --save-results)
                SAVE_RESULTS=1
                shift
                ;;
            --no-save-results)
                SAVE_RESULTS=0
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
    
    # Create report directory
    if [ $SAVE_RESULTS -eq 1 ]; then
        mkdir -p "$REPORT_DIR"
        local report_file="$REPORT_DIR/benchmark_$(date +%Y%m%d_%H%M%S)"
    fi
    
    # Run benchmarks
    case "$BENCHMARK_TYPE" in
        all)
            run_bandwidth_benchmark
            run_latency_benchmark
            run_speed_benchmark
            run_memory_benchmark
            run_npu_benchmark
            ;;
        bandwidth)
            run_bandwidth_benchmark
            ;;
        latency)
            run_latency_benchmark
            ;;
        speed)
            run_speed_benchmark
            ;;
        memory)
            run_memory_benchmark
            ;;
        npu)
            run_npu_benchmark
            ;;
        *)
            print_error "Invalid benchmark type: $BENCHMARK_TYPE"
            exit 1
            ;;
    esac
    
    # Output results
    case "$OUTPUT_FORMAT" in
        text)
            output_text "${SAVE_RESULTS:+$report_file.txt}"
            ;;
        csv)
            output_csv "${SAVE_RESULTS:-$report_file.csv}"
            ;;
        json)
            output_json "${SAVE_RESULTS:-$report_file.json}"
            ;;
        html)
            output_html "${SAVE_RESULTS:-$report_file.html}"
            ;;
        *)
            print_error "Invalid output format: $OUTPUT_FORMAT"
            exit 1
            ;;
    esac
    
    print_header "Benchmark Complete"
    echo -e "\n${GREEN}${BOLD}✅ Benchmark completed successfully!${NC}"
}

# Run main function
main "$@"
