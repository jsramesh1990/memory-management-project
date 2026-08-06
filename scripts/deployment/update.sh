#!/bin/bash
# update.sh - Update Script for RK3568 DDR Memory Manager
# Version: 1.0.0
# Author: Sebastian
# Description: This script updates the DDR Memory Manager to the latest version

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
INSTALL_DIR="${INSTALL_DIR:-/usr/local}"

# Update options
UPDATE_TYPE="full"
SOURCE="local"
VERSION="latest"
BACKUP=1
CONFIRM=1
DRY_RUN=0
VERBOSE=0

# Update sources
LOCAL_PATH="$PROJECT_ROOT"
GIT_REPO="https://github.com/yourusername/RK3568-DDR-Memory-Manager.git"
RELEASE_URL="https://github.com/yourusername/RK3568-DDR-Memory-Manager/releases"

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
${BOLD}RK3568 DDR Memory Manager - Update Script${NC}

${BOLD}Usage:${NC}
    $0 [OPTIONS]

${BOLD}Options:${NC}
    -t, --type TYPE         Update type (full, kernel, tools, config) [default: full]
    -s, --source SOURCE     Update source (local, git, release) [default: local]
    -v, --version VER       Version to update to [default: latest]
    --no-backup             Skip backup
    --no-confirm            Skip confirmation
    --dry-run               Perform dry run without updating
    --verbose               Verbose output
    -h, --help              Show this help message

${BOLD}Examples:${NC}
    $0 --type full
    $0 --type kernel
    $0 --source git --version v1.0.0

EOF
}

# ============================================================================
# Version Functions
# ============================================================================

get_current_version() {
    if [ -f "$INSTALL_DIR/share/ddr_manager/version.txt" ]; then
        cat "$INSTALL_DIR/share/ddr_manager/version.txt"
    else
        echo "0.0.0"
    fi
}

get_latest_version() {
    if [ "$SOURCE" = "git" ]; then
        git ls-remote --tags "$GIT_REPO" | grep -o 'v[0-9]*\.[0-9]*\.[0-9]*' | sort -V | tail -1
    elif [ "$SOURCE" = "release" ]; then
        curl -s "$RELEASE_URL" | grep -o 'v[0-9]*\.[0-9]*\.[0-9]*' | sort -V | tail -1
    else
        echo "1.0.0"
    fi
}

check_version() {
    local current=$(get_current_version)
    local latest=$(get_latest_version)
    
    print_info "Current version: $current"
    print_info "Latest version: $latest"
    
    if [ "$current" = "$latest" ]; then
        print_warning "Already at latest version"
        return 1
    fi
    
    return 0
}

# ============================================================================
# Backup Functions
# ============================================================================

backup_current() {
    if [ $BACKUP -eq 0 ]; then
        return 0
    fi
    
    print_step "Backing up current installation"
    
    local backup_dir="$PROJECT_ROOT/backup_update_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir"
    
    # Backup directories
    for dir in lib bin include etc share; do
        if [ -d "$INSTALL_DIR/$dir" ]; then
            if [ $DRY_RUN -eq 1 ]; then
                print_substep "[DRY RUN] Would backup: $INSTALL_DIR/$dir"
            else
                cp -r "$INSTALL_DIR/$dir" "$backup_dir/"
                print_substep "Backed up: $dir"
            fi
        fi
    done
    
    # Backup config
    if [ -d "/etc/ddr_manager" ]; then
        if [ $DRY_RUN -eq 1 ]; then
            print_substep "[DRY RUN] Would backup: /etc/ddr_manager"
        else
            cp -r "/etc/ddr_manager" "$backup_dir/"
            print_substep "Backed up: /etc/ddr_manager"
        fi
    fi
    
    print_success "Backup created: $backup_dir"
}

# ============================================================================
# Update Sources
# ============================================================================

update_from_local() {
    print_step "Updating from local source"
    
    if [ ! -d "$LOCAL_PATH" ]; then
        print_error "Local path not found: $LOCAL_PATH"
        return 1
    fi
    
    # Check if build exists
    if [ ! -d "$LOCAL_PATH/build" ]; then
        print_info "Building project..."
        "$LOCAL_PATH/scripts/build/build.sh" --type release
    fi
    
    # Install updated version
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would install from $LOCAL_PATH"
    else
        "$LOCAL_PATH/scripts/deployment/install.sh" --type full
    fi
    
    print_success "Update from local source completed"
}

update_from_git() {
    print_step "Updating from Git repository"
    
    # Check if git is installed
    if ! command -v git &>/dev/null; then
        print_error "Git not found"
        return 1
    fi
    
    # Check if project is a git repository
    if [ -d "$PROJECT_ROOT/.git" ]; then
        # Update existing repository
        print_substep "Updating existing repository..."
        
        if [ $DRY_RUN -eq 1 ]; then
            print_substep "[DRY RUN] Would git pull"
        else
            cd "$PROJECT_ROOT"
            git fetch --tags
            if [ "$VERSION" != "latest" ]; then
                git checkout "$VERSION"
            else
                git pull origin main
            fi
            cd - > /dev/null
        fi
    else
        # Clone repository
        print_substep "Cloning repository..."
        
        if [ $DRY_RUN -eq 1 ]; then
            print_substep "[DRY RUN] Would clone $GIT_REPO"
        else
            local temp_dir="$PROJECT_ROOT.tmp"
            git clone "$GIT_REPO" "$temp_dir"
            
            if [ "$VERSION" != "latest" ]; then
                cd "$temp_dir"
                git checkout "$VERSION"
                cd - > /dev/null
            fi
            
            # Replace project with new version
            rm -rf "$PROJECT_ROOT"
            mv "$temp_dir" "$PROJECT_ROOT"
        fi
    fi
    
    # Build and install
    if [ $DRY_RUN -eq 0 ]; then
        cd "$PROJECT_ROOT"
        ./scripts/build/build.sh --type release
        ./scripts/deployment/install.sh --type full
        cd - > /dev/null
    fi
    
    print_success "Update from Git completed"
}

