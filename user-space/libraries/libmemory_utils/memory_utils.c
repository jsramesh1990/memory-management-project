/**
 * memory_utils.c - Memory Utilities Library Implementation
 * 
 * This file implements utility functions for memory management,
 * debugging, and performance monitoring.
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
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <math.h>

#include "memory_utils.h"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MAX_POOLS       16
#define MAX_TRACES      1000
#define MAX_BLOCKS      10000
#define ALIGN_MASK      (MEMORY_UTILS_ALIGNMENT - 1)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * struct memory_pool - Memory pool
 */
struct memory_pool {
    char name[64];
    void *start;
    void *end;
    void *current;
    size_t size;
    size_t used;
    size_t align;
    int initialized;
    pthread_mutex_t lock;
};

/**
 * struct memory_utils_ctx - Memory utilities context
 */
struct memory_utils_ctx {
    int initialized;
    struct memory_utils_config config;
    struct memory_block *blocks;
    struct memory_trace traces[MAX_TRACES];
    int trace_count;
    struct memory_pool pools[MAX_POOLS];
    int pool_count;
    struct memory_stats stats;
    pthread_mutex_t lock;
    FILE *log_file;
};

/* ============================================================================
 * Internal Variables
 * ============================================================================ */

static struct memory_utils_ctx g_ctx = {
    .initialized = 0,
    .blocks = NULL,
    .trace_count = 0,
    .pool_count = 0,
    .log_file = NULL,
};

static pthread_once_t g_once = PTHREAD_ONCE_INIT;

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/**
 * memory_utils_lock - Lock the context
 */
static inline void memory_utils_lock(void)
{
    pthread_mutex_lock(&g_ctx.lock);
}

/**
 * memory_utils_unlock - Unlock the context
 */
static inline void memory_utils_unlock(void)
{
    pthread_mutex_unlock(&g_ctx.lock);
}

/**
 * memory_utils_init_once - One-time initialization
 */
static void memory_utils_init_once(void)
{
    pthread_mutex_init(&g_ctx.lock, NULL);
}

/**
 * memory_utils_timestamp - Get timestamp string
 */
static void memory_utils_timestamp(char *buffer, size_t size)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
}

/**
 * memory_utils_block_remove - Remove block from list
 */
static void memory_utils_block_remove(struct memory_block *block)
{
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        g_ctx.blocks = block->next;
    }
    
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    free(block);
}

/**
 * memory_utils_block_find_by_ptr - Find block by pointer
 */
static struct memory_block *memory_utils_block_find_by_ptr(void *ptr)
{
    struct memory_block *block = g_ctx.blocks;
    
    while (block) {
        if (block->address == ptr) {
            return block;
        }
        block = block->next;
    }
    
    return NULL;
}

/**
 * memory_utils_log_write - Write to log
 */
static void memory_utils_log_write(int level, const char *msg)
{
    char timestamp[32];
    const char *level_str;
    
    switch (level) {
        case LOG_LEVEL_ERROR:   level_str = "ERROR"; break;
        case LOG_LEVEL_WARNING: level_str = "WARN"; break;
        case LOG_LEVEL_INFO:    level_str = "INFO"; break;
        case LOG_LEVEL_DEBUG:   level_str = "DEBUG"; break;
        case LOG_LEVEL_TRACE:   level_str = "TRACE"; break;
        default:                level_str = "UNKNOWN"; break;
    }
    
    memory_utils_timestamp(timestamp, sizeof(timestamp));
    
    if (g_ctx.log_file) {
        fprintf(g_ctx.log_file, "[%s] [%s] %s\n", timestamp, level_str, msg);
        fflush(g_ctx.log_file);
    }
    
    if (g_ctx.config.log_level >= level) {
        fprintf(stderr, "[%s] [%s] %s\n", timestamp, level_str, msg);
    }
}

/* ============================================================================
 * Core Functions
 * ============================================================================ */

/**
 * memory_utils_init - Initialize memory utilities
 */
