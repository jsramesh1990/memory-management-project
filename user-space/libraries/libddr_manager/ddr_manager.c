/**
 * ddr_manager.c - DDR Manager Library Implementation
 * 
 * This file implements the DDR Manager library for user-space
 * applications on RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>

#include "ddr_manager.h"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define DDR_IOCTL_MAGIC         'D'
#define DDR_IOCTL_GET_INFO      _IOR(DDR_IOCTL_MAGIC, 1, struct ddr_info)
#define DDR_IOCTL_GET_STATS     _IOR(DDR_IOCTL_MAGIC, 2, struct ddr_stats)
#define DDR_IOCTL_GET_CONFIG    _IOR(DDR_IOCTL_MAGIC, 3, struct ddr_config)
#define DDR_IOCTL_SET_CONFIG    _IOW(DDR_IOCTL_MAGIC, 4, struct ddr_config)
#define DDR_IOCTL_ALLOC         _IOWR(DDR_IOCTL_MAGIC, 5, struct ddr_alloc_request)
#define DDR_IOCTL_FREE          _IOW(DDR_IOCTL_MAGIC, 6, unsigned long)
#define DDR_IOCTL_DMA_ALLOC     _IOWR(DDR_IOCTL_MAGIC, 7, struct ddr_alloc_request)
#define DDR_IOCTL_DMA_FREE      _IOW(DDR_IOCTL_MAGIC, 8, unsigned long)
#define DDR_IOCTL_RESET         _IO(DDR_IOCTL_MAGIC, 9)
#define DDR_IOCTL_CALIBRATE     _IO(DDR_IOCTL_MAGIC, 10)
#define DDR_IOCTL_GET_STATUS    _IOR(DDR_IOCTL_MAGIC, 11, unsigned int)

#define DDR_DEVICE_PATH         "/dev/ddr_manager"
#define DDR_MAX_FD              32

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * struct ddr_context - DDR Manager context
 */
struct ddr_context {
    int fd;                     /* Device file descriptor */
    int initialized;            /* Initialization flag */
    pthread_mutex_t lock;       /* Mutex for thread safety */
    struct ddr_info info;       /* Cached DDR info */
    struct ddr_stats stats;     /* Cached DDR stats */
    struct ddr_config config;   /* Cached DDR config */
};

/**
 * struct allocation_header - Memory allocation header
 */
struct allocation_header {
    size_t size;                /* Allocation size */
    unsigned int flags;         /* Allocation flags */
    unsigned long dma_handle;   /* DMA handle (if DMA allocation) */
    struct allocation_header *next;
    struct allocation_header *prev;
};

/* ============================================================================
 * Internal Variables
 * ============================================================================ */

static struct ddr_context g_ctx = {
    .fd = -1,
    .initialized = 0,
};

static pthread_once_t g_once = PTHREAD_ONCE_INIT;

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/**
 * ddr_lock - Lock the DDR context
 */
static inline void ddr_lock(void)
{
    pthread_mutex_lock(&g_ctx.lock);
}

/**
 * ddr_unlock - Unlock the DDR context
 */
static inline void ddr_unlock(void)
{
    pthread_mutex_unlock(&g_ctx.lock);
}

/**
 * ddr_open_device - Open DDR device file
 */
static int ddr_open_device(void)
{
    int fd;
    
    /* Try to open device file */
    fd = open(DDR_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        /* Try to create device file */
        if (errno == ENOENT) {
            fprintf(stderr, "DDR Manager: Device file not found: %s\n", DDR_DEVICE_PATH);
            fprintf(stderr, "DDR Manager: Please load the kernel module first\n");
        } else {
            fprintf(stderr, "DDR Manager: Failed to open device: %s\n", strerror(errno));
        }
        return fd;
    }
    
    return fd;
}

/**
 * ddr_init_once - One-time initialization
 */
static void ddr_init_once(void)
{
    pthread_mutex_init(&g_ctx.lock, NULL);
}

/**
 * ddr_validate_ptr - Validate pointer for allocation header
 */
static struct allocation_header *ddr_validate_ptr(void *ptr)
{
    struct allocation_header *header;
    
    if (!ptr) {
        return NULL;
    }
    
    /* Get header from pointer */
    header = (struct allocation_header *)((uintptr_t)ptr - sizeof(struct allocation_header));
    
    /* Basic validation */
    if (header->size == 0 || header->size > DDR_MANAGER_MAX_ALLOC_SIZE) {
        return NULL;
    }
    
    return header;
}

