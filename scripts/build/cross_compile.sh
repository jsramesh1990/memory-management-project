#!/bin/bash
# cross_compile.sh - Cross-Compilation Setup for RK3568
# Version: 1.0.0
# Author: Sebastian
# Description: This script sets up cross-compilation environment for RK3568

set -e

# ============================================================================
# Colors
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Default cross-compiler
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
SYSROOT="${SYSROOT:-}"
TARGET_ARCH="${TARGET_ARCH:-arm64}"
KERNEL_DIR="${KERNEL_DIR:-/lib/modules/$(uname -r)/build}"

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

print_step() {
    echo -e "\n${CYAN}▶${NC} $1"
}

show_help() {
    cat << EOF
Cross-Compilation Setup for RK3568 DDR Memory Manager

Usage:
    $0 [OPTIONS]

Options:
    -c, --cross COMPILER   Cross-compiler prefix [default: aarch64-linux-gnu-]
    -s, --sysroot DIR      Sysroot directory
    -a, --arch ARCH        Target architecture (arm64, armhf) [default: arm64]
    -k, --kernel DIR       Kernel build directory
    --install              Install cross-compiler tools
    --setup                Setup environment variables
    --test                 Test cross-compilation
    -v, --verbose          Verbose output
    -h, --help             Show this help message

Examples:
    $0 --setup                    # Setup environment variables
    $0 --install                  # Install cross-compiler
    $0 --test                     # Test cross-compilation
    $0 --cross arm-linux-gnueabihf- --arch armhf

EOF
}

# ============================================================================
# Cross-Compiler Installation
# ============================================================================

install_cross_compiler() {
    print_header "Installing Cross-Compiler"
    
    # Detect distribution
    if [ -f /etc/debian_version ]; then
        print_info "Detected Debian/Ubuntu system"
        
        sudo apt-get update
        sudo apt-get install -y \
            gcc-aarch64-linux-gnu \
            g++-aarch64-linux-gnu \
            binutils-aarch64-linux-gnu \
            libc6-dev-arm64-cross \
            libstdc++-dev-arm64-cross \
            gcc-arm-linux-gnueabihf \
            g++-arm-linux-gnueabihf \
            binutils-arm-linux-gnueabihf \
            libc6-dev-armhf-cross \
            libstdc++-dev-armhf-cross
        
        print_success "Cross-compiler installed"
        
    elif [ -f /etc/redhat-release ]; then
        print_info "Detected RedHat/Fedora system"
        
        sudo dnf install -y \
            gcc-aarch64-linux-gnu \
            binutils-aarch64-linux-gnu \
            glibc-aarch64-linux-gnu \
            gcc-arm-linux-gnu \
            binutils-arm-linux-gnu \
            glibc-arm-linux-gnu
        
        print_success "Cross-compiler installed"
        
    else
        print_error "Unsupported distribution"
        return 1
    fi
}

# ============================================================================
# Environment Setup
# ============================================================================

setup_environment() {
    print_header "Setting Up Cross-Compilation Environment"
    
    cat > "$PROJECT_ROOT/cross_env.sh" << 'EOF'
#!/bin/bash
# Cross-compilation environment for RK3568 DDR Memory Manager

export CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
export TARGET_ARCH="${TARGET_ARCH:-arm64}"

export CC="${CROSS_COMPILE}gcc"
export CXX="${CROSS_COMPILE}g++"
export AR="${CROSS_COMPILE}ar"
export LD="${CROSS_COMPILE}ld"
export STRIP="${CROSS_COMPILE}strip"
export OBJCOPY="${CROSS_COMPILE}objcopy"
export OBJDUMP="${CROSS_COMPILE}objdump"
export RANLIB="${CROSS_COMPILE}ranlib"
export NM="${CROSS_COMPILE}nm"
export SIZE="${CROSS_COMPILE}size"

export CFLAGS="-O2 -Wall -Wextra"
export CXXFLAGS="-O2 -Wall -Wextra"
export LDFLAGS="-Wl,-rpath-link=/usr/lib/${CROSS_COMPILE}"

export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:/usr/lib/${CROSS_COMPILE}pkgconfig"

echo "Cross-compilation environment configured"
echo "CROSS_COMPILE: $CROSS_COMPILE"
echo "TARGET_ARCH: $TARGET_ARCH"
echo "CC: $CC"
echo "CFLAGS: $CFLAGS"
EOF
    
    chmod +x "$PROJECT_ROOT/cross_env.sh"
    print_success "Environment file created: $PROJECT_ROOT/cross_env.sh"
    print_info "To use, run: source $PROJECT_ROOT/cross_env.sh"
}

# ============================================================================
# Test Cross-Compilation
# ============================================================================

test_cross_compilation() {
    print_header "Testing Cross-Compilation"
    
    # Create test program
    cat > /tmp/test.c << 'EOF'
#include <stdio.h>

int main() {
    printf("Cross-compilation test successful!\n");
    printf("Compiler: %s\n", __VERSION__);
    return 0;
}
EOF
    
    # Compile test program
    print_step "Compiling test program"
    
    if ${CC} -v /tmp/test.c -o /tmp/test 2>&1 | grep -q "arm"; then
        print_success "Cross-compilation working"
    else
        print_error "Cross-compilation test failed"
        return 1
    fi
    
    # Check binary
    print_step "Checking binary"
    file /tmp/test
    
    rm -f /tmp/test.c /tmp/test
    print_success "Test completed"
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 DDR Memory Manager - Cross-Compilation Setup"
    
    local install=0
    local setup=0
    local test=0
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--cross)
                CROSS_COMPILE="$2"
                shift 2
                ;;
            -s|--sysroot)
                SYSROOT="$2"
                shift 2
                ;;
            -a|--arch)
                TARGET_ARCH="$2"
                shift 2
                ;;
            -k|--kernel)
                KERNEL_DIR="$2"
                shift 2
                ;;
            --install)
                install=1
                shift
                ;;
            --setup)
                setup=1
                shift
                ;;
            --test)
                test=1
                shift
                ;;
            -v|--verbose)
                set -x
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
    
    # Install cross-compiler
    if [ $install -eq 1 ]; then
        install_cross_compiler
    fi
    
    # Setup environment
    if [ $setup -eq 1 ]; then
        setup_environment
    fi
    
    # Test cross-compilation
    if [ $test -eq 1 ]; then
        # Source environment if available
        if [ -f "$PROJECT_ROOT/cross_env.sh" ]; then
            source "$PROJECT_ROOT/cross_env.sh"
        fi
        test_cross_compilation
    fi
    
    # If no action, show help
    if [ $install -eq 0 ] && [ $setup -eq 0 ] && [ $test -eq 0 ]; then
        show_help
        exit 0
    fi
    
    print_success "Cross-compilation setup completed"
}

main "$@"
