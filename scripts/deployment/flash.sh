#!/bin/bash
# flash.sh - Flashing Script for RK3568 DDR Memory Manager
# Version: 1.0.0
# Author: Sebastian
# Description: This script flashes the DDR Memory Manager to the target device

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
BOOT_DIR="/boot"
DEVICE=""
FLASH_TYPE="full"
BACKUP=1
VERIFY=1
COMPRESS=1
PARTITION_TABLE=""
BOOTLOADER_FILE=""
KERNEL_FILE=""
DTB_FILE=""
ROOTFS_FILE=""

# Flash tools
DD_CMD="dd"
MKFS_CMD="mkfs"
UBOOT_CMD="u-boot"
RKTOOL="rkdeveloptool"

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
${BOLD}RK3568 DDR Memory Manager - Flashing Script${NC}

${BOLD}Usage:${NC}
    $0 [OPTIONS]

${BOLD}Options:${NC}
    -d, --device DEVICE     Target device (e.g., /dev/sdb, /dev/mmcblk0)
    -t, --type TYPE         Flash type (full, bootloader, kernel, rootfs) [default: full]
    -b, --bootloader FILE   Bootloader image file
    -k, --kernel FILE       Kernel image file
    -d, --dtb FILE          Device tree blob file
    -r, --rootfs FILE       Root filesystem image file
    -p, --partition TABLE   Partition table
    --no-backup             Skip backup
    --no-verify             Skip verification
    --no-compress           Skip compression
    --dry-run               Perform dry run without flashing
    -v, --verbose           Verbose output
    -h, --help              Show this help message

${BOLD}Examples:${NC}
    $0 --device /dev/sdb --type full
    $0 --device /dev/mmcblk0 --type bootloader --bootloader u-boot.img
    $0 --device /dev/sdb --type rootfs --rootfs rootfs.img

EOF
}

# ============================================================================
# Device Detection
# ============================================================================

detect_device() {
    print_step "Detecting target device"
    
    if [ -n "$DEVICE" ]; then
        print_info "Using specified device: $DEVICE"
        return 0
    fi
    
    # Try to detect device
    print_info "Searching for RK3568 device..."
    
    # Check for USB device
    if lsusb | grep -q "Rockchip"; then
        print_info "Found Rockchip device via USB"
        DEVICE="usb"
        return 0
    fi
    
    # Check for MMC device
    for dev in /dev/mmcblk*; do
        if [ -e "$dev" ] && [ "$dev" != "/dev/mmcblk0" ]; then
            print_info "Found MMC device: $dev"
            DEVICE="$dev"
            return 0
        fi
    done
    
    # Check for SD card
    for dev in /dev/sd*; do
        if [ -e "$dev" ] && [ "$dev" != "/dev/sda" ]; then
            print_info "Found SD card: $dev"
            DEVICE="$dev"
            return 0
        fi
    done
    
    print_error "No device found. Please specify device with --device"
    return 1
}

# ============================================================================
# Backup Functions
# ============================================================================

backup_device() {
    if [ $BACKUP -eq 0 ]; then
        return 0
    fi
    
    print_step "Backing up current device"
    
    local backup_dir="$PROJECT_ROOT/backup_flash_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir"
    
    # Check if device is a block device
    if [ -b "$DEVICE" ]; then
        print_info "Device: $DEVICE"
        print_substep "Reading first 1MB of device..."
        
        if [ $DRY_RUN -eq 1 ]; then
            print_substep "[DRY RUN] Would backup device to: $backup_dir"
        else
            dd if="$DEVICE" of="$backup_dir/backup.img" bs=1M count=1 2>/dev/null || true
            print_success "Backup created: $backup_dir/backup.img"
        fi
    fi
    
    print_success "Backup completed"
}

# ============================================================================
# Partition Functions
# ============================================================================

