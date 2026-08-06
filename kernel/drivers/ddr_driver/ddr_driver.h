/**
 * ddr_driver.h - Header file for DDR Memory Manager Driver
 * 
 * This header defines the public interface and data structures
 * for the DDR platform driver.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_DRIVER_H_
#define _DDR_DRIVER_H_

#include <linux/types.h>
#include <linux/ioctl.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DDR_DRIVER_NAME         "ddr_manager"
#define DDR_DRIVER_VERSION      "1.0.0"
#define DDR_DRIVER_CLASS        "ddr_class"

#define DDR_MAX_ALLOC_SIZE      (512 * 1024 * 1024)  /* 512 MB */
#define DDR_MIN_ALLOC_SIZE      4096                 /* 4 KB */
#define DDR_MAX_ALLOCATIONS     1024

/* DDR Flags */
#define DDR_FLAG_NONE           0x0000
#define DDR_FLAG_DMA            0x0001
#define DDR_FLAG_CACHED         0x0002
#define DDR_FLAG_WRITE_COMBINE  0x0004
#define DDR_FLAG_READ_ONLY      0x0008
#define DDR_FLAG_WRITE_ONLY     0x0010
#define DDR_FLAG_SECURE         0x0020
#define DDR_FLAG_ALLOCATED      0x8000

/* DDR Status */
#define DDR_STATUS_OK           0x0000
#define DDR_STATUS_ERROR        0x0001
#define DDR_STATUS_BUSY         0x0002
#define DDR_STATUS_LOW_POWER    0x0004
#define DDR_STATUS_OVER_TEMP    0x0008
#define DDR_STATUS_UNDER_VOLT   0x0010
#define DDR_STATUS_ECC_ERROR    0x0020
#define DDR_STATUS_CORRUPTED    0x0040

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * struct ddr_config - DDR configuration structure
 */
struct ddr_config {
    unsigned int frequency;      /* Frequency in MHz */
    unsigned int voltage;        /* Voltage in mV */
    unsigned int tCL;            /* CAS Latency */
    unsigned int tRCD;           /* RAS-to-CAS Delay */
    unsigned int tRP;            /* RAS Precharge */
    unsigned int tRAS;           /* Active to Precharge */
    unsigned int tRFC;           /* Refresh Cycle Time (ns) */
    unsigned int tRRD;           /* Row Active to Row Active Delay */
    unsigned int tWTR;           /* Write to Read Delay */
    unsigned int tFAW;           /* Four Active Window */
    unsigned int ecc_enabled;    /* ECC enabled flag */
    unsigned int power_save;     /* Power save mode */
    unsigned int calibration;    /* Calibration settings */
    unsigned int reserved[8];    /* Reserved for future use */
};

/**
 * struct ddr_stats - DDR statistics structure
 */
struct ddr_stats {
    unsigned long total_allocations;     /* Total number of allocations */
    unsigned long total_frees;           /* Total number of frees */
    unsigned long current_allocations;   /* Current active allocations */
    unsigned long peak_allocations;      /* Peak allocation count */
    unsigned long total_allocated;       /* Total allocated bytes */
    unsigned long total_freed;           /* Total freed bytes */
    unsigned long current_allocated;     /* Currently allocated bytes */
    unsigned long peak_allocated;        /* Peak allocated bytes */
    unsigned long total_errors;          /* Total errors */
    unsigned long total_warnings;        /* Total warnings */
    unsigned long bus_errors;            /* Bus errors */
    unsigned long ecc_errors;            /* ECC errors */
    unsigned long timeout_errors;        /* Timeout errors */
    unsigned int  fragmentation;         /* Fragmentation percentage */
    unsigned int  memory_pressure;       /* Memory pressure (0-100) */
    unsigned int  bandwidth_usage;       /* Bandwidth usage percentage */
    unsigned int  latency_avg;           /* Average latency (ns) */
    unsigned int  latency_max;           /* Maximum latency (ns) */
    unsigned int  latency_min;           /* Minimum latency (ns) */
    unsigned int  reserved[8];           /* Reserved for future use */
};

/**
 * struct ddr_info - DDR information structure
 */
struct ddr_info {
    unsigned long total_memory;          /* Total DDR memory (bytes) */
    unsigned long used_memory;           /* Used memory (bytes) */
    unsigned long free_memory;           /* Free memory (bytes) */
    unsigned long peak_memory;           /* Peak memory usage (bytes) */
    unsigned int  frequency;             /* Current frequency (MHz) */
    unsigned int  voltage;               /* Current voltage (mV) */
    unsigned int  temperature;           /* Current temperature (C) */
    unsigned int  power;                 /* Current power (mW) */
    unsigned int  ecc_enabled;           /* ECC enabled flag */
    unsigned int  status;                /* Status flags */
    unsigned int  errors;                /* Error count */
    unsigned int  warnings;              /* Warning count */
    unsigned int  fragmentation;         /* Fragmentation percentage */
    unsigned int  bandwidth;             /* Bandwidth usage (MB/s) */
    unsigned int  latency;               /* Average latency (ns) */
    unsigned int  tCL;                   /* Current CAS Latency */
    unsigned int  tRCD;                  /* Current RAS-to-CAS Delay */
    unsigned int  tRP;                   /* Current RAS Precharge */
    unsigned int  tRAS;                  /* Current Active to Precharge */
    unsigned int  total_allocations;     /* Total allocations */
    unsigned int  total_frees;           /* Total frees */
    unsigned int  current_allocations;   /* Current active allocations */
    unsigned int  reserved[8];           /* Reserved for future use */
};

