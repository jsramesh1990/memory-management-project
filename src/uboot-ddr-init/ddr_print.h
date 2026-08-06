/**
 * ddr_print.h - DDR Print Header for U-Boot
 * 
 * This header defines the debug printing functions for DDR initialization.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_PRINT_H_
#define _DDR_PRINT_H_

#include <common.h>

/* ============================================================================
 * Print Levels
 * ============================================================================ */

#define DDR_PRINT_DEBUG     0
#define DDR_PRINT_INFO      1
#define DDR_PRINT_WARNING   2
#define DDR_PRINT_ERROR     3
#define DDR_PRINT_NONE      4

/* ============================================================================
 * Print Macros
 * ============================================================================ */

/* Set the current print level (default: INFO) */
extern int ddr_print_level;

#define ddr_printf(level, fmt, ...) \
    do { \
        if (level >= ddr_print_level) { \
            switch (level) { \
                case DEBUG: \
                    printf("[DDR-DBG] " fmt, ##__VA_ARGS__); \
                    break; \
                case INFO: \
                    printf("[DDR-INF] " fmt, ##__VA_ARGS__); \
                    break; \
                case WARNING: \
                    printf("[DDR-WRN] " fmt, ##__VA_ARGS__); \
                    break; \
                case ERROR: \
                    printf("[DDR-ERR] " fmt, ##__VA_ARGS__); \
                    break; \
                default: \
                    printf("[DDR] " fmt, ##__VA_ARGS__); \
                    break; \
            } \
        } \
    } while (0)

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * ddr_set_print_level - Set print level
 * @level: Print level (DEBUG, INFO, WARNING, ERROR, NONE)
 */
void ddr_set_print_level(int level);

/**
 * ddr_get_print_level - Get current print level
 * 
 * Return: Current print level
 */
int ddr_get_print_level(void);

/**
 * ddr_print_hexdump - Print hex dump of memory
 * @addr: Memory address
 * @size: Size to dump
 * @prefix: Prefix string
 */
void ddr_print_hexdump(phys_addr_t addr, size_t size, const char *prefix);

/**
 * ddr_print_buffer - Print buffer contents
 * @buf: Buffer pointer
 * @size: Buffer size
 * @prefix: Prefix string
 */
void ddr_print_buffer(const void *buf, size_t size, const char *prefix);

/**
 * ddr_print_regs - Print DDR register values
 */
void ddr_print_regs(void);

/**
 * ddr_print_timing_table - Print timing table
 */
void ddr_print_timing_table(void);

#endif /* _DDR_PRINT_H_ */
