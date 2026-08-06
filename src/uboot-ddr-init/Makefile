# Makefile for DDR Initialization - U-Boot
# Version: 1.0.0
# Author: Sebastian

# ============================================================================
# Configuration
# ============================================================================

obj-y += ddr_init.o
obj-y += ddr_partition.o
obj-y += ddr_print.o

# ============================================================================
# Compiler Flags
# ============================================================================

ccflags-y += -I$(srctree)/arch/arm/include
ccflags-y += -I$(srctree)/arch/arm/mach-rockchip/include
ccflags-y += -I$(srctree)/arch/arm/include/asm/arch-rockchip
ccflags-y += -I$(srctree)/drivers/ram/rockchip

# Debug flags
ifdef CONFIG_DDR_DEBUG
ccflags-y += -DDEBUG
ccflags-y += -DDEBUG_DDR
endif

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean distclean

all: $(obj-y)

clean:
	rm -f *.o *.cmd *.gcno *.gcda *.gcov

distclean: clean
	rm -f *.mod.c *.mod *.ko

# ============================================================================
# Dependencies
# ============================================================================

ddr_init.o: ddr_init.c ddr_init.h ddr_partition.h ddr_print.h
ddr_partition.o: ddr_partition.c ddr_partition.h ddr_init.h ddr_print.h
ddr_print.o: ddr_print.c ddr_print.h ddr_init.h

# ============================================================================
# Help
# ============================================================================

help:
	@echo "DDR Initialization - U-Boot"
	@echo ""
	@echo "Available targets:"
	@echo "  all      - Build all objects"
	@echo "  clean    - Remove object files"
	@echo "  distclean- Remove all generated files"
	@echo "  help     - Show this help"
