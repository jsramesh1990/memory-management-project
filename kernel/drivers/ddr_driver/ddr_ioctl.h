/**
 * ddr_ioctl.h - IOCTL Definitions for DDR Memory Manager
 * 
 * This header defines the IOCTL interface for user-space applications
 * to interact with the DDR driver.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_IOCTL_H_
#define _DDR_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/* ============================================================================
 * IOCTL Magic Number
 * ============================================================================ */

#define DDR_IOCTL_MAGIC         'D'

/* ============================================================================
 * IOCTL Command Definitions
 * ============================================================================ */

/**
 * DDR_IOCTL_GET_INFO - Get DDR information
 * 
 * Arguments: struct ddr_info *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_INFO      _IOR(DDR_IOCTL_MAGIC, 1, struct ddr_info)

/**
 * DDR_IOCTL_GET_STATS - Get DDR statistics
 * 
 * Arguments: struct ddr_stats *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_STATS     _IOR(DDR_IOCTL_MAGIC, 2, struct ddr_stats)

/**
 * DDR_IOCTL_GET_CONFIG - Get DDR configuration
 * 
 * Arguments: struct ddr_config *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_CONFIG    _IOR(DDR_IOCTL_MAGIC, 3, struct ddr_config)

/**
 * DDR_IOCTL_SET_CONFIG - Set DDR configuration
 * 
 * Arguments: struct ddr_config *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_SET_CONFIG    _IOW(DDR_IOCTL_MAGIC, 4, struct ddr_config)

/**
 * DDR_IOCTL_ALLOC - Allocate DDR memory
 * 
 * Arguments: struct ddr_alloc *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_ALLOC         _IOWR(DDR_IOCTL_MAGIC, 5, struct ddr_alloc)

/**
 * DDR_IOCTL_FREE - Free DDR memory
 * 
 * Arguments: struct ddr_alloc *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_FREE          _IOW(DDR_IOCTL_MAGIC, 6, struct ddr_alloc)

/**
 * DDR_IOCTL_DMA_ALLOC - Allocate DMA-capable DDR memory
 * 
 * Arguments: struct ddr_alloc *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_DMA_ALLOC     _IOWR(DDR_IOCTL_MAGIC, 7, struct ddr_alloc)

/**
 * DDR_IOCTL_DMA_FREE - Free DMA-capable DDR memory
 * 
 * Arguments: struct ddr_alloc *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_DMA_FREE      _IOW(DDR_IOCTL_MAGIC, 8, struct ddr_alloc)

/**
 * DDR_IOCTL_RESET - Reset DDR controller
 * 
 * Arguments: None
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_RESET         _IO(DDR_IOCTL_MAGIC, 9)

/**
 * DDR_IOCTL_CALIBRATE - Calibrate DDR controller
 * 
 * Arguments: None
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_CALIBRATE     _IO(DDR_IOCTL_MAGIC, 10)

/**
 * DDR_IOCTL_GET_STATUS - Get DDR status
 * 
 * Arguments: unsigned int *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_STATUS    _IOR(DDR_IOCTL_MAGIC, 11, unsigned int)

/**
 * DDR_IOCTL_GET_REGIONS - Get DDR memory regions
 * 
 * Arguments: struct ddr_region *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_REGIONS   _IOR(DDR_IOCTL_MAGIC, 12, struct ddr_region)

/**
 * DDR_IOCTL_SET_LOWPOWER - Set low power mode
 * 
 * Arguments: unsigned int (0=disable, 1=enable)
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_SET_LOWPOWER  _IOW(DDR_IOCTL_MAGIC, 13, unsigned int)

/**
 * DDR_IOCTL_GET_TIMINGS - Get DDR timings
 * 
 * Arguments: struct ddr_timings *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_TIMINGS   _IOR(DDR_IOCTL_MAGIC, 14, struct ddr_timings)

/**
 * DDR_IOCTL_SET_TIMINGS - Set DDR timings
 * 
 * Arguments: struct ddr_timings *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_SET_TIMINGS   _IOW(DDR_IOCTL_MAGIC, 15, struct ddr_timings)

/**
 * DDR_IOCTL_ECC_ENABLE - Enable/disable ECC
 * 
 * Arguments: unsigned int (0=disable, 1=enable)
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_ECC_ENABLE    _IOW(DDR_IOCTL_MAGIC, 16, unsigned int)

/**
 * DDR_IOCTL_GET_ECC_STATUS - Get ECC status
 * 
 * Arguments: struct ddr_ecc_status *
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_ECC_STATUS _IOR(DDR_IOCTL_MAGIC, 17, struct ddr_ecc_status)

/**
 * DDR_IOCTL_GET_BANDWIDTH - Get bandwidth usage
 * 
 * Arguments: unsigned int * (bandwidth in MB/s)
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_BANDWIDTH _IOR(DDR_IOCTL_MAGIC, 18, unsigned int)

/**
 * DDR_IOCTL_GET_TEMPERATURE - Get temperature
 * 
 * Arguments: unsigned int * (temperature in Celsius)
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_TEMPERATURE _IOR(DDR_IOCTL_MAGIC, 19, unsigned int)

/**
 * DDR_IOCTL_GET_POWER - Get power consumption
 * 
 * Arguments: unsigned int * (power in mW)
 * Return: 0 on success, negative error code on failure
 */
