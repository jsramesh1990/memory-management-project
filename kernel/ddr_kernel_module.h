/**
 * ddr_kernel_module.h - Header file for DDR Memory Manager Kernel Module
 * 
 * This header defines the public interface for the DDR kernel module.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_KERNEL_MODULE_H_
#define _DDR_KERNEL_MODULE_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/* ============================================================================
 * IOCTL Definitions
 * ============================================================================ */

#define DDR_IOCTL_MAGIC     'D'

/* DDR IOCTL Commands */
#define DDR_IOCTL_GET_INFO  _IOR(DDR_IOCTL_MAGIC, 1, struct ddr_info)
#define DDR_IOCTL_ALLOC     _IOWR(DDR_IOCTL_MAGIC, 2, struct ddr_alloc)
#define DDR_IOCTL_FREE      _IOW(DDR_IOCTL_MAGIC, 3, unsigned long)
#define DDR_IOCTL_CONFIG    _IOW(DDR_IOCTL_MAGIC, 4, struct ddr_config)
#define DDR_IOCTL_STATS     _IOR(DDR_IOCTL_MAGIC, 5, struct ddr_stats)
#define DDR_IOCTL_RESET     _IO(DDR_IOCTL_MAGIC, 6)

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * struct ddr_info - DDR information structure
 */
struct ddr_info {
    unsigned long total_memory;      /* Total DDR memory (bytes) */
    unsigned long used_memory;       /* Used memory (bytes) */
    unsigned long free_memory;       /* Free memory (bytes) */
    unsigned int  frequency;         /* DDR frequency (MHz) */
    unsigned int  voltage;           /* DDR voltage (mV) */
    unsigned int  temperature;       /* DDR temperature (Celsius) */
    unsigned int  error_count;       /* ECC error count */
    unsigned int  status;            /* Status flags */
};

/**
 * struct ddr_alloc - DDR allocation structure
 */
struct ddr_alloc {
    unsigned long size;              /* Allocation size (bytes) */
    unsigned long address;           /* Physical address */
    unsigned int  flags;             /* Allocation flags */
    unsigned int  padding;           /* Padding for alignment */
};

/**
 * struct ddr_config - DDR configuration structure
 */
struct ddr_config {
    unsigned int frequency;          /* Target frequency (MHz) */
    unsigned int voltage;            /* Target voltage (mV) */
    unsigned int tCL;                /* CAS Latency */
    unsigned int tRCD;               /* RAS-to-CAS Delay */
    unsigned int tRP;                /* RAS Precharge */
    unsigned int tRAS;               /* Active to Precharge */
    unsigned int tRFC;               /* Refresh Cycle Time (ns) */
};

/**
 * struct ddr_stats - DDR statistics structure
 */
struct ddr_stats {
    unsigned long total_allocations; /* Total number of allocations */
    unsigned long total_frees;       /* Total number of frees */
    unsigned long current_allocations;/* Current active allocations */
    unsigned long peak_allocations;  /* Peak allocation count */
    unsigned long total_allocated;   /* Total allocated bytes */
    unsigned long total_freed;       /* Total freed bytes */
    unsigned long current_allocated; /* Currently allocated bytes */
    unsigned long peak_allocated;    /* Peak allocated bytes */
    unsigned int  errors;            /* Error count */
    unsigned int  warnings;          /* Warning count */
    unsigned int  fragmentation;     /* Fragmentation percentage */
};

/* ============================================================================
 * Public Functions
 * ============================================================================ */

/**
 * ddr_alloc - Allocate DDR memory from kernel
 * @size: Size to allocate
 * @flags: Allocation flags (GFP_KERNEL, GFP_ATOMIC, etc.)
 * 
 * Return: Pointer to allocated memory, or NULL on failure
 */
void *ddr_alloc_kernel(size_t size, gfp_t flags);

/**
 * ddr_free - Free DDR memory
 * @ptr: Pointer to memory to free
 * @size: Size of memory to free
 */
void ddr_free_kernel(void *ptr, size_t size);

/**
 * ddr_dma_alloc - Allocate DMA-capable DDR memory
 * @dev: Device pointer
 * @size: Size to allocate
 * @dma_handle: DMA address handle (output)
 * @flags: Allocation flags
 * 
 * Return: Pointer to allocated memory, or NULL on failure
 */
void *ddr_dma_alloc_kernel(struct device *dev, size_t size,
                          dma_addr_t *dma_handle, gfp_t flags);

/**
 * ddr_dma_free - Free DMA-capable DDR memory
 * @dev: Device pointer
 * @size: Size of memory to free
 * @ptr: Pointer to memory to free
 * @dma_handle: DMA address handle
 */
void ddr_dma_free_kernel(struct device *dev, size_t size,
                         void *ptr, dma_addr_t dma_handle);

/**
 * ddr_get_info - Get DDR information
 * @info: Pointer to info structure to fill
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_get_info_kernel(struct ddr_info *info);

/**
 * ddr_get_stats - Get DDR statistics
 * @stats: Pointer to stats structure to fill
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_get_stats_kernel(struct ddr_stats *stats);

/**
 * ddr_config - Configure DDR
 * @config: Pointer to configuration structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_kernel(struct ddr_config *config);

#endif /* _DDR_KERNEL_MODULE_H_ */
