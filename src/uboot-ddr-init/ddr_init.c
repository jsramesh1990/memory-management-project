/**
 * ddr_init.c - DDR Initialization for U-Boot
 * 
 * This file implements DDR initialization for RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch-rockchip/ddr.h>
#include <asm/arch-rockchip/grf_rk3568.h>
#include <asm/arch-rockchip/cru_rk3568.h>
#include <asm/arch-rockchip/pmu_rk3568.h>
#include <asm/arch-rockchip/sdram_rk3568.h>
#include <linux/delay.h>
#include "ddr_init.h"
#include "ddr_partition.h"
#include "ddr_print.h"

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static struct ddr_config g_ddr_config;
static struct ddr_info g_ddr_info;
static struct ddr_timing g_ddr_timing;
static bool g_ddr_initialized = false;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * ddr_readl - Read DDR register
 */
static inline unsigned int ddr_readl(unsigned int offset)
{
    return readl(DDRC_BASE_ADDR + offset);
}

/**
 * ddr_writel - Write DDR register
 */
static inline void ddr_writel(unsigned int offset, unsigned int value)
{
    writel(value, DDRC_BASE_ADDR + offset);
}

/**
 * ddr_set_bit - Set bit in DDR register
 */
static inline void ddr_set_bit(unsigned int offset, unsigned int bit)
{
    unsigned int val = ddr_readl(offset);
    ddr_writel(offset, val | bit);
}

/**
 * ddr_clear_bit - Clear bit in DDR register
 */
static inline void ddr_clear_bit(unsigned int offset, unsigned int bit)
{
    unsigned int val = ddr_readl(offset);
    ddr_writel(offset, val & ~bit);
}

/**
 * ddr_is_bit_set - Check if bit is set in DDR register
 */
static inline bool ddr_is_bit_set(unsigned int offset, unsigned int bit)
{
    return !!(ddr_readl(offset) & bit);
}

/* ============================================================================
 * Hardware Initialization Functions
 * ============================================================================ */

/**
 * ddr_phy_init - Initialize DDR PHY
 */
static int ddr_phy_init(void)
{
    unsigned int val;
    int retry = 100;
    
    ddr_printf(DEBUG, "DDR: Initializing PHY...\n");
    
    /* Enable PHY clocks */
    writel(0xFFFFFFFF, PMUGRF_BASE_ADDR + 0x8000);
    udelay(10);
    
    /* Configure PHY */
    writel(0x00000001, DDRPHY_BASE_ADDR + 0x0000);
    udelay(10);
    
    /* Wait for PHY ready */
    while (retry--) {
        val = readl(DDRPHY_BASE_ADDR + 0x0004);
        if (val & 0x1) {
            ddr_printf(DEBUG, "DDR: PHY ready\n");
            return 0;
        }
        udelay(10);
    }
    
    ddr_printf(ERROR, "DDR: PHY init failed\n");
    return -ETIMEDOUT;
}

/**
 * ddr_controller_init - Initialize DDR controller
 */
static int ddr_controller_init(void)
{
    unsigned int val;
    int retry = 100;
    
    ddr_printf(DEBUG, "DDR: Initializing controller...\n");
    
    /* Reset controller */
    writel(0x00000001, DDRC_BASE_ADDR + 0x0000);
    udelay(100);
    writel(0x00000000, DDRC_BASE_ADDR + 0x0000);
    udelay(100);
    
    /* Configure controller */
    writel(0x00000001, DDRC_BASE_ADDR + 0x0004);
    udelay(10);
    
    /* Set memory type */
    val = readl(DDRC_BASE_ADDR + 0x0008);
    val &= ~0x0F;
    val |= g_ddr_config.type;
    writel(val, DDRC_BASE_ADDR + 0x0008);
    
    /* Wait for controller ready */
    while (retry--) {
        val = readl(DDRC_BASE_ADDR + 0x000C);
        if (val & 0x1) {
            ddr_printf(DEBUG, "DDR: Controller ready\n");
            return 0;
        }
        udelay(10);
    }
    
    ddr_printf(ERROR, "DDR: Controller init failed\n");
    return -ETIMEDOUT;
}

