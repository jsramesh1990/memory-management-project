#!/bin/bash
# build_in_container.sh - Build RK3568 DDR Memory Manager Inside Docker Container
# Version: 1.0.0
# Author: Sebastian
# Description: This script builds the project inside the Docker container
#              with proper cross-compilation settings

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Default settings
CONTAINER_NAME="rk3568-ddr-dev"
BUILD_TYPE="release"
CLEAN_BUILD=false
INSTALL=false
TEST=false
DOCS=false
VERBOSE=false
JOBS=$(nproc)

# Function to print colored output
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

# Function to show help
show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Build RK3568 DDR Memory Manager inside Docker container.

Options:
    -c, --container NAME    Container name (default: rk3568-ddr-dev)
    -t, --type TYPE         Build type: release/debug (default: release)
    -j, --jobs N           Number of parallel jobs (default: nproc)
    --clean                Clean build before building
    --install              Install after building
    --test                 Run tests after building
    --docs                 Build documentation
    -v, --verbose          Verbose output
    -h, --help             Show this help message

Examples:
    $0                      # Build release version
    $0 --clean --test       # Clean, build, and test
    $0 --type debug -j 4    # Build debug with 4 jobs
    $0 --install            # Build and install
    $0 --docs               # Build documentation
EOF
}

# Function to check if container is running
check_container() {
    if ! docker ps --format '{{.Names}}' | grep -q "^$1$"; then
        print_error "Container '$1' is not running."
        print_info "Please run: ./docker/scripts/enter_container.sh"
        exit 1
    fi
}

# Function to execute command in container
exec_in_container() {
    local cmd="$1"
    docker exec -t "$CONTAINER_NAME" bash -c "$cmd"
}

# Function to clean build
clean_build() {
    print_info "Cleaning build artifacts..."
    
    exec_in_container "cd /workspace && make clean 2>/dev/null || true"
    exec_in_container "rm -rf /workspace/build/* 2>/dev/null || true"
    exec_in_container "rm -rf /workspace/output/* 2>/dev/null || true"
    
    print_success "Clean completed!"
}

# Function to configure build
configure_build() {
    local build_type="$1"
    local verbose="$2"
    
    print_info "Configuring build (type: $build_type)..."
    
    local cmake_args="-DCMAKE_BUILD_TYPE=${build_type}"
    
    if [ "$verbose" = true ]; then
        cmake_args="$cmake_args -DCMAKE_VERBOSE_MAKEFILE=ON"
    fi
    
    exec_in_container "cd /workspace && mkdir -p build && cd build && cmake $cmake_args .."
    
    print_success "Configuration completed!"
}

# Function to build project
build_project() {
    local jobs="$1"
    
    print_info "Building project with $jobs parallel jobs..."
    
    exec_in_container "cd /workspace/build && make -j$jobs"
    
    print_success "Build completed!"
}

# Function to install project
install_project() {
    print_info "Installing project..."
    
    exec_in_container "cd /workspace/build && make install"
    
    print_success "Installation completed!"
}

# Function to run tests
run_tests() {
    print_info "Running tests..."
    
    exec_in_container "cd /workspace/build && ctest --output-on-failure"
    
    print_success "Tests completed!"
}

# Function to build documentation
build_docs() {
    print_info "Building documentation..."
    
    exec_in_container "cd /workspace && doxygen Doxyfile 2>/dev/null || echo 'Doxygen not found, skipping docs'"
    exec_in_container "cd /workspace/docs && make html 2>/dev/null || echo 'Sphinx not found, skipping docs'"
    
    print_success "Documentation built!"
}

# Function to generate build report
generate_report() {
    print_info "Generating build report..."
    
    local report_file="/workspace/build_report.txt"
    
    exec_in_container "cat > $report_file << 'EOF'
=== RK3568 DDR Memory Manager Build Report ===
Build Date: $(date)
Build Type: $BUILD_TYPE
Container: $CONTAINER_NAME
Jobs: $JOBS

=== Build Statistics ===
$(exec_in_container "cd /workspace/build && ls -la")

=== Binary Sizes ===
$(exec_in_container "cd /workspace/build && find . -type f -executable -exec ls -lh {} \; 2>/dev/null | head -20")

=== Library Sizes ===
$(exec_in_container "cd /workspace/build && find . -name '*.so*' -exec ls -lh {} \; 2>/dev/null | head -20")

=== Build Log ===
$(tail -100 /workspace/build.log 2>/dev/null || echo 'No build log found')
EOF"
    
    print_info "Report saved to: $report_file"
}

# Main function
main() {
    print_info "RK3568 DDR Memory Manager - Build in Container"
    print_info "================================================"
    echo ""
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--container)
                CONTAINER_NAME="$2"
                shift 2
                ;;
            -t|--type)
                BUILD_TYPE="$2"
                shift 2
                ;;
            -j|--jobs)
                JOBS="$2"
                shift 2
                ;;
            --clean)
                CLEAN_BUILD=true
                shift
                ;;
            --install)
                INSTALL=true
                shift
                ;;
            --test)
                TEST=true
                shift
                ;;
            --docs)
                DOCS=true
                shift
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                echo "Use -h for help"
                exit 1
                ;;
        esac
    done
    
    # Validate build type
    case $BUILD_TYPE in
        release|debug)
            ;;
        *)
            print_error "Invalid build type: $BUILD_TYPE. Use 'release' or 'debug'"
            exit 1
            ;;
    esac
    
    # Check container
    check_container "$CONTAINER_NAME"
    
    # Prepare build environment
    print_info "Preparing build environment..."
    exec_in_container "mkdir -p /workspace/build /workspace/output"
    
    # Clean if requested
    if [ "$CLEAN_BUILD" = true ]; then
        clean_build
    fi
    
    # Configure
    configure_build "$BUILD_TYPE" "$VERBOSE"
    
    # Build
    build_project "$JOBS"
    
    # Run tests if requested
    if [ "$TEST" = true ]; then
        run_tests
    fi
    
    # Install if requested
    if [ "$INSTALL" = true ]; then
        install_project
    fi
    
    # Build docs if requested
    if [ "$DOCS" = true ]; then
        build_docs
    fi
    
    # Generate report
    generate_report
    
    print_success "Build completed successfully!"
    print_info "Build artifacts are in: $PROJECT_ROOT/build/"
    print_info "Output is in: $PROJECT_ROOT/output/"
}

# Run main function with all arguments
main "$@"