int memory_utils_init(struct memory_utils_config *config)
{
    int ret = 0;
    
    /* One-time initialization */
    pthread_once(&g_once, memory_utils_init_once);
    
    memory_utils_lock();
    
    if (g_ctx.initialized) {
        memory_utils_unlock();
        return MEM_UTILS_SUCCESS;
    }
    
    /* Initialize configuration */
    if (config) {
        memcpy(&g_ctx.config, config, sizeof(g_ctx.config));
    } else {
        /* Default configuration */
        memset(&g_ctx.config, 0, sizeof(g_ctx.config));
        g_ctx.config.log_level = LOG_LEVEL_INFO;
        g_ctx.config.trace_enabled = 0;
        g_ctx.config.leak_detection_enabled = 1;
        g_ctx.config.block_tracking_enabled = 1;
        g_ctx.config.auto_cleanup_enabled = 1;
        g_ctx.config.min_trace_size = 0;
        g_ctx.config.max_trace_size = 0;
        g_ctx.config.log_to_file = 0;
        strcpy(g_ctx.config.log_file, "/tmp/memory_utils.log");
    }
    
    /* Open log file */
    if (g_ctx.config.log_to_file) {
        g_ctx.log_file = fopen(g_ctx.config.log_file, "a");
        if (!g_ctx.log_file) {
            ret = MEM_UTILS_ERR_IO;
            goto error;
        }
    }
    
    /* Initialize statistics */
    memset(&g_ctx.stats, 0, sizeof(g_ctx.stats));
    clock_gettime(CLOCK_MONOTONIC, &g_ctx.stats.start_time);
    
    g_ctx.initialized = 1;
    
    memory_utils_log_info("Memory utilities initialized (version %s)", 
                         MEMORY_UTILS_VERSION_STRING);
    
    memory_utils_unlock();
    return MEM_UTILS_SUCCESS;

error:
    if (g_ctx.log_file) {
        fclose(g_ctx.log_file);
        g_ctx.log_file = NULL;
    }
    memory_utils_unlock();
    return ret;
}

/**
 * memory_utils_cleanup - Clean up memory utilities
 */
void memory_utils_cleanup(void)
{
    struct memory_block *block, *next_block;
    
    memory_utils_lock();
    
    if (!g_ctx.initialized) {
        memory_utils_unlock();
        return;
    }
    
    /* Check for leaks */
    if (g_ctx.config.leak_detection_enabled) {
        int leaks = memory_utils_detect_leaks(0);
        if (leaks > 0) {
            memory_utils_log_warning("%d memory leaks detected", leaks);
            memory_utils_print_leaks();
        }
    }
    
    /* Free all blocks */
    block = g_ctx.blocks;
    while (block) {
        next_block = block->next;
        if (block->address) {
            free(block->address);
        }
        free(block);
        block = next_block;
    }
    g_ctx.blocks = NULL;
    
    /* Destroy all pools */
    for (int i = 0; i < g_ctx.pool_count; i++) {
        if (g_ctx.pools[i].initialized) {
            memory_utils_pool_destroy(i);
        }
    }
    g_ctx.pool_count = 0;
    
    /* Close log file */
    if (g_ctx.log_file) {
        fclose(g_ctx.log_file);
        g_ctx.log_file = NULL;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &g_ctx.stats.end_time);
    g_ctx.initialized = 0;
    
    memory_utils_log_info("Memory utilities cleaned up");
    
    memory_utils_unlock();
}

/**
 * memory_utils_is_initialized - Check if initialized
 */
bool memory_utils_is_initialized(void)
{
    bool ret;
    memory_utils_lock();
    ret = g_ctx.initialized;
    memory_utils_unlock();
    return ret;
}

/**
 * memory_utils_get_config - Get current configuration
 */