/**
 * ddr_get_alloc_size - Get total allocation size with header
 */
static inline size_t ddr_get_alloc_size(size_t size)
{
    return size + sizeof(struct allocation_header);
}

/**
 * ddr_align_size - Align size to cache line
 */
static inline size_t ddr_align_size(size_t size)
{
    size_t align = 64;  /* Cache line size */
    return (size + align - 1) & ~(align - 1);
}

/* ============================================================================
 * Core Functions
 * ============================================================================ */

/**
 * ddr_init - Initialize DDR Manager
 */
int ddr_init(void)
{
    int ret;
    
    /* One-time initialization */
    pthread_once(&g_once, ddr_init_once);
    
    ddr_lock();
    
    if (g_ctx.initialized) {
        ddr_unlock();
        return 0;
    }
    
    /* Open device */
    g_ctx.fd = ddr_open_device();
    if (g_ctx.fd < 0) {
        ret = g_ctx.fd;
        g_ctx.fd = -1;
        ddr_unlock();
        return ret;
    }
    
    /* Get initial information */
    ret = ddr_get_info(&g_ctx.info);
    if (ret < 0) {
        close(g_ctx.fd);
        g_ctx.fd = -1;
        ddr_unlock();
        return ret;
    }
    
    ret = ddr_get_stats(&g_ctx.stats);
    if (ret < 0) {
        close(g_ctx.fd);
        g_ctx.fd = -1;
        ddr_unlock();
        return ret;
    }
    
    ret = ddr_get_config(&g_ctx.config);
    if (ret < 0) {
        close(g_ctx.fd);
        g_ctx.fd = -1;
        ddr_unlock();
        return ret;
    }
    
    g_ctx.initialized = 1;
    
    ddr_unlock();
    
    printf("DDR Manager initialized (version %s)\n", DDR_MANAGER_VERSION_STRING);
    printf("  Total Memory: %u MB\n", g_ctx.info.total_memory);
    printf("  Frequency: %u MHz\n", g_ctx.info.frequency);
    printf("  Voltage: %u mV\n", g_ctx.info.voltage);
    
    return 0;
}

/**
 * ddr_cleanup - Clean up DDR Manager
 */
void ddr_cleanup(void)
{
    ddr_lock();
    
    if (g_ctx.initialized && g_ctx.fd >= 0) {
        close(g_ctx.fd);
        g_ctx.fd = -1;
        g_ctx.initialized = 0;
    }
    
    ddr_unlock();
}

/**
 * ddr_is_initialized - Check if DDR Manager is initialized
 */
bool ddr_is_initialized(void)
{
    bool ret;
    ddr_lock();
    ret = g_ctx.initialized;
    ddr_unlock();
    return ret;
}

/**
 * ddr_get_version - Get DDR Manager version
 */
const char *ddr_get_version(void)
{
    return DDR_MANAGER_VERSION_STRING;
}

/* ============================================================================
 * Memory Allocation Functions
 * ============================================================================ */

/**
 * ddr_alloc - Allocate DDR memory
 */
void *ddr_alloc(size_t size, unsigned int flags)
{
    void *ptr = NULL;
    struct allocation_header *header;
    size_t alloc_size;
    int ret;
    
    if (!ddr_is_initialized()) {
        fprintf(stderr, "DDR Manager: Not initialized\n");
        return NULL;
    }
    
    if (size == 0 || size > DDR_MANAGER_MAX_ALLOC_SIZE) {
        fprintf(stderr, "DDR Manager: Invalid allocation size: %zu\n", size);
        return NULL;
    }
    
    alloc_size = ddr_get_alloc_size(ddr_align_size(size));
    
    ddr_lock();
    
    if (flags & DDR_ALLOC_FLAG_DMA) {
        /* DMA allocation is handled separately */
        ddr_unlock();
        fprintf(stderr, "DDR Manager: Use ddr_dma_alloc() for DMA allocation\n");
        return NULL;
    }
    
    /* Allocate memory from system */
    ptr = malloc(alloc_size);
    if (!ptr) {
        ddr_unlock();
        fprintf(stderr, "DDR Manager: Failed to allocate %zu bytes\n", size);
        return NULL;
    }
    
    /* Setup header */
    header = (struct allocation_header *)ptr;
    header->size = size;
    header->flags = flags;
    header->dma_handle = 0;
    header->next = NULL;
    header->prev = NULL;
    
    /* Zero-init if requested */
    if (flags & DDR_ALLOC_FLAG_ZERO_INIT) {
        memset(header + 1, 0, size);
    }
    
    /* Return pointer after header */
    ptr = (void *)(header + 1);
    
    ddr_unlock();
    
    return ptr;
}