create_partitions() {
    print_step "Creating partitions"
    
    if [ ! -b "$DEVICE" ]; then
        print_warning "Device $DEVICE is not a block device"
        return 0
    fi
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would create partitions on $DEVICE"
        return 0
    fi
    
    print_warning "This will erase all data on $DEVICE"
    read -p "Continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Aborted"
        return 1
    fi
    
    # Create partition table
    if [ -n "$PARTITION_TABLE" ]; then
        print_substep "Using partition table: $PARTITION_TABLE"
        sfdisk "$DEVICE" < "$PARTITION_TABLE"
    else
        print_substep "Creating default partition table"
        
        # Default partitions for RK3568
        cat > /tmp/partitions.txt << EOF
label: gpt
label-id: 0x12345678
device: $DEVICE
unit: sectors
first-lba: 64

${DEVICE}p1 : start=64, size=32768, type=0x83
${DEVICE}p2 : start=32832, size=65536, type=0x83
${DEVICE}p3 : start=98368, size=65536, type=0x83
${DEVICE}p4 : start=163904, size=2097152, type=0x83
${DEVICE}p5 : start=2261056, size=, type=0x83
EOF
        
        sfdisk "$DEVICE" < /tmp/partitions.txt
        rm -f /tmp/partitions.txt
    fi
    
    # Format partitions
    print_substep "Formatting partitions"
    mkfs.ext4 -F "${DEVICE}p4" 2>/dev/null || true
    mkfs.ext4 -F "${DEVICE}p5" 2>/dev/null || true
    
    print_success "Partitions created"
}

# ============================================================================
# Flash Functions
# ============================================================================

flash_bootloader() {
    print_step "Flashing bootloader"
    
    local bootloader_file="$BOOTLOADER_FILE"
    
    if [ -z "$bootloader_file" ]; then
        bootloader_file="$BUILD_DIR/bootloader/u-boot.img"
    fi
    
    if [ ! -f "$bootloader_file" ]; then
        print_error "Bootloader file not found: $bootloader_file"
        return 1
    fi
    
    print_info "Bootloader: $bootloader_file"
    
    if [ "$DEVICE" = "usb" ]; then
        # USB flashing with rkdeveloptool
        if [ $DRY_RUN -eq 1 ]; then
            print_substep "[DRY RUN] Would flash bootloader via USB"
        else
            if command -v rkdeveloptool &>/dev/null; then
                rkdeveloptool db "$bootloader_file"
                rkdeveloptool ul "$bootloader_file"
                rkdeveloptool rd
                print_success "Bootloader flashed via USB"
            else
                print_error "rkdeveloptool not found"
                return 1
            fi
        fi
    elif [ -b "$DEVICE" ]; then
        # Block device flashing
        if [ $DRY_RUN -eq 1 ]; then
            print_substep "[DRY RUN] Would flash bootloader to $DEVICE"
        else
            dd if="$bootloader_file" of="$DEVICE" bs=1M seek=1 conv=fsync 2>/dev/null
            print_success "Bootloader flashed to $DEVICE"
        fi
    else
        print_error "Unknown device type: $DEVICE"
        return 1
    fi
}

flash_kernel() {
    print_step "Flashing kernel"
    
    local kernel_file="$KERNEL_FILE"
    
    if [ -z "$kernel_file" ]; then
        kernel_file="$BUILD_DIR/kernel/Image"
    fi
    
    if [ ! -f "$kernel_file" ]; then
        print_error "Kernel file not found: $kernel_file"
        return 1
    fi
    
    print_info "Kernel: $kernel_file"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would flash kernel to partition 2"
    else
        if [ -b "$DEVICE" ]; then
            dd if="$kernel_file" of="${DEVICE}p2" bs=1M conv=fsync 2>/dev/null
            print_success "Kernel flashed to ${DEVICE}p2"
        else
            print_warning "Device not a block device, skipping"
        fi
    fi
}

flash_dtb() {
    print_step "Flashing device tree"
    
    local dtb_file="$DTB_FILE"
    
    if [ -z "$dtb_file" ]; then
        dtb_file="$BUILD_DIR/dtb/rk3568.dtb"
    fi
    
    if [ ! -f "$dtb_file" ]; then
        print_warning "DTB file not found: $dtb_file"
        return 0
    fi
    
    print_info "DTB: $dtb_file"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would flash DTB to partition 3"
    else
        if [ -b "$DEVICE" ]; then
            dd if="$dtb_file" of="${DEVICE}p3" bs=1M conv=fsync 2>/dev/null
            print_success "DTB flashed to ${DEVICE}p3"
        else
            print_warning "Device not a block device, skipping"
        fi
    fi
}

