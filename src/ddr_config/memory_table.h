/**
 * memory_table.h - Memory Table Definitions
 * 
 * This header defines memory table structures and constants for
 * DDR memory management on RK3568.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _MEMORY_TABLE_H_
#define _MEMORY_TABLE_H_

#include <linux/types.h>

/* ============================================================================
 * Memory Table Entry Types
 * ============================================================================ */

#define MEM_TYPE_SYSTEM     0x0001
#define MEM_TYPE_BOOT       0x0002
#define MEM_TYPE_KERNEL     0x0004
#define MEM_TYPE_USER       0x0008
#define MEM_TYPE_DMA        0x0010
#define MEM_TYPE_NPU        0x0020
#define MEM_TYPE_GPU        0x0040
#define MEM_TYPE_VPU        0x0080
#define MEM_TYPE_SECURE     0x0100
#define MEM_TYPE_CACHE      0x0200
#define MEM_TYPE_IO         0x0400
#define MEM_TYPE_RESERVED   0x0800

/* ============================================================================
 * Memory Table Entry Flags
 * ============================================================================ */

#define MEM_FLAG_READ       0x0001
#define MEM_FLAG_WRITE      0x0002
#define MEM_FLAG_EXEC       0x0004
#define MEM_FLAG_CACHE      0x0008
#define MEM_FLAG_IO         0x0010
#define MEM_FLAG_DMA        0x0020
#define MEM_FLAG_CONTIG     0x0040
#define MEM_FLAG_SECURE     0x0080
#define MEM_FLAG_SHARED     0x0100
#define MEM_FLAG_PERSISTENT 0x0200
#define MEM_FLAG_NO_MAP     0x0400
#define MEM_FLAG_ZERO_INIT  0x0800

/* ============================================================================
 * Memory Table Entry Structure
 * ============================================================================ */

/**
 * struct memory_table_entry - Memory table entry
 */
struct memory_table_entry {
    const char *name;          /* Entry name */
    phys_addr_t start;         /* Start address */
    phys_size_t size;          /* Size in bytes */
    unsigned int type;         /* Entry type */
    unsigned int flags;        /* Entry flags */
    unsigned int priority;     /* Access priority (0-10) */
    unsigned int cache_policy; /* Cache policy */
    unsigned int reserved[4];  /* Reserved for future use */
};

/**
 * struct memory_table - Memory table structure
 */
struct memory_table {
    unsigned int version;                  /* Table version */
    unsigned int num_entries;              /* Number of entries */
    struct memory_table_entry *entries;    /* Array of entries */
    unsigned int total_size;               /* Total memory size */
    unsigned int used_size;                /* Used memory size */
    unsigned int free_size;                /* Free memory size */
    unsigned int reserved[8];              /* Reserved for future use */
};

/* ============================================================================
 * Memory Table Functions
 * ============================================================================ */

/**
 * memory_table_init - Initialize memory table
 * @table: Pointer to memory table
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_table_init(struct memory_table *table);

/**
 * memory_table_add - Add entry to memory table
 * @table: Pointer to memory table
 * @entry: Pointer to entry to add
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_table_add(struct memory_table *table, struct memory_table_entry *entry);

/**
 * memory_table_remove - Remove entry from memory table
 * @table: Pointer to memory table
 * @name: Name of entry to remove
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_table_remove(struct memory_table *table, const char *name);

/**
 * memory_table_find - Find entry in memory table
 * @table: Pointer to memory table
 * @name: Name of entry to find
 * @entry: Pointer to store found entry
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_table_find(struct memory_table *table, const char *name,
                      struct memory_table_entry *entry);

/**
 * memory_table_get - Get entry from memory table
 * @table: Pointer to memory table
 * @index: Index of entry to get
 * @entry: Pointer to store entry
 * 
 * Return: 0 on success, negative error code on failure
 */
int memory_table_get(struct memory_table *table, unsigned int index,
                     struct memory_table_entry *entry);

/**
 * memory_table_print - Print memory table
 * @table: Pointer to memory table
 */
void memory_table_print(struct memory_table *table);

/**
 * memory_table_cleanup - Clean up memory table
 * @table: Pointer to memory table
 */
void memory_table_cleanup(struct memory_table *table);

/* ============================================================================
 * Memory Table Constants
 * ============================================================================ */

/* Memory Table Version */
#define MEMORY_TABLE_VERSION 1

/* Memory Table Sizes */
#define MEMORY_TABLE_MAX_ENTRIES 64

/* Memory Table Default Entries */
extern const struct memory_table_entry memory_table_default_entries[];

#endif /* _MEMORY_TABLE_H_ */