/**
 * ddr_free - Free DDR memory
 */
void ddr_free(void *ptr)
{
    struct allocation_header *header;
    
    if (!ptr) {
        return;
    }
    
    if (!ddr_is_initialized()) {
        fprintf(stderr, "DDR Manager: Not initialized\n");
        return;
    }
    
    ddr_lock();
    
    /* Validate pointer */
    header = ddr_validate_ptr(ptr);
    if (!header) {
        ddr_unlock();
        fprintf(stderr, "DDR Manager: Invalid pointer: %p\n", ptr);
        return;
    }
    
    /* Free memory */
    free(header);
    
    ddr_unlock();
}

/**
 * ddr_alloc_aligned - Allocate aligned DDR memory
 */
void *ddr_alloc_aligned(size_t size, size_t align, unsigned int flags)
{
    void *ptr;
    size_t alloc_size;
    uintptr_t addr;
    size_t offset;
    
    if (!ddr_is_initialized()) {
        fprintf(stderr, "DDR Manager: Not initialized\n");
        return NULL;
    }
    
    if (size == 0 || size > DDR_MANAGER_MAX_ALLOC_SIZE) {
        fprintf(stderr, "DDR Manager: Invalid allocation size: %zu\n", size);
        return NULL;
    }
    
    /* Check alignment is power of 2 */
    if ((align & (align - 1)) != 0 || align > DDR_MANAGER_MAX_ALIGNMENT) {
        fprintf(stderr, "DDR Manager: Invalid alignment: %zu\n", align);
        return NULL;
    }
    
    if (align < sizeof(void *)) {
        align = sizeof(void *);
    }
    
    /* Allocate extra memory for alignment */
    alloc_size = size + align + sizeof(struct allocation_header);
    
    ptr = malloc(alloc_size);
    if (!ptr) {
        fprintf(stderr, "DDR Manager: Failed to allocate %zu bytes\n", size);
        return NULL;
    }
    
    /* Calculate aligned address */
    addr = (uintptr_t)ptr + sizeof(struct allocation_header);
    addr = (addr + align - 1) & ~(align - 1);
    offset = addr - (uintptr_t)ptr;
    
    /* Setup header at the start of allocation */
    struct allocation_header *header = (struct allocation_header *)ptr;
    header->size = size;
    header->flags = flags;
    header->dma_handle = 0;
    header->next = NULL;
    header->prev = NULL;
    
    /* Store header pointer at the aligned address */
    *(struct allocation_header **)addr = header;
    
    /* Zero-init if requested */
    if (flags & DDR_ALLOC_FLAG_ZERO_INIT) {
        memset((void *)addr, 0, size);
    }
    
    return (void *)addr;
}

/**
 * ddr_dma_alloc - Allocate DMA-capable DDR memory
 */
void *ddr_dma_alloc(size_t size, unsigned long *dma_handle, unsigned int flags)
{
    void *ptr = NULL;
    struct allocation_header *header;
    size_t alloc_size;
    int ret;
    struct ddr_alloc_request req;
    
    if (!ddr_is_initialized()) {
        fprintf(stderr, "DDR Manager: Not initialized\n");
        return NULL;
    }
    
    if (size == 0 || size > DDR_MANAGER_MAX_ALLOC_SIZE) {
        fprintf(stderr, "DDR Manager: Invalid allocation size: %zu\n", size);
        return NULL;
    }
    
    if (!dma_handle) {
        fprintf(stderr, "DDR Manager: NULL DMA handle\n");
        return NULL;
    }
    
    alloc_size = ddr_get_alloc_size(ddr_align_size(size));
    
    ddr_lock();
    
    /* Prepare request */
    req.size = size;
    req.flags = flags | DDR_ALLOC_FLAG_DMA;
    req.address = 0;
    req.status = 0;
    
    /* Send IOCTL */
    ret = ioctl(g_ctx.fd, DDR_IOCTL_DMA_ALLOC, &req);
    if (ret < 0) {
        ddr_unlock();
        fprintf(stderr, "DDR Manager: DMA allocation failed: %s\n", strerror(errno));
        return NULL;
    }
    
    /* Map memory */
    ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, g_ctx.fd, req.address);
    if (ptr == MAP_FAILED) {
        ddr_unlock();
        fprintf(stderr, "DDR Manager: DMA mmap failed: %s\n", strerror(errno));
        return NULL;
    }
    
    /* Setup header */
    header = (struct allocation_header *)ptr;
    header->size = size;
    header->flags = flags | DDR_ALLOC_FLAG_DMA;
    header->dma_handle = req.address;
    header->next = NULL;
    header->prev = NULL;
    
    *dma_handle = req.address;
    
    /* Zero-init if requested */
    if (flags & DDR_ALLOC_FLAG_ZERO_INIT) {
        memset(header + 1, 0, size);
    }
    
    /* Return pointer after header */
    ptr = (void *)(header + 1);
    
    ddr_unlock();
    
    return ptr;
}