flash_rootfs() {
    print_step "Flashing root filesystem"
    
    local rootfs_file="$ROOTFS_FILE"
    
    if [ -z "$rootfs_file" ]; then
        rootfs_file="$BUILD_DIR/rootfs.img"
    fi
    
    if [ ! -f "$rootfs_file" ]; then
        print_error "RootFS file not found: $rootfs_file"
        return 1
    fi
    
    print_info "RootFS: $rootfs_file"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would flash rootfs to partition 5"
    else
        if [ -b "$DEVICE" ]; then
            # Extract if compressed
            if [ "$rootfs_file" = *.gz ]; then
                gunzip -c "$rootfs_file" | dd of="${DEVICE}p5" bs=1M conv=fsync
            elif [ "$rootfs_file" = *.xz ]; then
                unxz -c "$rootfs_file" | dd of="${DEVICE}p5" bs=1M conv=fsync
            else
                dd if="$rootfs_file" of="${DEVICE}p5" bs=1M conv=fsync 2>/dev/null
            fi
            print_success "RootFS flashed to ${DEVICE}p5"
        else
            print_warning "Device not a block device, skipping"
        fi
    fi
}

flash_full() {
    print_step "Performing full flash"
    
    flash_bootloader
    flash_kernel
    flash_dtb
    flash_rootfs
}

# ============================================================================
# Verification
# ============================================================================

verify_flash() {
    if [ $VERIFY -eq 0 ]; then
        return 0
    fi
    
    print_step "Verifying flash"
    
    if [ $DRY_RUN -eq 1 ]; then
        print_substep "[DRY RUN] Would verify flash"
        return 0
    fi
    
    # Check if device is bootable
    if [ -b "$DEVICE" ]; then
        if file "${DEVICE}" | grep -q "boot sector"; then
            print_success "Device is bootable"
        else
            print_warning "Device may not be bootable"
        fi
    fi
    
    print_success "Verification completed"
}

# ============================================================================
# Main Function
# ============================================================================

main() {
    print_header "RK3568 DDR Memory Manager - Flashing"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -d|--device)
                DEVICE="$2"
                shift 2
                ;;
            -t|--type)
                FLASH_TYPE="$2"
                shift 2
                ;;
            -b|--bootloader)
                BOOTLOADER_FILE="$2"
                shift 2
                ;;
            -k|--kernel)
                KERNEL_FILE="$2"
                shift 2
                ;;
            --dtb)
                DTB_FILE="$2"
                shift 2
                ;;
            -r|--rootfs)
                ROOTFS_FILE="$2"
                shift 2
                ;;
            -p|--partition)
                PARTITION_TABLE="$2"
                shift 2
                ;;
            --no-backup)
                BACKUP=0
                shift
                ;;
            --no-verify)
                VERIFY=0
                shift
                ;;
            --no-compress)
                COMPRESS=0
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
    
    # Detect device
    detect_device || exit 1
    
    # Validate flash type
    case "$FLASH_TYPE" in
        full|bootloader|kernel|rootfs)
            print_info "Flash type: $FLASH_TYPE"
            ;;
        *)
            print_error "Invalid flash type: $FLASH_TYPE"
            show_help
            exit 1
            ;;
    esac
    
    if [ $DRY_RUN -eq 1 ]; then
        print_warning "DRY RUN: No changes will be made"
    fi
    
    # Run flashing
    backup_device
    
    case "$FLASH_TYPE" in
        full)
            create_partitions
            flash_full
            ;;
        bootloader)
            flash_bootloader
            ;;
        kernel)
            flash_kernel
            ;;
        rootfs)
            flash_rootfs
            ;;
    esac
    
    verify_flash
    
    print_header "Flashing Complete"
    echo -e "\n${GREEN}${BOLD}✅ Flashing completed successfully!${NC}"
    echo ""
    echo "Device: $DEVICE"
    echo "Type: $FLASH_TYPE"
    echo ""
    echo "Next steps:"
    echo "  1. Remove the device and insert into RK3568 board"
    echo "  2. Power on the device"
    echo "  3. Check serial console for boot messages"
}

# Run main function
main "$@"
