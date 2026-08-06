/**
 * memory_utils.h - Memory Utilities Library Header
 * 
 * This header defines utility functions for memory management,
 * debugging, and performance monitoring on RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 * 
 * Compilation:
 *   gcc -o app app.c -lmemory_utils
 * 
 * Usage:
 *   #include <memory_utils.h>
 *   memory_utils_init();
 *   memory_utils_memdump(ptr, 64);
 *   memory_utils_cleanup();
 */

#ifndef _MEMORY_UTILS_H_
#define _MEMORY_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */

#define MEMORY_UTILS_VERSION_MAJOR  1
#define MEMORY_UTILS_VERSION_MINOR  0
#define MEMORY_UTILS_VERSION_PATCH  0
#define MEMORY_UTILS_VERSION_STRING "1.0.0"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MEMORY_UTILS_BUFFER_SIZE    4096
#define MEMORY_UTILS_MAX_BLOCKS     1024
#define MEMORY_UTILS_HISTORY_SIZE   100
#define MEMORY_UTILS_ALIGNMENT      64

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define MEM_UTILS_SUCCESS           0
#define MEM_UTILS_ERR_INVALID       -1
#define MEM_UTILS_ERR_NOMEM         -2
#define MEM_UTILS_ERR_NOT_INIT      -3
#define MEM_UTILS_ERR_IO            -4
#define MEM_UTILS_ERR_OVERFLOW      -5
#define MEM_UTILS_ERR_NOT_FOUND     -6

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * enum memory_utils_log_level - Log levels
 */
enum memory_utils_log_level {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,
};

/**
 * enum memory_utils_pattern - Test patterns
 */
enum memory_utils_pattern {
    PATTERN_ZERO = 0x00,
    PATTERN_ONE = 0x01,
    PATTERN_AA = 0xAA,
    PATTERN_55 = 0x55,
    PATTERN_FF = 0xFF,
    PATTERN_DEAD = 0xDE,
    PATTERN_BEEF = 0xBE,
    PATTERN_RANDOM = 0xFF,
};

/**
 * struct memory_block - Memory block descriptor
 */
struct memory_block {
    void *address;
    size_t size;
    unsigned int flags;
    time_t alloc_time;
    time_t free_time;
    char file[256];
    int line;
    char function[64];
    struct memory_block *next;
    struct memory_block *prev;
};

/**
 * struct memory_stats - Memory statistics
 */
struct memory_stats {
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    unsigned long total_blocks;
    unsigned long current_blocks;
    unsigned long peak_blocks;
    unsigned int errors;
    unsigned int warnings;
    double fragmentation;
    struct timespec start_time;
    struct timespec end_time;
};

/**
 * struct memory_trace - Memory trace entry
 */
struct memory_trace {
    void *address;
    size_t size;
    time_t timestamp;
    char operation[16];
    char file[256];
    int line;
    char function[64];
};

/**
 * struct memory_utils_config - Memory utilities configuration
 */
struct memory_utils_config {
    int log_level;
    int trace_enabled;
    int leak_detection_enabled;
    int block_tracking_enabled;
    int auto_cleanup_enabled;
    size_t min_trace_size;
    size_t max_trace_size;
    char log_file[256];
    int log_to_file;
};

/* ============================================================================
 * Core Functions
 * ============================================================================ */

/**
 * memory_utils_init - Initialize memory utilities
 * @config: Configuration (NULL for defaults)
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_init(struct memory_utils_config *config);

/**
 * memory_utils_cleanup - Clean up memory utilities
 */
void memory_utils_cleanup(void);

/**
 * memory_utils_is_initialized - Check if initialized
 * 
 * Return: true if initialized, false otherwise
 */
bool memory_utils_is_initialized(void);

/**
 * memory_utils_get_config - Get current configuration
 * @config: Pointer to config structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_get_config(struct memory_utils_config *config);

/**
 * memory_utils_set_config - Set configuration
 * @config: Pointer to config structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_set_config(struct memory_utils_config *config);

/* ============================================================================
 * Memory Tracking Functions
 * ============================================================================ */

/**
 * memory_utils_track_alloc - Track memory allocation
 * @ptr: Pointer to allocated memory
 * @size: Size of allocation
 * @file: Source file name
 * @line: Line number
 * @function: Function name
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_track_alloc(void *ptr, size_t size, 
                             const char *file, int line, 
                             const char *function);

/**
 * memory_utils_track_free - Track memory free
 * @ptr: Pointer to memory being freed
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_track_free(void *ptr);

/**
 * memory_utils_find_block - Find memory block
 * @ptr: Pointer to find
 * @block: Pointer to block structure (output)
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_find_block(void *ptr, struct memory_block *block);

/**
 * memory_utils_get_stats - Get memory statistics
 * @stats: Pointer to stats structure (output)
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_get_stats(struct memory_stats *stats);

/**
 * memory_utils_detect_leaks - Detect memory leaks
 * @threshold: Minimum leak size to report
 * 
 * Return: Number of leaks detected, negative on error
 */
