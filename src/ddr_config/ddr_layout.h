/**
 * ddr_layout.h - DDR Layout Header
 * 
 * This header defines the DDR memory layout structures and functions.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_LAYOUT_H_
#define _DDR_LAYOUT_H_

#include <linux/types.h>
#include "ddr_config.h"

/* ============================================================================
 * Layout Functions
 * ============================================================================ */

/**
 * ddr_layout_init - Initialize DDR layout
 * @board_type: Board type identifier
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_layout_init(enum board_type board_type);

/**
 * ddr_layout_get - Get current DDR layout
 * @layout: Pointer to layout structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_layout_get(struct ddr_layout *layout);

/**
 * ddr_layout_set - Set DDR layout
 * @layout: Pointer to layout structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_layout_set(struct ddr_layout *layout);

/**
 * ddr_layout_region_info - Get information about a region
 * @name: Region name
 * @start: Pointer to store start address
 * @size: Pointer to store region size
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_layout_region_info(const char *name, phys_addr_t *start, phys_size_t *size);

/**
 * ddr_layout_find_free - Find free memory region
 * @size: Required size
 * @start: Pointer to store start address
 * @limit: Maximum address limit
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_layout_find_free(size_t size, phys_addr_t *start, phys_addr_t limit);

/**
 * ddr_layout_print - Print layout information
 */
void ddr_layout_print(void);

/**
 * ddr_layout_cleanup - Clean up DDR layout
 */
void ddr_layout_cleanup(void);

#endif /* _DDR_LAYOUT_H_ */