/**
 * ddr_dma_free - Free DMA-capable DDR memory
 */
void ddr_dma_free(void *ptr, size_t size, unsigned long dma_handle)
{
    struct allocation_header *header;
    int ret;
    
    if (!ptr) {
        return;
    }
    
    if (!ddr_is_initialized()) {
        fprintf(stderr, "DDR Manager: Not initialized\n");
        return;
    }
    
    ddr_lock();
    
    /* Validate pointer */
    header = ddr_validate_ptr(ptr);
    if (!header) {
        ddr_unlock();
        fprintf(stderr, "DDR Manager: Invalid pointer: %p\n", ptr);
        return;
    }
    
    /* Unmap memory */
    munmap(header, size + sizeof(struct allocation_header));
    
    /* Free DMA memory */
    ret = ioctl(g_ctx.fd, DDR_IOCTL_DMA_FREE, dma_handle);
    if (ret < 0) {
        fprintf(stderr, "DDR Manager: DMA free failed: %s\n", strerror(errno));
    }
    
    ddr_unlock();
}

/**
 * ddr_realloc - Reallocate DDR memory
 */
void *ddr_realloc(void *ptr, size_t size)
{
    void *new_ptr;
    struct allocation_header *header;
    size_t old_size;
    
    if (!ptr) {
        return ddr_alloc(size, 0);
    }
    
    if (size == 0) {
        ddr_free(ptr);
        return NULL;
    }
    
    if (!ddr_is_initialized()) {
        fprintf(stderr, "DDR Manager: Not initialized\n");
        return NULL;
    }
    
    ddr_lock();
    
    /* Validate pointer */
    header = ddr_validate_ptr(ptr);
    if (!header) {
        ddr_unlock();
        fprintf(stderr, "DDR Manager: Invalid pointer: %p\n", ptr);
        return NULL;
    }
    
    old_size = header->size;
    
    /* Allocate new memory */
    new_ptr = ddr_alloc(size, header->flags);
    if (!new_ptr) {
        ddr_unlock();
        return NULL;
    }
    
    /* Copy old data */
    memcpy(new_ptr, ptr, old_size < size ? old_size : size);
    
    /* Free old memory */
    ddr_free(ptr);
    
    ddr_unlock();
    
    return new_ptr;
}

/**
 * ddr_calloc - Allocate zero-initialized DDR memory
 */
void *ddr_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    
    if (total == 0 || nmemb == 0 || size == 0) {
        return NULL;
    }
    
    /* Check for overflow */
    if (total / nmemb != size) {
        fprintf(stderr, "DDR Manager: Integer overflow in calloc\n");
        return NULL;
    }
    
    return ddr_alloc(total, DDR_ALLOC_FLAG_ZERO_INIT);
}

/* ============================================================================
 * Information Functions
 * ============================================================================ */

/**
 * ddr_get_info - Get DDR information
 */
int ddr_get_info(struct ddr_info *info)
{
    int ret;
    
    if (!info) {
        return DDR_ERR_INVALID;
    }
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    ddr_lock();
    
    ret = ioctl(g_ctx.fd, DDR_IOCTL_GET_INFO, info);
    if (ret < 0) {
        ret = -errno;
        ddr_unlock();
        return ret;
    }
    
    /* Cache info */
    memcpy(&g_ctx.info, info, sizeof(*info));
    
    ddr_unlock();
    
    return 0;
}

/**
 * ddr_get_stats - Get DDR statistics
 */
int ddr_get_stats(struct ddr_stats *stats)
{
    int ret;
    
    if (!stats) {
        return DDR_ERR_INVALID;
    }
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    ddr_lock();
    
    ret = ioctl(g_ctx.fd, DDR_IOCTL_GET_STATS, stats);
    if (ret < 0) {
        ret = -errno;
        ddr_unlock();
        return ret;
    }
    
    /* Cache stats */
    memcpy(&g_ctx.stats, stats, sizeof(*stats));
    
    ddr_unlock();
    
    return 0;
}