int memory_utils_detect_leaks(size_t threshold);

/**
 * memory_utils_print_leaks - Print memory leak report
 */
void memory_utils_print_leaks(void);

/**
 * memory_utils_clear_leaks - Clear leak detection data
 */
void memory_utils_clear_leaks(void);

/* ============================================================================
 * Memory Debug Functions
 * ============================================================================ */

/**
 * memory_utils_memdump - Dump memory in hex format
 * @ptr: Pointer to memory
 * @size: Size to dump
 * @title: Title for the dump
 */
void memory_utils_memdump(const void *ptr, size_t size, const char *title);

/**
 * memory_utils_memcheck - Check memory for corruption
 * @ptr: Pointer to memory
 * @size: Size to check
 * @pattern: Expected pattern
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_memcheck(const void *ptr, size_t size, 
                          unsigned char pattern);

/**
 * memory_utils_memfill - Fill memory with pattern
 * @ptr: Pointer to memory
 * @size: Size to fill
 * @pattern: Pattern to fill
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_memfill(void *ptr, size_t size, unsigned char pattern);

/**
 * memory_utils_memtest - Test memory region
 * @ptr: Pointer to memory
 * @size: Size to test
 * @iterations: Number of iterations
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_memtest(void *ptr, size_t size, int iterations);

/**
 * memory_utils_memcmp_pattern - Compare memory to pattern
 * @ptr: Pointer to memory
 * @size: Size to compare
 * @pattern: Pattern to compare against
 * 
 * Return: 0 on match, negative error code on mismatch
 */
int memory_utils_memcmp_pattern(const void *ptr, size_t size, 
                                unsigned char pattern);

/**
 * memory_utils_memfind - Find pattern in memory
 * @haystack: Pointer to memory to search
 * @haystack_size: Size of haystack
 * @needle: Pattern to find
 * @needle_size: Size of pattern
 * @offset: Offset of found pattern (output)
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_memfind(const void *haystack, size_t haystack_size,
                         const void *needle, size_t needle_size,
                         size_t *offset);

/* ============================================================================
 * Memory Access Functions
 * ============================================================================ */

/**
 * memory_utils_read8 - Read 8-bit value from memory
 * @addr: Memory address
 * 
 * Return: Read value
 */
uint8_t memory_utils_read8(volatile void *addr);

/**
 * memory_utils_read16 - Read 16-bit value from memory
 * @addr: Memory address
 * 
 * Return: Read value
 */
uint16_t memory_utils_read16(volatile void *addr);

/**
 * memory_utils_read32 - Read 32-bit value from memory
 * @addr: Memory address
 * 
 * Return: Read value
 */
uint32_t memory_utils_read32(volatile void *addr);

/**
 * memory_utils_read64 - Read 64-bit value from memory
 * @addr: Memory address
 * 
 * Return: Read value
 */
uint64_t memory_utils_read64(volatile void *addr);

/**
 * memory_utils_write8 - Write 8-bit value to memory
 * @addr: Memory address
 * @value: Value to write
 */
void memory_utils_write8(volatile void *addr, uint8_t value);

/**
 * memory_utils_write16 - Write 16-bit value to memory
 * @addr: Memory address
 * @value: Value to write
 */
void memory_utils_write16(volatile void *addr, uint16_t value);

/**
 * memory_utils_write32 - Write 32-bit value to memory
 * @addr: Memory address
 * @value: Value to write
 */
void memory_utils_write32(volatile void *addr, uint32_t value);

/**
 * memory_utils_write64 - Write 64-bit value to memory
 * @addr: Memory address
 * @value: Value to write
 */
void memory_utils_write64(volatile void *addr, uint64_t value);

/* ============================================================================
 * Performance Monitoring
 * ============================================================================ */

/**
 * memory_utils_get_time_ns - Get current time in nanoseconds
 * 
 * Return: Current time in nanoseconds
 */
uint64_t memory_utils_get_time_ns(void);

/**
 * memory_utils_get_time_us - Get current time in microseconds
 * 
 * Return: Current time in microseconds
 */
uint64_t memory_utils_get_time_us(void);