update_from_release() {
    print_step "Updating from release"
    
    # Get release URL
    local release_url="$RELEASE_URL/download/$VERSION/rk3568-ddr-manager-$VERSION.tar.gz"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would download: $release_url"
    else
        # Download release
        print_substep "Downloading release..."
        wget -q "$release_url" -O "/tmp/ddr-manager-release.tar.gz"
        
        # Extract
        print_substep "Extracting..."
        tar -xzf "/tmp/ddr-manager-release.tar.gz" -C "/tmp/"
        
        # Install
        print_substep "Installing..."
        cd "/tmp/rk3568-ddr-manager-$VERSION"
        ./scripts/deployment/install.sh --type full
        cd - > /dev/null
        
        # Cleanup
        rm -f "/tmp/ddr-manager-release.tar.gz"
        rm -rf "/tmp/rk3568-ddr-manager-$VERSION"
    fi
    
    print_success "Update from release completed"
}

# ============================================================================
# Update Functions
# ============================================================================

update_kernel() {
    print_step "Updating kernel components"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would update kernel modules"
    else
        # Build kernel modules
        if [ -d "$PROJECT_ROOT/kernel" ]; then
            cd "$PROJECT_ROOT/kernel"
            make clean
            make modules
            make modules_install
            depmod -a
            cd - > /dev/null
        fi
    fi
    
    print_success "Kernel components updated"
}

update_tools() {
    print_step "Updating tools"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would update tools"
    else
        if [ -f "$PROJECT_ROOT/scripts/build/build.sh" ]; then
            "$PROJECT_ROOT/scripts/build/build.sh" --type release --no-kernel
        fi
    fi
    
    print_success "Tools updated"
}

update_config() {
    print_step "Updating configuration"
    
    local config_dir="/etc/ddr_manager"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would update configuration"
    else
        if [ -d "$PROJECT_ROOT/configs" ]; then
            # Backup existing config
            if [ -d "$config_dir" ]; then
                cp -r "$config_dir" "$config_dir.bak"
            fi
            
            # Copy new configs
            mkdir -p "$config_dir"
            cp -r "$PROJECT_ROOT/configs"/* "$config_dir/"
            print_substep "Configuration updated"
        fi
    fi
    
    print_success "Configuration updated"
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 DDR Memory Manager - Update"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -t|--type)
                UPDATE_TYPE="$2"
                shift 2
                ;;
            -s|--source)
                SOURCE="$2"
                shift 2
                ;;
            -v|--version)
                VERSION="$2"
                shift 2
                ;;
            --no-backup)
                BACKUP=0
                shift
                ;;
            --no-confirm)
                CONFIRM=0
                shift
                ;;
            --dry-run)
                DRY_RUN=1
                shift
                ;;
            --verbose)
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
    
    # Validate update type
    case "$UPDATE_TYPE" in
        full|kernel|tools|config)
            print_info "Update type: $UPDATE_TYPE"
            ;;
        *)
            print_error "Invalid update type: $UPDATE_TYPE"
            show_help
            exit 1
            ;;
    esac
    
    # Validate source
    case "$SOURCE" in
        local|git|release)
            print_info "Update source: $SOURCE"
            ;;
        *)
            print_error "Invalid source: $SOURCE"
            show_help
            exit 1
            ;;
    esac
    
    # Check version
    if [ "$VERSION" = "latest" ] && [ "$SOURCE" != "local" ]; then
        VERSION=$(get_latest_version)
        print_info "Latest version: $VERSION"
    fi
    
    if [ $CONFIRM -eq 1 ] && [ $DRY_RUN -eq 0 ]; then
        print_warning "This will update the DDR Memory Manager"
        read -p "Continue? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Update cancelled"
            exit 0
        fi
    fi
    
    if [ $DRY_RUN -eq 1 ]; then
        print_warning "DRY RUN: No changes will be made"
    fi
    
    # Backup current installation
    backup_current
    
    # Update based on source
    case "$SOURCE" in
        local)
            update_from_local
            ;;
        git)
            update_from_git
            ;;
        release)
            update_from_release
            ;;
    esac
    
    # Additional updates based on type
    case "$UPDATE_TYPE" in
        full)
            update_kernel
            update_tools
            update_config
            ;;
        kernel)
            update_kernel
            ;;
        tools)
            update_tools
            ;;
        config)
            update_config
            ;;
    esac
    
    # Update version file
    if [ $DRY_RUN -eq 0 ]; then
        echo "$VERSION" > "$INSTALL_DIR/share/ddr_manager/version.txt"
    fi
    
    print_header "Update Complete"
    echo -e "\n${GREEN}${BOLD}✅ Update completed successfully!${NC}"
    echo ""
    echo "Updated to version: $VERSION"
    echo "Update type: $UPDATE_TYPE"
    echo "Update source: $SOURCE"
    
    echo ""
    print_info "It's recommended to restart services after update:"
    echo "  sudo systemctl restart ddr-manager.service"
}

# Run main function
main "$@"
