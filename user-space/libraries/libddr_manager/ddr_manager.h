/**
 * ddr_manager.h - DDR Manager Library Header
 * 
 * This header defines the public API for the DDR Manager library
 * used by user-space applications on RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 * 
 * Compilation:
 *   gcc -o app app.c -lddr_manager
 * 
 * Usage:
 *   #include <ddr_manager.h>
 *   ddr_init();
 *   void *ptr = ddr_alloc(1024);
 *   ddr_free(ptr);
 */

#ifndef _DDR_MANAGER_H_
#define _DDR_MANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */

#define DDR_MANAGER_VERSION_MAJOR   1
#define DDR_MANAGER_VERSION_MINOR   0
#define DDR_MANAGER_VERSION_PATCH   0
#define DDR_MANAGER_VERSION_STRING  "1.0.0"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DDR_MANAGER_DEVICE_PATH     "/dev/ddr_manager"
#define DDR_MANAGER_MAX_ALLOC_SIZE  (512 * 1024 * 1024)  /* 512 MB */
#define DDR_MANAGER_MIN_ALLOC_SIZE  64
#define DDR_MANAGER_MAX_ALIGNMENT   4096

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define DDR_ERR_SUCCESS             0
#define DDR_ERR_INVALID             -EINVAL
#define DDR_ERR_NOMEM               -ENOMEM
#define DDR_ERR_IO                  -EIO
#define DDR_ERR_BUSY                -EBUSY
#define DDR_ERR_NOT_SUPPORTED       -ENOTSUP
#define DDR_ERR_NOT_INIT            -ENODEV
#define DDR_ERR_PERM                -EPERM
#define DDR_ERR_FAULT               -EFAULT
#define DDR_ERR_TIMEOUT             -ETIMEDOUT
#define DDR_ERR_OVERFLOW            -EOVERFLOW

/* ============================================================================
 * DDR Types
 * ============================================================================ */

/**
 * enum ddr_memory_type - DDR memory types
 */
enum ddr_memory_type {
    DDR_MEMORY_TYPE_UNKNOWN = 0,
    DDR_MEMORY_TYPE_DDR3,
    DDR_MEMORY_TYPE_DDR4,
    DDR_MEMORY_TYPE_LPDDR3,
    DDR_MEMORY_TYPE_LPDDR4,
    DDR_MEMORY_TYPE_LPDDR4X,
    DDR_MEMORY_TYPE_MAX,
};

/**
 * enum ddr_allocation_flags - DDR allocation flags
 */
enum ddr_allocation_flags {
    DDR_ALLOC_FLAG_NONE         = 0x0000,
    DDR_ALLOC_FLAG_DMA          = 0x0001,
    DDR_ALLOC_FLAG_CACHED       = 0x0002,
    DDR_ALLOC_FLAG_WRITE_COMBINE = 0x0004,
    DDR_ALLOC_FLAG_READ_ONLY    = 0x0008,
    DDR_ALLOC_FLAG_WRITE_ONLY   = 0x0010,
    DDR_ALLOC_FLAG_SECURE       = 0x0020,
    DDR_ALLOC_FLAG_CONTIGUOUS   = 0x0040,
    DDR_ALLOC_FLAG_ZERO_INIT    = 0x0080,
};

/**
 * enum ddr_ioctl_cmd - DDR IOCTL commands
 */
enum ddr_ioctl_cmd {
    DDR_IOCTL_GET_INFO      = 0x01,
    DDR_IOCTL_GET_STATS     = 0x02,
    DDR_IOCTL_GET_CONFIG    = 0x03,
    DDR_IOCTL_SET_CONFIG    = 0x04,
    DDR_IOCTL_ALLOC         = 0x05,
    DDR_IOCTL_FREE          = 0x06,
    DDR_IOCTL_DMA_ALLOC     = 0x07,
    DDR_IOCTL_DMA_FREE      = 0x08,
    DDR_IOCTL_RESET         = 0x09,
    DDR_IOCTL_CALIBRATE     = 0x0A,
    DDR_IOCTL_GET_STATUS    = 0x0B,
};

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * struct ddr_timing - DDR timing parameters
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
    char name[64];
    enum ddr_memory_type type;
    unsigned int size_mb;
    unsigned int channels;
    unsigned int frequency_mhz;
    unsigned int voltage_mv;
    struct ddr_timing timings;
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
    unsigned int free_memory;      /* Free memory in MB */
    unsigned int frequency;        /* Current frequency in MHz */
    unsigned int voltage;          /* Current voltage in mV */
    unsigned int temperature;      /* Current temperature in C */
    unsigned int status;           /* Status flags */
    unsigned int errors;           /* Error count */
};

