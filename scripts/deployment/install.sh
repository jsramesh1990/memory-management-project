#!/bin/bash
# install.sh - Installation Script for RK3568 DDR Memory Manager
# Version: 1.0.0
# Author: Sebastian
# Description: This script installs the DDR Memory Manager on the target system

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
INSTALL_DIR="${INSTALL_DIR:-/usr/local}"
CONFIG_DIR="/etc/ddr_manager"
LOG_DIR="/var/log/ddr_manager"
DATA_DIR="/var/lib/ddr_manager"
RUN_DIR="/var/run/ddr_manager"

# Default values
INSTALL_TYPE="full"
INSTALL_KERNEL_MODULES=1
INSTALL_SYSTEMD_SERVICE=1
INSTALL_CONFIG=1
INSTALL_HEADERS=1
INSTALL_TOOLS=1
INSTALL_LIBRARIES=1
CREATE_SYMLINKS=1
BACKUP_EXISTING=1
DRY_RUN=0
VERBOSE=0

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
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
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
${BOLD}RK3568 DDR Memory Manager - Installation Script${NC}

${BOLD}Usage:${NC}
    $0 [OPTIONS]

${BOLD}Options:${NC}
    -t, --type TYPE         Install type (full, minimal, custom) [default: full]
    -d, --dir DIR           Installation directory [default: /usr/local]
    --no-kernel             Skip kernel modules installation
    --no-service            Skip systemd service installation
    --no-config             Skip configuration installation
    --no-headers            Skip header files installation
    --no-tools              Skip tools installation
    --no-libraries          Skip libraries installation
    --no-symlinks           Skip creating symlinks
    --no-backup             Skip backup of existing files
    --dry-run               Perform dry run without installing
    -v, --verbose           Verbose output
    -h, --help              Show this help message

${BOLD}Install Types:${NC}
    full        - Install everything
    minimal     - Install only essential components
    custom      - Install selected components

${BOLD}Examples:${NC}
    $0 --type full
    $0 --type minimal --no-service
    $0 --type custom --dir /opt/ddr --no-kernel

EOF
}

# ============================================================================
# Pre-Installation Checks
# ============================================================================

check_prerequisites() {
    print_step "Checking prerequisites"
    
    # Check if running as root
    if [ "$EUID" -eq 0 ]; then
        print_info "Running as root"
    else
        print_warning "Not running as root. Some operations may fail."
        print_info "Consider running with sudo"
    fi
    
    # Check build directory
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory not found: $BUILD_DIR"
        print_info "Please build the project first: ./scripts/build/build.sh"
        return 1
    fi
    
    # Check required files
    local required_files=(
        "$BUILD_DIR/lib/libddr_manager.so"
        "$BUILD_DIR/bin/ddr_info_tool"
    )
    
    for file in "${required_files[@]}"; do
        if [ ! -f "$file" ]; then
            print_error "Required file not found: $file"
            return 1
        fi
    done
    
    print_success "Prerequisites check passed"
}

# ============================================================================
# Backup Functions
# ============================================================================

backup_existing() {
    if [ $BACKUP_EXISTING -eq 0 ]; then
        return 0
    fi
    
    print_step "Backing up existing files"
    
    local backup_dir="$PROJECT_ROOT/backup_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir"
    
    # Backup libraries
    if [ -f "$INSTALL_DIR/lib/libddr_manager.so" ]; then
        cp "$INSTALL_DIR/lib/libddr_manager.so" "$backup_dir/"
        print_substep "Backed up: libddr_manager.so"
    fi
    
    # Backup binaries
    for tool in ddr_info_tool ddr_monitor memory_benchmark; do
        if [ -f "$INSTALL_DIR/bin/$tool" ]; then
            cp "$INSTALL_DIR/bin/$tool" "$backup_dir/"
            print_substep "Backed up: $tool"
        fi
    done
    
    # Backup configuration
    if [ -d "$CONFIG_DIR" ]; then
        cp -r "$CONFIG_DIR" "$backup_dir/"
        print_substep "Backed up: configuration"
    fi
    
    # Backup systemd service
    if [ -f "/etc/systemd/system/ddr-manager.service" ]; then
        cp "/etc/systemd/system/ddr-manager.service" "$backup_dir/"
        print_substep "Backed up: systemd service"
    fi
    
    print_success "Backup created: $backup_dir"
}

# ============================================================================
# Installation Functions
# ============================================================================