/**
 * memory_utils_get_time_ms - Get current time in milliseconds
 * 
 * Return: Current time in milliseconds
 */
uint64_t memory_utils_get_time_ms(void);

/**
 * memory_utils_measure_start - Start performance measurement
 * 
 * Return: Start timestamp
 */
uint64_t memory_utils_measure_start(void);

/**
 * memory_utils_measure_stop - Stop performance measurement
 * @start: Start timestamp from memory_utils_measure_start()
 * 
 * Return: Elapsed time in nanoseconds
 */
uint64_t memory_utils_measure_stop(uint64_t start);

/**
 * memory_utils_measure_operation - Measure operation time
 * @name: Operation name
 * @func: Function to measure
 * @arg: Function argument
 * 
 * Return: Elapsed time in nanoseconds
 */
uint64_t memory_utils_measure_operation(const char *name,
                                        void *(*func)(void *),
                                        void *arg);

/* ============================================================================
 * Memory Pool Functions
 * ============================================================================ */

/**
 * memory_utils_pool_create - Create memory pool
 * @name: Pool name
 * @size: Pool size
 * @align: Alignment requirement
 * 
 * Return: Pool handle, negative on error
 */
int memory_utils_pool_create(const char *name, size_t size, size_t align);

/**
 * memory_utils_pool_destroy - Destroy memory pool
 * @pool_id: Pool ID
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_pool_destroy(int pool_id);

/**
 * memory_utils_pool_alloc - Allocate from pool
 * @pool_id: Pool ID
 * @size: Size to allocate
 * 
 * Return: Pointer to allocated memory, NULL on failure
 */
void *memory_utils_pool_alloc(int pool_id, size_t size);

/**
 * memory_utils_pool_free - Free to pool
 * @pool_id: Pool ID
 * @ptr: Pointer to free
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_pool_free(int pool_id, void *ptr);

/**
 * memory_utils_pool_stats - Get pool statistics
 * @pool_id: Pool ID
 * @used: Output used size
 * @free: Output free size
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_pool_stats(int pool_id, size_t *used, size_t *free);

/**
 * memory_utils_pool_reset - Reset pool
 * @pool_id: Pool ID
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_pool_reset(int pool_id);

/* ============================================================================
 * Logging Functions
 * ============================================================================ */

/**
 * memory_utils_log - Log message
 * @level: Log level
 * @format: Format string
 * @...: Variable arguments
 */
void memory_utils_log(int level, const char *format, ...);

/**
 * memory_utils_log_error - Log error message
 * @format: Format string
 * @...: Variable arguments
 */
