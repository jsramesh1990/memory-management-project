/**
 * ddr_partition.c - DDR Partition Management for U-Boot
 * 
 * This file implements DDR partition management for RK3568.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#include <common.h>
#include <malloc.h>
#include <string.h>
#include "ddr_partition.h"
#include "ddr_init.h"
#include "ddr_print.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DDR_PARTITION_TABLE_VERSION 1
#define DDR_MAX_PARTITIONS 32

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static struct ddr_partition_table g_partition_table;
static bool g_partition_initialized = false;

/* ============================================================================
 * Default Partitions
 * ============================================================================ */

const struct ddr_partition ddr_default_partitions[] = {
    {
        .name = "bootloader",
        .id = 1,
        .start = 0x00000000,
        .size = 0x01000000,      /* 16 MB */
        .type = PART_TYPE_BOOTLOADER,
        .flags = PART_FLAG_READONLY | PART_FLAG_BOOTABLE,
        .priority = 10,
    },
    {
        .name = "uboot",
        .id = 2,
        .start = 0x01000000,
        .size = 0x01000000,      /* 16 MB */
        .type = PART_TYPE_UBOOT,
        .flags = PART_FLAG_READONLY,
        .priority = 9,
    },
    {
        .name = "kernel",
        .id = 3,
        .start = 0x02000000,
        .size = 0x0E000000,      /* 224 MB */
        .type = PART_TYPE_KERNEL,
        .flags = PART_FLAG_READONLY,
        .priority = 8,
    },
    {
        .name = "dtb",
        .id = 4,
        .start = 0x10000000,
        .size = 0x01000000,      /* 16 MB */
        .type = PART_TYPE_DTB,
        .flags = PART_FLAG_READONLY,
        .priority = 7,
    },
    {
        .name = "reserved",
        .id = 5,
        .start = 0x11000000,
        .size = 0x0F000000,      /* 240 MB */
        .type = PART_TYPE_RESERVED,
        .flags = PART_FLAG_READONLY,
        .priority = 6,
    },
    {
        .name = "npu",
        .id = 6,
        .start = 0x20000000,
        .size = 0x08000000,      /* 128 MB */
        .type = PART_TYPE_NPU,
        .flags = PART_FLAG_DMA | PART_FLAG_CONTIGUOUS,
        .priority = 5,
    },
    {
        .name = "gpu",
        .id = 7,
        .start = 0x28000000,
        .size = 0x08000000,      /* 128 MB */
        .type = PART_TYPE_GPU,
        .flags = PART_FLAG_DMA | PART_FLAG_CONTIGUOUS,
        .priority = 4,
    },
    {
        .name = "vpu",
        .id = 8,
        .start = 0x30000000,
        .size = 0x10000000,      /* 256 MB */
        .type = PART_TYPE_VPU,
        .flags = PART_FLAG_DMA | PART_FLAG_CONTIGUOUS,
        .priority = 3,
    },
    {
        .name = "system",
        .id = 9,
        .start = 0x40000000,
        .size = 0xC0000000,      /* 3 GB */
        .type = PART_TYPE_SYSTEM,
        .flags = 0,
        .priority = 2,
    },
};

/* ============================================================================
 * Partition Management Functions
 * ============================================================================ */

/**
 * ddr_partition_init - Initialize partition table
 */
int ddr_partition_init(void)
{
    unsigned int i;
    struct ddr_partition *part;
    
    ddr_printf(DEBUG, "DDR Partition: Initializing...\n");
    
    if (g_partition_initialized) {
        ddr_printf(WARNING, "DDR Partition: Already initialized\n");
        return 0;
    }
    
    /* Initialize partition table */
    memset(&g_partition_table, 0, sizeof(g_partition_table));
    g_partition_table.version = DDR_PARTITION_TABLE_VERSION;
    g_partition_table.num_partitions = 0;
    
    /* Allocate partition array */
    g_partition_table.partitions = malloc(sizeof(struct ddr_partition) *
                                          DDR_MAX_PARTITIONS);
    if (!g_partition_table.partitions) {
        ddr_printf(ERROR, "DDR Partition: Failed to allocate partitions\n");
        return -ENOMEM;
    }
    
    /* Add default partitions */
    for (i = 0; i < ARRAY_SIZE(ddr_default_partitions); i++) {
        part = (struct ddr_partition *)&ddr_default_partitions[i];
        if (ddr_partition_add(part) < 0) {
            ddr_printf(ERROR, "DDR Partition: Failed to add default partition %s\n",
                       part->name);
        }
    }
    
    g_partition_initialized = true;
    
    ddr_printf(INFO, "DDR Partition: Initialized with %u partitions\n",
               g_partition_table.num_partitions);
    
    return 0;
}