/**
 * struct ddr_stats - DDR statistics
 */
struct ddr_stats {
    unsigned long total_allocations;     /* Total allocations */
    unsigned long total_frees;           /* Total frees */
    unsigned long current_allocations;   /* Current active allocations */
    unsigned long peak_allocations;      /* Peak allocation count */
    unsigned long total_allocated;       /* Total allocated bytes */
    unsigned long total_freed;           /* Total freed bytes */
    unsigned long current_allocated;     /* Currently allocated bytes */
    unsigned long peak_allocated;        /* Peak allocated bytes */
    unsigned int  errors;                /* Error count */
    unsigned int  warnings;              /* Warning count */
    unsigned int  fragmentation;         /* Fragmentation percentage */
};

/**
 * struct ddr_alloc_request - DDR allocation request
 */
struct ddr_alloc_request {
    unsigned long size;          /* Requested size */
    unsigned long address;       /* Physical address (output) */
    unsigned int  flags;         /* Allocation flags */
    unsigned int  status;        /* Allocation status */
};

/* ============================================================================
 * Core Functions
 * ============================================================================ */

/**
 * ddr_init - Initialize DDR Manager
 * 
 * This function initializes the DDR Manager library and opens
 * the device file for communication with the kernel module.
 * 
 * Return: 0 on success, negative error code on failure
 * 
 * Example:
 *   if (ddr_init() < 0) {
 *       printf("Failed to initialize DDR Manager\n");
 *       return -1;
 *   }
 */
int ddr_init(void);

/**
 * ddr_cleanup - Clean up DDR Manager
 * 
 * This function cleans up the DDR Manager library and closes
 * the device file.
 * 
 * Example:
 *   ddr_cleanup();
 */
void ddr_cleanup(void);

/**
 * ddr_is_initialized - Check if DDR Manager is initialized
 * 
 * Return: true if initialized, false otherwise
 */
bool ddr_is_initialized(void);

/**
 * ddr_get_version - Get DDR Manager version
 * 
 * Return: Version string
 */
const char *ddr_get_version(void);

/* ============================================================================
 * Memory Allocation Functions
 * ============================================================================ */

/**
 * ddr_alloc - Allocate DDR memory
 * @size: Size to allocate in bytes
 * @flags: Allocation flags (see enum ddr_allocation_flags)
 * 
 * Return: Pointer to allocated memory, or NULL on failure
 * 
 * Example:
 *   void *ptr = ddr_alloc(1024 * 1024, DDR_ALLOC_FLAG_NONE);
 *   if (ptr) {
 *       memset(ptr, 0, 1024 * 1024);
 *       // Use memory...
 *       ddr_free(ptr);
 *   }
 */
void *ddr_alloc(size_t size, unsigned int flags);

/**
 * ddr_free - Free DDR memory
 * @ptr: Pointer to memory to free
 * 
 * Example:
 *   void *ptr = ddr_alloc(1024, 0);
 *   if (ptr) {
 *       // Use memory...
 *       ddr_free(ptr);
 *   }
 */
void ddr_free(void *ptr);

/**
 * ddr_alloc_aligned - Allocate aligned DDR memory
 * @size: Size to allocate in bytes
 * @align: Alignment requirement (must be power of 2)
 * @flags: Allocation flags
 * 
 * Return: Pointer to allocated memory, or NULL on failure
 * 
 * Example:
 *   void *ptr = ddr_alloc_aligned(4096, 64, 0);
 *   if (ptr) {
 *       // Use memory...
 *       ddr_free(ptr);
 *   }
 */
void *ddr_alloc_aligned(size_t size, size_t align, unsigned int flags);

/**
 * ddr_dma_alloc - Allocate DMA-capable DDR memory
 * @size: Size to allocate in bytes
 * @dma_handle: Output DMA address handle
 * @flags: Allocation flags
 * 
 * Return: Pointer to allocated memory, or NULL on failure
 * 
 * Example:
 *   dma_addr_t dma_handle;
 *   void *ptr = ddr_dma_alloc(4096, &dma_handle, 0);
 *   if (ptr) {
 *       // Use DMA memory...
 *       ddr_dma_free(ptr, 4096, dma_handle);
 *   }
 */
void *ddr_dma_alloc(size_t size, unsigned long *dma_handle, unsigned int flags);