/**
 * ddr_timing_init - Initialize DDR timing parameters
 */
static int ddr_timing_init(struct ddr_timing *timing)
{
    unsigned int val;
    
    ddr_printf(DEBUG, "DDR: Setting timing parameters...\n");
    
    if (!timing) {
        ddr_printf(ERROR, "DDR: Invalid timing parameters\n");
        return -EINVAL;
    }
    
    /* Set timing0 (tCL, tRCD, tRP, tRAS) */
    val = (timing->tCL << 24) | (timing->tRCD << 16) |
          (timing->tRP << 8) | timing->tRAS;
    writel(val, DDRC_BASE_ADDR + 0x0010);
    
    /* Set timing1 (tRFC, tRRD, tWTR, tFAW) */
    val = (timing->tRFC << 24) | (timing->tRRD << 16) |
          (timing->tWTR << 8) | timing->tFAW;
    writel(val, DDRC_BASE_ADDR + 0x0014);
    
    /* Set timing2 (tWR, tRTP, tCWL) */
    val = (timing->tWR << 16) | (timing->tRTP << 8) | timing->tCWL;
    writel(val, DDRC_BASE_ADDR + 0x0018);
    
    /* Set frequency */
    writel(g_ddr_config.frequency_mhz, DDRC_BASE_ADDR + 0x001C);
    
    /* Set voltage */
    writel(g_ddr_config.voltage_mv, DDRC_BASE_ADDR + 0x0020);
    
    /* Wait for timing to take effect */
    udelay(100);
    
    ddr_printf(INFO, "DDR: Timings set: CL%d-tRCD%d-tRP%d-tRAS%d @ %dMHz %dmV\n",
               timing->tCL, timing->tRCD, timing->tRP,
               timing->tRAS, g_ddr_config.frequency_mhz,
               g_ddr_config.voltage_mv);
    
    return 0;
}

/**
 * ddr_calibrate - Calibrate DDR controller
 */
int ddr_calibrate(void)
{
    unsigned int val;
    int retry = 100;
    
    ddr_printf(DEBUG, "DDR: Calibrating...\n");
    
    /* Start calibration */
    writel(0x00000001, DDRC_BASE_ADDR + 0x0024);
    udelay(10);
    
    /* Wait for calibration complete */
    while (retry--) {
        val = readl(DDRC_BASE_ADDR + 0x0028);
        if (val & 0x1) {
            ddr_printf(INFO, "DDR: Calibration complete\n");
            return 0;
        }
        udelay(10);
    }
    
    ddr_printf(ERROR, "DDR: Calibration failed\n");
    return -ETIMEDOUT;
}

/**
 * ddr_test - Test DDR memory
 */
int ddr_test(phys_addr_t start, phys_size_t size)
{
    volatile unsigned int *ptr;
    unsigned int pattern;
    phys_addr_t addr;
    int ret = 0;
    
    ddr_printf(DEBUG, "DDR: Testing memory 0x%08lx - 0x%08lx\n",
               (unsigned long)start, (unsigned long)(start + size));
    
    /* Write test patterns */
    for (addr = start; addr < start + size; addr += 4) {
        ptr = (volatile unsigned int *)addr;
        pattern = (unsigned int)(addr & 0xFFFFFFFF);
        *ptr = pattern;
    }
    
    /* Read and verify */
    for (addr = start; addr < start + size; addr += 4) {
        ptr = (volatile unsigned int *)addr;
        if (*ptr != (unsigned int)(addr & 0xFFFFFFFF)) {
            ddr_printf(ERROR, "DDR: Memory error at 0x%08lx: expected 0x%08x, got 0x%08x\n",
                       (unsigned long)addr, (unsigned int)(addr & 0xFFFFFFFF), *ptr);
            ret = -EIO;
            break;
        }
    }
    
    if (ret == 0) {
        ddr_printf(INFO, "DDR: Memory test passed\n");
    }
    
    return ret;
}