/**
 * ddr_get_config - Get DDR configuration
 */
int ddr_get_config(struct ddr_config *config)
{
    int ret;
    
    if (!config) {
        return DDR_ERR_INVALID;
    }
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    ddr_lock();
    
    ret = ioctl(g_ctx.fd, DDR_IOCTL_GET_CONFIG, config);
    if (ret < 0) {
        ret = -errno;
        ddr_unlock();
        return ret;
    }
    
    /* Cache config */
    memcpy(&g_ctx.config, config, sizeof(*config));
    
    ddr_unlock();
    
    return 0;
}

/**
 * ddr_set_config - Set DDR configuration
 */
int ddr_set_config(struct ddr_config *config)
{
    int ret;
    
    if (!config) {
        return DDR_ERR_INVALID;
    }
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    ddr_lock();
    
    ret = ioctl(g_ctx.fd, DDR_IOCTL_SET_CONFIG, config);
    if (ret < 0) {
        ret = -errno;
        ddr_unlock();
        return ret;
    }
    
    /* Update cached config */
    memcpy(&g_ctx.config, config, sizeof(*config));
    
    ddr_unlock();
    
    return 0;
}

/**
 * ddr_get_total_memory - Get total DDR memory size
 */
unsigned int ddr_get_total_memory(void)
{
    struct ddr_info info;
    
    if (ddr_get_info(&info) < 0) {
        return 0;
    }
    
    return info.total_memory;
}

/**
 * ddr_get_used_memory - Get used DDR memory size
 */
unsigned int ddr_get_used_memory(void)
{
    struct ddr_info info;
    
    if (ddr_get_info(&info) < 0) {
        return 0;
    }
    
    return info.used_memory;
}

/**
 * ddr_get_free_memory - Get free DDR memory size
 */
unsigned int ddr_get_free_memory(void)
{
    struct ddr_info info;
    
    if (ddr_get_info(&info) < 0) {
        return 0;
    }
    
    return info.free_memory;
}

/* ============================================================================
 * Control Functions
 * ============================================================================ */

/**
 * ddr_reset - Reset DDR controller
 */
int ddr_reset(void)
{
    int ret;
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    ddr_lock();
    
    ret = ioctl(g_ctx.fd, DDR_IOCTL_RESET);
    if (ret < 0) {
        ret = -errno;
    }
    
    ddr_unlock();
    
    return ret;
}

/**
 * ddr_calibrate - Calibrate DDR controller
 */
int ddr_calibrate(void)
{
    int ret;
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    ddr_lock();
    
    ret = ioctl(g_ctx.fd, DDR_IOCTL_CALIBRATE);
    if (ret < 0) {
        ret = -errno;
    }
    
    ddr_unlock();
    
    return ret;
}

/**
 * ddr_set_voltage - Set DDR voltage
 */
int ddr_set_voltage(unsigned int voltage_mv)
{
    struct ddr_config config;
    int ret;
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    /* Get current config */
    ret = ddr_get_config(&config);
    if (ret < 0) {
        return ret;
    }
    
    /* Update voltage */
    config.voltage_mv = voltage_mv;
    
    /* Set config */
    return ddr_set_config(&config);
}

/**
 * ddr_set_frequency - Set DDR frequency
 */
int ddr_set_frequency(unsigned int frequency_mhz)
{
    struct ddr_config config;
    int ret;
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    /* Get current config */
    ret = ddr_get_config(&config);
    if (ret < 0) {
        return ret;
    }
    
    /* Update frequency */
    config.frequency_mhz = frequency_mhz;
    
    /* Set config */
    return ddr_set_config(&config);
}

/**
 * ddr_set_power_save - Enable/disable power save mode
 */
int ddr_set_power_save(bool enable)
{
    struct ddr_config config;
    int ret;
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    /* Get current config */
    ret = ddr_get_config(&config);
    if (ret < 0) {
        return ret;
    }
    
    /* Update power save */
    config.power_save = enable ? 1 : 0;
    
    /* Set config */
    return ddr_set_config(&config);
}

/**
 * ddr_set_performance_mode - Enable/disable performance mode
 */
