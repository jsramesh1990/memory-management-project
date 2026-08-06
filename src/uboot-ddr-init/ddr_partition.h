/**
 * ddr_partition.h - DDR Partition Header for U-Boot
 * 
 * This header defines the DDR partition management functions and structures.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_PARTITION_H_
#define _DDR_PARTITION_H_

#include <common.h>
#include <asm/types.h>
#include "ddr_init.h"

/* ============================================================================
 * Partition Constants
 * ============================================================================ */

/* Partition Types */
#define PART_TYPE_BOOTLOADER    0x0001
#define PART_TYPE_UBOOT         0x0002
#define PART_TYPE_KERNEL        0x0004
#define PART_TYPE_DTB           0x0008
#define PART_TYPE_RESERVED      0x0010
#define PART_TYPE_SYSTEM        0x0020
#define PART_TYPE_USER          0x0040
#define PART_TYPE_NPU           0x0080
#define PART_TYPE_GPU           0x0100
#define PART_TYPE_VPU           0x0200
#define PART_TYPE_DMA           0x0400
#define PART_TYPE_SECURE        0x0800

/* Partition Flags */
#define PART_FLAG_READONLY      0x0001
#define PART_FLAG_BOOTABLE      0x0002
#define PART_FLAG_HIDDEN        0x0004
#define PART_FLAG_SECURE        0x0008
#define PART_FLAG_CACHE         0x0010
#define PART_FLAG_DMA           0x0020
#define PART_FLAG_CONTIGUOUS    0x0040

/* ============================================================================
 * Partition Structures
 * ============================================================================ */

/**
 * struct ddr_partition - DDR partition
 */
struct ddr_partition {
    const char *name;          /* Partition name */
    unsigned int id;           /* Partition ID */
    phys_addr_t start;         /* Start address */
    phys_size_t size;          /* Size in bytes */
    unsigned int type;         /* Partition type */
    unsigned int flags;        /* Partition flags */
    unsigned int priority;     /* Access priority */
    unsigned int ref_count;    /* Reference count */
};

/**
 * struct ddr_partition_table - DDR partition table
 */
struct ddr_partition_table {
    unsigned int version;                     /* Table version */
    unsigned int num_partitions;              /* Number of partitions */
    struct ddr_partition *partitions;         /* Partition array */
    unsigned int total_size;                  /* Total size */
    unsigned int used_size;                   /* Used size */
    unsigned int free_size;                   /* Free size */
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * ddr_partition_init - Initialize partition table
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_partition_init(void);

/**
 * ddr_partition_add - Add partition to table
 * @partition: Pointer to partition structure
 * 
 * Return: Partition ID on success, negative error code on failure
 */
int ddr_partition_add(struct ddr_partition *partition);

/**
 * ddr_partition_remove - Remove partition from table
 * @id: Partition ID
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_partition_remove(unsigned int id);

/**
 * ddr_partition_get - Get partition by ID
 * @id: Partition ID
 * @partition: Pointer to store partition info
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_partition_get(unsigned int id, struct ddr_partition *partition);

/**
 * ddr_partition_find - Find partition by name
 * @name: Partition name
 * @partition: Pointer to store partition info
 * 
 * Return: Partition ID on success, negative error code on failure
 */
int ddr_partition_find(const char *name, struct ddr_partition *partition);

/**
 * ddr_partition_get_by_type - Get partition by type
 * @type: Partition type
 * @index: Index to retrieve
 * @partition: Pointer to store partition info
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_partition_get_by_type(unsigned int type, unsigned int index,
                              struct ddr_partition *partition);

/**
 * ddr_partition_get_count - Get number of partitions
 * 
 * Return: Number of partitions
 */
unsigned int ddr_partition_get_count(void);

/**
 * ddr_partition_dump - Dump partition table
 */
void ddr_partition_dump(void);

/**
 * ddr_partition_print - Print partition information
 * @partition: Pointer to partition structure
 */
void ddr_partition_print(struct ddr_partition *partition);

/**
 * ddr_partition_cleanup - Clean up partition table
 */
void ddr_partition_cleanup(void);

/* ============================================================================
 * Default Partition Definitions
 * ============================================================================ */

/* Default partitions for RK3568 */
extern const struct ddr_partition ddr_default_partitions[];

#endif /* _DDR_PARTITION_H_ */
