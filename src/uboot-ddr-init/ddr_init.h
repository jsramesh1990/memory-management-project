/**
 * ddr_init.h - DDR Initialization Header for U-Boot
 * 
 * This header defines the DDR initialization functions and structures
 * for U-Boot on RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_INIT_H_
#define _DDR_INIT_H_

#include <common.h>
#include <asm/types.h>
#include <asm/arch-rockchip/ddr.h>

/* ============================================================================
 * DDR Initialization Constants
 * ============================================================================ */

/* DDR Controller Base Addresses */
#define DDRC_BASE_ADDR          0xFE000000
#define DDRPHY_BASE_ADDR        0xFE100000
#define PMUGRF_BASE_ADDR        0xFD200000

/* DDR Timing Parameters */
#define DDR_TCL_DEFAULT         18
#define DDR_TRCD_DEFAULT        18
#define DDR_TRP_DEFAULT         18
#define DDR_TRAS_DEFAULT        42
#define DDR_TRFC_DEFAULT        350
#define DDR_TRRD_DEFAULT        4
#define DDR_TWTR_DEFAULT        4
#define DDR_TFAW_DEFAULT        16

/* DDR Frequency Options */
#define DDR_FREQ_800            800
#define DDR_FREQ_1066           1066
#define DDR_FREQ_1333           1333
#define DDR_FREQ_1600           1600
#define DDR_FREQ_1800           1800
#define DDR_FREQ_2133           2133
#define DDR_FREQ_DEFAULT        DDR_FREQ_1800

/* DDR Voltage Options (mV) */
#define DDR_VOLTAGE_1_05V       1050
#define DDR_VOLTAGE_1_1V        1100
#define DDR_VOLTAGE_1_2V        1200
#define DDR_VOLTAGE_DEFAULT     DDR_VOLTAGE_1_1V

/* DDR Status Flags */
#define DDR_STATUS_READY        BIT(0)
#define DDR_STATUS_INIT         BIT(1)
#define DDR_STATUS_ECC          BIT(2)
#define DDR_STATUS_ERROR        BIT(3)
#define DDR_STATUS_CALIBRATED   BIT(4)

/* ============================================================================
 * DDR Data Structures
 * ============================================================================ */

/**
 * enum ddr_type - DDR memory types
 */
enum ddr_type {
    DDR_TYPE_UNKNOWN = 0,
    DDR_TYPE_DDR3,
    DDR_TYPE_DDR4,
    DDR_TYPE_LPDDR3,
    DDR_TYPE_LPDDR4,
    DDR_TYPE_LPDDR4X,
    DDR_TYPE_MAX,
};

/**
 * enum ddr_channel - DDR channel configuration
 */
enum ddr_channel {
    DDR_CHANNEL_SINGLE = 1,
    DDR_CHANNEL_DUAL = 2,
};

/**
 * struct ddr_timing - DDR timing configuration
 */
struct ddr_timing {
    unsigned int tCL;      /* CAS Latency */
    unsigned int tRCD;     /* RAS-to-CAS Delay */
    unsigned int tRP;      /* RAS Precharge */
    unsigned int tRAS;     /* Active to Precharge */
    unsigned int tRFC;     /* Refresh Cycle Time (ns) */
    unsigned int tRRD;     /* Row Active to Row Active Delay */
    unsigned int tWTR;     /* Write to Read Delay */
    unsigned int tFAW;     /* Four Active Window */
    unsigned int tWR;      /* Write Recovery Time */
    unsigned int tRTP;     /* Read to Precharge Delay */
    unsigned int tCWL;     /* CAS Write Latency */
};

/**
 * struct ddr_config - DDR configuration
 */
struct ddr_config {
    enum ddr_type type;
    enum ddr_channel channels;
    unsigned int frequency_mhz;
    unsigned int voltage_mv;
    struct ddr_timing timing;
    unsigned int ecc_enabled:1;
    unsigned int power_save:1;
    unsigned int performance_mode:1;
};

/**
 * struct ddr_info - DDR information
 */
struct ddr_info {
    unsigned int total_memory;     /* Total memory in MB */
    unsigned int used_memory;      /* Used memory in MB */
    unsigned int frequency;        /* Current frequency in MHz */
    unsigned int voltage;          /* Current voltage in mV */
    unsigned int temperature;      /* Current temperature in C */
    unsigned int status;           /* Status flags */
    unsigned int errors;           /* Error count */
};

/**
 * struct ddr_partition - DDR partition
 */
struct ddr_partition {
    const char *name;
    phys_addr_t start;
    phys_size_t size;
    unsigned int flags;
};

/**
 * struct ddr_region - DDR memory region
 */
struct ddr_region {
    const char *name;
    phys_addr_t start;
    phys_size_t size;
    unsigned int type;
    unsigned int flags;
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * ddr_init - Initialize DDR controller
 * @config: DDR configuration
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_init(struct ddr_config *config);

/**
 * ddr_get_info - Get DDR information
 * @info: Pointer to info structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_get_info(struct ddr_info *info);

/**
 * ddr_get_config - Get current DDR configuration
 * @config: Pointer to config structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_get_config(struct ddr_config *config);

/**
 * ddr_set_timing - Set DDR timing parameters
 * @timing: Pointer to timing structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_timing(struct ddr_timing *timing);

/**
 * ddr_set_frequency - Set DDR frequency
 * @freq_mhz: Frequency in MHz
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_frequency(unsigned int freq_mhz);

/**
 * ddr_set_voltage - Set DDR voltage
 * @voltage_mv: Voltage in mV
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_voltage(unsigned int voltage_mv);

/**
 * ddr_calibrate - Calibrate DDR controller
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_calibrate(void);

/**
 * ddr_test - Test DDR memory
 * @start: Start address
 * @size: Test size
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_test(phys_addr_t start, phys_size_t size);

/**
 * ddr_get_total_memory - Get total DDR memory size
 * 
 * Return: Total memory size in MB
 */
unsigned int ddr_get_total_memory(void);

/**
 * ddr_get_used_memory - Get used DDR memory size
 * 
 * Return: Used memory size in MB
 */
unsigned int ddr_get_used_memory(void);

/**
 * ddr_get_free_memory - Get free DDR memory size
 * 
 * Return: Free memory size in MB
 */
unsigned int ddr_get_free_memory(void);

/**
 * ddr_dump_info - Dump DDR information
 */
void ddr_dump_info(void);

/**
 * ddr_print_timing - Print DDR timing parameters
 * @timing: Pointer to timing structure
 */
void ddr_print_timing(struct ddr_timing *timing);

#endif /* _DDR_INIT_H_ */