int ddr_set_performance_mode(bool enable)
{
    struct ddr_config config;
    int ret;
    
    if (!ddr_is_initialized()) {
        return DDR_ERR_NOT_INIT;
    }
    
    /* Get current config */
    ret = ddr_get_config(&config);
    if (ret < 0) {
        return ret;
    }
    
    /* Update performance mode */
    config.performance_mode = enable ? 1 : 0;
    
    /* Set config */
    return ddr_set_config(&config);
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

/**
 * ddr_dump_info - Dump DDR information
 */
void ddr_dump_info(void)
{
    struct ddr_info info;
    
    if (ddr_get_info(&info) < 0) {
        printf("DDR Manager: Failed to get info\n");
        return;
    }
    
    printf("\n=== DDR Information ===\n");
    printf("Total Memory: %u MB\n", info.total_memory);
    printf("Used Memory: %u MB\n", info.used_memory);
    printf("Free Memory: %u MB\n", info.free_memory);
    printf("Frequency: %u MHz\n", info.frequency);
    printf("Voltage: %u mV\n", info.voltage);
    printf("Temperature: %u C\n", info.temperature);
    printf("Status: 0x%04x\n", info.status);
    printf("Errors: %u\n", info.errors);
    printf("========================\n");
}

/**
 * ddr_dump_stats - Dump DDR statistics
 */
void ddr_dump_stats(void)
{
    struct ddr_stats stats;
    
    if (ddr_get_stats(&stats) < 0) {
        printf("DDR Manager: Failed to get stats\n");
        return;
    }
    
    printf("\n=== DDR Statistics ===\n");
    printf("Total Allocations: %lu\n", stats.total_allocations);
    printf("Total Frees: %lu\n", stats.total_frees);
    printf("Current Allocations: %lu\n", stats.current_allocations);
    printf("Peak Allocations: %lu\n", stats.peak_allocations);
    printf("Total Allocated: %lu MB\n", stats.total_allocated / (1024 * 1024));
    printf("Total Freed: %lu MB\n", stats.total_freed / (1024 * 1024));
    printf("Current Allocated: %lu MB\n", stats.current_allocated / (1024 * 1024));
    printf("Peak Allocated: %lu MB\n", stats.peak_allocated / (1024 * 1024));
    printf("Errors: %u\n", stats.errors);
    printf("Warnings: %u\n", stats.warnings);
    printf("Fragmentation: %u%%\n", stats.fragmentation);
    printf("========================\n");
}

/**
 * ddr_dump_config - Dump DDR configuration
 */
void ddr_dump_config(void)
{
    struct ddr_config config;
    
    if (ddr_get_config(&config) < 0) {
        printf("DDR Manager: Failed to get config\n");
        return;
    }
    
    printf("\n=== DDR Configuration ===\n");
    printf("Name: %s\n", config.name);
    printf("Type: %s\n", ddr_get_type_string(config.type));
    printf("Size: %u MB\n", config.size_mb);
    printf("Channels: %u\n", config.channels);
    printf("Frequency: %u MHz\n", config.frequency_mhz);
    printf("Voltage: %u mV\n", config.voltage_mv);
    printf("Timings: CL%u-tRCD%u-tRP%u-tRAS%u\n",
           config.timings.tCL, config.timings.tRCD,
           config.timings.tRP, config.timings.tRAS);
    printf("ECC: %s\n", config.ecc_enabled ? "Enabled" : "Disabled");
    printf("Power Save: %s\n", config.power_save ? "Enabled" : "Disabled");
    printf("Performance Mode: %s\n", config.performance_mode ? "Enabled" : "Disabled");
    printf("============================\n");
}

/**
 * ddr_hexdump - Dump memory in hex format
 */
void ddr_hexdump(const void *ptr, size_t size, const char *prefix)
{
    const unsigned char *bytes = (const unsigned char *)ptr;
    size_t i, j;
    char line[128];
    int offset = 0;
    
    if (!ptr || size == 0) {
        return;
    }
    
    if (!prefix) {
        prefix = "";
    }
    
    for (i = 0; i < size; i += 16) {
        offset = snprintf(line, sizeof(line), "%s0x%08zx: ", prefix, i);
        
        for (j = 0; j < 16 && i + j < size; j++) {
            offset += snprintf(line + offset, sizeof(line) - offset,
                             "%02x ", bytes[i + j]);
        }
        
        for (; j < 16; j++) {
            offset += snprintf(line + offset, sizeof(line) - offset, "   ");
        }
        
        offset += snprintf(line + offset, sizeof(line) - offset, " ");
        
        for (j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = bytes[i + j];
            if (c >= 0x20 && c <= 0x7E) {
                offset += snprintf(line + offset, sizeof(line) - offset, "%c", c);
            } else {
                offset += snprintf(line + offset, sizeof(line) - offset, ".");
            }
        }
        
        printf("%s\n", line);
    }
}

/**
 * ddr_memtest - Test memory region
 */
int ddr_memtest(void *ptr, size_t size, int iterations)
{
    unsigned char *bytes = (unsigned char *)ptr;
    size_t i;
    int ret = 0;
    
    if (!ptr || size == 0) {
        return DDR_ERR_INVALID;
    }
    
    for (int iter = 0; iter < iterations; iter++) {
        /* Write pattern */
        for (i = 0; i < size; i++) {
            bytes[i] = (i + iter) & 0xFF;
        }
        
        /* Verify pattern */
        for (i = 0; i < size; i++) {
            if (bytes[i] != ((i + iter) & 0xFF)) {
                fprintf(stderr, "DDR memtest: Mismatch at offset %zu: expected 0x%02x, got 0x%02x\n",
                       i, (unsigned char)((i + iter) & 0xFF), bytes[i]);
                ret = DDR_ERR_IO;
                break;
            }
        }
        
        if (ret < 0) {
            break;
        }
    }
    
    return ret;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * ddr_get_error_string - Get error string for error code
 */
const char *ddr_get_error_string(int err)
{
    switch (err) {
        case DDR_ERR_SUCCESS:      return "Success";
        case DDR_ERR_INVALID:      return "Invalid argument";
        case DDR_ERR_NOMEM:        return "Out of memory";
        case DDR_ERR_IO:           return "I/O error";
        case DDR_ERR_BUSY:         return "Resource busy";
        case DDR_ERR_NOT_SUPPORTED: return "Not supported";
        case DDR_ERR_NOT_INIT:     return "Not initialized";
        case DDR_ERR_PERM:         return "Permission denied";
        case DDR_ERR_FAULT:        return "Bad address";
        case DDR_ERR_TIMEOUT:      return "Operation timeout";
        case DDR_ERR_OVERFLOW:     return "Integer overflow";
        default:                   return "Unknown error";
    }
}

/**
 * ddr_get_type_string - Get DDR type string
 */
const char *ddr_get_type_string(enum ddr_memory_type type)
{
    switch (type) {
        case DDR_MEMORY_TYPE_DDR3:      return "DDR3";
        case DDR_MEMORY_TYPE_DDR4:      return "DDR4";
        case DDR_MEMORY_TYPE_LPDDR3:    return "LPDDR3";
        case DDR_MEMORY_TYPE_LPDDR4:    return "LPDDR4";
        case DDR_MEMORY_TYPE_LPDDR4X:   return "LPDDR4X";
        default:                        return "Unknown";
    }
}

/**
 * ddr_get_flag_string - Get allocation flag string
 */
const char *ddr_get_flag_string(unsigned int flags)
{
    static char buffer[256];
    int offset = 0;
    
    buffer[0] = '\0';
    
    if (flags & DDR_ALLOC_FLAG_DMA) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "DMA ");
    }
    if (flags & DDR_ALLOC_FLAG_CACHED) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "CACHED ");
    }
    if (flags & DDR_ALLOC_FLAG_WRITE_COMBINE) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "WRITE_COMBINE ");
    }
    if (flags & DDR_ALLOC_FLAG_READ_ONLY) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "READ_ONLY ");
    }
    if (flags & DDR_ALLOC_FLAG_WRITE_ONLY) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "WRITE_ONLY ");
    }
    if (flags & DDR_ALLOC_FLAG_SECURE) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "SECURE ");
    }
    if (flags & DDR_ALLOC_FLAG_CONTIGUOUS) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "CONTIGUOUS ");
    }
    if (flags & DDR_ALLOC_FLAG_ZERO_INIT) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "ZERO_INIT ");
    }
    
    if (offset == 0) {
        return "NONE";
    }
    
    return buffer;
}