int memory_utils_get_config(struct memory_utils_config *config)
{
    if (!config) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    memory_utils_lock();
    memcpy(config, &g_ctx.config, sizeof(*config));
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_set_config - Set configuration
 */
int memory_utils_set_config(struct memory_utils_config *config)
{
    if (!config) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    memory_utils_lock();
    memcpy(&g_ctx.config, config, sizeof(*config));
    memory_utils_unlock();
    
    memory_utils_log_info("Configuration updated");
    
    return MEM_UTILS_SUCCESS;
}

/* ============================================================================
 * Memory Tracking Functions
 * ============================================================================ */

/**
 * memory_utils_track_alloc - Track memory allocation
 */
int memory_utils_track_alloc(void *ptr, size_t size, 
                             const char *file, int line,
                             const char *function)
{
    struct memory_block *block;
    
    if (!ptr || size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    if (!g_ctx.config.block_tracking_enabled) {
        return MEM_UTILS_SUCCESS;
    }
    
    memory_utils_lock();
    
    block = calloc(1, sizeof(*block));
    if (!block) {
        memory_utils_unlock();
        return MEM_UTILS_ERR_NOMEM;
    }
    
    block->address = ptr;
    block->size = size;
    block->alloc_time = time(NULL);
    
    if (file) {
        strncpy(block->file, file, sizeof(block->file) - 1);
    }
    block->line = line;
    if (function) {
        strncpy(block->function, function, sizeof(block->function) - 1);
    }
    
    /* Add to list */
    block->next = g_ctx.blocks;
    if (g_ctx.blocks) {
        g_ctx.blocks->prev = block;
    }
    g_ctx.blocks = block;
    
    /* Update statistics */
    g_ctx.stats.total_allocated += size;
    g_ctx.stats.current_allocated += size;
    g_ctx.stats.total_blocks++;
    g_ctx.stats.current_blocks++;
    
    if (g_ctx.stats.current_allocated > g_ctx.stats.peak_allocated) {
        g_ctx.stats.peak_allocated = g_ctx.stats.current_allocated;
    }
    if (g_ctx.stats.current_blocks > g_ctx.stats.peak_blocks) {
        g_ctx.stats.peak_blocks = g_ctx.stats.current_blocks;
    }
    
    memory_utils_log_trace("Alloc: %p (%zu bytes) at %s:%d", 
                          ptr, size, file ? file : "unknown", line);
    
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_track_free - Track memory free
 */
int memory_utils_track_free(void *ptr)
{
    struct memory_block *block;
    
    if (!ptr) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    if (!g_ctx.config.block_tracking_enabled) {
        return MEM_UTILS_SUCCESS;
    }
    
    memory_utils_lock();
    
    block = memory_utils_block_find_by_ptr(ptr);
    if (!block) {
        memory_utils_log_warning("Free of untracked pointer: %p", ptr);
        memory_utils_unlock();
        return MEM_UTILS_ERR_NOT_FOUND;
    }
    
    block->free_time = time(NULL);
    
    /* Update statistics */
    g_ctx.stats.total_freed += block->size;
    g_ctx.stats.current_allocated -= block->size;
    g_ctx.stats.current_blocks--;
    
    memory_utils_log_trace("Free: %p (%zu bytes)", ptr, block->size);
    
    /* Remove block from list */
    memory_utils_block_remove(block);
    
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_find_block - Find memory block
 */
int memory_utils_find_block(void *ptr, struct memory_block *block)
{
    struct memory_block *found;
    
    if (!ptr || !block) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    memory_utils_lock();
    
    found = memory_utils_block_find_by_ptr(ptr);
    if (!found) {
        memory_utils_unlock();
        return MEM_UTILS_ERR_NOT_FOUND;
    }
    
    memcpy(block, found, sizeof(*block));
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_get_stats - Get memory statistics
 */
int memory_utils_get_stats(struct memory_stats *stats)
{
    if (!stats) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    memory_utils_lock();
    memcpy(stats, &g_ctx.stats, sizeof(*stats));
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_detect_leaks - Detect memory leaks
 */
int memory_utils_detect_leaks(size_t threshold)
{
    struct memory_block *block;
    int count = 0;
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    if (!g_ctx.config.leak_detection_enabled) {
        return 0;
    }
    
    memory_utils_lock();
    
    block = g_ctx.blocks;
    while (block) {
        if (block->size >= threshold) {
            count++;
        }
        block = block->next;
    }
    
    memory_utils_unlock();
    
    return count;
}

/**
 * memory_utils_print_leaks - Print memory leak report
 */
void memory_utils_print_leaks(void)
{
    struct memory_block *block;
    int count = 0;
    
    if (!g_ctx.initialized) {
        return;
    }
    
    memory_utils_lock();
    
    block = g_ctx.blocks;
    if (!block) {
        memory_utils_log_info("No memory leaks detected");
        memory_utils_unlock();
        return;
    }
    
    memory_utils_log_warning("Memory leak report:");
    
    while (block) {
        char time_str[32];
        struct tm *tm = localtime(&block->alloc_time);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
        
        memory_utils_log_warning("  %p: %zu bytes allocated at %s:%d (%s) [%s]",
                                block->address, block->size,
                                block->file, block->line,
                                block->function, time_str);
        count++;
        block = block->next;
    }
    
    memory_utils_log_warning("Total: %d leaks, %zu bytes",
                            count, g_ctx.stats.current_allocated);
    
    memory_utils_unlock();
}

/**
 * memory_utils_clear_leaks - Clear leak detection data
 */
void memory_utils_clear_leaks(void)
{
    struct memory_block *block, *next;
    
    if (!g_ctx.initialized) {
        return;
    }
    
    memory_utils_lock();
    
    block = g_ctx.blocks;
    while (block) {
        next = block->next;
        free(block);
        block = next;
    }
    g_ctx.blocks = NULL;
    
    g_ctx.stats.current_allocated = 0;
    g_ctx.stats.current_blocks = 0;
    
    memory_utils_log_info("Leak data cleared");
    
    memory_utils_unlock();
}

/* ============================================================================
 * Memory Debug Functions
 * ============================================================================ */

/**
 * memory_utils_memdump - Dump memory in hex format
 */
void memory_utils_memdump(const void *ptr, size_t size, const char *title)
{
    const unsigned char *bytes = (const unsigned char *)ptr;
    char line[128];
    int offset = 0;
    size_t i, j;
    
    if (!ptr || size == 0) {
        return;
    }
    
    if (title) {
        memory_utils_log_info("=== %s ===", title);
    }
    
    for (i = 0; i < size; i += 16) {
        offset = snprintf(line, sizeof(line), "0x%04zx: ", i);
        
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
        
        memory_utils_log_info("%s", line);
    }
}

/**
 * memory_utils_memcheck - Check memory for corruption
 */
int memory_utils_memcheck(const void *ptr, size_t size, unsigned char pattern)
{
    const unsigned char *bytes = (const unsigned char *)ptr;
    size_t i;
    
    if (!ptr || size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    for (i = 0; i < size; i++) {
        if (bytes[i] != pattern) {
            memory_utils_log_warning("Corruption at offset %zu: expected 0x%02x, got 0x%02x",
                                    i, pattern, bytes[i]);
            return MEM_UTILS_ERR_IO;
        }
    }
    
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_memfill - Fill memory with pattern
 */
int memory_utils_memfill(void *ptr, size_t size, unsigned char pattern)
{
    if (!ptr || size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    memset(ptr, pattern, size);
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_memtest - Test memory region
 */
int memory_utils_memtest(void *ptr, size_t size, int iterations)
{
    unsigned char *bytes = (unsigned char *)ptr;
    unsigned char patterns[] = {0x00, 0xFF, 0xAA, 0x55, 0x33, 0xCC};
    int ret = 0;
    size_t i, j;
    
    if (!ptr || size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    for (i = 0; i < (size_t)iterations; i++) {
        unsigned char pattern = patterns[i % (sizeof(patterns) / sizeof(patterns[0]))];
        
        /* Write pattern */
        memset(bytes, pattern, size);
        
        /* Verify pattern */
        for (j = 0; j < size; j++) {
            if (bytes[j] != pattern) {
                memory_utils_log_error("Test failed at offset %zu: expected 0x%02x, got 0x%02x",
                                      j, pattern, bytes[j]);
                ret = MEM_UTILS_ERR_IO;
                break;
            }
        }
        
        if (ret < 0) {
            break;
        }
    }
    
    return ret;
}

/**
 * memory_utils_memcmp_pattern - Compare memory to pattern
 */
int memory_utils_memcmp_pattern(const void *ptr, size_t size, unsigned char pattern)
{
    const unsigned char *bytes = (const unsigned char *)ptr;
    size_t i;
    
    if (!ptr || size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    for (i = 0; i < size; i++) {
        if (bytes[i] != pattern) {
            return MEM_UTILS_ERR_IO;
        }
    }
    
    return MEM_UTILS_SUCCESS;
}

/**
 * memory_utils_memfind - Find pattern in memory
 */
int memory_utils_memfind(const void *haystack, size_t haystack_size,
                         const void *needle, size_t needle_size,
                         size_t *offset)
{
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    
    if (!haystack || !needle || haystack_size == 0 || needle_size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (needle_size > haystack_size) {
        return MEM_UTILS_ERR_NOT_FOUND;
    }
    
    for (size_t i = 0; i <= haystack_size - needle_size; i++) {
        bool found = true;
        for (size_t j = 0; j < needle_size; j++) {
            if (h[i + j] != n[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            if (offset) {
                *offset = i;
            }
            return MEM_UTILS_SUCCESS;
        }
    }
    
    return MEM_UTILS_ERR_NOT_FOUND;
}

/* ============================================================================
 * Memory Access Functions
 * ============================================================================ */

uint8_t memory_utils_read8(volatile void *addr)
{
    return *(volatile uint8_t *)addr;
}

uint16_t memory_utils_read16(volatile void *addr)
{
    return *(volatile uint16_t *)addr;
}

uint32_t memory_utils_read32(volatile void *addr)
{
    return *(volatile uint32_t *)addr;
}

uint64_t memory_utils_read64(volatile void *addr)
{
    return *(volatile uint64_t *)addr;
}

void memory_utils_write8(volatile void *addr, uint8_t value)
{
    *(volatile uint8_t *)addr = value;
}

void memory_utils_write16(volatile void *addr, uint16_t value)
{
    *(volatile uint16_t *)addr = value;
}

void memory_utils_write32(volatile void *addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

void memory_utils_write64(volatile void *addr, uint64_t value)
{
    *(volatile uint64_t *)addr = value;
}

/* ============================================================================
 * Performance Monitoring
 * ============================================================================ */

uint64_t memory_utils_get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

uint64_t memory_utils_get_time_us(void)
{
    return memory_utils_get_time_ns() / 1000;
}

uint64_t memory_utils_get_time_ms(void)
{
    return memory_utils_get_time_ns() / 1000000;
}

uint64_t memory_utils_measure_start(void)
{
    return memory_utils_get_time_ns();
}

uint64_t memory_utils_measure_stop(uint64_t start)
{
    return memory_utils_get_time_ns() - start;
}

uint64_t memory_utils_measure_operation(const char *name,
                                        void *(*func)(void *),
                                        void *arg)
{
    uint64_t start, elapsed;
    void *result;
    
    memory_utils_log_info("Measuring: %s...", name);
    
    start = memory_utils_measure_start();
    result = func(arg);
    elapsed = memory_utils_measure_stop(start);
    
    memory_utils_log_info("  Elapsed: %.3f ms", elapsed / 1000000.0);
    
    return elapsed;
}

/* ============================================================================
 * Memory Pool Functions
 * ============================================================================ */

int memory_utils_pool_create(const char *name, size_t size, size_t align)
{
    struct memory_pool *pool;
    void *memory;
    
    if (!name || size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (!g_ctx.initialized) {
        return MEM_UTILS_ERR_NOT_INIT;
    }
    
    if (g_ctx.pool_count >= MAX_POOLS) {
        return MEM_UTILS_ERR_OVERFLOW;
    }
    
    memory_utils_lock();
    
    pool = &g_ctx.pools[g_ctx.pool_count];
    
    /* Allocate memory */
    memory = malloc(size);
    if (!memory) {
        memory_utils_unlock();
        return MEM_UTILS_ERR_NOMEM;
    }
    
    pool->start = memory;
    pool->end = memory + size;
    pool->current = memory;
    pool->size = size;
    pool->used = 0;
    pool->align = align ? align : MEMORY_UTILS_ALIGNMENT;
    pool->initialized = 1;
    strncpy(pool->name, name, sizeof(pool->name) - 1);
    pthread_mutex_init(&pool->lock, NULL);
    
    g_ctx.pool_count++;
    
    memory_utils_log_info("Pool created: %s (%zu bytes)", name, size);
    
    memory_utils_unlock();
    
    return g_ctx.pool_count - 1;
}

int memory_utils_pool_destroy(int pool_id)
{
    struct memory_pool *pool;
    
    if (pool_id < 0 || pool_id >= g_ctx.pool_count) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    memory_utils_lock();
    
    pool = &g_ctx.pools[pool_id];
    if (!pool->initialized) {
        memory_utils_unlock();
        return MEM_UTILS_ERR_NOT_FOUND;
    }
    
    if (pool->start) {
        free(pool->start);
    }
    
    memset(pool, 0, sizeof(*pool));
    g_ctx.pool_count--;
    
    memory_utils_log_info("Pool destroyed");
    
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

void *memory_utils_pool_alloc(int pool_id, size_t size)
{
    struct memory_pool *pool;
    void *ptr;
    uintptr_t aligned;
    
    if (pool_id < 0 || pool_id >= g_ctx.pool_count) {
        return NULL;
    }
    
    memory_utils_lock();
    
    pool = &g_ctx.pools[pool_id];
    if (!pool->initialized) {
        memory_utils_unlock();
        return NULL;
    }
    
    pthread_mutex_lock(&pool->lock);
    
    /* Align pointer */
    aligned = memory_utils_align_up((uintptr_t)pool->current, pool->align);
    ptr = (void *)aligned;
    
    /* Check if enough space */
    if ((char *)ptr + size > (char *)pool->end) {
        pthread_mutex_unlock(&pool->lock);
        memory_utils_unlock();
        return NULL;
    }
    
    pool->current = (char *)ptr + size;
    pool->used = (char *)pool->current - (char *)pool->start;
    
    pthread_mutex_unlock(&pool->lock);
    
    memory_utils_unlock();
    
    return ptr;
}

int memory_utils_pool_free(int pool_id, void *ptr)
{
    /* Pool doesn't support individual frees - all freed at once */
    if (pool_id < 0 || pool_id >= g_ctx.pool_count) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    return MEM_UTILS_SUCCESS;
}

int memory_utils_pool_stats(int pool_id, size_t *used, size_t *free)
{
    struct memory_pool *pool;
    
    if (pool_id < 0 || pool_id >= g_ctx.pool_count) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    memory_utils_lock();
    
    pool = &g_ctx.pools[pool_id];
    if (!pool->initialized) {
        memory_utils_unlock();
        return MEM_UTILS_ERR_NOT_FOUND;
    }
    
    if (used) {
        *used = pool->used;
    }
    if (free) {
        *free = pool->size - pool->used;
    }
    
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

int memory_utils_pool_reset(int pool_id)
{
    struct memory_pool *pool;
    
    if (pool_id < 0 || pool_id >= g_ctx.pool_count) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    memory_utils_lock();
    
    pool = &g_ctx.pools[pool_id];
    if (!pool->initialized) {
        memory_utils_unlock();
        return MEM_UTILS_ERR_NOT_FOUND;
    }
    
    pthread_mutex_lock(&pool->lock);
    
    pool->current = pool->start;
    pool->used = 0;
    
    pthread_mutex_unlock(&pool->lock);
    
    memory_utils_log_info("Pool reset: %s", pool->name);
    
    memory_utils_unlock();
    
    return MEM_UTILS_SUCCESS;
}

/* ============================================================================
 * Logging Functions
 * ============================================================================ */

void memory_utils_log(int level, const char *format, ...)
{
    va_list args;
    char buffer[MEMORY_UTILS_BUFFER_SIZE];
    
    if (!g_ctx.initialized) {
        return;
    }
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    memory_utils_log_write(level, buffer);
}

/* ============================================================================
 * String Utilities
 * ============================================================================ */

char *memory_utils_strdup(const char *str)
{
    char *dup;
    size_t len;
    
    if (!str) {
        return NULL;
    }
    
    len = strlen(str) + 1;
    dup = malloc(len);
    if (!dup) {
        return NULL;
    }
    
    memcpy(dup, str, len);
    
    if (g_ctx.initialized && g_ctx.config.block_tracking_enabled) {
        memory_utils_track_alloc(dup, len, __FILE__, __LINE__, __func__);
    }
    
    return dup;
}

char *memory_utils_strndup(const char *str, size_t n)
{
    char *dup;
    size_t len;
    
    if (!str) {
        return NULL;
    }
    
    len = strlen(str);
    if (len > n) {
        len = n;
    }
    
    dup = malloc(len + 1);
    if (!dup) {
        return NULL;
    }
    
    memcpy(dup, str, len);
    dup[len] = '\0';
    
    if (g_ctx.initialized && g_ctx.config.block_tracking_enabled) {
        memory_utils_track_alloc(dup, len + 1, __FILE__, __LINE__, __func__);
    }
    
    return dup;
}

int memory_utils_hexdump_to_str(const void *ptr, size_t size,
                                char *buffer, size_t buffer_size)
{
    const unsigned char *bytes = (const unsigned char *)ptr;
    int offset = 0;
    size_t i;
    
    if (!ptr || !buffer || buffer_size == 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    for (i = 0; i < size && offset < (int)buffer_size - 3; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "%02x ", bytes[i]);
    }
    
    return offset;
}

int memory_utils_str_to_hex(const char *str, uint8_t *bytes, size_t max_bytes)
{
    size_t len;
    size_t i;
    
    if (!str || !bytes) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    len = strlen(str);
    if (len % 2 != 0) {
        return MEM_UTILS_ERR_INVALID;
    }
    
    if (len / 2 > max_bytes) {
        return MEM_UTILS_ERR_OVERFLOW;
    }
    
    for (i = 0; i < len / 2; i++) {
        char hex[3] = {str[i * 2], str[i * 2 + 1], '\0'};
        char *endptr;
        long val = strtol(hex, &endptr, 16);
        if (*endptr != '\0') {
            return MEM_UTILS_ERR_INVALID;
        }
        bytes[i] = (uint8_t)val;
    }
    
    return len / 2;
}

/* ============================================================================
 * Alignment Utilities
 * ============================================================================ */

uintptr_t memory_utils_align_up(uintptr_t addr, size_t align)
{
    if (align == 0) {
        align = 1;
    }
    return (addr + align - 1) & ~(align - 1);
}

uintptr_t memory_utils_align_down(uintptr_t addr, size_t align)
{
    if (align == 0) {
        align = 1;
    }
    return addr & ~(align - 1);
}

bool memory_utils_is_aligned(uintptr_t addr, size_t align)
{
    if (align == 0) {
        return true;
    }
    return (addr & (align - 1)) == 0;
}

bool memory_utils_is_power_of_two(size_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}

size_t memory_utils_next_power_of_two(size_t n)
{
    size_t power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

/* ============================================================================
 * Cache Operations
 * ============================================================================ */

int memory_utils_cache_flush(void *ptr, size_t size)
{
    #ifdef __arm__
    asm volatile (
        "mcr p15, 0, %0, c7, c10, 1\n"  /* DCCMVAC */
        : : "r" (ptr)
    );
    #elif defined(__aarch64__)
    asm volatile (
        "dc cvac, %0\n"
        : : "r" (ptr)
    );
    #endif
    return MEM_UTILS_SUCCESS;
}

int memory_utils_cache_invalidate(void *ptr, size_t size)
{
    #ifdef __arm__
    asm volatile (
        "mcr p15, 0, %0, c7, c6, 1\n"  /* DCIMVAC */
        : : "r" (ptr)
    );
    #elif defined(__aarch64__)
    asm volatile (
        "dc ivac, %0\n"
        : : "r" (ptr)
    );
    #endif
    return MEM_UTILS_SUCCESS;
}

int memory_utils_cache_clean(void *ptr, size_t size)
{
    return memory_utils_cache_flush(ptr, size);
}

/* ============================================================================
 * Library Initialization
 * ============================================================================ */

/**
 * __attribute__((constructor)) - Library constructor
 */
static void __attribute__((constructor)) memory_utils_library_init(void)
{
    struct memory_utils_config config;
    
    /* Default configuration */
    memset(&config, 0, sizeof(config));
    config.log_level = LOG_LEVEL_INFO;
    config.trace_enabled = 0;
    config.leak_detection_enabled = 1;
    config.block_tracking_enabled = 1;
    config.auto_cleanup_enabled = 1;
    config.log_to_file = 0;
    strcpy(config.log_file, "/tmp/memory_utils.log");
    
    memory_utils_init(&config);
}

/**
 * __attribute__((destructor)) - Library destructor
 */
static void __attribute__((destructor)) memory_utils_library_cleanup(void)
{
    memory_utils_cleanup();
}