/* ============================================================================
 * Main Initialization Functions
 * ============================================================================ */

/**
 * ddr_init - Initialize DDR controller
 */
int ddr_init(struct ddr_config *config)
{
    int ret;
    
    ddr_printf(INFO, "DDR: Initializing...\n");
    
    if (!config) {
        ddr_printf(ERROR, "DDR: Invalid configuration\n");
        return -EINVAL;
    }
    
    /* Save configuration */
    memcpy(&g_ddr_config, config, sizeof(g_ddr_config));
    memcpy(&g_ddr_timing, &config->timing, sizeof(g_ddr_timing));
    
    /* Initialize PHY */
    ret = ddr_phy_init();
    if (ret) {
        ddr_printf(ERROR, "DDR: PHY init failed\n");
        return ret;
    }
    
    /* Initialize controller */
    ret = ddr_controller_init();
    if (ret) {
        ddr_printf(ERROR, "DDR: Controller init failed\n");
        return ret;
    }
    
    /* Set timing parameters */
    ret = ddr_timing_init(&g_ddr_timing);
    if (ret) {
        ddr_printf(ERROR, "DDR: Timing init failed\n");
        return ret;
    }
    
    /* Calibrate */
    ret = ddr_calibrate();
    if (ret) {
        ddr_printf(ERROR, "DDR: Calibration failed\n");
        return ret;
    }
    
    /* Test memory */
    ret = ddr_test(0x40000000, 0x01000000); /* Test 16MB */
    if (ret) {
        ddr_printf(ERROR, "DDR: Memory test failed\n");
        return ret;
    }
    
    /* Get memory info */
    g_ddr_info.total_memory = g_ddr_config.size_mb;
    g_ddr_info.used_memory = 0;
    g_ddr_info.frequency = g_ddr_config.frequency_mhz;
    g_ddr_info.voltage = g_ddr_config.voltage_mv;
    g_ddr_info.status = DDR_STATUS_READY | DDR_STATUS_INIT | DDR_STATUS_CALIBRATED;
    g_ddr_info.errors = 0;
    
    g_ddr_initialized = true;
    
    ddr_printf(INFO, "DDR: Initialized successfully\n");
    ddr_dump_info();
    
    return 0;
}

/**
 * ddr_get_info - Get DDR information
 */
int ddr_get_info(struct ddr_info *info)
{
    if (!info) {
        return -EINVAL;
    }
    
    if (!g_ddr_initialized) {
        return -ENODEV;
    }
    
    memcpy(info, &g_ddr_info, sizeof(*info));
    return 0;
}

/**
 * ddr_get_config - Get current DDR configuration
 */
int ddr_get_config(struct ddr_config *config)
{
    if (!config) {
        return -EINVAL;
    }
    
    if (!g_ddr_initialized) {
        return -ENODEV;
    }
    
    memcpy(config, &g_ddr_config, sizeof(*config));
    return 0;
}

/**
 * ddr_set_timing - Set DDR timing parameters
 */
int ddr_set_timing(struct ddr_timing *timing)
{
    int ret;
    
    if (!timing) {
        return -EINVAL;
    }
    
    if (!g_ddr_initialized) {
        return -ENODEV;
    }
    
    ret = ddr_timing_init(timing);
    if (ret) {
        return ret;
    }
    
    memcpy(&g_ddr_timing, timing, sizeof(g_ddr_timing));
    return 0;
}

/**
 * ddr_set_frequency - Set DDR frequency
 */
int ddr_set_frequency(unsigned int freq_mhz)
{
    if (!g_ddr_initialized) {
        return -ENODEV;
    }
    
    ddr_printf(INFO, "DDR: Setting frequency to %d MHz\n", freq_mhz);
    
    writel(freq_mhz, DDRC_BASE_ADDR + 0x001C);
    udelay(100);
    
    g_ddr_config.frequency_mhz = freq_mhz;
    g_ddr_info.frequency = freq_mhz;
    
    return 0;
}

