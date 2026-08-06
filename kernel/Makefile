# Makefile for DDR Memory Manager Kernel Module
# Version: 1.0.0
# Author: Sebastian

# Kernel build configuration
obj-m += ddr_kernel_module.o

# Module source files
ddr_kernel_module-y := ddr_kernel_module.o

# Kernel build system
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Default target
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

# Install target
install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install
	depmod -a

# Load module
load:
	sudo insmod ddr_kernel_module.ko

# Unload module
unload:
	sudo rmmod ddr_kernel_module

# Check module
check:
	lsmod | grep ddr_kernel_module

# Test module
test:
	sudo dmesg | grep DDR | tail -20

# Debug target
debug:
	sudo dmesg -C
	sudo insmod ddr_kernel_module.ko debug=1

# Clean and rebuild
rebuild: clean all

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build the kernel module"
	@echo "  clean    - Clean build artifacts"
	@echo "  install  - Install the kernel module"
	@echo "  load     - Load the kernel module"
	@echo "  unload   - Unload the kernel module"
	@echo "  check    - Check if module is loaded"
	@echo "  test     - Test the module"
	@echo "  debug    - Load with debug output"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  help     - Show this help"

.PHONY: all clean install load unload check test debug rebuild help