/**
 * struct ddr_alloc - DDR allocation request
 */
struct ddr_alloc {
    unsigned long size;                  /* Requested size (bytes) */
    unsigned long address;               /* Physical address (output) */
    unsigned int  flags;                 /* Allocation flags */
    unsigned int  status;                /* Allocation status */
    unsigned long user_data;             /* User data */
    unsigned int  reserved[4];           /* Reserved for future use */
};

/**
 * struct ddr_region - DDR region descriptor
 */
struct ddr_region {
    unsigned long start;                 /* Region start address */
    unsigned long end;                   /* Region end address */
    unsigned long size;                  /* Region size (bytes) */
    unsigned int  flags;                 /* Region flags */
    char          name[32];              /* Region name */
    unsigned int  reserved[4];           /* Reserved for future use */
};

/* ============================================================================
 * IOCTL Commands
 * ============================================================================ */

#define DDR_IOCTL_MAGIC         'D'

/* DDR IOCTL Commands */
#define DDR_IOCTL_GET_INFO      _IOR(DDR_IOCTL_MAGIC, 1, struct ddr_info)
#define DDR_IOCTL_GET_STATS     _IOR(DDR_IOCTL_MAGIC, 2, struct ddr_stats)
#define DDR_IOCTL_GET_CONFIG    _IOR(DDR_IOCTL_MAGIC, 3, struct ddr_config)
#define DDR_IOCTL_SET_CONFIG    _IOW(DDR_IOCTL_MAGIC, 4, struct ddr_config)
#define DDR_IOCTL_ALLOC         _IOWR(DDR_IOCTL_MAGIC, 5, struct ddr_alloc)
#define DDR_IOCTL_FREE          _IOW(DDR_IOCTL_MAGIC, 6, struct ddr_alloc)
#define DDR_IOCTL_DMA_ALLOC     _IOWR(DDR_IOCTL_MAGIC, 7, struct ddr_alloc)
#define DDR_IOCTL_DMA_FREE      _IOW(DDR_IOCTL_MAGIC, 8, struct ddr_alloc)
#define DDR_IOCTL_RESET         _IO(DDR_IOCTL_MAGIC, 9)
#define DDR_IOCTL_CALIBRATE     _IO(DDR_IOCTL_MAGIC, 10)
#define DDR_IOCTL_GET_STATUS    _IOR(DDR_IOCTL_MAGIC, 11, unsigned int)
#define DDR_IOCTL_GET_REGIONS   _IOR(DDR_IOCTL_MAGIC, 12, struct ddr_region)
#define DDR_IOCTL_SET_LOWPOWER  _IOW(DDR_IOCTL_MAGIC, 13, unsigned int)
#define DDR_IOCTL_GET_TIMINGS   _IOR(DDR_IOCTL_MAGIC, 14, struct ddr_timings)
#define DDR_IOCTL_SET_TIMINGS   _IOW(DDR_IOCTL_MAGIC, 15, struct ddr_timings)

/* ============================================================================
 * Public Functions
 * ============================================================================ */

/**
 * ddr_get_info - Get DDR information
 * @info: Pointer to info structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_get_info(struct ddr_info *info);

/**
 * ddr_get_stats - Get DDR statistics
 * @stats: Pointer to stats structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_get_stats(struct ddr_stats *stats);

/**
 * ddr_get_config - Get DDR configuration
 * @config: Pointer to config structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_get_config(struct ddr_config *config);

/**
 * ddr_set_config - Set DDR configuration
 * @config: Pointer to config structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_config(struct ddr_config *config);

/**
 * ddr_alloc - Allocate DDR memory
 * @size: Size to allocate
 * @flags: Allocation flags
 * 
 * Return: Physical address on success, 0 on failure
 */
unsigned long ddr_alloc(size_t size, unsigned int flags);

/**
 * ddr_free - Free DDR memory
 * @addr: Physical address to free
 * @size: Size of memory to free
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_free(unsigned long addr, size_t size);

/**
 * ddr_dma_alloc - Allocate DMA-capable DDR memory
 * @size: Size to allocate
 * @dma_handle: DMA address handle (output)
 * @flags: Allocation flags
 * 
 * Return: Virtual address on success, NULL on failure
 */
void *ddr_dma_alloc(size_t size, dma_addr_t *dma_handle, unsigned int flags);

/**
 * ddr_dma_free - Free DMA-capable DDR memory
 * @size: Size of memory to free
 * @ptr: Virtual address to free
 * @dma_handle: DMA address handle
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_dma_free(size_t size, void *ptr, dma_addr_t dma_handle);

/**
 * ddr_reset - Reset DDR controller
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_reset(void);

/**
 * ddr_calibrate - Calibrate DDR controller
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_calibrate(void);

/**
 * ddr_get_status - Get DDR status
 * 
 * Return: Status flags
 */
unsigned int ddr_get_status(void);

#endif /* _DDR_DRIVER_H_ */