/**
 * ddr_snprintf_info - Print DDR info to string
 */
int ddr_snprintf_info(char *buffer, size_t size)
{
    struct ddr_info info;
    int ret;
    
    if (!buffer || size == 0) {
        return DDR_ERR_INVALID;
    }
    
    if (ddr_get_info(&info) < 0) {
        return snprintf(buffer, size, "Failed to get DDR info");
    }
    
    ret = snprintf(buffer, size,
                   "Total: %u MB, Used: %u MB, Free: %u MB, "
                   "Freq: %u MHz, Volt: %u mV, Temp: %u C",
                   info.total_memory, info.used_memory, info.free_memory,
                   info.frequency, info.voltage, info.temperature);
    
    return ret;
}

/**
 * ddr_snprintf_stats - Print DDR stats to string
 */
int ddr_snprintf_stats(char *buffer, size_t size)
{
    struct ddr_stats stats;
    int ret;
    
    if (!buffer || size == 0) {
        return DDR_ERR_INVALID;
    }
    
    if (ddr_get_stats(&stats) < 0) {
        return snprintf(buffer, size, "Failed to get DDR stats");
    }
    
    ret = snprintf(buffer, size,
                   "Allocs: %lu, Frees: %lu, Active: %lu, "
                   "Alloc: %lu MB, Frag: %u%%",
                   stats.total_allocations, stats.total_frees,
                   stats.current_allocations,
                   stats.current_allocated / (1024 * 1024),
                   stats.fragmentation);
    
    return ret;
}

