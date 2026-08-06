/**
 * ddr_print.c - DDR Print Functions for U-Boot
 * 
 * This file implements debug printing functions for DDR initialization.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#include <common.h>
#include <asm/io.h>
#include "ddr_init.h"
#include "ddr_print.h"

/* ============================================================================
 * Global Variables
 * ============================================================================ */

int ddr_print_level = DDR_PRINT_INFO;

/* ============================================================================
 * Public Functions
 * ============================================================================ */

/**
 * ddr_set_print_level - Set print level
 */
void ddr_set_print_level(int level)
{
    if (level >= DDR_PRINT_DEBUG && level <= DDR_PRINT_NONE) {
        ddr_print_level = level;
        ddr_printf(INFO, "DDR: Print level set to %d\n", level);
    }
}

/**
 * ddr_get_print_level - Get current print level
 */
int ddr_get_print_level(void)
{
    return ddr_print_level;
}

/**
 * ddr_print_hexdump - Print hex dump of memory
 */
void ddr_print_hexdump(phys_addr_t addr, size_t size, const char *prefix)
{
    unsigned char *ptr = (unsigned char *)addr;
    size_t i, j;
    char line[80];
    int offset = 0;
    
    if (!prefix) {
        prefix = "";
    }
    
    for (i = 0; i < size; i += 16) {
        offset += sprintf(line + offset, "%s0x%08lx: ", prefix, (unsigned long)(addr + i));
        
        for (j = 0; j < 16 && i + j < size; j++) {
            offset += sprintf(line + offset, "%02x ", ptr[i + j]);
        }
        
        for (; j < 16; j++) {
            offset += sprintf(line + offset, "   ");
        }
        
        offset += sprintf(line + offset, " ");
        
        for (j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = ptr[i + j];
            if (c >= 0x20 && c <= 0x7E) {
                offset += sprintf(line + offset, "%c", c);
            } else {
                offset += sprintf(line + offset, ".");
            }
        }
        
        ddr_printf(DEBUG, "%s\n", line);
        offset = 0;
    }
}

/**
 * ddr_print_buffer - Print buffer contents
 */
void ddr_print_buffer(const void *buf, size_t size, const char *prefix)
{
    ddr_print_hexdump((phys_addr_t)buf, size, prefix);
}

/**
 * ddr_print_regs - Print DDR register values
 */
void ddr_print_regs(void)
{
    ddr_printf(INFO, "\n=== DDR Registers ===\n");
    ddr_printf(INFO, "0x00: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x00));
    ddr_printf(INFO, "0x04: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x04));
    ddr_printf(INFO, "0x08: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x08));
    ddr_printf(INFO, "0x0C: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x0C));
    ddr_printf(INFO, "0x10: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x10));
    ddr_printf(INFO, "0x14: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x14));
    ddr_printf(INFO, "0x18: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x18));
    ddr_printf(INFO, "0x1C: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x1C));
    ddr_printf(INFO, "0x20: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x20));
    ddr_printf(INFO, "0x24: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x24));
    ddr_printf(INFO, "0x28: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x28));
    ddr_printf(INFO, "0x2C: 0x%08x\n", readl(DDRC_BASE_ADDR + 0x2C));
    ddr_printf(INFO, "====================\n");
}

/**
 * ddr_print_timing_table - Print timing table
 */
void ddr_print_timing_table(void)
{
    struct ddr_timing timing;
    unsigned int val;
    
    ddr_printf(INFO, "\n=== DDR Timing Table ===\n");
    
    /* Read timing0 */
    val = readl(DDRC_BASE_ADDR + 0x0010);
    timing.tCL = (val >> 24) & 0xFF;
    timing.tRCD = (val >> 16) & 0xFF;
    timing.tRP = (val >> 8) & 0xFF;
    timing.tRAS = val & 0xFF;
    
    /* Read timing1 */
    val = readl(DDRC_BASE_ADDR + 0x0014);
    timing.tRFC = (val >> 24) & 0xFF;
    timing.tRRD = (val >> 16) & 0xFF;
    timing.tWTR = (val >> 8) & 0xFF;
    timing.tFAW = val & 0xFF;
    
    /* Read timing2 */
    val = readl(DDRC_BASE_ADDR + 0x0018);
    timing.tWR = (val >> 16) & 0xFF;
    timing.tRTP = (val >> 8) & 0xFF;
    timing.tCWL = val & 0xFF;
    
    ddr_printf(INFO, "tCL:  %u\n", timing.tCL);
    ddr_printf(INFO, "tRCD: %u\n", timing.tRCD);
    ddr_printf(INFO, "tRP:  %u\n", timing.tRP);
    ddr_printf(INFO, "tRAS: %u\n", timing.tRAS);
    ddr_printf(INFO, "tRFC: %u\n", timing.tRFC);
    ddr_printf(INFO, "tRRD: %u\n", timing.tRRD);
    ddr_printf(INFO, "tWTR: %u\n", timing.tWTR);
    ddr_printf(INFO, "tFAW: %u\n", timing.tFAW);
    ddr_printf(INFO, "tWR:  %u\n", timing.tWR);
    ddr_printf(INFO, "tRTP: %u\n", timing.tRTP);
    ddr_printf(INFO, "tCWL: %u\n", timing.tCWL);
    ddr_printf(INFO, "=======================\n");
}

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("DDR Print Functions for RK3568 U-Boot");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