install_directories() {
    print_step "Creating directories"
    
    local dirs=(
        "$INSTALL_DIR/lib"
        "$INSTALL_DIR/bin"
        "$INSTALL_DIR/include/ddr_manager"
        "$INSTALL_DIR/share/ddr_manager"
        "$CONFIG_DIR"
        "$LOG_DIR"
        "$DATA_DIR"
        "$RUN_DIR"
    )
    
    for dir in "${dirs[@]}"; do
        if [ ! -d "$dir" ]; then
            if [ $DRY_RUN -eq 1 ]; then
                print_substep "[DRY RUN] Would create: $dir"
            else
                mkdir -p "$dir"
                print_substep "Created: $dir"
            fi
        fi
    done
    
    # Set permissions
    if [ $DRY_RUN -eq 0 ]; then
        chmod 755 "$INSTALL_DIR/lib" 2>/dev/null || true
        chmod 755 "$INSTALL_DIR/bin" 2>/dev/null || true
        chmod 755 "$CONFIG_DIR" 2>/dev/null || true
        chmod 755 "$LOG_DIR" 2>/dev/null || true
        chmod 755 "$DATA_DIR" 2>/dev/null || true
        chmod 755 "$RUN_DIR" 2>/dev/null || true
    fi
    
    print_success "Directories created"
}