#define memory_utils_log_error(format, ...) \
    memory_utils_log(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

/**
 * memory_utils_log_warning - Log warning message
 * @format: Format string
 * @...: Variable arguments
 */
#define memory_utils_log_warning(format, ...) \
    memory_utils_log(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)

/**
 * memory_utils_log_info - Log info message
 * @format: Format string
 * @...: Variable arguments
 */
#define memory_utils_log_info(format, ...) \
    memory_utils_log(LOG_LEVEL_INFO, format, ##__VA_ARGS__)

/**
 * memory_utils_log_debug - Log debug message
 * @format: Format string
 * @...: Variable arguments
 */
#define memory_utils_log_debug(format, ...) \
    memory_utils_log(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

/**
 * memory_utils_log_trace - Log trace message
 * @format: Format string
 * @...: Variable arguments
 */
#define memory_utils_log_trace(format, ...) \
    memory_utils_log(LOG_LEVEL_TRACE, format, ##__VA_ARGS__)

/* ============================================================================
 * String Utilities
 * ============================================================================ */

/**
 * memory_utils_strdup - Duplicate string with memory tracking
 * @str: String to duplicate
 * 
 * Return: Duplicated string, NULL on failure
 */
char *memory_utils_strdup(const char *str);

/**
 * memory_utils_strndup - Duplicate first n characters
 * @str: String to duplicate
 * @n: Number of characters
 * 
 * Return: Duplicated string, NULL on failure
 */
char *memory_utils_strndup(const char *str, size_t n);

/**
 * memory_utils_hexdump_to_str - Convert hexdump to string
 * @ptr: Pointer to memory
 * @size: Size to dump
 * @buffer: Output buffer
 * @buffer_size: Buffer size
 * 
 * Return: Number of characters written
 */
int memory_utils_hexdump_to_str(const void *ptr, size_t size,
                                char *buffer, size_t buffer_size);

/**
 * memory_utils_str_to_hex - Convert string to hex
 * @str: Input string
 * @bytes: Output bytes
 * @max_bytes: Maximum bytes
 * 
 * Return: Number of bytes converted, negative on error
 */
int memory_utils_str_to_hex(const char *str, uint8_t *bytes, size_t max_bytes);

/* ============================================================================
 * Alignment Utilities
 * ============================================================================ */

/**
 * memory_utils_align_up - Align address up
 * @addr: Address to align
 * @align: Alignment requirement
 * 
 * Return: Aligned address
 */
uintptr_t memory_utils_align_up(uintptr_t addr, size_t align);

/**
 * memory_utils_align_down - Align address down
 * @addr: Address to align
 * @align: Alignment requirement
 * 
 * Return: Aligned address
 */
uintptr_t memory_utils_align_down(uintptr_t addr, size_t align);

/**
 * memory_utils_is_aligned - Check if address is aligned
 * @addr: Address to check
 * @align: Alignment requirement
 * 
 * Return: true if aligned, false otherwise
 */
bool memory_utils_is_aligned(uintptr_t addr, size_t align);

/**
 * memory_utils_is_power_of_two - Check if number is power of two
 * @n: Number to check
 * 
 * Return: true if power of two, false otherwise
 */
bool memory_utils_is_power_of_two(size_t n);

/**
 * memory_utils_next_power_of_two - Get next power of two
 * @n: Number
 * 
 * Return: Next power of two
 */
size_t memory_utils_next_power_of_two(size_t n);

/* ============================================================================
 * Cache Operations
 * ============================================================================ */

/**
 * memory_utils_cache_flush - Flush cache lines
 * @ptr: Pointer to memory
 * @size: Size to flush
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_cache_flush(void *ptr, size_t size);

/**
 * memory_utils_cache_invalidate - Invalidate cache lines
 * @ptr: Pointer to memory
 * @size: Size to invalidate
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_cache_invalidate(void *ptr, size_t size);

/**
 * memory_utils_cache_clean - Clean cache lines
 * @ptr: Pointer to memory
 * @size: Size to clean
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_utils_cache_clean(void *ptr, size_t size);

/* ============================================================================
 * Bit Manipulation
 * ============================================================================ */

/**
 * memory_utils_bit_set - Set bit
 * @bits: Bit array
 * @bit: Bit to set
 */
#define memory_utils_bit_set(bits, bit) ((bits)[(bit) / 8] |= (1 << ((bit) % 8)))

/**
 * memory_utils_bit_clear - Clear bit
 * @bits: Bit array
 * @bit: Bit to clear
 */
#define memory_utils_bit_clear(bits, bit) ((bits)[(bit) / 8] &= ~(1 << ((bit) % 8)))

/**
 * memory_utils_bit_test - Test bit
 * @bits: Bit array
 * @bit: Bit to test
 * 
 * Return: true if set, false otherwise
 */
#define memory_utils_bit_test(bits, bit) (((bits)[(bit) / 8] >> ((bit) % 8)) & 1)

/**
 * memory_utils_bit_toggle - Toggle bit
 * @bits: Bit array
 * @bit: Bit to toggle
 */
#define memory_utils_bit_toggle(bits, bit) ((bits)[(bit) / 8] ^= (1 << ((bit) % 8)))

/* ============================================================================
 * Endian Conversion
 * ============================================================================ */

/**
 * memory_utils_htobe16 - Host to big-endian 16-bit
 */
static inline uint16_t memory_utils_htobe16(uint16_t x)
{
    return __builtin_bswap16(x);
}

/**
 * memory_utils_be16toh - Big-endian to host 16-bit
 */
static inline uint16_t memory_utils_be16toh(uint16_t x)
{
    return __builtin_bswap16(x);
}

/**
 * memory_utils_htobe32 - Host to big-endian 32-bit
 */
static inline uint32_t memory_utils_htobe32(uint32_t x)
{
    return __builtin_bswap32(x);
}

/**
 * memory_utils_be32toh - Big-endian to host 32-bit
 */
static inline uint32_t memory_utils_be32toh(uint32_t x)
{
    return __builtin_bswap32(x);
}

/**
 * memory_utils_htobe64 - Host to big-endian 64-bit
 */
static inline uint64_t memory_utils_htobe64(uint64_t x)
{
    return __builtin_bswap64(x);
}

/**
 * memory_utils_be64toh - Big-endian to host 64-bit
 */
static inline uint64_t memory_utils_be64toh(uint64_t x)
{
    return __builtin_bswap64(x);
}

#ifdef __cplusplus
}
#endif

#endif /* _MEMORY_UTILS_H_ */