/* ============================================================================
 * Performance Monitoring
 * ============================================================================ */

/**
 * ddr_perf_start - Start performance measurement
 */
uint64_t ddr_perf_start(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * ddr_perf_stop - Stop performance measurement
 */
uint64_t ddr_perf_stop(uint64_t start_ts)
{
    uint64_t end_ts = ddr_perf_start();
    return end_ts - start_ts;
}

/**
 * ddr_perf_measure - Measure performance of an operation
 */
uint64_t ddr_perf_measure(const char *operation, void *(*func)(void *), void *arg)
{
    uint64_t start, elapsed;
    void *result;
    
    printf("Measuring: %s...\n", operation);
    
    start = ddr_perf_start();
    result = func(arg);
    elapsed = ddr_perf_stop(start);
    
    printf("  Elapsed: %.3f ms\n", elapsed / 1000000.0);
    
    return elapsed;
}

/* ============================================================================
 * Module Information
 * ============================================================================ */

/**
 * ddr_manager_self_test - Self-test function
 */
static void ddr_manager_self_test(void)
{
    void *ptr1, *ptr2, *ptr3;
    struct ddr_info info;
    
    printf("\n=== DDR Manager Self-Test ===\n");
    
    /* Test 1: Basic allocation */
    printf("Test 1: Basic allocation...\n");
    ptr1 = ddr_alloc(1024, 0);
    if (ptr1) {
        memset(ptr1, 0xAA, 1024);
        printf("  ✓ Allocated and filled 1024 bytes\n");
        ddr_free(ptr1);
        printf("  ✓ Freed memory\n");
    } else {
        printf("  ✗ Allocation failed\n");
    }
    
    /* Test 2: Aligned allocation */
    printf("Test 2: Aligned allocation...\n");
    ptr2 = ddr_alloc_aligned(4096, 64, 0);
    if (ptr2) {
        if (((uintptr_t)ptr2 % 64) == 0) {
            printf("  ✓ Memory aligned to 64 bytes\n");
        } else {
            printf("  ✗ Memory not aligned\n");
        }
        ddr_free(ptr2);
        printf("  ✓ Freed memory\n");
    } else {
        printf("  ✗ Allocation failed\n");
    }
    
    /* Test 3: Multiple allocations */
    printf("Test 3: Multiple allocations...\n");
    ptr1 = ddr_alloc(1024, 0);
    ptr2 = ddr_alloc(2048, 0);
    ptr3 = ddr_alloc(4096, 0);
    
    if (ptr1 && ptr2 && ptr3) {
        printf("  ✓ Allocated 3 blocks\n");
        ddr_free(ptr1);
        ddr_free(ptr2);
        ddr_free(ptr3);
        printf("  ✓ Freed all blocks\n");
    } else {
        printf("  ✗ Some allocations failed\n");
    }
    
    /* Test 4: Get info */
    printf("Test 4: Get info...\n");
    if (ddr_get_info(&info) == 0) {
        printf("  ✓ Total memory: %u MB\n", info.total_memory);
        printf("  ✓ Used memory: %u MB\n", info.used_memory);
    } else {
        printf("  ✗ Failed to get info\n");
    }
    
    printf("=== Self-Test Complete ===\n");
}

/* ============================================================================
 * Library Initialization
 * ============================================================================ */

/**
 * __attribute__((constructor)) - Library constructor
 */
static void __attribute__((constructor)) ddr_library_init(void)
{
    /* Initialize library - this is called when the library is loaded */
    ddr_init();
}

/**
 * __attribute__((destructor)) - Library destructor
 */
static void __attribute__((destructor)) ddr_library_cleanup(void)
{
    /* Clean up library - this is called when the library is unloaded */
    ddr_cleanup();
}