/**
 * ddr_partition_add - Add partition to table
 */
int ddr_partition_add(struct ddr_partition *partition)
{
    struct ddr_partition *part;
    unsigned int i;
    
    if (!partition || !partition->name) {
        ddr_printf(ERROR, "DDR Partition: Invalid partition\n");
        return -EINVAL;
    }
    
    if (!g_partition_initialized) {
        ddr_printf(ERROR, "DDR Partition: Not initialized\n");
        return -ENODEV;
    }
    
    if (g_partition_table.num_partitions >= DDR_MAX_PARTITIONS) {
        ddr_printf(ERROR, "DDR Partition: Max partitions reached\n");
        return -ENOSPC;
    }
    
    /* Check for duplicate name */
    for (i = 0; i < g_partition_table.num_partitions; i++) {
        part = &g_partition_table.partitions[i];
        if (strcmp(part->name, partition->name) == 0) {
            ddr_printf(ERROR, "DDR Partition: Duplicate partition name: %s\n",
                       partition->name);
            return -EEXIST;
        }
    }
    
    /* Add partition */
    part = &g_partition_table.partitions[g_partition_table.num_partitions];
    memcpy(part, partition, sizeof(*part));
    part->id = g_partition_table.num_partitions + 1;
    g_partition_table.num_partitions++;
    g_partition_table.total_size += partition->size;
    
    ddr_printf(DEBUG, "DDR Partition: Added partition %s (ID: %u, Size: %lu MB)\n",
               partition->name, part->id,
               (unsigned long)partition->size / (1024 * 1024));
    
    return part->id;
}

/**
 * ddr_partition_remove - Remove partition from table
 */
int ddr_partition_remove(unsigned int id)
{
    unsigned int i, j;
    
    if (!g_partition_initialized) {
        ddr_printf(ERROR, "DDR Partition: Not initialized\n");
        return -ENODEV;
    }
    
    for (i = 0; i < g_partition_table.num_partitions; i++) {
        if (g_partition_table.partitions[i].id == id) {
            /* Remove partition */
            for (j = i; j < g_partition_table.num_partitions - 1; j++) {
                memcpy(&g_partition_table.partitions[j],
                       &g_partition_table.partitions[j + 1],
                       sizeof(struct ddr_partition));
            }
            g_partition_table.num_partitions--;
            
            ddr_printf(DEBUG, "DDR Partition: Removed partition ID %u\n", id);
            return 0;
        }
    }
    
    ddr_printf(ERROR, "DDR Partition: Partition ID %u not found\n", id);
    return -ENOENT;
}

/**
 * ddr_partition_get - Get partition by ID
 */