/**
 * ddr_set_voltage - Set DDR voltage
 */
int ddr_set_voltage(unsigned int voltage_mv)
{
    if (!g_ddr_initialized) {
        return -ENODEV;
    }
    
    ddr_printf(INFO, "DDR: Setting voltage to %d mV\n", voltage_mv);
    
    writel(voltage_mv, DDRC_BASE_ADDR + 0x0020);
    udelay(100);
    
    g_ddr_config.voltage_mv = voltage_mv;
    g_ddr_info.voltage = voltage_mv;
    
    return 0;
}

/**
 * ddr_get_total_memory - Get total DDR memory size
 */
unsigned int ddr_get_total_memory(void)
{
    if (!g_ddr_initialized) {
        return 0;
    }
    
    return g_ddr_info.total_memory;
}

/**
 * ddr_get_used_memory - Get used DDR memory size
 */
unsigned int ddr_get_used_memory(void)
{
    if (!g_ddr_initialized) {
        return 0;
    }
    
    return g_ddr_info.used_memory;
}

/**
 * ddr_get_free_memory - Get free DDR memory size
 */
unsigned int ddr_get_free_memory(void)
{
    if (!g_ddr_initialized) {
        return 0;
    }
    
    return g_ddr_info.total_memory - g_ddr_info.used_memory;
}

/**
 * ddr_dump_info - Dump DDR information
 */
void ddr_dump_info(void)
{
    if (!g_ddr_initialized) {
        ddr_printf(WARNING, "DDR: Not initialized\n");
        return;
    }
    
    ddr_printf(INFO, "\n=== DDR Information ===\n");
    ddr_printf(INFO, "Total Memory: %u MB\n", g_ddr_info.total_memory);
    ddr_printf(INFO, "Used Memory: %u MB\n", g_ddr_info.used_memory);
    ddr_printf(INFO, "Free Memory: %u MB\n", ddr_get_free_memory());
    ddr_printf(INFO, "Frequency: %u MHz\n", g_ddr_info.frequency);
    ddr_printf(INFO, "Voltage: %u mV\n", g_ddr_info.voltage);
    ddr_printf(INFO, "Temperature: %u C\n", g_ddr_info.temperature);
    ddr_printf(INFO, "Status: 0x%04x\n", g_ddr_info.status);
    ddr_printf(INFO, "Errors: %u\n", g_ddr_info.errors);
    ddr_printf(INFO, "========================\n");
}

/**
 * ddr_print_timing - Print DDR timing parameters
 */
void ddr_print_timing(struct ddr_timing *timing)
{
    if (!timing) {
        return;
    }
    
    ddr_printf(INFO, "\n=== DDR Timing Parameters ===\n");
    ddr_printf(INFO, "tCL:  %u\n", timing->tCL);
    ddr_printf(INFO, "tRCD: %u\n", timing->tRCD);
    ddr_printf(INFO, "tRP:  %u\n", timing->tRP);
    ddr_printf(INFO, "tRAS: %u\n", timing->tRAS);
    ddr_printf(INFO, "tRFC: %u\n", timing->tRFC);
    ddr_printf(INFO, "tRRD: %u\n", timing->tRRD);
    ddr_printf(INFO, "tWTR: %u\n", timing->tWTR);
    ddr_printf(INFO, "tFAW: %u\n", timing->tFAW);
    ddr_printf(INFO, "tWR:  %u\n", timing->tWR);
    ddr_printf(INFO, "tRTP: %u\n", timing->tRTP);
    ddr_printf(INFO, "tCWL: %u\n", timing->tCWL);
    ddr_printf(INFO, "=============================\n");
}

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("DDR Initialization for RK3568 U-Boot");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