#define DDR_IOCTL_GET_POWER    _IOR(DDR_IOCTL_MAGIC, 20, unsigned int)

/* ============================================================================
 * Data Structures for IOCTL
 * ============================================================================ */

/**
 * struct ddr_timings - DDR timing structure
 */
struct ddr_timings {
    unsigned int tCL;      /* CAS Latency */
    unsigned int tRCD;     /* RAS-to-CAS Delay */
    unsigned int tRP;      /* RAS Precharge */
    unsigned int tRAS;     /* Active to Precharge */
    unsigned int tRFC;     /* Refresh Cycle Time (ns) */
    unsigned int tRRD;     /* Row Active to Row Active Delay */
    unsigned int tWTR;     /* Write to Read Delay */
    unsigned int tFAW;     /* Four Active Window */
    unsigned int reserved[8]; /* Reserved for future use */
};

/**
 * struct ddr_ecc_status - ECC status structure
 */
struct ddr_ecc_status {
    unsigned int enabled;      /* ECC enabled flag */
    unsigned int errors;       /* Number of ECC errors */
    unsigned int corrected;    /* Number of corrected errors */
    unsigned int uncorrected;  /* Number of uncorrected errors */
    unsigned int last_error;   /* Last error address */
    unsigned int last_correct; /* Last corrected address */
    unsigned int reserved[4];  /* Reserved for future use */
};

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * DDR_IOCTL_GET_IOCTL_NR - Get IOCTL number from command
 */
#define DDR_IOCTL_GET_IOCTL_NR(cmd)  _IOC_NR(cmd)

/**
 * DDR_IOCTL_GET_IOCTL_TYPE - Get IOCTL type from command
 */
#define DDR_IOCTL_GET_IOCTL_TYPE(cmd)  _IOC_TYPE(cmd)

/**
 * DDR_IOCTL_GET_IOCTL_SIZE - Get IOCTL size from command
 */
#define DDR_IOCTL_GET_IOCTL_SIZE(cmd)  _IOC_SIZE(cmd)

/**
 * DDR_IOCTL_IS_READ - Check if IOCTL is read
 */
#define DDR_IOCTL_IS_READ(cmd)  (_IOC_DIR(cmd) & _IOC_READ)

/**
 * DDR_IOCTL_IS_WRITE - Check if IOCTL is write
 */
#define DDR_IOCTL_IS_WRITE(cmd) (_IOC_DIR(cmd) & _IOC_WRITE)

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define DDR_ERR_SUCCESS         0
#define DDR_ERR_INVALID         -EINVAL
#define DDR_ERR_NOMEM           -ENOMEM
#define DDR_ERR_BUSY            -EBUSY
#define DDR_ERR_IO              -EIO
#define DDR_ERR_TIMEOUT         -ETIMEDOUT
#define DDR_ERR_OVERFLOW        -EOVERFLOW
#define DDR_ERR_UNDERFLOW       -ERANGE
#define DDR_ERR_NOT_SUPPORTED   -ENOTSUPP
#define DDR_ERR_NOT_READY       -ENODEV
#define DDR_ERR_PERM            -EPERM
#define DDR_ERR_FAULT           -EFAULT

/* ============================================================================
 * Debugging Macros
 * ============================================================================ */

#ifdef DDR_DEBUG
#define DDR_TRACE(fmt, args...) \
    pr_debug("DDR: " fmt, ##args)
#define DDR_INFO(fmt, args...) \
    pr_info("DDR: " fmt, ##args)
#define DDR_ERROR(fmt, args...) \
    pr_err("DDR: " fmt, ##args)
#define DDR_WARN(fmt, args...) \
    pr_warn("DDR: " fmt, ##args)
#else
#define DDR_TRACE(fmt, args...)
#define DDR_INFO(fmt, args...) \
    pr_info("DDR: " fmt, ##args)
#define DDR_ERROR(fmt, args...) \
    pr_err("DDR: " fmt, ##args)
#define DDR_WARN(fmt, args...) \
    pr_warn("DDR: " fmt, ##args)
#endif /* DDR_DEBUG */

#endif /* _DDR_IOCTL_H_ */
