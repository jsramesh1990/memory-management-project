#!/bin/bash
# clean.sh - Clean Script for RK3568 DDR Memory Manager
# Version: 1.0.0
# Author: Sebastian
# Description: This script cleans all build artifacts and temporary files

set -e

# ============================================================================
# Colors
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ============================================================================
# Configuration
# ============================================================================

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
LOG_DIR="$PROJECT_ROOT/logs"
KERNEL_DIR="$PROJECT_ROOT/kernel"
OUTPUT_DIR="$PROJECT_ROOT/output"
CACHE_DIR="$PROJECT_ROOT/.cache"

# ============================================================================
# Helper Functions
# ============================================================================

print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    cat << EOF
Clean Script for RK3568 DDR Memory Manager

Usage:
    $0 [OPTIONS]

Options:
    --all       Clean everything (including dependencies)
    --build     Clean build directory only
    --logs      Clean logs directory
    --kernel    Clean kernel modules
    --output    Clean output directory
    --cache     Clean cache directory
    --dist      Clean distribution files
    -h, --help  Show this help message

Examples:
    $0              Clean build directory only (default)
    $0 --all        Clean everything
    $0 --build --logs  Clean build and logs

EOF
}

# ============================================================================
# Clean Functions
# ============================================================================

clean_build() {
    print_header "Cleaning Build Directory"
    
    if [ -d "$BUILD_DIR" ]; then
        print_info "Removing: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_info "Build directory not found: $BUILD_DIR"
    fi
}

clean_logs() {
    print_header "Cleaning Logs"
    
    if [ -d "$LOG_DIR" ]; then
        print_info "Removing: $LOG_DIR"
        rm -rf "$LOG_DIR"
        print_success "Logs cleaned"
    else
        print_info "Log directory not found: $LOG_DIR"
    fi
}

clean_kernel() {
    print_header "Cleaning Kernel Modules"
    
    if [ -d "$KERNEL_DIR" ]; then
        print_info "Cleaning kernel modules..."
        make -C "$KERNEL_DIR" clean 2>/dev/null || true
        
        # Remove .ko files
        find "$KERNEL_DIR" -name "*.ko" -type f -delete 2>/dev/null || true
        find "$KERNEL_DIR" -name "*.o" -type f -delete 2>/dev/null || true
        find "$KERNEL_DIR" -name "*.cmd" -type f -delete 2>/dev/null || true
        find "$KERNEL_DIR" -name "*.mod" -type f -delete 2>/dev/null || true
        find "$KERNEL_DIR" -name "*.mod.c" -type f -delete 2>/dev/null || true
        
        print_success "Kernel modules cleaned"
    else
        print_info "Kernel directory not found: $KERNEL_DIR"
    fi
}

clean_output() {
    print_header "Cleaning Output Directory"
    
    if [ -d "$OUTPUT_DIR" ]; then
        print_info "Removing: $OUTPUT_DIR"
        rm -rf "$OUTPUT_DIR"
        print_success "Output directory cleaned"
    else
        print_info "Output directory not found: $OUTPUT_DIR"
    fi
}

clean_cache() {
    print_header "Cleaning Cache"
    
    if [ -d "$CACHE_DIR" ]; then
        print_info "Removing: $CACHE_DIR"
        rm -rf "$CACHE_DIR"
        print_success "Cache cleaned"
    else
        print_info "Cache directory not found: $CACHE_DIR"
    fi
}

clean_dist() {
    print_header "Cleaning Distribution Files"
    
    # Remove package files
    find "$PROJECT_ROOT" -name "*.deb" -type f -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -name "*.rpm" -type f -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -name "*.tar.gz" -type f -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -name "*.tar.bz2" -type f -delete 2>/dev/null || true
    
    # Remove temporary files
    find "$PROJECT_ROOT" -name "*~" -type f -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -name ".#*" -type f -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -name "*.swp" -type f -delete 2>/dev/null || true
    
    print_success "Distribution files cleaned"
}

clean_all() {
    print_header "Cleaning Everything"
    
    clean_build
    clean_logs
    clean_kernel
    clean_output
    clean_cache
    clean_dist
    
    print_success "Everything cleaned"
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 DDR Memory Manager - Clean Script"
    
    # If no arguments, clean build only
    if [ $# -eq 0 ]; then
        clean_build
        exit 0
    fi
    
    # Parse arguments
    local clean_all_flag=0
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --all)
                clean_all_flag=1
                shift
                ;;
            --build)
                clean_build
                shift
                ;;
            --logs)
                clean_logs
                shift
                ;;
            --kernel)
                clean_kernel
                shift
                ;;
            --output)
                clean_output
                shift
                ;;
            --cache)
                clean_cache
                shift
                ;;
            --dist)
                clean_dist
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
    
    if [ $clean_all_flag -eq 1 ]; then
        clean_all
    fi
    
    print_success "Clean completed"
}

main "$@"