/**
 * ddr_dma_free - Free DMA-capable DDR memory
 * @ptr: Pointer to memory to free
 * @size: Size of memory to free
 * @dma_handle: DMA address handle
 */
void ddr_dma_free(void *ptr, size_t size, unsigned long dma_handle);

/**
 * ddr_realloc - Reallocate DDR memory
 * @ptr: Pointer to previously allocated memory
 * @size: New size in bytes
 * 
 * Return: Pointer to reallocated memory, or NULL on failure
 * 
 * Note: The contents of the original memory are preserved
 */
void *ddr_realloc(void *ptr, size_t size);

/**
 * ddr_calloc - Allocate zero-initialized DDR memory
 * @nmemb: Number of elements
 * @size: Size of each element in bytes
 * 
 * Return: Pointer to allocated memory, or NULL on failure
 * 
 * Example:
 *   int *arr = ddr_calloc(100, sizeof(int));
 *   if (arr) {
 *       // Use array...
 *       ddr_free(arr);
 *   }
 */
void *ddr_calloc(size_t nmemb, size_t size);

/* ============================================================================
 * Information Functions
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

/* ============================================================================
 * Control Functions
 * ============================================================================ */

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
 * ddr_set_voltage - Set DDR voltage
 * @voltage_mv: Voltage in millivolts
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_voltage(unsigned int voltage_mv);

/**
 * ddr_set_frequency - Set DDR frequency
 * @frequency_mhz: Frequency in MHz
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_frequency(unsigned int frequency_mhz);

/**
 * ddr_set_power_save - Enable/disable power save mode
 * @enable: Enable flag
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_power_save(bool enable);

/**
 * ddr_set_performance_mode - Enable/disable performance mode
 * @enable: Enable flag
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_set_performance_mode(bool enable);

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

/**
 * ddr_dump_info - Dump DDR information to stdout
 */
void ddr_dump_info(void);

/**
 * ddr_dump_stats - Dump DDR statistics to stdout
 */
void ddr_dump_stats(void);

/**
 * ddr_dump_config - Dump DDR configuration to stdout
 */
void ddr_dump_config(void);

/**
 * ddr_hexdump - Dump memory in hex format
 * @ptr: Pointer to memory
 * @size: Size to dump
 * @prefix: Prefix string for each line
 */
void ddr_hexdump(const void *ptr, size_t size, const char *prefix);

/**
 * ddr_memtest - Test memory region
 * @ptr: Pointer to memory
 * @size: Size to test in bytes
 * @iterations: Number of test iterations
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_memtest(void *ptr, size_t size, int iterations);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * ddr_get_error_string - Get error string for error code
 * @err: Error code
 * 
 * Return: Error string
 */
const char *ddr_get_error_string(int err);

/**
 * ddr_get_type_string - Get DDR type string
 * @type: DDR memory type
 * 
 * Return: Type string
 */
const char *ddr_get_type_string(enum ddr_memory_type type);

/**
 * ddr_get_flag_string - Get allocation flag string
 * @flags: Allocation flags
 * 
 * Return: Flag string
 */
const char *ddr_get_flag_string(unsigned int flags);

/**
 * ddr_snprintf_info - Print DDR info to string
 * @buffer: Destination buffer
 * @size: Buffer size
 * 
 * Return: Number of characters written
 */
int ddr_snprintf_info(char *buffer, size_t size);

/**
 * ddr_snprintf_stats - Print DDR stats to string
 * @buffer: Destination buffer
 * @size: Buffer size
 * 
 * Return: Number of characters written
 */
int ddr_snprintf_stats(char *buffer, size_t size);

/* ============================================================================
 * Performance Monitoring
 * ============================================================================ */

/**
 * ddr_perf_start - Start performance measurement
 * 
 * Return: Timestamp for measurement
 */
uint64_t ddr_perf_start(void);

/**
 * ddr_perf_stop - Stop performance measurement
 * @start_ts: Start timestamp from ddr_perf_start()
 * 
 * Return: Elapsed time in nanoseconds
 */
uint64_t ddr_perf_stop(uint64_t start_ts);

/**
 * ddr_perf_measure - Measure performance of an operation
 * @operation: Operation name
 * @func: Function to measure
 * @arg: Function argument
 * 
 * Return: Elapsed time in nanoseconds
 */
uint64_t ddr_perf_measure(const char *operation, void *(*func)(void *), void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _DDR_MANAGER_H_ */