install_libraries() {
    if [ $INSTALL_LIBRARIES -eq 0 ]; then
        return 0
    fi
    
    print_step "Installing libraries"
    
    local lib_dir="$BUILD_DIR/lib"
    
    for lib in "$lib_dir"/*.so*; do
        if [ -f "$lib" ]; then
            local name=$(basename "$lib")
            if [ $DRY_RUN -eq 1 ]; then
                print_substep "[DRY RUN] Would install: $name"
            else
                cp "$lib" "$INSTALL_DIR/lib/"
                chmod 755 "$INSTALL_DIR/lib/$name"
                print_substep "Installed: $name"
            fi
        fi
    done
    
    for lib in "$lib_dir"/*.a; do
        if [ -f "$lib" ]; then
            local name=$(basename "$lib")
            if [ $DRY_RUN -eq 1 ]; then
                print_substep "[DRY RUN] Would install: $name"
            else
                cp "$lib" "$INSTALL_DIR/lib/"
                chmod 644 "$INSTALL_DIR/lib/$name"
                print_substep "Installed: $name"
            fi
        fi
    done
    
    # Update library cache
    if [ $DRY_RUN -eq 0 ]; then
        ldconfig 2>/dev/null || true
    fi
    
    print_success "Libraries installed"
}

install_headers() {
    if [ $INSTALL_HEADERS -eq 0 ]; then
        return 0
    fi
    
    print_step "Installing headers"
    
    local include_dir="$BUILD_DIR/include"
    
    for header in "$include_dir"/*.h; do
        if [ -f "$header" ]; then
            local name=$(basename "$header")
            if [ $DRY_RUN -eq 1 ]; then
                print_substep "[DRY RUN] Would install: $name"
            else
                cp "$header" "$INSTALL_DIR/include/ddr_manager/"
                chmod 644 "$INSTALL_DIR/include/ddr_manager/$name"
                print_substep "Installed: $name"
            fi
        fi
    done
    
    print_success "Headers installed"
}

install_tools() {
    if [ $INSTALL_TOOLS -eq 0 ]; then
        return 0
    fi
    
    print_step "Installing tools"
    
    local bin_dir="$BUILD_DIR/bin"
    
    for tool in "$bin_dir"/*; do
        if [ -f "$tool" ] && [ -x "$tool" ]; then
            local name=$(basename "$tool")
            if [ $DRY_RUN -eq 1 ]; then
                print_substep "[DRY RUN] Would install: $name"
            else
                cp "$tool" "$INSTALL_DIR/bin/"
                chmod 755 "$INSTALL_DIR/bin/$name"
                print_substep "Installed: $name"
            fi
        fi
    done
    
    print_success "Tools installed"
}

install_kernel_modules() {
    if [ $INSTALL_KERNEL_MODULES -eq 0 ]; then
        return 0
    fi
    
    print_step "Installing kernel modules"
    
    local modules_dir="$BUILD_DIR/modules"
    
    if [ ! -d "$modules_dir" ]; then
        print_warning "No kernel modules found to install"
        return 0
    fi
    
    local kernel_version=$(uname -r)
    local target_dir="/lib/modules/$kernel_version/extra/ddr_manager"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would install modules to: $target_dir"
    else
        mkdir -p "$target_dir"
        cp "$modules_dir"/*.ko "$target_dir/" 2>/dev/null || true
        depmod -a 2>/dev/null || true
        print_substep "Modules installed to: $target_dir"
    fi
    
    print_success "Kernel modules installed"
}

install_config() {
    if [ $INSTALL_CONFIG -eq 0 ]; then
        return 0
    fi
    
    print_step "Installing configuration"
    
    local config_src="$PROJECT_ROOT/configs"
    
    # Copy default configuration
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would copy configuration files"
    else
        if [ -f "$config_src/default_config.json" ]; then
            cp "$config_src/default_config.json" "$CONFIG_DIR/"
            chmod 644 "$CONFIG_DIR/default_config.json"
            print_substep "Installed: default_config.json"
        fi
        
        # Copy board configurations
        if [ -d "$config_src/board_defconfigs" ]; then
            mkdir -p "$CONFIG_DIR/boards"
            cp "$config_src/board_defconfigs"/* "$CONFIG_DIR/boards/" 2>/dev/null || true
            print_substep "Installed: board configurations"
        fi
        
        # Copy memory profiles
        if [ -d "$config_src/memory_profiles" ]; then
            mkdir -p "$CONFIG_DIR/profiles"
            cp "$config_src/memory_profiles"/* "$CONFIG_DIR/profiles/" 2>/dev/null || true
            print_substep "Installed: memory profiles"
        fi
    fi
    
    print_success "Configuration installed"
}

install_systemd_service() {
    if [ $INSTALL_SYSTEMD_SERVICE -eq 0 ]; then
        return 0
    fi
    
    print_step "Installing systemd service"
    
    local service_file="$PROJECT_ROOT/scripts/ddr-manager.service"
    
    if [ ! -f "$service_file" ]; then
        print_warning "Systemd service file not found"
        return 0
    fi
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would install systemd service"
    else
        cp "$service_file" "/etc/systemd/system/"
        chmod 644 "/etc/systemd/system/ddr-manager.service"
        systemctl daemon-reload 2>/dev/null || true
        print_substep "Systemd service installed"
    fi
    
    print_success "Systemd service installed"
}

create_symlinks() {
    if [ $CREATE_SYMLINKS -eq 0 ]; then
        return 0
    fi
    
    print_step "Creating symlinks"
    
    local links=(
        "ddr_info_tool:ddr-info"
        "ddr_monitor:ddr-monitor"
        "memory_benchmark:ddr-benchmark"
    )
    
    for link in "${links[@]}"; do
        local src="${link%:*}"
        local dst="${link#*:}"
        
        if [ -f "$INSTALL_DIR/bin/$src" ]; then
            if [ $DRY_RUN -eq 1 ]; then
                print_substep "[DRY RUN] Would create: $dst -> $src"
            else
                ln -sf "$INSTALL_DIR/bin/$src" "$INSTALL_DIR/bin/$dst"
                print_substep "Created: $dst -> $src"
            fi
        fi
    done
    
    print_success "Symlinks created"
}

# ============================================================================
# Post-Installation
# ============================================================================

post_install() {
    print_step "Post-installation tasks"
    
    # Create log file
    if [ $DRY_RUN -eq 0 ]; then
        touch "$LOG_DIR/ddr_manager.log"
        chmod 644 "$LOG_DIR/ddr_manager.log"
    fi
    
    # Set permissions
    if [ $DRY_RUN -eq 0 ]; then
        chown -R root:root "$CONFIG_DIR" 2>/dev/null || true
        chown -R root:root "$LOG_DIR" 2>/dev/null || true
        chown -R root:root "$DATA_DIR" 2>/dev/null || true
    fi
    
    # Enable service if installed
    if [ $INSTALL_SYSTEMD_SERVICE -eq 1 ] && [ $DRY_RUN -eq 0 ]; then
        if systemctl is-enabled ddr-manager.service &>/dev/null; then
            systemctl enable ddr-manager.service 2>/dev/null || true
            print_info "Service enabled (will start on boot)"
            print_info "To start now: systemctl start ddr-manager.service"
        fi
    fi
    
    print_success "Post-installation tasks completed"
}

# ============================================================================
# Verification
# ============================================================================

verify_installation() {
    print_step "Verifying installation"
    
    local failed=0
    
    # Check libraries
    if [ $INSTALL_LIBRARIES -eq 1 ]; then
        if [ -f "$INSTALL_DIR/lib/libddr_manager.so" ]; then
            print_success "Library found: libddr_manager.so"
        else
            print_error "Library not found: libddr_manager.so"
            failed=1
        fi
    fi
    
    # Check tools
    if [ $INSTALL_TOOLS -eq 1 ]; then
        for tool in ddr_info_tool ddr_monitor memory_benchmark; do
            if [ -f "$INSTALL_DIR/bin/$tool" ]; then
                print_success "Tool found: $tool"
            else
                print_warning "Tool not found: $tool"
            fi
        done
    fi
    
    # Check configuration
    if [ $INSTALL_CONFIG -eq 1 ]; then
        if [ -d "$CONFIG_DIR" ]; then
            print_success "Configuration directory found"
        else
            print_warning "Configuration directory not found"
        fi
    fi
    
    # Test library
    if [ $DRY_RUN -eq 0 ]; then
        if [ -f "$INSTALL_DIR/lib/libddr_manager.so" ]; then
            if ldconfig -p | grep -q libddr_manager; then
                print_success "Library registered with ldconfig"
            fi
        fi
    fi
    
    if [ $failed -eq 1 ]; then
        print_error "Installation verification failed"
        return 1
    fi
    
    print_success "Installation verification passed"
    return 0
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 DDR Memory Manager - Installation"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -t|--type)
                INSTALL_TYPE="$2"
                shift 2
                ;;
            -d|--dir)
                INSTALL_DIR="$2"
                shift 2
                ;;
            --no-kernel)
                INSTALL_KERNEL_MODULES=0
                shift
                ;;
            --no-service)
                INSTALL_SYSTEMD_SERVICE=0
                shift
                ;;
            --no-config)
                INSTALL_CONFIG=0
                shift
                ;;
            --no-headers)
                INSTALL_HEADERS=0
                shift
                ;;
            --no-tools)
                INSTALL_TOOLS=0
                shift
                ;;
            --no-libraries)
                INSTALL_LIBRARIES=0
                shift
                ;;
            --no-symlinks)
                CREATE_SYMLINKS=0
                shift
                ;;
            --no-backup)
                BACKUP_EXISTING=0
                shift
                ;;
            --dry-run)
                DRY_RUN=1
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
    
    # Validate install type
    case "$INSTALL_TYPE" in
        full)
            print_info "Install type: Full installation"
            ;;
        minimal)
            print_info "Install type: Minimal installation"
            INSTALL_KERNEL_MODULES=0
            INSTALL_SYSTEMD_SERVICE=0
            INSTALL_HEADERS=0
            CREATE_SYMLINKS=0
            ;;
        custom)
            print_info "Install type: Custom installation"
            ;;
        *)
            print_error "Invalid install type: $INSTALL_TYPE"
            show_help
            exit 1
            ;;
    esac
    
    if [ $DRY_RUN -eq 1 ]; then
        print_warning "DRY RUN: No changes will be made"
    fi
    
    # Run installation
    check_prerequisites || exit 1
    backup_existing
    install_directories
    install_libraries
    install_headers
    install_tools
    install_kernel_modules
    install_config
    install_systemd_service
    create_symlinks
    post_install
    
    # Verify only if not dry run
    if [ $DRY_RUN -eq 0 ]; then
        verify_installation || exit 1
    fi
    
    print_header "Installation Complete"
    echo -e "\n${GREEN}${BOLD}✅ Installation completed successfully!${NC}"
    echo ""
    echo "Installation summary:"
    echo "  Installation directory: $INSTALL_DIR"
    echo "  Configuration: $CONFIG_DIR"
    echo "  Logs: $LOG_DIR"
    echo "  Data: $DATA_DIR"
    echo "  Run: $RUN_DIR"
    
    if [ $INSTALL_SYSTEMD_SERVICE -eq 1 ] && [ $DRY_RUN -eq 0 ]; then
        echo ""
        echo "To start the service:"
        echo "  sudo systemctl start ddr-manager.service"
        echo "  sudo systemctl status ddr-manager.service"
    fi
    
    echo ""
    echo "To test the installation:"
    echo "  ddr_info_tool --info"
    echo "  ddr_info_tool --memory"
}

# Run main function
main "$@"