int ddr_partition_get(unsigned int id, struct ddr_partition *partition)
{
    unsigned int i;
    
    if (!partition) {
        return -EINVAL;
    }
    
    if (!g_partition_initialized) {
        return -ENODEV;
    }
    
    for (i = 0; i < g_partition_table.num_partitions; i++) {
        if (g_partition_table.partitions[i].id == id) {
            memcpy(partition, &g_partition_table.partitions[i],
                   sizeof(*partition));
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * ddr_partition_find - Find partition by name
 */
int ddr_partition_find(const char *name, struct ddr_partition *partition)
{
    unsigned int i;
    
    if (!name) {
        return -EINVAL;
    }
    
    if (!g_partition_initialized) {
        return -ENODEV;
    }
    
    for (i = 0; i < g_partition_table.num_partitions; i++) {
        if (strcmp(g_partition_table.partitions[i].name, name) == 0) {
            if (partition) {
                memcpy(partition, &g_partition_table.partitions[i],
                       sizeof(*partition));
            }
            return g_partition_table.partitions[i].id;
        }
    }
    
    return -ENOENT;
}

/**
 * ddr_partition_get_by_type - Get partition by type
 */
int ddr_partition_get_by_type(unsigned int type, unsigned int index,
                              struct ddr_partition *partition)
{
    unsigned int i;
    unsigned int count = 0;
    
    if (!partition) {
        return -EINVAL;
    }
    
    if (!g_partition_initialized) {
        return -ENODEV;
    }
    
    for (i = 0; i < g_partition_table.num_partitions; i++) {
        if (g_partition_table.partitions[i].type == type) {
            if (count == index) {
                memcpy(partition, &g_partition_table.partitions[i],
                       sizeof(*partition));
                return 0;
            }
            count++;
        }
    }
    
    return -ENOENT;
}

/**
 * ddr_partition_get_count - Get number of partitions
 */
unsigned int ddr_partition_get_count(void)
{
    if (!g_partition_initialized) {
        return 0;
    }
    
    return g_partition_table.num_partitions;
}

/**
 * ddr_partition_dump - Dump partition table
 */
void ddr_partition_dump(void)
{
    unsigned int i;
    struct ddr_partition *part;
    
    if (!g_partition_initialized) {
        ddr_printf(WARNING, "DDR Partition: Not initialized\n");
        return;
    }
    
    ddr_printf(INFO, "\n=== DDR Partition Table ===\n");
    ddr_printf(INFO, "Version: %u\n", g_partition_table.version);
    ddr_printf(INFO, "Total Partitions: %u\n", g_partition_table.num_partitions);
    ddr_printf(INFO, "Total Size: %lu MB\n",
               (unsigned long)g_partition_table.total_size / (1024 * 1024));
    ddr_printf(INFO, "Used Size: %lu MB\n",
               (unsigned long)g_partition_table.used_size / (1024 * 1024));
    ddr_printf(INFO, "Free Size: %lu MB\n\n",
               (unsigned long)g_partition_table.free_size / (1024 * 1024));
    
    ddr_printf(INFO, "ID  Name          Start       End         Size (MB)  Type  Flags\n");
    ddr_printf(INFO, "--  ----          -----       ---         --------  ----  -----\n");
    
    for (i = 0; i < g_partition_table.num_partitions; i++) {
        part = &g_partition_table.partitions[i];
        ddr_printf(INFO, "%2u  %-12s 0x%08lx - 0x%08lx %8lu    0x%04x 0x%04x\n",
                   part->id, part->name,
                   (unsigned long)part->start,
                   (unsigned long)(part->start + part->size),
                   (unsigned long)part->size / (1024 * 1024),
                   part->type, part->flags);
    }
    
    ddr_printf(INFO, "=============================\n");
}

/**
 * ddr_partition_print - Print partition information
 */
void ddr_partition_print(struct ddr_partition *partition)
{
    if (!partition) {
        return;
    }
    
    ddr_printf(INFO, "\n=== Partition Information ===\n");
    ddr_printf(INFO, "Name: %s\n", partition->name);
    ddr_printf(INFO, "ID: %u\n", partition->id);
    ddr_printf(INFO, "Start: 0x%08lx\n", (unsigned long)partition->start);
    ddr_printf(INFO, "End: 0x%08lx\n",
               (unsigned long)(partition->start + partition->size));
    ddr_printf(INFO, "Size: %lu MB\n",
               (unsigned long)partition->size / (1024 * 1024));
    ddr_printf(INFO, "Type: 0x%04x\n", partition->type);
    ddr_printf(INFO, "Flags: 0x%04x\n", partition->flags);
    ddr_printf(INFO, "Priority: %u\n", partition->priority);
    ddr_printf(INFO, "Ref Count: %u\n", partition->ref_count);
    ddr_printf(INFO, "============================\n");
}

/**
 * ddr_partition_cleanup - Clean up partition table
 */
void ddr_partition_cleanup(void)
{
    if (!g_partition_initialized) {
        return;
    }
    
    if (g_partition_table.partitions) {
        free(g_partition_table.partitions);
        g_partition_table.partitions = NULL;
    }
    
    memset(&g_partition_table, 0, sizeof(g_partition_table));
    g_partition_initialized = false;
    
    ddr_printf(DEBUG, "DDR Partition: Cleaned up\n");
}

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("DDR Partition Management for RK3568 U-Boot");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
